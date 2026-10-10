#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGShared/Shared/Structure/PGDungeonTypes.h"
#include "PGDungeonGenerator.generated.h"
class UPGDungeonDefinition;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class APGStageManager;
class UStaticMeshComponent;
class UPGDungeonDiscoverySubsystem;
class UPCGComponent;
class APGDungeonTreasure;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGDungeonStateChanged, EPGDungeonState, State);

// Geometry host; encounter progression and rewards remain owned by StageManager.
UCLASS()
class UPLAYGROUND_API APGDungeonGenerator : public AActor
{
    GENERATED_BODY()
public:
    APGDungeonGenerator();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon") TObjectPtr<UPGDungeonDefinition> Definition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon") int32 Seed = 101026;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon") bool bEnableCombat = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon") TObjectPtr<APGStageManager> CombatManager;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon") FPGDungeonLayout Layout;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon") EPGDungeonState State = EPGDungeonState::Idle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon") FString LastError;
    UPROPERTY(BlueprintAssignable) FPGDungeonStateChanged OnStateChanged;
    UFUNCTION(BlueprintCallable, Category="Dungeon") void Generate(int32 InSeed);
    UFUNCTION(CallInEditor, BlueprintCallable, Category="Dungeon") bool GeneratePreview();
    UFUNCTION(BlueprintCallable, Category="Dungeon") void CancelGeneration();
    UFUNCTION(BlueprintCallable, Category="Dungeon") bool ValidateNavigation();
    UFUNCTION(BlueprintCallable, Category="Dungeon") void DrawDebugLayout();
    UFUNCTION(BlueprintPure, Category="Dungeon|Map") UPGDungeonDiscoverySubsystem* GetDiscovery() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon|P2") int32 DecorationPointCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon|P2") int32 FadedInstanceCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon|P2") TArray<FName> SelectedModules;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon|P2") TArray<TObjectPtr<APGDungeonTreasure>> Treasures;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon|P2") bool bBossGateLocked = true;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BossGates;
    bool StartDungeonCombat();
    void StopDungeonCombat();
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> GeneratedComponents;
    FTimerHandle NavigationTimer;
    uint32 RequestId = 0;
    int32 Attempt = 0;
    bool bFallback = false;
    double NavigationStarted = 0;
    bool bPlayerSuspended = false;
    UPROPERTY(Transient, DuplicateTransient) TObjectPtr<UPCGComponent> EnvironmentPCG;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TreasureFixtures;
    TArray<FVector> GateClosedLocations;
    struct FOccluder { TWeakObjectPtr<UInstancedStaticMeshComponent> Component; int32 Index; FBox Bounds; float Fade=0; };
    TArray<FOccluder> Occluders;
    void StopEnvironment();
    bool StartEnvironment();
    void CompleteEnvironment(UPCGComponent* Component, uint32 Request);
    bool AssembleModules();
    bool SpawnTreasures();
    void BuildOccluders();
    void UpdateOcclusion(float DeltaSeconds);
    void BeginNavigation();
    void SetState(EPGDungeonState NewState);
    void StartAttempt();
    void PollNavigation(uint32 Request);
    void ClearGeometry();
    bool Assemble();
    FVector RoomCenter(int32 Id) const;
    void SuspendPlayer(bool bSuspend);
    UInstancedStaticMeshComponent* MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollision);
};
