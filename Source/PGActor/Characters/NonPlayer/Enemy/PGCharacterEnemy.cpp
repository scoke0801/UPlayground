// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterEnemy.h"

#include "Components/CapsuleComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PGActor/Components/Combat/PGEnemyCombatComponent.h"
#include "PGActor/Components/Stat/PGEnemyStatComponent.h"
#include "PGActor/Handler/Skill/PGEnemySkillHandler.h"
#include "PGData/DataAsset/StartUpData/PGDataAsset_StartUpDataBase.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGUI/Component/Base/PGWidgetComponentBase.h"
#include "PGUI/Manager/PGDamageFloaterManager.h"
#include "PGUI/Widget/Billboard/PGUIEnemyNamePlate.h"

UPGPawnCombatComponent* APGCharacterEnemy::GetCombatComponent() const
{
	return CombatComponent;
}

UPGStatComponent* APGCharacterEnemy::GetStatComponent() const
{
	return EnemyStatComponent;
}

UPGEnemyStatComponent* APGCharacterEnemy::GetEnemyStatComponent() const
{
	return EnemyStatComponent;
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
	EnemyStatComponent = CreateDefaultSubobject<UPGEnemyStatComponent>(TEXT("EnemyStatComponent"));
	EnemyNameplateWidgetComponent = CreateDefaultSubobject<UPGWidgetComponentBase>(TEXT("EnemyNameplate"));
	if (EnemyNameplateWidgetComponent)
	{
		EnemyNameplateWidgetComponent->SetupAttachment(GetCapsuleComponent());
		
		// 캡슐의 Half Height 가져오기
		float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		FVector BottomPosition = FVector(0.0f, 0.0f, CapsuleHalfHeight + 15);
		EnemyNameplateWidgetComponent->SetRelativeLocation(BottomPosition);

		EnemyNamePlate = Cast<UPGUIEnemyNamePlate>(EnemyNameplateWidgetComponent->GetWidget());
	}
}

void APGCharacterEnemy::BeginPlay()
{
	Super::BeginPlay();

	// [TODO] 데이터를 주입할 수 있는 다른 방안을 모색해보자.
	SkillHandler->AddSkill(EPGSkillSlot::NormalAttack, 1001);
	
	InitEnemyStartUpData();
	InitUIComponents();
	
	UpdateHpBar();
}

void APGCharacterEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void APGCharacterEnemy::OnHit(UPGStatComponent* StatComponent)
{
	Super::OnHit(StatComponent);
	
	int32 CurrentHp = EnemyStatComponent->CurrentHP;

	// TODO 데미지 계산하도록 수정 필요
	int32 DamageAmount = 10;
	EnemyStatComponent->CurrentHP = FMath::Max(0, CurrentHp - DamageAmount);

	if (EnemyNamePlate)
	{
		EnemyNamePlate->ShowWidget(5.0f);
	}

	PGDamageFloater()->AddFloater(DamageAmount,
		EPGDamageType::Normal, GetActorLocation(), true);
	
	UpdateHpBar();
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

void APGCharacterEnemy::InitUIComponents()
{
	if (EnemyNameplateWidgetComponent)
	{
		EnemyNamePlate = Cast<UPGUIEnemyNamePlate>(EnemyNameplateWidgetComponent->GetWidget());

		// 기본적으로 노출하지 않는다. 피격 시에만 노출
		EnemyNamePlate->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void APGCharacterEnemy::UpdateHpBar()
{
	if (nullptr == EnemyNamePlate)
	{
		return;
	}
	EnemyNamePlate->SetHpPercent(static_cast<float>(EnemyStatComponent->CurrentHP) / EnemyStatComponent->MaxHP);
}
