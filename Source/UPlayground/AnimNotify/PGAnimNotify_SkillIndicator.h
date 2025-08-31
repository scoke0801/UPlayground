// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "PGAnimNotify_SkillIndicator.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGAnimNotify_SkillIndicator : public UAnimNotify
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|AnimNotify")
	int32 SkillIndicatorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|AnimNotify")
	float SpawnDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
	FVector SpawnOffset = FVector::ZeroVector;
    
private:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
