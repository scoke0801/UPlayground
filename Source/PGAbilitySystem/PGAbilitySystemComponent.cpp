


#include "PGAbilitySystemComponent.h"
#include "PGAtrributeSet.h"
#include "GameplayEffect.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "TimerManager.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"
#include "Combat/PGCombatMath.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Stat/PGStatComponent.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"

#include "Abilities/PGPlayerGameplayAbility.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UPGMessageManager* Manager = UPGMessageManager::Get(this))
		DelegateHandle = Manager->RegisterDelegate(EPGUIMessageType::ClickSkillButton, this, &ThisClass::OnClickedSkillButton);
}

void UPGAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearBufferedInput();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BleedTimer);
    if (UPGMessageManager* Manager = UPGMessageManager::Get(this))
        Manager->UnregisterDelegate(EPGUIMessageType::ClickSkillButton, DelegateHandle);
    Super::EndPlay(EndPlayReason);
}

void UPGAbilitySystemComponent::OnAbilityInputPressed(const FGameplayTag& InInputTag)
{
    if (!InInputTag.IsValid() || HasMatchingGameplayTag(PGGamePlayTags::Shared_Status_Dead)) return;
    if (const APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetAvatarActor()))
        if (!Player->IsGameplayInputAllowed()) { ClearBufferedInput(); return; }
    ClearBufferedInput();
    if (TryInput(InInputTag)) return;
    // Toggle actions must never turn on later after an unrelated state change.
    if (InputBufferSeconds <= 0.f || InInputTag.MatchesTag(PGGamePlayTags::InputTag_Toggleable)) return;
    BufferedInput = InInputTag;
    BufferExpiresAt = FPlatformTime::Seconds() + InputBufferSeconds;
    GetWorld()->GetTimerManager().SetTimer(InputBufferTimer, this, &ThisClass::RetryBufferedInput, 0.01f, true);
}

void UPGAbilitySystemComponent::OnAbilityInputReleased(const FGameplayTag& InInputTag)
{
	if (InInputTag == BufferedInput && InInputTag.MatchesTag(PGGamePlayTags::InputTag_MustBeHeld)) ClearBufferedInput();
    if (false == InInputTag.IsValid() ||
		false == InInputTag.MatchesTag(PGGamePlayTags::InputTag_MustBeHeld))
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.DynamicAbilityTags.HasTagExact(InInputTag) && AbilitySpec.IsActive())
		{
			CancelAbilityHandle(AbilitySpec.Handle);
		}
	}
}

void UPGAbilitySystemComponent::GrantPlayerWeaponAbilities(const TArray<FPGPlayerAbilitySet>& InDefaultWeaponAbilities,
	int32 ApplyLevel, TArray<FGameplayAbilitySpecHandle>& OutGrandesdAbilitySpecHandles)
{
	if (true == InDefaultWeaponAbilities.IsEmpty())
	{
		return;
	}

	for (const FPGPlayerAbilitySet& AbilitySet : InDefaultWeaponAbilities)
	{
		if (false == AbilitySet.IsValid())
		{
			continue;
		}

		// Startup/profile loadouts may already grant the same weapon skill.
        if (FindAbilitySpecFromClass(AbilitySet.AbilityToGrant)) continue;
		FGameplayAbilitySpec AbilitySpec(AbilitySet.AbilityToGrant);
		AbilitySpec.SourceObject = GetAvatarActor();
		AbilitySpec.Level = ApplyLevel;
		AbilitySpec.DynamicAbilityTags.AddTag(AbilitySet.InputTag);
		
		OutGrandesdAbilitySpecHandles.AddUnique(GiveAbility(AbilitySpec));
	}
}

void UPGAbilitySystemComponent::RemoveGrantedPlayerAbilities(UPARAM(ref) TArray<FGameplayAbilitySpecHandle>& InSpecHandlesToRemove)
{
	if (true == InSpecHandlesToRemove.IsEmpty())
	{	
		return;
	}

	for (const FGameplayAbilitySpecHandle& AbilitySpec : InSpecHandlesToRemove)
	{
		if (AbilitySpec.IsValid())
		{
			ClearAbility(AbilitySpec);
		}
	}

	InSpecHandlesToRemove.Empty();
}

bool UPGAbilitySystemComponent::TryActivateAbilityByTag(FGameplayTag AbilityTagToActivate)
{
	check(AbilityTagToActivate.IsValid());

	TArray<FGameplayAbilitySpec*> FoundAbilitySpecs;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTagToActivate.GetSingleTagContainer(),
		FoundAbilitySpecs);

	if (false == FoundAbilitySpecs.IsEmpty())
	{
		const int32 RandomAbilityIndex = FMath::RandRange(0, FoundAbilitySpecs.Num() - 1);
		FGameplayAbilitySpec* SpecToActivate = FoundAbilitySpecs[RandomAbilityIndex];

		check(SpecToActivate);

		if (false == SpecToActivate->IsActive())
		{
			// Ability 에서 Tag정보 사용할 수 있도록 임시 설정
			SpecToActivate->DynamicAbilityTags.AddTag(AbilityTagToActivate);
			
			bool bResult = TryActivateAbility(SpecToActivate->Handle);

			// 활성화 후 태그 제거
			SpecToActivate->DynamicAbilityTags.RemoveTag(AbilityTagToActivate);

			return bResult;
		}
	}

	return false;
}

void UPGAbilitySystemComponent::OnClickedSkillButton(const IPGEventData* InData)
{
	const FPGEventDataOneParam<FGameplayTag>* CastedParam =
		static_cast<const FPGEventDataOneParam<FGameplayTag>*>(InData);

	if (nullptr == CastedParam)
	{
		return;
	}
	if (const APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetAvatarActor()))
        if (Player->IsLocallyControlled()) OnAbilityInputPressed(CastedParam->Value);
}

UPGAbilitySystemComponent::UPGAbilitySystemComponent()
{

}
FGameplayAttribute UPGAbilitySystemComponent::AttributeForStat(EPGStatType Type)
{
    switch (Type)
    {
    case EPGStatType::Health: return UPGAtrributeSet::GetMaxHealthAttribute();
    case EPGStatType::Attack: return UPGAtrributeSet::GetAttackPowerAttribute();
    case EPGStatType::Defense: return UPGAtrributeSet::GetDefensePowerAttribute();
    case EPGStatType::CriticalRate: return UPGAtrributeSet::GetCriticalRateAttribute();
    case EPGStatType::CriticalDamage: return UPGAtrributeSet::GetCriticalDamageAttribute();
    case EPGStatType::HealAmount: return UPGAtrributeSet::GetHealAmountAttribute();
    case EPGStatType::MovementSpeed: return UPGAtrributeSet::GetMovementSpeedAttribute();
    default: return FGameplayAttribute();
    }
}
void UPGAbilitySystemComponent::InitializeCombatStats(const TMap<EPGStatType, int32>& Stats)
{
    if (bStatsInitialized) return;
    bStatsInitialized = true;
    CombatAttributes = const_cast<UPGAtrributeSet*>(Cast<UPGAtrributeSet>(InitStats(UPGAtrributeSet::StaticClass(), nullptr)));
    for (const auto& Pair : Stats)
    {
        const auto Attribute = AttributeForStat(Pair.Key);
        if (Attribute.IsValid()) SetNumericAttributeBase(Attribute, FMath::Max(0, Pair.Value));
    }
    SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), GetCombatStat(EPGStatType::Health));
}
float UPGAbilitySystemComponent::GetCombatStat(EPGStatType Type) const
{
    const auto Attribute = AttributeForStat(Type);
    return Attribute.IsValid() ? GetNumericAttribute(Attribute) : 0.f;
}
float UPGAbilitySystemComponent::GetHealth() const { return GetNumericAttribute(UPGAtrributeSet::GetCurrentHealthAttribute()); }

float UPGAbilitySystemComponent::ReceiveCombatHit(UPGAbilitySystemComponent* Source, EPGDamageType& OutType)
{
    OutType = EPGDamageType::Normal;
    if (!Source || GetHealth() <= 0.f || HasMatchingGameplayTag(PGGamePlayTags::Shared_Status_Dead)) return 0.f;
    const UPGCombatTuningData* Tuning = CombatTuning ? CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    const bool bCritical = FMath::FRand() * 10000.f < FMath::Clamp(Source->GetCombatStat(EPGStatType::CriticalRate), 0.f, 10000.f);
    OutType = bCritical ? EPGDamageType::Critical : EPGDamageType::Normal;
    float Damage = PGCombatMath::Damage(Source->GetCombatStat(EPGStatType::Attack), GetCombatStat(EPGStatType::Defense),
        bCritical, Source->GetCombatStat(EPGStatType::CriticalDamage), Tuning->DefenseConstant, Tuning->BaseCriticalMultiplier, Tuning->MinimumDamage);
    const float Before = GetHealth();
    const auto* SourceTuning = Source->CombatTuning ? Source->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    float Bonus = 0.f;
    if (Before <= GetCombatStat(EPGStatType::Health) * FMath::Clamp(SourceTuning->ExecutionHealthThreshold, 0.f, 1.f))
        Bonus += Source->GetPerkPercent(EPGCombatPerk::Execution) * .01f;
    if (IsRecoveryExposed()) Bonus += RecoveryDamageBonus + Source->GetPerkPercent(EPGCombatPerk::Counter) * .01f;
    Damage *= (1.f + Bonus) * (1.f - FMath::Min(60, GetPerkPercent(EPGCombatPerk::FrenzyGuard)) * .01f * (GetFrenzyRate() > 1.f ? 1.f : 0.f));
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UPGAtrributeSet::GetDamageTakenAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(Damage);
    Effect->Modifiers.Add(Modifier);
    ApplyGameplayEffectToSelf(Effect, 1.f, Source->MakeEffectContext());
    const float Applied = FMath::Max(0.f, Before - GetHealth());
    // Overkill is excluded, so a nearly dead target cannot provide a full-hit heal.
    if (Source != this) Source->RestoreHealth(Applied * Source->GetPerkPercent(EPGCombatPerk::LifeSteal) * .01f);
    if (Applied > 0 && Source != this && Cast<APGCharacterPlayer>(Source->GetAvatarActor()) && Cast<APGCharacterEnemy>(GetAvatarActor()))
    {
        const double Now = GetWorld()->GetTimeSeconds();
        if (Source->GetPerkPercent(EPGCombatPerk::Frenzy) > 0)
        {
            if (Now > Source->FrenzyUntil) Source->FrenzyStacks = 0;
            Source->FrenzyStacks = FMath::Min(SourceTuning->FrenzyMaxStacks, Source->FrenzyStacks + 1);
            Source->FrenzyUntil = Now + SourceTuning->FrenzySeconds * (1.f + Source->GetPerkPercent(EPGCombatPerk::FrenzyDuration) * .01f);
            Source->RestoreHealth(Applied * Source->GetPerkPercent(EPGCombatPerk::FrenzyLeech) * .01f);
            if (Now >= Source->NextFrenzyVFXAt) { Source->NextFrenzyVFXAt = Now + 1.; Source->PlayBuildVFX(SourceTuning->FrenzyVFX, Source->GetAvatarActor()->GetActorLocation()); }
        }
        const bool bWasBleeding = BleedRemaining > 0;
        if (GetHealth() > 0 && Source->bHeavySkill && bWasBleeding && Source->GetPerkPercent(EPGCombatPerk::BleedBurst) > 0)
        {
            const float Burst = BleedDamage * BleedRemaining * Source->GetPerkPercent(EPGCombatPerk::BleedBurst) * .01f;
            BleedRemaining = 0; BleedStacks = 0;
            ReceiveProcDamage(Source, Burst);
        }
        if (GetHealth() > 0 && Source->GetPerkPercent(EPGCombatPerk::Bleed) > 0)
            AddBleed(Source, Damage * Source->GetPerkPercent(EPGCombatPerk::Bleed) * .01f);
        if (GetHealth() <= 0 && bWasBleeding && Source->GetPerkPercent(EPGCombatPerk::BleedSpread) > 0)
            Source->Pulse(GetAvatarActor()->GetActorLocation(), Damage * .2f, SourceTuning->ProcRadius, true);
        if (Source->bHeavySkill && Source->GetPerkPercent(EPGCombatPerk::Shockwave) > 0 && Now >= Source->NextShockAt)
        {
            Source->NextShockAt = Now + FMath::Max(.1f, SourceTuning->ShockCooldown);
            const FVector Center = GetAvatarActor()->GetActorLocation();
            const float Radius = SourceTuning->ProcRadius * (1.f + Source->GetPerkPercent(EPGCombatPerk::ShockRadius) * .01f);
            const float Power = Damage * Source->GetPerkPercent(EPGCombatPerk::Shockwave) * .01f;
            Source->Pulse(Center, Power, Radius, false);
            if (Source->GetPerkPercent(EPGCombatPerk::ShockEcho) > 0)
            {
                FTimerHandle Echo;
                const float EchoPower = Power * Source->GetPerkPercent(EPGCombatPerk::ShockEcho) * .01f;
                GetWorld()->GetTimerManager().SetTimer(Echo, FTimerDelegate::CreateWeakLambda(Source, [Source, Center, EchoPower, Radius]()
                { if (Source->GetHealth() > 0) Source->Pulse(Center, EchoPower, Radius, false); }), .3f, false);
            }
        }
    }
    return Applied;
}
void UPGAbilitySystemComponent::SetCombatPerks(const TMap<EPGCombatPerk, int32>& Perks)
{
    if (CombatTuning && PreparedBuildEffects.IsEmpty())
        for (const auto& Asset : {CombatTuning->BleedVFX, CombatTuning->ShockVFX, CombatTuning->FrenzyVFX})
            if (auto* Loaded = Asset.LoadSynchronous()) PreparedBuildEffects.Add(Loaded);
    CombatPerks.Reset();
    for (auto Pair : Perks)
        if (Pair.Key > EPGCombatPerk::None && Pair.Key < EPGCombatPerk::Max) CombatPerks.Add(Pair.Key, FMath::Clamp(Pair.Value, 0, 100));
}
int32 UPGAbilitySystemComponent::GetPerkPercent(EPGCombatPerk Perk) const { return CombatPerks.FindRef(Perk); }
void UPGAbilitySystemComponent::OpenRecoveryWindow(float Duration, float Bonus)
{
    CloseRecoveryWindow();
    if (!GetWorld() || !FMath::IsFinite(Duration) || !FMath::IsFinite(Bonus) || Duration <= 0.f) return;
    RecoveryExpiresAt = GetWorld()->GetTimeSeconds() + Duration;
    RecoveryDamageBonus = FMath::Clamp(Bonus, 0.f, 2.f);
}
void UPGAbilitySystemComponent::CloseRecoveryWindow() { RecoveryExpiresAt = 0.; RecoveryDamageBonus = 0.f; }
bool UPGAbilitySystemComponent::IsRecoveryExposed() const
{
    return GetWorld() && GetHealth() > 0.f && GetWorld()->GetTimeSeconds() < RecoveryExpiresAt;
}
float UPGAbilitySystemComponent::RestoreHealth(float Amount)
{
    if (!FMath::IsFinite(Amount) || Amount <= 0.f || GetHealth() <= 0.f || HasMatchingGameplayTag(PGGamePlayTags::Shared_Status_Dead)) return 0.f;
    const float Before = GetHealth();
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UPGAtrributeSet::GetCurrentHealthAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(FMath::Min(Amount, FMath::Max(0.f, GetCombatStat(EPGStatType::Health) - Before)));
    Effect->Modifiers.Add(Modifier);
    ApplyGameplayEffectToSelf(Effect, 1.f, MakeEffectContext());
    return FMath::Max(0.f, GetHealth() - Before);
}
bool UPGAbilitySystemComponent::ApplyStatBonus(EPGStatType Type, float Amount)
{
    const FGameplayAttribute Attribute = AttributeForStat(Type);
    if (!Attribute.IsValid() || !FMath::IsFinite(Amount) || Amount <= 0.f) return false;
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = Attribute;
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(Amount);
    Effect->Modifiers.Add(Modifier);
    return ApplyGameplayEffectToSelf(Effect, 1.f, MakeEffectContext()).WasSuccessfullyApplied();
}
void UPGAbilitySystemComponent::SetEquipmentBonuses(const TMap<EPGStatType, int32>& Bonuses)
{
    ClearBufferedInput();
    if (EquipmentEffect.IsValid()) RemoveActiveGameplayEffect(EquipmentEffect);
    EquipmentEffect.Invalidate();
    if (Bonuses.IsEmpty()) return;
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
    for (const auto& Pair : Bonuses)
    {
        const auto Attribute = AttributeForStat(Pair.Key);
        if (!Attribute.IsValid()) continue;
        FGameplayModifierInfo Modifier;
        Modifier.Attribute = Attribute;
        Modifier.ModifierOp = EGameplayModOp::Additive;
        Modifier.ModifierMagnitude = FScalableFloat(static_cast<float>(Pair.Value));
        Effect->Modifiers.Add(Modifier);
    }
    EquipmentEffect = ApplyGameplayEffectToSelf(Effect, 1.f, MakeEffectContext());
}
int32 UPGAbilitySystemComponent::HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
    if (EventTag == PGGamePlayTags::Shared_Event_HitReact)
    {
        APGCharacterBase* Victim = Cast<APGCharacterBase>(GetAvatarActor());
        const APGCharacterBase* Source = Payload ? Cast<APGCharacterBase>(Payload->Instigator.Get()) : nullptr;
        if (!IsValid(Victim) || !IsValid(Source) || GetHealth() <= 0.f) return 0;
        Victim->OnHit(Source->GetStatComponent(), Source->GetCombatComponent());
        if (GetHealth() <= 0.f) return 0;
    }
    return Super::HandleGameplayEvent(EventTag, Payload);
}
bool UPGAbilitySystemComponent::TryInput(const FGameplayTag& Tag)
{
    TArray<FGameplayAbilitySpecHandle> Handles;
    for (const auto& Spec : GetActivatableAbilities())
        if (Spec.GetDynamicSpecSourceTags().HasTagExact(Tag)) Handles.Add(Spec.Handle);
    for (const auto Handle : Handles)
    {
        const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
        if (!Spec) continue;
        if (Tag.MatchesTag(PGGamePlayTags::InputTag_Toggleable) && Spec->IsActive())
        {
            CancelAbilityHandle(Handle);
            return true;
        }
        // GAS checks whether this ability allows retriggering during its cancel window.
        if (TryActivateAbility(Handle)) return true;
    }
    return false;
}
void UPGAbilitySystemComponent::ClearBufferedInput()
{
    BufferedInput = FGameplayTag();
    BufferExpiresAt = 0.;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(InputBufferTimer);
}
void UPGAbilitySystemComponent::RetryBufferedInput()
{
    const APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetAvatarActor());
    if (!BufferedInput.IsValid() || FPlatformTime::Seconds() > BufferExpiresAt ||
        HasMatchingGameplayTag(PGGamePlayTags::Shared_Status_Dead) || (Player && !Player->IsGameplayInputAllowed()))
    {
        ClearBufferedInput();
        return;
    }
    if (TryInput(BufferedInput)) ClearBufferedInput();
}
void UPGAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
    Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
    CombatAttributes = const_cast<UPGAtrributeSet*>(Cast<UPGAtrributeSet>(InitStats(UPGAtrributeSet::StaticClass(), nullptr)));
}

void UPGAbilitySystemComponent::SetProfileBonuses(const TMap<EPGStatType, int32>& Bonuses)
{
    const float PreviousHealth = GetHealth();
    ClearBufferedInput();
    if (ProfileEffect.IsValid()) RemoveActiveGameplayEffect(ProfileEffect);
    ProfileEffect.Invalidate();
    if (Bonuses.IsEmpty()) return;
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
    for (const auto& Pair : Bonuses)
    {
        const auto Attribute = AttributeForStat(Pair.Key);
        if (!Attribute.IsValid()) continue;
        FGameplayModifierInfo Modifier;
        Modifier.Attribute = Attribute;
        Modifier.ModifierOp = EGameplayModOp::Additive;
        Modifier.ModifierMagnitude = FScalableFloat(static_cast<float>(Pair.Value));
        Effect->Modifiers.Add(Modifier);
    }
    ProfileEffect = ApplyGameplayEffectToSelf(Effect, 1.f, MakeEffectContext());
    // Replacing a max-health modifier must not lose health to the transient removal.
    if (PreviousHealth > 0.f) SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), FMath::Min(PreviousHealth, GetCombatStat(EPGStatType::Health)));
}
