#include "PGDungeonGenerator.h"
#include "PGData/DataAsset/Dungeon/PGDungeonDefinition.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGActor/Dungeon/PGDungeonDiscoverySubsystem.h"
#include "PGActor/Dungeon/PGDungeonTreasure.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

bool APGDungeonGenerator::StartDungeonCombat()
{
    // This isolated host must never start a second progression authority in a fixed map.
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It) return false;
    TArray<FVector> Centers;
    Centers.SetNum(6);
    TSet<int32> Objectives;
    int32 BossId = INDEX_NONE;
    for (const auto& Room : Layout.Rooms)
    {
        const int32 Objective = Room.Role == EPGDungeonRoomRole::Boss ? 6 : Room.Objective;
        if (!Objective) continue;
        if (Objective < 1 || Objective > 6 || Objectives.Contains(Objective)) return false;
        Objectives.Add(Objective);
        Centers[Objective - 1] = RoomCenter(Room.Id);
        if (Objective == 6) BossId = Room.Id;
    }
    if (Objectives.Num() != 6 || BossId == INDEX_NONE) return false;
    CombatManager = GetWorld()->SpawnActor<APGStageManager>();
    if (!CombatManager || !CombatManager->ConfigureDungeonCombat(Centers, Definition->RoomSize * .5f,
        Definition->CombatEntryInset, Definition->CombatSpawnInset, Definition->UnreachableEnemyTimeout, Layout.Seed)) return false;
    UStaticMesh* Mesh = Definition->GateMesh.LoadSynchronous();
    if (!Mesh) return false;
    const FVector MeshSize = Mesh->GetBoundingBox().GetSize();
    for (const auto& Edge : Layout.Connections)
    {
        const int32 Other = Edge.A == BossId ? Edge.B : Edge.B == BossId ? Edge.A : INDEX_NONE;
        if (Other == INDEX_NONE) continue;
        const FVector Direction = (RoomCenter(Other) - RoomCenter(BossId)).GetSafeNormal();
        const FVector Size = FMath::Abs(Direction.X) > .5f ? FVector(80, Definition->CorridorWidth, 320)
            : FVector(Definition->CorridorWidth, 80, 320);
        auto* Gate = NewObject<UStaticMeshComponent>(this);
        Gate->ComponentTags.Add(TEXT("PGDungeonGate"));
        AddInstanceComponent(Gate); Gate->SetupAttachment(RootComponent);
        Gate->SetMobility(EComponentMobility::Movable);
        Gate->SetStaticMesh(Mesh); Gate->SetMaterial(0, Definition->GateMaterial.LoadSynchronous());
        Gate->SetCollisionProfileName(TEXT("BlockAll"));
        // Progress barriers must not trigger a rebuild of validated navigation.
        Gate->SetCanEverAffectNavigation(false); Gate->RegisterComponent();
        const FVector Scale = Size / MeshSize;
        Gate->SetWorldTransform(FTransform(FQuat::Identity,
            RoomCenter(BossId) + Direction * (Definition->RoomSize * .5f) + FVector(0,0,160)
                - Mesh->GetBoundingBox().GetCenter() * Scale, Scale));
        BossGates.Add(Gate);
        GateClosedLocations.Add(Gate->GetComponentLocation());
    }
    // Also override tick intervals serialized in existing level instances.
    SetActorTickInterval(0.f);
    SetActorTickEnabled(true);
    bBossGateLocked=true;
    return !BossGates.IsEmpty();
}

void APGDungeonGenerator::StopDungeonCombat()
{
    if (auto* Map = GetWorld()->GetSubsystem<UPGDungeonDiscoverySubsystem>()) Map->ResetMap();
    SetActorTickEnabled(false);
    if (IsValid(CombatManager))
    {
        CombatManager->StopDungeonCombat();
        // Corpses and unclaimed drops are no longer tracked by StageManager.
        TArray<AActor*> Remaining;
        for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It) if (It->EncounterOwner.Get() == CombatManager) Remaining.Add(*It);
        for (TActorIterator<APGLootDrop> It(GetWorld()); It; ++It) if (It->GetOwner() == CombatManager) Remaining.Add(*It);
        for (auto* Actor : Remaining) Actor->Destroy();
        CombatManager->Destroy();
    }
    CombatManager = nullptr;
    for (UStaticMeshComponent* Gate : BossGates) if (IsValid(Gate)) { RemoveInstanceComponent(Gate); Gate->DestroyComponent(); }
    BossGates.Reset();
    GateClosedLocations.Reset();
    for (APGDungeonTreasure* Treasure:Treasures) if (IsValid(Treasure)) Treasure->Destroy();
    Treasures.Reset();
    for (UStaticMeshComponent* Fixture:TreasureFixtures) if (IsValid(Fixture)) { RemoveInstanceComponent(Fixture); Fixture->DestroyComponent(); }
    TreasureFixtures.Reset();
}

void APGDungeonGenerator::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateOcclusion(DeltaSeconds);
    if (State == EPGDungeonState::Ready)
        if (auto* Player = UGameplayStatics::GetPlayerPawn(this, 0))
            if (auto* Map = GetWorld()->GetSubsystem<UPGDungeonDiscoverySubsystem>())
                Map->Observe(Player->GetActorLocation(), IsValid(CombatManager) ? CombatManager->GetCurrentStageId() : 0);
    if (!IsValid(CombatManager)) return;
    const auto StageState = CombatManager->GetCurrentStageState();
    const bool Failed = StageState == EPGStageState::Failed || StageState == EPGStageState::Finished;
    const bool Locked = !Failed && (CombatManager->GetCurrentStageId() < 6 ||
        StageState == EPGStageState::InProgress || StageState == EPGStageState::WaveIntermission);
    if (Locked!=bBossGateLocked)
    {
        bBossGateLocked=Locked;
        if (auto* Sound=Definition->GateSound.LoadSynchronous())
            if (auto* Player=UGameplayStatics::GetPlayerPawn(this,0))
                if (!GateClosedLocations.IsEmpty() && FVector::DistSquared(Player->GetActorLocation(),GateClosedLocations[0])<FMath::Square(2500.f))
                    UGameplayStatics::PlaySoundAtLocation(this,Sound,GateClosedLocations[0],.6f);
    }
    for (int32 I=0; I<BossGates.Num(); ++I)
    {
        auto* Gate=BossGates[I].Get();
        const auto Collision = Locked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
        if (Gate->GetCollisionEnabled() != Collision) Gate->SetCollisionEnabled(Collision);
        const FVector Destination=GateClosedLocations[I]-FVector(0,0,Locked ? 0 : 360);
        Gate->SetWorldLocation(FMath::VInterpConstantTo(Gate->GetComponentLocation(),Destination,DeltaSeconds,360.f/FMath::Max(.1f,Definition->GateTransitionSeconds)));
        Gate->SetHiddenInGame(!Locked && Gate->GetComponentLocation().Equals(Destination,1.f));
    }
}
