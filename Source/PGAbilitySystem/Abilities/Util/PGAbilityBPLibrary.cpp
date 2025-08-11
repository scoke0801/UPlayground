// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityBPLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GenericTeamAgentInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

UPGAbilitySystemComponent* UPGAbilityBPLibrary::NativeGetPGASCFromActor(AActor* InActor)
{
	return CastChecked<UPGAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InActor));
}

bool UPGAbilityBPLibrary::NativeDoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck)
{
	UPGAbilitySystemComponent* ASC = NativeGetPGASCFromActor(InActor);

	return ASC->HasMatchingGameplayTag(TagToCheck);
}

void UPGAbilityBPLibrary::AddGameplayTagToActorIfNone(AActor* InActor, FGameplayTag TagToAdd)
{
	UPGAbilitySystemComponent* ASC = NativeGetPGASCFromActor(InActor);

	if (false == ASC->HasMatchingGameplayTag(TagToAdd))
	{
		//"Loose" 태그는 임시적이거나 조건적으로 적용되는 태그를 의미
		ASC->AddLooseGameplayTag(TagToAdd);
	}	
}

void UPGAbilityBPLibrary::RemoveGameplayTagFromActorIfFound(AActor* InActor, FGameplayTag TagToRemove)
{
	UPGAbilitySystemComponent* ASC = NativeGetPGASCFromActor(InActor);

	if (true == ASC->HasMatchingGameplayTag(TagToRemove))
	{
		//"Loose" 태그는 임시적이거나 조건적으로 적용되는 태그를 의미
		ASC->RemoveLooseGameplayTag(TagToRemove);
	}
}

bool UPGAbilityBPLibrary::IsTargetPawnHostile(APawn* QueryPawn, APawn* TargetPawn)
{
	IGenericTeamAgentInterface* QueryTeamAgent = Cast<IGenericTeamAgentInterface>(QueryPawn->GetController());
	IGenericTeamAgentInterface* TargetTeamAgent = Cast<IGenericTeamAgentInterface>(TargetPawn->GetController());

	if (QueryTeamAgent && TargetTeamAgent)
	{
		return QueryTeamAgent->GetGenericTeamId() != TargetTeamAgent->GetGenericTeamId();
	}
	return false;
}

float UPGAbilityBPLibrary::GetScalableFloatValueAtLevel(const FScalableFloat& InScalableFloat, float InLevel)
{
	return InScalableFloat.GetValueAtLevel(InLevel);
}

bool UPGAbilityBPLibrary::IsValidBlock(AActor* InAttacker, AActor* InDefender)
{
	const float DotResult = FVector::DotProduct(InAttacker->GetActorForwardVector(),
		InDefender->GetActorForwardVector());

	return DotResult < -0.1f;
}

