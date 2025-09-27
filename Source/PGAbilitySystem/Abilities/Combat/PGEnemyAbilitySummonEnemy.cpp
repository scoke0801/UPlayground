// Fill out your copyright notice in the Description page of Project Settings.


#include "PGEnemyAbilitySummonEnemy.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGAI/Task/PGAbilityTask_WaitSpawnEnemy.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

void UPGEnemyAbilitySummonEnemy::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                 const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                 const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APGCharacterEnemy* Character = GetEnemyCharacterFromActorInfo();
	if (nullptr == Character)
	{
		EndAbilitySelf();
		return;
	}
	
	FPGSkillDataRow* Row = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(SkillId);
	if(nullptr == Row)
	{
		EndAbilitySelf();
		return;
	}
	
	UAnimMontage* MontageToPlay = nullptr;
	if (UObject* LoadedObject = Row->MontagePath.TryLoad())
	{
		MontageToPlay = Cast<UAnimMontage>(LoadedObject);
	}

	if (nullptr == MontageToPlay)
	{
		EndAbilitySelf();
	}

	// 현재 적 캐릭터 가져오기
	APGCharacterEnemy* EnemyCharacter = GetEnemyCharacterFromActorInfo();
	if (!EnemyCharacter)
	{
		K2_EndAbility();
		return;
	}
	
	// 소환 원점(현재 적의 위치) 가져오기
	FVector SpawnOrigin = EnemyCharacter->GetActorLocation();

	// WaitSpawnEnemy 태스크 생성 및 실행
	UPGAbilityTask_WaitSpawnEnemy* SpawnTask = UPGAbilityTask_WaitSpawnEnemy::WaitSpawnEnemy(
		this,
		EventTag,
		SoftClassToSpawn,
		NumToSpawn,
		SpawnOrigin,
		RandomSpawnRadius
	);

	if (!SpawnTask)
	{
		K2_EndAbility();
		return;
	}

	// 델리게이트 바인딩
	SpawnTask->OnSpawnFinished.AddDynamic(this, &ThisClass::OnSpawnFinished);
	SpawnTask->DidNotSpawn.AddDynamic(this, &ThisClass::OnSpawnFailed);

	// 태스크 활성화
	SpawnTask->ReadyForActivation();
}

void UPGEnemyAbilitySummonEnemy::OnSpawnFinished(const TArray<AActor*>& Actors)
{
	// 현재 적 캐릭터 가져오기
	APGCharacterEnemy* EnemyCharacter = GetEnemyCharacterFromActorInfo();
	if (EnemyCharacter)
	{
		// 블랙보드 컴포넌트 가져오기
		UBlackboardComponent* BlackboardComp = UAIBlueprintHelperLibrary::GetBlackboard(EnemyCharacter);
		if (BlackboardComp)
		{
			// "HasSpawnedEnemies" 키를 true로 설정
			BlackboardComp->SetValueAsBool(FName("HasSpawnedEnemies"), true);
		}
	}

	EndAbilitySelf();
}

void UPGEnemyAbilitySummonEnemy::OnSpawnFailed()
{
	EndAbilitySelf();
}
