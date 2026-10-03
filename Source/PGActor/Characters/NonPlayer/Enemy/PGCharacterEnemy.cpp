// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterEnemy.h"
#include "AIController.h"
#include "PGAbilitySystem/Abilities/Combat/PGEnemyAbilityAttack.h"
#include "PGShared/Shared/Message/Combat/PGBossPresentation.h"
#include "Kismet/GameplayStatics.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGMessage/Managaer/PGMessageManager.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimMontage.h"
#include "Sound/SoundBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGActor/Components/Combat/PGEnemyCombatComponent.h"
#include "PGActor/Components/Combat/PGEnemyPresentationComponent.h"
#include "PGData/DataAsset/Combat/PGEnemyPresentationData.h"
#include "PGActor/Components/Combat/PGSkillMontageController.h"
#include "PGActor/Components/Stat/PGEnemyStatComponent.h"
#include "PGActor/Handler/Skill/PGEnemySkillHandler.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataAsset/StartUpData/PGDataAsset_StartUpDataBase.h"
#include "PGData/DataTable/ActorAssetPath/PGDeathDataRow.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"
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

	// 적 캐릭터끼리 충돌하지 않도록 설정
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("EnemyCharacter"));
	
	// 메시도 Enemy 채널 무시 설정
	GetMesh()->SetCollisionObjectType(ECC_GameTraceChannel1);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);

	CombatComponent = CreateDefaultSubobject<UPGEnemyCombatComponent>("EnemyCombatComponent");
    EnemyPresentation = CreateDefaultSubobject<UPGEnemyPresentationComponent>(TEXT("EnemyPresentation"));
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
	LeftHandCollisionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	LeftHandCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);
	
	RightHandCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RightHandCollisionBox"));
	RightHandCollisionBox->SetupAttachment(GetMesh());
	RightHandCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightHandCollisionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	RightHandCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);

	LeftFootCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftFootCollisionBox"));
	LeftFootCollisionBox->SetupAttachment(GetMesh());
	LeftFootCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftFootCollisionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	LeftFootCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);
	
	RightFootCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RightFootCollisionBox"));
	RightFootCollisionBox->SetupAttachment(GetMesh());
	RightFootCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightFootCollisionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	RightFootCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);

	TailCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TailCollisionBox"));
	TailCollisionBox->SetupAttachment(GetMesh());
	TailCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TailCollisionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	TailCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);

}

void APGCharacterEnemy::BeginPlay()
{
	Super::BeginPlay();

	if(FPGEnemyDataRow* EnemyData = PGData()->GetRowData<FPGEnemyDataRow>(CharacterTID))
	{
        EnemyPresentation->Initialize(EnemyData->Presentation.LoadSynchronous());
		uint8 Index = 0;
		for (int32 SkillId : EnemyData->SkillIdList)
		{
			if(FPGSkillDataRow* SkillIDataRow = PGData()->GetRowData<FPGSkillDataRow>(SkillId))
			{
				EPGSkillSlot SkillSlot = static_cast<EPGSkillSlot>(Index++);
				SkillHandler->AddSkill(SkillSlot, SkillId);

                // Hold presentation references for the enemy lifetime; no first-use loads during impact.
                for (const FSoftObjectPath& Path : {SkillIDataRow->TelegraphMaterial.ToSoftObjectPath(),
                    SkillIDataRow->ElitePresentationMontage.ToSoftObjectPath(), SkillIDataRow->SlamVFX.ToSoftObjectPath(),
                    SkillIDataRow->AttackSound.ToSoftObjectPath(), SkillIDataRow->ProjectileClass.ToSoftObjectPath()})
                    if (auto* Asset = Path.TryLoad()) PreparedPatternAssets.AddUnique(Asset);
			}
		}
        if (EnemyData->Role == EPGEnemyRole::Boss)
            for (const FSoftObjectPath& Path : {EnemyData->PhaseVFX.ToSoftObjectPath(), EnemyData->PhaseSound.ToSoftObjectPath(),
                EnemyData->DefeatVFX.ToSoftObjectPath(), EnemyData->DefeatSound.ToSoftObjectPath()})
                if (auto* Asset = Path.TryLoad()) PreparedPatternAssets.AddUnique(Asset);
	}

	LeftHandCollisionBox->IgnoreActorWhenMoving(this, true);
	RightHandCollisionBox->IgnoreActorWhenMoving(this, true);
	LeftFootCollisionBox->IgnoreActorWhenMoving(this, true);
	RightFootCollisionBox->IgnoreActorWhenMoving(this, true);
	
	InitEnemyStartUpData();
	InitUIComponents();
	
	UpdateHpBar();
    PublishBossPresentation();
}

void APGCharacterEnemy::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(BossTransitionTimer);
    PublishBossPresentation(true);
    bBossPresentationClosed = true;
    Super::EndPlay(Reason);
}

void APGCharacterEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void APGCharacterEnemy::OnHit(UPGStatComponent* Source, const UPGPawnCombatComponent* Combat)
{
    const bool bGuardedHit = Source && GetDirectionalDamageScale(Source->GetOwner()) < 1.f;
    EPGDamageType Type = EPGDamageType::Normal;
    const float Damage = AbilitySystemComponent->ReceiveCombatHit(Source ? Source->GetASC() : nullptr, Type);
    if (Damage > 0.f)
    {
        if (auto* Manager = UPGDamageFloaterManager::Get(this)) Manager->AddFloater(FMath::RoundToInt(Damage), Type, this, false);
        if (EnemyNamePlate) EnemyNamePlate->ShowWidget(5.f);
        if (!bGuardedHit || !EnemyPresentation->PlayGuardImpact())
            PlayCombatFeedback(Source ? Source->GetOwner() : nullptr, Type);
        else if (FeedbackIntensity > 0.f)
            ApplyHitStop(.015f * FMath::Clamp(FeedbackIntensity, 0.f, 1.f));
    }
}

void APGCharacterEnemy::OnHeal(UPGStatComponent* Source, int32 HealAmount)
{
    const float Healed = AbilitySystemComponent->RestoreHealth(HealAmount);
    if (Healed > 0.f)
        if (auto* Manager = UPGDamageFloaterManager::Get(this)) Manager->AddFloater(FMath::RoundToInt(Healed), EPGDamageType::Heal, this, false);
}

void APGCharacterEnemy::OnDied()
{
    if (bDeathFinished) return;
    bDeathFinished = true;
    GetWorldTimerManager().ClearTimer(DeathFallbackTimer);
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
		if (EnemyNamePlate) EnemyNamePlate->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 스테이지 매니저에 처치 알림
	NotifyStageManagerOnDeath();

	// Dissolve VFX 재생
	if (UPGDataTableManager* DataManager = PGData())
	{
		if (FPGDeathDataRow* Data = DataManager->GetRowData<FPGDeathDataRow>(CharacterTID))
		{
			if (false == Data->DissolveVFXPath.IsNull())
			{
				UAssetManager::GetStreamableManager().RequestAsyncLoad(
					Data->DissolveVFXPath.ToSoftObjectPath(), 
					FStreamableDelegate::CreateWeakLambda(this, [this, VFXPath = Data->DissolveVFXPath]()
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
		FStreamableDelegate::CreateWeakLambda(this, [this]()
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
		if (EnemyNamePlate) EnemyNamePlate->SetVisibility(ESlateVisibility::Collapsed);

		if (FPGEnemyDataRow* EnemyData = PGData()->GetRowData<FPGEnemyDataRow>(CharacterTID))
		{
			if (EnemyNamePlate) EnemyNamePlate->SetNameText(EnemyData->EnemyName);
		}
	}
}

void APGCharacterEnemy::UpdateHpBar()
{
	if (nullptr == EnemyNamePlate)
	{
		return;
	}
	EnemyNamePlate->SetHpPercent(EnemyStatComponent->GetHealthRatio());
}

void APGCharacterEnemy::NotifyStageManagerOnDeath()
{
	if (UPGMessageManager* Manager = UPGMessageManager::Get(this))
	{
		FPGEventDataOneParam<TWeakObjectPtr<APGCharacterEnemy>> EventData(this);
		Manager->SendMessage(EPGSharedMessageType::OnDied, &EventData);
	}
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
    EnemyPresentation->SetDissolve(Value);
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

void APGCharacterEnemy::OnClicked_Implementation(AActor* ClickedActor, const FVector& ClickLocation)
{
	UE_LOG(LogTemp, Log, TEXT("적 캐릭터 클릭: %s"), *GetName());
	
	// 클릭 시 네임플레이트 표시
	if (EnemyNamePlate)
	{
		EnemyNamePlate->ShowWidget(10.0f);
	}
}

void APGCharacterEnemy::OnClickCancelled_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("적 캐릭터 클릭 취소: %s"), *GetName());
	
	// 클릭 취소 시 네임플레이트 숨김
	if (EnemyNamePlate)
	{
		if (EnemyNamePlate) EnemyNamePlate->SetVisibility(ESlateVisibility::Collapsed);
	}

}

bool APGCharacterEnemy::IsClickable_Implementation() const
{
	// 죽은 상태가 아닐 때만 클릭 가능
	return !AbilitySystemComponent->HasMatchingGameplayTag(PGGamePlayTags::Shared_Status_Dead);
}

void APGCharacterEnemy::OnHealthChanged()
{
    const bool bWasDead = bDeathStarted;
    Super::OnHealthChanged();
    UpdateHpBar();
    if (bDeathStarted)
    {
        EnemyPresentation->ResetPresentation(true);
        SetGuarding(false);
        GetWorldTimerManager().ClearTimer(BossTransitionTimer);
        PhaseTransitionUntil = 0;
    }
    if (auto* Tables = UPGDataTableManager::Get(this))
    {
        const auto* Row = Tables->GetRowData<FPGEnemyDataRow>(CharacterTID);
        if (Row && Row->Role == EPGEnemyRole::Boss)
        {
            TryBeginBossPhase(*Row);
            if (!bWasDead && bDeathStarted)
            {
                if (auto* VFX = Row->DefeatVFX.Get()) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX, GetActorLocation());
                if (auto* Sound = Row->DefeatSound.Get()) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
                UE_LOG(LogTemp, Log, TEXT("PGBoss Defeated phase=%d"), BossPhase);
            }
        }
    }
    PublishBossPresentation(); // Publish defeat before stage completion can destroy the actor.
    if (!bWasDead && bDeathStarted) { APGLootDrop::SpawnForEnemy(this); NotifyStageManagerOnDeath(); }
}

bool APGCharacterEnemy::IsBossTransitioning() const
{
    return GetWorld() && GetWorld()->GetTimeSeconds() < PhaseTransitionUntil && AbilitySystemComponent->GetHealth() > 0;
}

bool APGCharacterEnemy::TryBeginBossPhase(const FPGEnemyDataRow& Row)
{
    const float Health = AbilitySystemComponent->GetHealth();
    const float MaxHealth = AbilitySystemComponent->GetCombatStat(EPGStatType::Health);
    if (Row.Role != EPGEnemyRole::Boss || bDeathStarted || Health <= 0 || MaxHealth <= 0 || BossPhase != 1 ||
        !FMath::IsFinite(Row.PhaseTwoHealthRatio) || Health / MaxHealth > FMath::Clamp(Row.PhaseTwoHealthRatio, .01f, .99f)) return false;
    // Set both gates before cancellation: a callback must not start a new attack in this transition.
    BossPhase = 2;
    const float Duration = FMath::IsFinite(Row.PhaseTransitionSeconds) ? FMath::Clamp(Row.PhaseTransitionSeconds, 0.f, 5.f) : 1.2f;
    PhaseTransitionUntil = GetWorld()->GetTimeSeconds() + Duration;
    RequestedSkillID = 0;
    if (const auto* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(UPGEnemyAbilityAttack::StaticClass()))
        AbilitySystemComponent->CancelAbilityHandle(Spec->Handle);
    ClearPatternHitboxes();
    if (auto* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    if (Duration > 0.f) GetWorldTimerManager().SetTimer(BossTransitionTimer, this, &ThisClass::FinishBossTransition, Duration, false);
    if (auto* VFX = Row.PhaseVFX.Get()) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX, GetActorLocation());
    if (auto* Sound = Row.PhaseSound.Get()) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
    UE_LOG(LogTemp, Log, TEXT("PGBoss Phase=2 health=%.1f duration=%.2f"), Health, Duration);
    return true;
}

void APGCharacterEnemy::FinishBossTransition()
{
    PhaseTransitionUntil = 0;
    PublishBossPresentation();
}

void APGCharacterEnemy::PublishBossPresentation(bool bHidePresentation) const
{
    if (bBossPresentationClosed) return;
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Row = Tables ? Tables->GetRowData<FPGEnemyDataRow>(CharacterTID) : nullptr;
    auto* Messages = UPGMessageManager::Get(this);
    if (!Row || Row->Role != EPGEnemyRole::Boss || !Messages || !AbilitySystemComponent) return;
    FPGSharedBossPresentation View;
    View.Owner = const_cast<APGCharacterEnemy*>(this);
    View.Name = FText::FromName(Row->EnemyName);
    View.HealthRatio = FMath::Clamp(AbilitySystemComponent->GetHealth() / FMath::Max(1.f, AbilitySystemComponent->GetCombatStat(EPGStatType::Health)), 0.f, 1.f);
    View.Phase = BossPhase;
    View.DefeatDisplaySeconds = FMath::Clamp(Row->DefeatDisplaySeconds, 0.f, 10.f);
    View.State = bHidePresentation ? EPGBossCombatState::Hidden : View.HealthRatio <= 0 ? EPGBossCombatState::Defeated :
        IsBossTransitioning() ? EPGBossCombatState::Transition : bPatternRecovering ? EPGBossCombatState::Recovery :
        bPatternStriking ? EPGBossCombatState::Attacking : bPatternActive ? EPGBossCombatState::Windup : EPGBossCombatState::Preparing;
    if (ActivePatternID > 0)
        if (const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(ActivePatternID)) View.Attack = FText::FromString(Skill->Desc);
    Messages->SendMessage(EPGUIMessageType::BossPresentation, &View);
}

float APGCharacterEnemy::GetDirectionalDamageScale(const AActor* Attacker) const
{
    if (!bGuarding || bPatternRecovering || !Attacker) return 1.f;
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Row = Tables ? Tables->GetRowData<FPGEnemyDataRow>(CharacterTID) : nullptr;
    if (!Row || Row->Role != EPGEnemyRole::Guardian) return 1.f;
    const FVector Direction = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    return !Direction.IsNearlyZero() && FVector::DotProduct(GetActorForwardVector(), Direction) >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Row->GuardHalfAngle, 0.f, 180.f)))
        ? 1.f - FMath::Clamp(Row->GuardReduction, 0.f, .95f) : 1.f;
}

void APGCharacterEnemy::ClearPatternHitboxes()
{
    for (auto* Box : {LeftHandCollisionBox, RightHandCollisionBox, LeftFootCollisionBox, RightFootCollisionBox, TailCollisionBox})
        if (Box) Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (APGWeaponBase* Weapon = CombatComponent ? CombatComponent->GetCharacterCurrentEquippedWeapon() : nullptr)
        if (auto* Box = Weapon->GetWeaponCollisionBox()) Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APGCharacterEnemy::SetGuarding(bool bEnabled)
{
    bGuarding = bEnabled;
    EnemyPresentation->SetGuarding(bEnabled);
    if (!bEnabled)
    {
        if (GuardDecal) GuardDecal->SetVisibility(false);
        return;
    }
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Row = Tables ? Tables->GetRowData<FPGEnemyDataRow>(CharacterTID) : nullptr;
    if (!Row || Row->Role != EPGEnemyRole::Guardian || Row->SkillIdList.IsEmpty()) return;
    const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(Row->SkillIdList[0]);
    if (!GuardDecal && Skill && Skill->TelegraphMaterial.IsValid())
    {
        GuardDecal = NewObject<UDecalComponent>(this);
        GuardDecal->SetupAttachment(GetRootComponent());
        GuardDecal->DecalSize = FVector(120.f, 120.f, 120.f);
        GuardDecal->SetDecalMaterial(Skill->TelegraphMaterial.Get());
        GuardDecal->RegisterComponent();
        GuardDecal->CreateDynamicMaterialInstance();
    }
    if (!GuardDecal) return;
    const FVector Ground = GetActorLocation() - FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    GuardDecal->SetWorldLocationAndRotation(Ground, FRotator(-90,0,0));
    GuardDecal->SetVisibility(true);
    if (auto* Material = Cast<UMaterialInstanceDynamic>(GuardDecal->GetDecalMaterial()))
    {
        const FVector Forward = GetActorForwardVector();
        Material->SetVectorParameterValue(TEXT("Center"), FLinearColor(Ground.X,Ground.Y,Ground.Z));
        Material->SetVectorParameterValue(TEXT("Forward"), FLinearColor(Forward.X,Forward.Y,0));
        Material->SetVectorParameterValue(TEXT("GradeColor"), FLinearColor(.12f,.35f,1.2f));
        Material->SetScalarParameterValue(TEXT("Shape"), 1.f);
        Material->SetScalarParameterValue(TEXT("Radius"), 120.f);
        Material->SetScalarParameterValue(TEXT("CosAngle"), FMath::Cos(FMath::DegreesToRadians(Row->GuardHalfAngle)));
    }
}
