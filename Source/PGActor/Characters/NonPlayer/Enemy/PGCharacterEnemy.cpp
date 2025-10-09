// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterEnemy.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGActor/Components/Combat/PGEnemyCombatComponent.h"
#include "PGActor/Components/Combat/PGSkillMontageController.h"
#include "PGActor/Components/Stat/PGEnemyStatComponent.h"
#include "PGActor/Handler/Skill/PGEnemySkillHandler.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataAsset/StartUpData/PGDataAsset_StartUpDataBase.h"
#include "PGData/DataTable/ActorAssetPath/PGDeathDataRow.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"
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
	GetCharacterMovement()->BrakingDecelerationWalking = 600.f;  // 1000 → 600 (자연스러운 감속)
	GetCharacterMovement()->MaxAcceleration = 1024.f;  // 부드러운 가속

	CombatComponent = CreateDefaultSubobject<UPGEnemyCombatComponent>("EnemyCombatComponent");
	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimeline"));

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
	
	SkillMontageController = CreateDefaultSubobject<UPGSkillMontageController>(TEXT("SkillMontageController"));

	
	LeftHandCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftHandCollisionBox"));
	LeftHandCollisionBox->SetupAttachment(GetMesh());
	LeftHandCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftHandCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);
	
	RightHandCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RightHandCollisionBox"));
	RightHandCollisionBox->SetupAttachment(GetMesh());
	RightHandCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightHandCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);

	LeftFootCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftFootCollisionBox"));
	LeftFootCollisionBox->SetupAttachment(GetMesh());
	LeftFootCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftFootCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);
	
	RightFootCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RightFootCollisionBox"));
	RightFootCollisionBox->SetupAttachment(GetMesh());
	RightFootCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightFootCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);

	TailCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TailCollisionBox"));
	TailCollisionBox->SetupAttachment(GetMesh());
	TailCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TailCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);

}

void APGCharacterEnemy::BeginPlay()
{
	Super::BeginPlay();

	if(FPGEnemyDataRow* EnemyData = PGData()->GetRowData<FPGEnemyDataRow>(CharacterTID))
	{
		uint8 Index = 0;
		for (int32 SkillId : EnemyData->SkillIdList)
		{
			if(FPGSkillDataRow* SkillIDataRow = PGData()->GetRowData<FPGSkillDataRow>(SkillId))
			{
				EPGSkillSlot SkillSlot = static_cast<EPGSkillSlot>(Index++);
				SkillHandler->AddSkill(SkillSlot, SkillId);
			}
		}
	}

	LeftHandCollisionBox->IgnoreActorWhenMoving(this, true);
	RightHandCollisionBox->IgnoreActorWhenMoving(this, true);
	LeftFootCollisionBox->IgnoreActorWhenMoving(this, true);
	RightFootCollisionBox->IgnoreActorWhenMoving(this, true);
	
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
	int32 DamageAmount = FMath::RandRange(20,50);
	EnemyStatComponent->CurrentHP = FMath::Max(0, CurrentHp - DamageAmount);

	if (EnemyNamePlate)
	{
		EnemyNamePlate->ShowWidget(5.0f);
	}

	PGDamageFloater()->AddFloater(DamageAmount,
		EPGDamageType::Normal, GetActorLocation(), true);
	
	UpdateHpBar();

	if (EnemyStatComponent->CurrentHP == 0.f)
	{
		UPGAbilityBPLibrary::AddGameplayTagToActorIfNone(this, PGGamePlayTags::Shared_Status_Dead);
	}
}

void APGCharacterEnemy::OnDied()
{
	// 충돌 비활성화
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = true;

		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 보유 위젯 비활성화
	if (EnemyNamePlate)
	{
		EnemyNamePlate->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Dissolve VFX 재생
	if (FPGDeathDataRow* Data = PGData()->GetRowData<FPGDeathDataRow>(CharacterTID))
	{
		if (false == Data->DissolveVFXPath.IsNull())
		{
			UAssetManager::GetStreamableManager().RequestAsyncLoad(
				Data->DissolveVFXPath.ToSoftObjectPath(), 
				FStreamableDelegate::CreateLambda([this, VFXPath = Data->DissolveVFXPath]()
				{
					if (UNiagaraSystem* Template = VFXPath.Get())
					{
						PlayDeathDissolveVFX(Template);
						StartDissolveEffect();
					}
				}));
		}
		else
		{
			StartDissolveEffect();
		}
	}
	else
	{
		StartDissolveEffect();
	}
}

#if WITH_EDITOR
void APGCharacterEnemy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(
		ThisClass, LeftHandCollisionBoxAttachBoneName))
	{
		LeftHandCollisionBox->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetIncludingScale,
			LeftHandCollisionBoxAttachBoneName);
	}

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(
		ThisClass, RightHandCollisionBoxAttachBoneName))
	{
		RightHandCollisionBox->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetIncludingScale,
			RightHandCollisionBoxAttachBoneName);
	}

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(
		ThisClass, LeftFootCollisionBoxAttachBoneName))
	{
		LeftFootCollisionBox->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetIncludingScale,
			LeftFootCollisionBoxAttachBoneName);
	}

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(
		ThisClass, RightFootCollisionBoxAttachBoneName))
	{
		RightFootCollisionBox->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetIncludingScale,
			RightFootCollisionBoxAttachBoneName);
	}

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(
		ThisClass, TailCollisionBoxAttachBoneName))
	{
		TailCollisionBox->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetIncludingScale,
			TailCollisionBoxAttachBoneName);
	}
}
#endif

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

		if (FPGEnemyDataRow* EnemyData = PGData()->GetRowData<FPGEnemyDataRow>(CharacterTID))
		{
			EnemyNamePlate->SetNameText(EnemyData->EnemyName);
		}
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

void APGCharacterEnemy::StartDissolveEffect()
{
	if (!DissolveTimeline || !DissolveCurve)
	{
		UE_LOG(LogTemp, Warning, TEXT("DissolveTimeline 또는 DissolveCurve가 설정되지 않았습니다."));

		OnDissolveTimelineFinished();
		return;
	}

	FOnTimelineFloat DissolveTimelineUpdateDelegate;
	DissolveTimelineUpdateDelegate.BindDynamic(this, &APGCharacterEnemy::OnDissolveTimelineUpdate);
	DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTimelineUpdateDelegate);

	FOnTimelineEvent DissolveTimelineFinishedDelegate;
	DissolveTimelineFinishedDelegate.BindDynamic(this, &APGCharacterEnemy::OnDissolveTimelineFinished);
	DissolveTimeline->SetTimelineFinishedFunc(DissolveTimelineFinishedDelegate);
	
	// Timeline 재생 시작
	DissolveTimeline->SetPlayRate(1.0f / TotalDissolveTime);
	DissolveTimeline->PlayFromStart();
}

void APGCharacterEnemy::OnDissolveTimelineUpdate(float Value)
{
	// 캐릭터 메쉬에 DissolveAmount 파라미터 설정
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetScalarParameterValueOnMaterials(FName("DissolveAmount"), Value);
	}

	// 현재 장착된 무기가 있다면 해당 무기 메쉬에도 적용
	if (CombatComponent)
	{
		if (APGWeaponBase* CurrentWeapon = CombatComponent->GetCharacterCurrentEquippedWeapon())
		{
			if (UMeshComponent* WeaponMesh = CurrentWeapon->GetMeshComponent())
			{
				WeaponMesh->SetScalarParameterValueOnMaterials(FName("DissolveAmount"), Value);
			}
		}
	}
}

void APGCharacterEnemy::OnDissolveTimelineFinished()
{
	// 무기가 있다면 먼저 파괴
	if (CombatComponent)
	{
		if (APGWeaponBase* CurrentWeapon = CombatComponent->GetCharacterCurrentEquippedWeapon())
		{
			CurrentWeapon->Destroy();
		}
	}

	// 캐릭터 액터 파괴
	Destroy();
}

void APGCharacterEnemy::OnBodyCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (UPGAbilityBPLibrary::IsTargetPawnHostile(this,HitPawn))
		{
			CombatComponent->OnHitTargetActor(HitPawn);
		}
	}
}
