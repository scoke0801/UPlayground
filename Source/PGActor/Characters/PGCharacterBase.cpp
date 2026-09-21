// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterBase.h"
#include "PGData/DataAsset/Combat/PGCombatFeedbackData.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"

#include "MotionWarpingComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "MotionWarpingComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PGActor/Components/Stat/PGStatComponent.h"

const FName DissolveEdgeColorName = FName("DissolveEdgeColor");
const FName DissolveParticleColorName = FName("DissolveParticleColor");

// Sets default values
APGCharacterBase::APGCharacterBase()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GetMesh()->bReceivesDecals = false;
	
	AbilitySystemComponent = CreateDefaultSubobject<UPGAbilitySystemComponent>(TEXT("PGAbilitySystemComponent"));

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void APGCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (nullptr != GetStatComponent())
	{
		GetStatComponent()->InitData(CharacterTID);

		UpdateMovementSpeed();
	}
	// SkillHandler = FPGHandler::Create<FPGSkillHandler>();
}

void APGCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AbilitySystemComponent) AbilitySystemComponent->CancelAbilities();
	if (nullptr != SkillHandler)
	{
		delete SkillHandler;
        SkillHandler = nullptr;
	}
	EndHitStop();
    GetWorldTimerManager().ClearAllTimersForObject(this);
    Super::EndPlay(EndPlayReason);
}

void APGCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (nullptr != AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void APGCharacterBase::OnDied()
{
}

UAbilitySystemComponent* APGCharacterBase::GetAbilitySystemComponent() const
{
	return GetPGAbilitySystemComponent();
}

UPGAbilitySystemComponent* APGCharacterBase::GetPGAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APGCharacterBase::PlayVFX(UNiagaraSystem* ToPlayTemplate)
{
}

void APGCharacterBase::PlayDeathDissolveVFX(UNiagaraSystem* ToPlayTemplate)
{
	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
						ToPlayTemplate, GetMesh(), NAME_None,
						FVector::ZeroVector,FRotator::ZeroRotator, EAttachLocation::Type::KeepRelativeOffset,
						true,true);


	UMaterialInstanceDynamic* DynamicMaterial = GetMesh()->CreateDynamicMaterialInstance(0, GetMesh()->GetMaterial(0));
	if (!DynamicMaterial)
	{
		return;
	}

	FLinearColor DissolveEdgeColor;
	if (DynamicMaterial->GetVectorParameterValue(DissolveEdgeColorName, DissolveEdgeColor))
	{
		NiagaraComp->SetVariableLinearColor(DissolveParticleColorName, DissolveEdgeColor);
	}
	
	NiagaraComp->SetCustomTimeDilation(3.0f);
}

void APGCharacterBase::UpdateMovementSpeed()
{
	UPGStatComponent* StatComp = GetStatComponent();
	if (nullptr == StatComp)
	{
		return;
	}

	int32 MoveSpeed= StatComp->GetStat(EPGStatType::MovementSpeed);
	if (0 < MoveSpeed)
	{
		if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
		{
			MovementComp->MaxWalkSpeed = MoveSpeed;
		}
	}
}


void APGCharacterBase::OnHealthChanged()
{
    if (!bDeathStarted && GetStatComponent() && GetStatComponent()->GetCurrentHealth() <= 0.f)
    {
        bDeathStarted = true;
        AbilitySystemComponent->CancelAbilities();
        EndHitStop();
        AbilitySystemComponent->ClearBufferedInput();
        GetCharacterMovement()->StopMovementImmediately();
        // Keep the existing death ability/animation; guarantee cleanup if it is missing.
        GetWorldTimerManager().SetTimer(DeathFallbackTimer, this, &ThisClass::OnDied, 3.f, false);
        if (!AbilitySystemComponent->HasMatchingGameplayTag(PGGamePlayTags::Shared_Status_Dead))
            AbilitySystemComponent->AddLooseGameplayTag(PGGamePlayTags::Shared_Status_Dead);
    }
}
void APGCharacterBase::ApplyHitStop(float Seconds)
{
    if (!GetMesh() || Seconds <= 0.f) return;
    if (GetWorldTimerManager().IsTimerActive(HitStopTimer)) { ++SuppressedFeedbackRequests; return; }
    TotalHitStopSeconds += FMath::Clamp(Seconds, 0.001f, 0.15f);
    SavedAnimRate = GetMesh()->GlobalAnimRateScale;
    GetMesh()->GlobalAnimRateScale = 0.f;
    GetWorldTimerManager().SetTimer(HitStopTimer, this, &ThisClass::EndHitStop, FMath::Clamp(Seconds, 0.001f, 0.15f), false);
}
void APGCharacterBase::EndHitStop()
{
    if (GetMesh() && GetWorldTimerManager().IsTimerActive(HitStopTimer)) GetMesh()->GlobalAnimRateScale = SavedAnimRate;
    // The timer may already be expired when this callback runs.
    if (GetMesh() && GetMesh()->GlobalAnimRateScale == 0.f) GetMesh()->GlobalAnimRateScale = SavedAnimRate;
    GetWorldTimerManager().ClearTimer(HitStopTimer);
}
const UPGCombatFeedbackData* APGCharacterBase::GetCombatFeedbackData() const
{
    return FeedbackData ? FeedbackData.Get() : GetDefault<UPGCombatFeedbackData>();
}
void APGCharacterBase::PlayCombatFeedback(AActor* Source, EPGDamageType Type)
{
    if (FeedbackIntensity <= 0.f) return;
    auto* Attacker = Cast<APGCharacterBase>(Source);
    const auto* Data = Attacker && Attacker->IsPlayerControlled() ? Attacker->GetCombatFeedbackData() : GetCombatFeedbackData();
    const double Now = FPlatformTime::Seconds();
    if (Now - LastFeedbackTime < FMath::Max(0.f, Data->MinimumFeedbackInterval)) { ++SuppressedFeedbackRequests; return; }
    LastFeedbackTime = Now;
    const auto& Impact = Type == EPGDamageType::Critical ? Data->Critical : Attacker && Attacker->bPerformingHeavyAttack ? Data->Heavy : Data->Normal;
    const float Scale = FMath::Clamp(FeedbackIntensity, 0.f, 1.f) * FMath::Clamp(Impact.Intensity, 0.f, 3.f);
    const float Stop = FMath::Clamp(Impact.HitStop * Scale, 0.f, FMath::Clamp(Data->MaximumHitStop, 0.f, .15f));
    ApplyHitStop(Stop);
    if (Attacker) Attacker->ApplyHitStop(Stop * FMath::Clamp(Attacker->FeedbackIntensity, 0.f, 1.f));
    auto* VFX = Impact.VFX ? Impact.VFX.Get() : Data->ImpactVFX.Get();
    auto* Sound = Impact.Sound ? Impact.Sound.Get() : Data->ImpactSound.Get();
    if (VFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX, GetActorLocation(), FRotator::ZeroRotator, FVector(FMath::Max(.1f, Scale)));
    if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), FMath::Min(1.f, Scale));
    APGCharacterBase* Viewer = IsPlayerControlled() ? this : Attacker && Attacker->IsPlayerControlled() ? Attacker : nullptr;
    if (Viewer)
    {
        const auto Shake = Impact.Shake ? Impact.Shake : Data->CameraShake;
        if (Now - Viewer->LastCameraShakeTime >= FMath::Max(.01f, Data->CameraShakeInterval))
        {
            if (auto* PC = Cast<APlayerController>(Viewer->GetController()); PC && Shake)
            {
                Viewer->LastCameraShakeTime = Now;
                PC->ClientStartCameraShake(Shake, Scale * Viewer->FeedbackIntensity);
            }
        }
        else ++Viewer->SuppressedFeedbackRequests;
    }
}
