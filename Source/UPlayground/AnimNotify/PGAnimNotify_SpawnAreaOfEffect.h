// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "PGAnimNotify_SpawnAreaOfEffect.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGAnimNotify_SpawnAreaOfEffect : public UAnimNotify
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|AnimNotify")
	int32 EffectId;

private:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
