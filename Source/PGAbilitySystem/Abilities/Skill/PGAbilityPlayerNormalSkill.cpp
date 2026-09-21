// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityPlayerNormalSkill.h"
#include "PGActor/Controllers/PGPlayerController.h"

bool UPGAbilityPlayerNormalSkill::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (ActorInfo && ActorInfo->PlayerController.IsValid())
        if (const APGPlayerController* PC = Cast<APGPlayerController>(ActorInfo->PlayerController.Get()))
            if (PC->IsPointerOverUI()) return false;
    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}
