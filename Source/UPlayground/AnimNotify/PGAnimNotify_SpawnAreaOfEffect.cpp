// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAnimNotify_SpawnAreaOfEffect.h"

#include "PGActor/AreaOfEffect/PGAreaOfEffectBase.h"

void UPGAnimNotify_SpawnAreaOfEffect::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                           const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp->GetOwner();
	if (nullptr == Owner)
	{
		return;
	}
	
	APGAreaOfEffectBase::Fire(Owner, Owner->GetActorLocation(), EffectId);
}
