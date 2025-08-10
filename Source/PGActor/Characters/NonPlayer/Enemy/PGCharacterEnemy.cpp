// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterEnemy.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PGActor/Components/Combat/PGEnemyCombatComponent.h"
#include "PGActor/Handler/Skill/PGEnemySkillHandler.h"
#include "PGData/DataAsset/StartUpData/PGDataAsset_StartUpDataBase.h"

UPGPawnCombatComponent* APGCharacterEnemy::GetCombatComponent() const
{
	return CombatComponent;
}

APGCharacterEnemy::APGCharacterEnemy()
{
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 180.f, 0.f);

	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1000.f;

	CombatComponent = CreateDefaultSubobject<UPGEnemyCombatComponent>("EnemyCombatComponent");

	SkillHandler =  FPGHandler::Create<FPGEnemySkillHandler>();
}

void APGCharacterEnemy::BeginPlay()
{
	Super::BeginPlay();
	
	// [TODO] 데이터를 주입할 수 있는 다른 방안을 모색해보자.
	SkillHandler->AddSkill(EPGSkillSlot::NormalAttack, 1001);
}

void APGCharacterEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitEnemyStartUpData();
}

void APGCharacterEnemy::InitEnemyStartUpData()
{
	if (CharacterStartUpData.IsNull())
	{
		return;
	}

	UAssetManager::GetStreamableManager().RequestAsyncLoad(
		CharacterStartUpData.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([this]()
		{
			if (UPGDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.Get())
			{
				LoadedData->GiveToAbilitySystemComponent(AbilitySystemComponent);
			}
		})
		);
}
