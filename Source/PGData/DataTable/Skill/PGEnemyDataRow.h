#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGAttackPattern.h"
#include "PGEnemyDataRow.generated.h"

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
