#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGAttackPattern.h"
#include "PGEnemyDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGCombatPositioning
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.5")) float ReconsiderSeconds = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="60")) float Separation = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="60")) float MaxMoveDistance = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="10")) float MinimumImprovement = 45.f;
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGEnemyDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 EnemyID = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName EnemyName;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> SkillIdList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowedClasses = "/Script/Engine.Actor"))
	TSoftClassPtr<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot")
    FName DropPoolId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    TSoftObjectPtr<class UPGEnemyPresentationData> Presentation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role")
    EPGEnemyRole Role = EPGEnemyRole::Legacy;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Movement")
    EPGEnemyMobility Mobility = EPGEnemyMobility::Ground;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Movement", meta=(ClampMin="0", ClampMax="300"))
    float FlightHeight = 60.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Entrance") TSoftObjectPtr<class UAnimMontage> EntranceMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Entrance") TSoftObjectPtr<class UBlendSpace> DormantLocomotion;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Entrance", meta=(ClampMin="0", ClampMax="5")) float EntranceSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Entrance", meta=(ClampMin="0")) float WakeDistance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Behavior", meta=(ClampMin="0")) float SelectionReconsiderSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Behavior") bool bPrioritizeGuard = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Behavior", meta=(ClampMin="0")) float GuardRepeatCooldown = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Behavior")
    TSoftObjectPtr<class UBehaviorTree> CombatBehaviorTree;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Behavior")
    FPGCombatPositioning Positioning;
    // Pattern enemies receive hits through their capsule. Opt in only for bone-level
    // queries/physics; legacy enemies retain their Blueprint collision settings.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role|Collision")
    bool bUseSkeletalMeshCollision = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role", meta=(ClampMin="0"))
    float PreferredDistance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role", meta=(ClampMin="0"))
    float DistanceTolerance = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role", meta=(ClampMin="1"))
    float TurnSpeed = 240.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role", meta=(ClampMin="1"))
    float RetreatDistance = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role", meta=(ClampMin="0.1"))
    float RetreatSeconds = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role", meta=(ClampMin="0.1"))
    float RetreatCooldown = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role", meta=(ClampMin="0", ClampMax="180"))
    float GuardHalfAngle = 70.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Role", meta=(ClampMin="0", ClampMax="0.95"))
    float GuardReduction = .7f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss", meta=(ClampMin="0.01", ClampMax="0.99"))
    float PhaseTwoHealthRatio = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss", meta=(ClampMin="0", ClampMax="5"))
    float PhaseTransitionSeconds = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss", meta=(ClampMin="0"))
    float MinimumCombatWait = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss")
    FText PhaseTransitionText = NSLOCTEXT("PG", "TwilightBossTransition", "황혼 각성 · 공격 조합 변경");
    // Prefer this ordered combination in phase two; unavailable attacks are skipped.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss")
    TArray<int32> PhaseTwoSkillSequence;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss")
    TSoftObjectPtr<class UNiagaraSystem> PhaseVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss")
    TSoftObjectPtr<class USoundBase> PhaseSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss")
    TSoftObjectPtr<class UNiagaraSystem> DefeatVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss")
    TSoftObjectPtr<class USoundBase> DefeatSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss", meta=(ClampMin="0", ClampMax="10"))
    float DefeatDisplaySeconds = 3.f;
};
