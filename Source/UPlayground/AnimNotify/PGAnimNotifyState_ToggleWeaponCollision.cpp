// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/PGAnimNotifyState_ToggleWeaponCollision.h"

#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGShared/Shared/Enum/PGEnumTypes.h"

namespace
{
void TraceGripNotify(USkeletalMeshComponent* Mesh, bool bBegin)
{
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("PGGripTrace"))) return;
    if (auto* Player=Cast<APGCharacterPlayer>(Mesh->GetOwner()))
    {
        const auto* Attack=Player->GetPlayerAttackComponent();
        const auto Context=Attack->GetCastContext();
        UE_LOG(LogTemp,Display,TEXT("PGGrip Notify Skill=%d Begin=%d Source=%d Time=%.6f"),
            Context?Context->SkillID:0,bBegin,Mesh==Player->GetMesh(),Attack->GetLogicalTime());
    }
#endif
}
}

void UPGAnimNotifyState_ToggleWeaponCollision::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                           float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
    TraceGripNotify(MeshComp,true);

	if (APGCharacterBase* Character = Cast<APGCharacterBase>(MeshComp->GetOwner()))
	{
		if (UPGPawnCombatComponent* CombatComponent = Character->GetCombatComponent())
		{
			CombatComponent->ToggleWeaponCollision(true, DamageType);
		}
	}
}

void UPGAnimNotifyState_ToggleWeaponCollision::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
    TraceGripNotify(MeshComp,false);

	if (APGCharacterBase* Character = Cast<APGCharacterBase>(MeshComp->GetOwner()))
	{
		if (UPGPawnCombatComponent* CombatComponent = Character->GetCombatComponent())
		{
			CombatComponent->ToggleWeaponCollision(false, DamageType);
		}
	}
}
