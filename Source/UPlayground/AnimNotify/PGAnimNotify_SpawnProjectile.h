// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "PGAnimNotify_SpawnProjectile.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGAnimNotify_SpawnProjectile : public UAnimNotify
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|AnimNotify")
	int32 ProjectileId;

private:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
