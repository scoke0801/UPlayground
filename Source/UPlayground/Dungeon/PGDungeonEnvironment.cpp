#include "PGDungeonGenerator.h"
#include "PGDungeonScatterSettings.h"
#include "PGData/DataAsset/Dungeon/PGDungeonDefinition.h"
#include "PGData/DataAsset/Dungeon/PGDungeonRoomDefinition.h"
#include "PGData/Dungeon/PGDungeonLayoutBuilder.h"
#include "PGActor/Dungeon/PGDungeonTreasure.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "Data/PCGPointData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

bool APGDungeonGenerator::AssembleModules()
{
    if (Definition->RoomModules.IsEmpty()) { LastError=TEXT("Missing authored room kit"); return false; }
    TArray<UPGDungeonRoomDefinition*> Modules;
    for (const auto& Ref:Definition->RoomModules)
    {
        auto* Module=Ref.LoadSynchronous();
        if (!Module || Module->ModuleId.IsNone() || !FMath::IsNearlyEqual(Module->RoomSize,Definition->RoomSize) || Module->AllowedQuarterTurns.IsEmpty())
        { LastError=TEXT("Invalid room module contract"); return false; }
        Modules.Add(Module);
    }
    Modules.Sort([](const auto& A,const auto& B){ return A.ModuleId.LexicalLess(B.ModuleId); });
    // Reuse instances by mesh, material and occlusion policy across all authored rooms.
    TMap<FString,UInstancedStaticMeshComponent*> Groups;
    for (const auto& Room:Layout.Rooms)
    {
        auto Candidates=Modules.FilterByPredicate([&](const auto* M){ return M->Role==Room.Role; });
        if (Candidates.IsEmpty()) { LastError=TEXT("Missing module for room role"); return false; }
        FRandomStream Random(PGDungeon::StreamSeed(Layout.Seed,5,Room.Id));
        auto* Module=Candidates[Random.RandRange(0,Candidates.Num()-1)];
        const int32 Turn=Module->AllowedQuarterTurns[Random.RandRange(0,Module->AllowedQuarterTurns.Num()-1)];
        if (Turn<0 || Turn>3) { LastError=TEXT("Invalid module rotation"); return false; }
        SelectedModules.Add(Module->ModuleId);
        const FTransform RoomTransform(FRotator(0,90.f*Turn,0),RoomCenter(Room.Id));
        for (const auto& Piece:Module->Pieces)
        {
            auto* Mesh=Piece.Mesh.LoadSynchronous();
            auto* Material=Piece.Material.IsNull() ? nullptr : Piece.Material.LoadSynchronous();
            if (!Mesh || (!Piece.Material.IsNull() && !Material) || !Piece.Transform.IsValid())
            { LastError=TEXT("Missing/invalid module piece"); return false; }
            const FBox Bounds=Mesh->GetBoundingBox().TransformBy(Piece.Transform);
            const float Half=Definition->RoomSize*.5f;
            if (Bounds.Min.X < -Half || Bounds.Max.X>Half || Bounds.Min.Y < -Half || Bounds.Max.Y>Half || Bounds.Max.Z>450)
            { LastError=TEXT("Module piece exceeds room envelope"); return false; }
            // Floor inlays can cover the center. Tall pieces must stay in outer corner pockets.
            const float Clear=Half-Definition->CombatSpawnInset;
            if (Bounds.Max.Z>30 && (Bounds.Intersect(FBox(FVector(-Clear,-Half,-100),FVector(Clear,Half,1000))) ||
                Bounds.Intersect(FBox(FVector(-Half,-Clear,-100),FVector(Half,Clear,1000)))))
            { LastError=FString::Printf(TEXT("Module %s piece %s overlaps reserve %.1f: %s"),*Module->ModuleId.ToString(),*Mesh->GetName(),Clear,*Bounds.ToString()); return false; }
            const FString Key=Mesh->GetPathName()+GetPathNameSafe(Material)+(Piece.bOccluder ? TEXT("fade") : TEXT("floor"));
            auto*& Group=Groups.FindOrAdd(Key);
            if (!Group)
            {
                Group=MakeInstances(Mesh,Material,false);
                if (Piece.bOccluder) Group->ComponentTags.Add(TEXT("PGDungeonOccluder"));
            }
            const int32 I=Group->AddInstance(Piece.Transform*RoomTransform,true);
            Group->SetCustomDataValue(I,0,RoomCenter(Room.Id).X);
            Group->SetCustomDataValue(I,1,RoomCenter(Room.Id).Y,true);
        }
    }
    return true;
}

void APGDungeonGenerator::StopEnvironment()
{
    // Also remove old editor-preview components serialized before PCG became transient.
    TInlineComponentArray<UPCGComponent*> Components(this);
    for (UPCGComponent* Component:Components)
    {
        if (auto* Graph=Component->GetGraph()) if (Graph->GetOuter()==this)
        { Graph->ClearFlags(RF_Public | RF_Standalone); Graph->SetFlags(RF_Transient | RF_DuplicateTransient); }
        Component->OnPCGGraphGeneratedDelegate.RemoveAll(this);
        Component->CancelGeneration();
        Component->CleanupLocalImmediate(true);
        RemoveInstanceComponent(Component);
        Component->DestroyComponent();
    }
    EnvironmentPCG=nullptr;
    DecorationPointCount=0;
}

bool APGDungeonGenerator::StartEnvironment()
{
    auto* Template=Cast<UPCGGraph>(Definition->DecorationGraph.LoadSynchronous());
    if (!Template) { LastError=TEXT("Missing PCG decoration graph"); return false; }
    auto* Graph=DuplicateObject<UPCGGraph>(Template,this);
    Graph->ClearFlags(RF_Public | RF_Standalone);
    Graph->SetFlags(RF_Transient | RF_DuplicateTransient);
    int32 Nodes=0;
    for (auto* Node:Graph->GetNodes()) if (auto* Settings=Cast<UPGDungeonScatterSettings>(Node->GetSettings()))
    { Settings->Layout=Layout; Settings->Origin=GetActorLocation(); Settings->Definition=Definition; ++Nodes; }
    if (Nodes!=1) { LastError=TEXT("PCG graph requires exactly one dungeon scatter input"); return false; }
    EnvironmentPCG=NewObject<UPCGComponent>(this,NAME_None,RF_Transient | RF_DuplicateTransient);
    AddInstanceComponent(EnvironmentPCG);
    EnvironmentPCG->GenerationTrigger=EPCGComponentGenerationTrigger::GenerateOnDemand;
    EnvironmentPCG->Seed=PGDungeon::StreamSeed(Layout.Seed,3,0);
    EnvironmentPCG->RegisterComponent();
    EnvironmentPCG->SetGraphLocal(Graph);
    const uint32 Request=RequestId;
    EnvironmentPCG->OnPCGGraphGeneratedDelegate.AddWeakLambda(this,[this,Request](UPCGComponent* Component){ CompleteEnvironment(Component,Request); });
    SetState(EPGDungeonState::Environment);
    NavigationStarted=FPlatformTime::Seconds();
    GetWorldTimerManager().SetTimer(NavigationTimer,FTimerDelegate::CreateWeakLambda(this,[this,Request]()
    {
        if (Request!=RequestId || State!=EPGDungeonState::Environment) return;
        if (FPlatformTime::Seconds()-NavigationStarted<FMath::Clamp(Definition->EnvironmentTimeout,1.f,120.f)) return;
        GetWorldTimerManager().ClearTimer(NavigationTimer);
        LastError=TEXT("PCG generation timed out"); StopEnvironment(); SetState(EPGDungeonState::Failed);
    }),.1f,true);
    EnvironmentPCG->GenerateLocal(true);
    return true;
}

void APGDungeonGenerator::CompleteEnvironment(UPCGComponent* Component,uint32 Request)
{
    if (Request!=RequestId || Component!=EnvironmentPCG || State!=EPGDungeonState::Environment) return;
    GetWorldTimerManager().ClearTimer(NavigationTimer);
    bool Valid=true;
    const auto& Output=Component->GetGeneratedGraphOutput();
    if (Output.TaggedData.IsEmpty()) Valid=false;
    for (const auto& Data:Output.TaggedData)
    {
        const auto* Points=Cast<UPCGPointData>(Data.Data);
        if (!Points || Data.Tags.Num()!=1) { Valid=false; break; }
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,**Data.Tags.CreateConstIterator());
        if (!Mesh) { Valid=false; break; }
        auto* Instances=MakeInstances(Mesh,nullptr,false);
        Instances->ComponentTags.Add(TEXT("PGDungeonOccluder"));
        for (const auto& Point:Points->GetPoints())
        {
            if (!Point.Transform.IsValid()) { Valid=false; break; }
            Instances->AddInstance(Point.Transform,true); ++DecorationPointCount;
        }
    }
    if (!Valid) { LastError=TEXT("Invalid PCG output"); ClearGeometry(); SetState(EPGDungeonState::Failed); return; }
    BuildOccluders();
    UE_LOG(LogTemp,Display,TEXT("PGDungeon PCG complete points=%d modules=%d request=%u"),DecorationPointCount,SelectedModules.Num(),Request);
    if (GetWorld()->IsGameWorld()) BeginNavigation(); else SetState(EPGDungeonState::Idle);
}

void APGDungeonGenerator::BuildOccluders()
{
    Occluders.Reset();
    for (UInstancedStaticMeshComponent* C:GeneratedComponents)
        if (C && C->ComponentHasTag(TEXT("PGDungeonOccluder")))
            for (int32 I=0; I<C->GetInstanceCount(); ++I)
            {
                FTransform Transform; C->GetInstanceTransform(I,Transform,true);
                Occluders.Add({C,I,C->GetStaticMesh()->GetBoundingBox().TransformBy(Transform),0.f});
            }
}

void APGDungeonGenerator::UpdateOcclusion(float DeltaSeconds)
{
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    APawn* Player=PC ? PC->GetPawn() : nullptr;
    if (!Player || !Definition) return;
    FVector View; FRotator Rotation; PC->GetPlayerViewPoint(View,Rotation);
    const FVector Target=Player->GetActorLocation()-FVector(0,0,40);
    const bool Active=PC->GetViewTarget()==Player && State==EPGDungeonState::Ready;
    TSet<UInstancedStaticMeshComponent*> Dirty;
    FadedInstanceCount=0;
    for (auto& Entry:Occluders)
    {
        auto* C=Entry.Component.Get(); if (!C) continue;
        // Expanded line-box test protects both the player and the nearby floor telegraph.
        const bool Blocked=Active && FMath::LineBoxIntersection(Entry.Bounds.ExpandBy(100.f),View,Target,Target-View);
        const float Desired=Blocked ? 1.f-FMath::Clamp(Definition->OccludedOpacity,0.f,1.f) : 0.f;
        const float Fade=FMath::FInterpConstantTo(Entry.Fade,Desired,DeltaSeconds,1.f/FMath::Max(.1f,Definition->OcclusionFadeSeconds));
        if (!FMath::IsNearlyEqual(Fade,Entry.Fade,.001f))
        { C->SetCustomDataValue(Entry.Index,2,Fade,false); Entry.Fade=Fade; Dirty.Add(C); }
        if (Fade>.01f) ++FadedInstanceCount;
    }
    for (auto* C:Dirty) C->MarkRenderStateDirty();
}

bool APGDungeonGenerator::SpawnTreasures()
{
    auto* Mesh=Definition->TreasureMesh.LoadSynchronous();
    if (!Mesh) { LastError=TEXT("Missing treasure fixture"); return false; }
    for (const auto& Room:Layout.Rooms) if (Room.Role==EPGDungeonRoomRole::Treasure)
    {
        const FVector Position=RoomCenter(Room.Id)+FVector(350,350,0);
        auto* Fixture=NewObject<UStaticMeshComponent>(this); AddInstanceComponent(Fixture);
        Fixture->SetupAttachment(RootComponent); Fixture->SetStaticMesh(Mesh);
        if (auto* Material=Definition->TreasureMaterial.LoadSynchronous()) Fixture->SetMaterial(0,Material);
        Fixture->SetCollisionEnabled(ECollisionEnabled::NoCollision); Fixture->SetCanEverAffectNavigation(false);
        Fixture->RegisterComponent();
        const auto Bounds=Mesh->GetBoundingBox();
        const float Scale=95.f/FMath::Max(1.,Bounds.GetSize().GetMax());
        Fixture->SetWorldTransform(FTransform(FQuat::Identity,Position-FVector(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Min.Z)*Scale,FVector(Scale)));
        TreasureFixtures.Add(Fixture);
        FActorSpawnParameters Params; Params.Owner=CombatManager;
        auto* Treasure=GetWorld()->SpawnActor<APGDungeonTreasure>(Position+FVector(0,0,75),FRotator::ZeroRotator,Params);
        if (!Treasure) return false;
        Treasures.Add(Treasure);
        if (!Treasure->InitializeTreasure(Layout.Seed,Room.Id,Definition->ContentVersion,Definition->BranchLootPool))
        { LastError=TEXT("Treasure initialization failed"); return false; }
    }
    return true;
}
