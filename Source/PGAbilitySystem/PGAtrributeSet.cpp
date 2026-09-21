#include "PGAtrributeSet.h"
#include "GameplayEffectExtension.h"

UPGAtrributeSet::UPGAtrributeSet()
{
    InitCurrentHealth(1.f); InitMaxHealth(1.f);
    InitCurrentRage(0.f); InitMaxRage(1.f);
    InitAttackPower(1.f); InitDefensePower(0.f);
    InitDamageTaken(0.f); InitCriticalRate(0.f); InitCriticalDamage(0.f);
    InitHealAmount(0.f); InitMovementSpeed(600.f);
}
void UPGAtrributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    NewValue = FMath::IsFinite(NewValue) ? FMath::Max(0.f, NewValue) : 0.f;
    if (Attribute == GetCurrentHealthAttribute()) NewValue = FMath::Min(NewValue, GetMaxHealth());
    if (Attribute == GetCurrentRageAttribute()) NewValue = FMath::Min(NewValue, GetMaxRage());
    if (Attribute == GetCriticalRateAttribute()) NewValue = FMath::Min(NewValue, 10000.f);
}
void UPGAtrributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);
    // Increasing maximum health does not heal; decreasing it clamps current health.
    if (Attribute == GetMaxHealthAttribute() && GetCurrentHealth() > NewValue) SetCurrentHealth(NewValue);
    if (Attribute == GetMaxRageAttribute() && GetCurrentRage() > NewValue) SetCurrentRage(NewValue);
}
void UPGAtrributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    if (Data.EvaluatedData.Attribute == GetDamageTakenAttribute())
    {
        const float Damage = FMath::IsFinite(GetDamageTaken()) ? FMath::Max(0.f, GetDamageTaken()) : 0.f;
        SetDamageTaken(0.f);
        SetCurrentHealth(FMath::Clamp(GetCurrentHealth() - Damage, 0.f, GetMaxHealth()));
    }
    if (Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
        SetCurrentHealth(FMath::Clamp(GetCurrentHealth(), 0.f, GetMaxHealth()));
    if (Data.EvaluatedData.Attribute == GetCurrentRageAttribute())
        SetCurrentRage(FMath::Clamp(GetCurrentRage(), 0.f, GetMaxRage()));
}