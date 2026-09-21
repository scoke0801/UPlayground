#include "PGStatComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Stat/PGCharacterStatDataRow.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGAbilitySystem/Combat/PGCombatMath.h"
#include "GameFramework/CharacterMovementComponent.h"

UPGAbilitySystemComponent* UPGStatComponent::GetASC() const
{
    const APGCharacterBase* Character = Cast<APGCharacterBase>(GetOwner());
    return Character ? Character->GetPGAbilitySystemComponent() : nullptr;
}
void UPGStatComponent::InitData(int32 CharacterTID)
{
    if (PGData())
        if (const auto* Data = PGData()->GetRowData<FPGCharacterStatDataRow>(CharacterTID)) StatMap = Data->Stats;
    if (UPGAbilitySystemComponent* ASC = GetASC())
    {
        ASC->InitializeCombatStats(StatMap);
        if (!HealthChangedHandle.IsValid())
        {
            HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetCurrentHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
            MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
            MovementChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetMovementSpeedAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
        }
    }
}
void UPGStatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UPGAbilitySystemComponent* ASC = GetASC())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetCurrentHealthAttribute()).Remove(HealthChangedHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetMovementSpeedAttribute()).Remove(MovementChangedHandle);
    }
    Super::EndPlay(EndPlayReason);
}
void UPGStatComponent::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (APGCharacterBase* Character = Cast<APGCharacterBase>(GetOwner()))
    {
        Character->OnHealthChanged();
        if (Data.Attribute == UPGAtrributeSet::GetMovementSpeedAttribute())
            Character->GetCharacterMovement()->MaxWalkSpeed = GetStat(EPGStatType::MovementSpeed);
    }
}
float UPGStatComponent::GetCurrentHealth() const { const auto* ASC = GetASC(); return ASC ? ASC->GetHealth() : 0.f; }
float UPGStatComponent::GetHealthRatio() const
{
    const int32 Maximum = GetStat(EPGStatType::Health);
    return Maximum > 0 ? FMath::Clamp(GetCurrentHealth() / Maximum, 0.f, 1.f) : 0.f;
}
int32 UPGStatComponent::GetStat(EPGStatType Type) const
{
    if (const auto* ASC = GetASC()) return FMath::RoundToInt(ASC->GetCombatStat(Type));
    return StatMap.FindRef(Type);
}
int32 UPGStatComponent::CalculateDamage(const UPGStatComponent* Source, EPGDamageType& OutType) const
{
    OutType = EPGDamageType::Normal;
    if (!Source) return 0;
    const auto* ASC = GetASC();
    const UPGCombatTuningData* Tuning = ASC && ASC->CombatTuning ? ASC->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    const bool bCritical = FMath::FRand() * 10000.f < FMath::Clamp(Source->GetStat(EPGStatType::CriticalRate), 0, 10000);
    OutType = bCritical ? EPGDamageType::Critical : EPGDamageType::Normal;
    return FMath::RoundToInt(PGCombatMath::Damage(Source->GetStat(EPGStatType::Attack), GetStat(EPGStatType::Defense), bCritical,
        Source->GetStat(EPGStatType::CriticalDamage), Tuning->DefenseConstant, Tuning->BaseCriticalMultiplier, Tuning->MinimumDamage));
}
int32 UPGStatComponent::CalculateDamageWithWeapon(const UPGStatComponent* Source, const UPGPawnCombatComponent* Combat, EPGDamageType& Type) const { return CalculateDamage(Source, Type); }
int32 UPGStatComponent::CalculateDamageAuto(const UPGStatComponent* Source, const UPGPawnCombatComponent* Combat, EPGDamageType& Type) const { return CalculateDamage(Source, Type); }
int32 UPGStatComponent::GetTotalAttackPower(const UPGPawnCombatComponent* Combat) const { return GetStat(EPGStatType::Attack); }
int32 UPGStatComponent::CalculateHealAmount() const { return GetStat(EPGStatType::HealAmount); }
bool UPGStatComponent::ApplyStatReward(EPGStatType Type, int32 Amount) { auto* ASC = GetASC(); return ASC && ASC->ApplyStatBonus(Type, Amount); }