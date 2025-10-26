// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityTask_WaitSpawnEnemy.h"

#include "AbilitySystemComponent.h"
#include "NavigationSystem.h"
#include "Engine/AssetManager.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"

void UPGAbilityTask_WaitSpawnEnemy::OnDestroy(bool bInOwnerFinished)
{
	FGameplayEventMulticastDelegate& Delegate =	AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(CachedEventTag);

	Delegate.Remove(CachedDelegateHandle);
	
	Super::OnDestroy(bInOwnerFinished);
}

void UPGAbilityTask_WaitSpawnEnemy::Activate()
{
	FGameplayEventMulticastDelegate& Delegate =	AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(CachedEventTag);

	CachedDelegateHandle = Delegate.AddUObject(this, &ThisClass::OnGameplayEventReceived);
}

UPGAbilityTask_WaitSpawnEnemy* UPGAbilityTask_WaitSpawnEnemy::WaitSpawnEnemy(UGameplayAbility* OwningAbility,
	FGameplayTag EventTag, TSoftClassPtr<AActor> SoftClassToSpawn, int32 NumToSpawn,
	const FVector& SpawnOrigin, float RandomSpawnRadius)
{
	UPGAbilityTask_WaitSpawnEnemy* Node = NewAbilityTask<UPGAbilityTask_WaitSpawnEnemy>(OwningAbility);

	Node->CachedEventTag = EventTag;
	Node->CachedSoftEnemyClassToSpawn = SoftClassToSpawn;
	Node->CachedNumToSpawn = NumToSpawn;
	Node->CachedSpawnOrigin = SpawnOrigin;
	Node->CachedRandomSpawnRadius = RandomSpawnRadius;
	
	return Node;
}

void UPGAbilityTask_WaitSpawnEnemy::OnGameplayEventReceived(const FGameplayEventData* InPayload)
{
	if (ensure(false == CachedSoftEnemyClassToSpawn.IsNull()))
	{
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			CachedSoftEnemyClassToSpawn.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &ThisClass::OnEnemyClassLoaded));
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			DidNotSpawn.Broadcast();
		}

		EndTask();
	}
}

void UPGAbilityTask_WaitSpawnEnemy::OnEnemyClassLoaded()
{
	UClass* LoadedClass = CachedSoftEnemyClassToSpawn.Get();
	UWorld* World = GetWorld();

	if (nullptr == LoadedClass || nullptr == World)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			DidNotSpawn.Broadcast();
		}
		EndTask();
		return;
	}

	TArray<AActor*> SpawndEnemies;

	FActorSpawnParameters SpawnParam;
	SpawnParam.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	for (int32 i = 0; i < CachedNumToSpawn; ++i)
	{
		FVector RandomLocation;
		UNavigationSystemV1::K2_GetRandomReachablePointInRadius(this,
			CachedSpawnOrigin, RandomLocation, CachedRandomSpawnRadius);

		// 바닥에 꽂히지 않고록 z축만 위로 보정
		RandomLocation += FVector(0.f, 0.f, 150.0f);

		FRotator SpawnFacingRotator = AbilitySystemComponent->GetAvatarActor()->GetActorForwardVector().ToOrientationRotator();

		AActor* SpawnedEnemy = World->SpawnActor<AActor>(LoadedClass,
			RandomLocation, SpawnFacingRotator, SpawnParam);

		if (SpawnedEnemy)
		{
			SpawndEnemies.Add(SpawnedEnemy);
			
			FPGEventDataOneParam<TWeakObjectPtr<AActor>> ToSendData(SpawnedEnemy);
			UPGMessageManager::Get()->SendMessage(EPGSharedMessageType::OnSpawned, &ToSendData);
		}
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if(false == SpawndEnemies.IsEmpty())
		{
			OnSpawnFinished.Broadcast(SpawndEnemies);
		}
		else
		{
			DidNotSpawn.Broadcast();
		}
	}

	EndTask();
}
