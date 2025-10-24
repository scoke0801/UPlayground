// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "PGAI/Service/PGBTService_SelectSkill.h"
#include "PGAnimNotify_Heal.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGAnimNotify_Heal : public UAnimNotify
{
	GENERATED_BODY()

protected:
	/** Blackboard - Heal 타겟 키*/
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SkillTargetActorKey;
	
public:
	UPGAnimNotify_Heal();

private:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
