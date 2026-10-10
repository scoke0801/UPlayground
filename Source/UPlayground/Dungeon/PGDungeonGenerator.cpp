#include "Dungeon/PGDungeonGenerator.h"
#include "PGData/DataAsset/Dungeon/PGDungeonDefinition.h"
#include "PGData/Dungeon/PGDungeonLayoutBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGActor/Dungeon/PGDungeonDiscoverySubsystem.h"

APGDungeonGenerator::APGDungeonGenerator()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    // Follow the player every frame, after character movement and physics have finished.
    PrimaryActorTick.TickInterval=0.f;
    PrimaryActorTick.TickGroup=TG_PostPhysics;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("DungeonRoot"));
}
UPGDungeonDiscoverySubsystem* APGDungeonGenerator::GetDiscovery() const
{
    return GetWorld() ? GetWorld()->GetSubsystem<UPGDungeonDiscoverySubsystem>() : nullptr;
}
void APGDungeonGenerator::BeginPlay()
{
    Super::BeginPlay();
    FParse::Value(FCommandLine::Get(),TEXT("PGDungeonSeed="),Seed);
    if (FParse::Param(FCommandLine::Get(),TEXT("PGDungeonCombat"))) bEnableCombat=true;
    if (FParse::Param(FCommandLine::Get(),TEXT("PGDungeonPreview"))) bEnableCombat=false;
    // Defer until GameMode has spawned the actual player agent.
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this](){ Generate(Seed); }));
}
void APGDungeonGenerator::EndPlay(const EEndPlayReason::Type Reason)
{
    ++RequestId; GetWorldTimerManager().ClearTimer(NavigationTimer);
    StopEnvironment();
    StopDungeonCombat();
    SuspendPlayer(false); Super::EndPlay(Reason);
}
void APGDungeonGenerator::SetState(EPGDungeonState NewState)
{
    if (auto* Map = GetWorld()->GetSubsystem<UPGDungeonDiscoverySubsystem>())
    {
        if (NewState == EPGDungeonState::Ready && Definition)
        {
            Map->InitializeMap(Layout, GetActorLocation(), Definition->RoomSize, Definition->RoomSize + Definition->CorridorLength);
            Map->EnableRewards(bEnableCombat);
            SetActorTickInterval(0.f);
            SetActorTickEnabled(true);
        }
        else Map->ResetMap();
    }
    State=NewState; OnStateChanged.Broadcast(State);
    if (State==EPGDungeonState::Failed) UE_LOG(LogTemp,Warning,TEXT("PGDungeon FAILED: %s"),*LastError);
    if (GEngine && GetWorld()->IsGameWorld())
        GEngine->AddOnScreenDebugMessage(uint64(GetUniqueID()), State==EPGDungeonState::Ready ? 3.f : 60.f,
            State==EPGDungeonState::Failed ? FColor::Red : FColor::White,
            State==EPGDungeonState::Ready ? TEXT("던전 탐험을 시작합니다") : State==EPGDungeonState::Failed ?
            TEXT("던전을 준비하지 못했습니다. 다시 시도해 주세요") : TEXT("던전을 준비하고 있습니다"));
}
void APGDungeonGenerator::SuspendPlayer(bool bSuspend)
{
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    auto* Player=PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
    if (!Player) return;
    if (bSuspend && !bPlayerSuspended)
    {
        Player->DisableInput(PC); Player->GetCharacterMovement()->StopMovementImmediately();
        Player->GetCharacterMovement()->DisableMovement(); bPlayerSuspended=true;
    }
    else if (!bSuspend && bPlayerSuspended)
    {
        Player->EnableInput(PC); Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking); bPlayerSuspended=false;
    }
}
void APGDungeonGenerator::ClearGeometry()
{
    StopEnvironment(); Occluders.Reset(); FadedInstanceCount=0; SelectedModules.Reset();
    for (UInstancedStaticMeshComponent* C:GeneratedComponents) if (IsValid(C)) { RemoveInstanceComponent(C); C->DestroyComponent(); }
    GeneratedComponents.Reset();
}
UInstancedStaticMeshComponent* APGDungeonGenerator::MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollision)
{
    auto* C=NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(C); C->SetupAttachment(RootComponent); C->SetMobility(EComponentMobility::Movable);
    C->SetNumCustomDataFloats(3); C->SetStaticMesh(Mesh); if (Material) C->SetMaterial(0,Material);
    C->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
    C->SetCanEverAffectNavigation(bCollision); C->RegisterComponent(); GeneratedComponents.Add(C); return C;
}
FVector APGDungeonGenerator::RoomCenter(int32 Id) const
{
    const FIntPoint P=Layout.Rooms[Id].Cell;
    return GetActorLocation()+FVector(P.X,P.Y,0)*(Definition->RoomSize+Definition->CorridorLength);
}
bool APGDungeonGenerator::Assemble()
{
    // World-aligned contract also matches the existing sanctuary's world-space materials.
    if (!GetActorRotation().IsNearlyZero() || !GetActorScale3D().Equals(FVector::OneVector))
    { LastError=TEXT("Generator requires identity rotation and scale"); return false; }
    UStaticMesh* Block=Definition->BlockMesh.LoadSynchronous();
    auto* Ground=Definition->GroundMaterial.LoadSynchronous(); auto* Wall=Definition->WallMaterial.LoadSynchronous();
    if (!Block || !Ground || !Wall) { LastError=TEXT("Missing theme block or material"); return false; }
    const FVector Size=Block->GetBoundingBox().GetSize();
    if (Size.GetMin()<=0) { LastError=TEXT("Invalid block bounds"); return false; }
    auto* Floors=MakeInstances(Block,Ground,true); auto* Walls=MakeInstances(Block,Wall,true);
    Walls->ComponentTags.Add(TEXT("PGDungeonOccluder"));
    auto Box=[&](UInstancedStaticMeshComponent* C,FVector Center,FVector Dimensions)
    {
        const FVector Scale=Dimensions/Size;
        Center-=Block->GetBoundingBox().GetCenter()*Scale;
        const int32 Index=C->AddInstance(FTransform(FQuat::Identity,Center,Scale),true);
        C->SetCustomDataValue(Index,0,Center.X,true);
        C->SetCustomDataValue(Index,1,Center.Y,true);
    };
    const float S=Definition->RoomSize, Gap=Definition->CorridorLength, W=Definition->CorridorWidth;
    const float H=Definition->WallHeight, T=80.f;
    const FIntPoint Dirs[]={ {1,0},{0,1},{-1,0},{0,-1} };
    for (const auto& R:Layout.Rooms)
    {
        const FVector Center=RoomCenter(R.Id);
        Box(Floors,Center-FVector(0,0,50),FVector(S,S,100));
        for (auto D:Dirs)
        {
            bool Open=false;
            for (const auto& E:Layout.Connections)
            {
                const int32 Other=E.A==R.Id ? E.B : E.B==R.Id ? E.A : INDEX_NONE;
                if (Other!=INDEX_NONE && Layout.Rooms[Other].Cell-R.Cell==D) Open=true;
            }
            const FVector Normal(D.X,D.Y,0), Along(-D.Y,D.X,0);
            const float Length=Open ? (S-W)*.5f : S;
            for (int32 Side=0; Side<(Open ? 2 : 1); ++Side)
            {
                const float Offset=Open ? (Side==0 ? -1.f : 1.f)*(W+Length)*.5f : 0.f;
                Box(Walls,Center+Normal*(S*.5f+T*.5f)+Along*Offset+FVector(0,0,H*.5f),
                    D.X ? FVector(T,Length,H) : FVector(Length,T,H));
            }
        }
    }
    for (const auto& E:Layout.Connections)
    {
        const FVector Center=(RoomCenter(E.A)+RoomCenter(E.B))*.5f;
        const bool X=Layout.Rooms[E.A].Cell.X!=Layout.Rooms[E.B].Cell.X;
        Box(Floors,Center-FVector(0,0,50),X ? FVector(Gap,W,100) : FVector(W,Gap,100));
        for (float Sign:{-1.f,1.f})
            Box(Walls,Center+(X ? FVector(0,Sign*(W+T)*.5f,H*.5f) : FVector(Sign*(W+T)*.5f,0,H*.5f)),
                X ? FVector(Gap,T,H) : FVector(T,Gap,H));
    }
    // Low, non-colliding woodland ground avoids floating-room silhouettes. Exterior
    // trees use an independent stream and never enter walkable room/corridor bounds.
    FBox Landscape(ForceInit);
    TArray<FBox> Exclusions;
    for (const auto& R:Layout.Rooms)
    {
        const FVector C=RoomCenter(R.Id);
        Landscape+=C;
        Exclusions.Add(FBox(C-FVector(S*.5f+500,S*.5f+500,1000),C+FVector(S*.5f+500,S*.5f+500,1000)));
    }
    for (const auto& E:Layout.Connections)
    {
        const FVector C=(RoomCenter(E.A)+RoomCenter(E.B))*.5f;
        const bool X=Layout.Rooms[E.A].Cell.X!=Layout.Rooms[E.B].Cell.X;
        const FVector Extent=X ? FVector(Gap*.5f+500,W*.5f+500,1000) : FVector(W*.5f+500,Gap*.5f+500,1000);
        Exclusions.Add(FBox(C-Extent,C+Extent));
    }
    Landscape=Landscape.ExpandBy(FVector(S*.5f+2600,S*.5f+2600,0));
    auto* Background=MakeInstances(Block,Ground,false);
    Box(Background,Landscape.GetCenter()-FVector(0,0,180),FVector(Landscape.GetSize().X,Landscape.GetSize().Y,100));
    Background->SetCustomDataValue(0,0,1000000.f,true);
    Background->SetCustomDataValue(0,1,1000000.f,true);
    return AssembleModules();
}
bool APGDungeonGenerator::GeneratePreview()
{
    if (GetWorld()->IsGameWorld() || !Definition) return false;
    ++RequestId; GetWorldTimerManager().ClearTimer(NavigationTimer); ClearGeometry(); LastError.Reset();
    bool bBuilt=false;
    for (int32 Try=0; Try<FMath::Clamp(Definition->MaxAttempts,1,3); ++Try)
        if (PGDungeon::Build(*Definition,Seed,Try,Layout,LastError)) { bBuilt=true; break; }
    if (!bBuilt) bBuilt=PGDungeon::Build(*Definition,Seed,3,Layout,LastError,true);
    if (!bBuilt || !Assemble())
    { ClearGeometry(); SetState(EPGDungeonState::Failed); return false; }
    if (!StartEnvironment()) { ClearGeometry(); SetState(EPGDungeonState::Failed); return false; }
    return true;
}
void APGDungeonGenerator::Generate(int32 InSeed)
{
    if (!GetWorld()->IsGameWorld()) { Seed=InSeed; GeneratePreview(); return; }
    ++RequestId; GetWorldTimerManager().ClearTimer(NavigationTimer);
    StopDungeonCombat();
    Seed=InSeed; Attempt=0; bFallback=false; SuspendPlayer(true); StartAttempt();
}
void APGDungeonGenerator::StartAttempt()
{
    ClearGeometry(); LastError.Reset(); SetState(EPGDungeonState::Structure);
    const bool Valid=Definition && FMath::IsFinite(Definition->NavigationTimeout) && Definition->NavigationTimeout>=1;
    if (!Valid) { LastError=TEXT("Invalid dungeon definition"); SetState(EPGDungeonState::Failed); return; }
    if (!PGDungeon::Build(*Definition,Seed,Attempt,Layout,LastError,bFallback))
    {
        UE_LOG(LogTemp,Warning,TEXT("PGDungeon seed=%d attempt=%d fallback=%d structure: %s"),Seed,Attempt,bFallback,*LastError);
        if (++Attempt<FMath::Clamp(Definition->MaxAttempts,1,3)) { StartAttempt(); return; }
        if (!bFallback) { bFallback=true; StartAttempt(); return; }
        SetState(EPGDungeonState::Failed); return;
    }
    SetState(EPGDungeonState::Assembly);
    if (!Assemble()) { ClearGeometry(); SetState(EPGDungeonState::Failed); return; }
    if (!StartEnvironment()) { ClearGeometry(); SetState(EPGDungeonState::Failed); }
}
void APGDungeonGenerator::BeginNavigation()
{
    SetState(EPGDungeonState::Navigation); NavigationStarted=FPlatformTime::Seconds();
    const uint32 Request=RequestId;
    GetWorldTimerManager().SetTimer(NavigationTimer,FTimerDelegate::CreateWeakLambda(this,[this,Request](){ PollNavigation(Request); }),.1f,true);
}
bool APGDungeonGenerator::ValidateNavigation()
{
    if (!Definition || !PGDungeon::Validate(*Definition,Layout,LastError)) return false;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    auto* Player=Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Nav || !Player || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(this)) { LastError=TEXT("Navigation or player not ready"); return false; }
    FNavAgentProperties Agent=Player->GetNavAgentPropertiesRef();
    Agent.AgentRadius=Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
    Agent.AgentHeight=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2;
    const ANavigationData* Data=Nav->GetNavDataForProps(Agent,RoomCenter(0));
    if (!Data) { LastError=TEXT("No navigation data for player agent"); return false; }
    FNavLocation Start;
    if (!Nav->ProjectPointToNavigation(RoomCenter(0),Start,FVector(50,50,150),Data)) { LastError=TEXT("Entrance is outside navigation"); return false; }
    // Validate complete routes through room centers. Structural connectivity plus a
    // capsule-clear path for every edge proves an entrance-to-room route without
    // trusting smaller-agent shortcuts around corners of intermediate rooms.
    for (const auto& Edge:Layout.Connections)
    {
        const int32 RoomId=Edge.B;
        FNavLocation End;
        if (!Nav->ProjectPointToNavigation(RoomCenter(Edge.A),Start,FVector(50,50,150),Data) ||
            !Nav->ProjectPointToNavigation(RoomCenter(Edge.B),End,FVector(50,50,150),Data))
        { LastError=FString::Printf(TEXT("Room %d is outside navigation"),RoomId); return false; }
        FPathFindingQuery Query(this,*Data,Start.Location,End.Location);
        Query.SetAllowPartialPaths(false);
        const FPathFindingResult Path=Nav->FindPathSync(Agent,Query);
        if (!Path.IsSuccessful() || !Path.Path.IsValid() || Path.Path->IsPartial())
        { LastError=FString::Printf(TEXT("No complete player path to room %d"),RoomId); return false; }
        // Recast's supported-agent config may be smaller than this pawn. Validate the
        // complete returned route against the real capsule as well, never a partial path.
        const auto& Points=Path.Path->GetPathPoints();
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PGDungeonRoute),false,Player);
        const FVector Lift(0,0,Agent.AgentHeight*.5f+4.f);
        const FCollisionShape Capsule=FCollisionShape::MakeCapsule(Agent.AgentRadius,Agent.AgentHeight*.5f);
        for (int32 I=1; I<Points.Num(); ++I)
        {
            FHitResult Hit;
            if (GetWorld()->SweepSingleByChannel(Hit,Points[I-1].Location+Lift,Points[I].Location+Lift,
                FQuat::Identity,ECC_Pawn,Capsule,Params))
            { LastError=FString::Printf(TEXT("Player capsule route blocked for room %d"),RoomId); return false; }
        }
    }
    LastError.Reset(); return true;
}
void APGDungeonGenerator::PollNavigation(uint32 Request)
{
    if (Request!=RequestId || State!=EPGDungeonState::Navigation) return;
    SuspendPlayer(true);
    if (ValidateNavigation())
    {
        auto* Player=Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this,0));
        const FVector Spawn=RoomCenter(0)+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5);
        if (!Player->TeleportTo(Spawn,FRotator::ZeroRotator)) { LastError=TEXT("Entrance spawn blocked"); }
        else
        {
            GetWorldTimerManager().ClearTimer(NavigationTimer);
            if (bEnableCombat && (!StartDungeonCombat() || !SpawnTreasures()))
            { LastError=TEXT("Dungeon combat setup or new-run save failed"); StopDungeonCombat(); SetState(EPGDungeonState::Failed); return; }
            SuspendPlayer(false); SetState(EPGDungeonState::Ready);
            UE_LOG(LogTemp,Display,TEXT("PGDungeon READY seed=%d version=%d content=%d attempt=%d fallback=%d rooms=%d components=%d"),
                Seed,Layout.Version,Definition->ContentVersion,Attempt,bFallback,Layout.Rooms.Num(),GeneratedComponents.Num()); return;
        }
    }
    if (FPlatformTime::Seconds()-NavigationStarted<FMath::Clamp(Definition->NavigationTimeout,1.f,120.f)) return;
    GetWorldTimerManager().ClearTimer(NavigationTimer);
    UE_LOG(LogTemp,Warning,TEXT("PGDungeon seed=%d attempt=%d fallback=%d navigation: %s"),Seed,Attempt,bFallback,*LastError);
    if (!bFallback && ++Attempt<FMath::Clamp(Definition->MaxAttempts,1,3)) { StartAttempt(); return; }
    if (!bFallback) { bFallback=true; StartAttempt(); return; }
    ClearGeometry(); SetState(EPGDungeonState::Failed);
}
void APGDungeonGenerator::CancelGeneration()
{
    StopDungeonCombat();
    ++RequestId; GetWorldTimerManager().ClearTimer(NavigationTimer);
    // Keep the player suspended on cancellation; a fresh generation or map exit owns recovery.
    SuspendPlayer(true); ClearGeometry(); SetState(EPGDungeonState::Cancelled);
}
void APGDungeonGenerator::DrawDebugLayout()
{
    if (!Definition) return;
    for (const auto& R:Layout.Rooms)
    {
        const FVector C=RoomCenter(R.Id);
        DrawDebugBox(GetWorld(),C,FVector(Definition->RoomSize*.5f,Definition->RoomSize*.5f,30),FColor::Green,false,15);
        DrawDebugString(GetWorld(),C+FVector(0,0,180),FString::Printf(TEXT("Room %d / Objective %d"),R.Id,R.Objective),nullptr,FColor::White,15);
    }
    for (const auto& E:Layout.Connections) DrawDebugLine(GetWorld(),RoomCenter(E.A)+FVector(0,0,20),RoomCenter(E.B)+FVector(0,0,20),FColor::Cyan,false,15,0,12);
}
