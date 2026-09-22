#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

float UPGAbilitySystemComponent::ReceiveProcDamage(UPGAbilitySystemComponent* Source, float Damage)
{
    if (!IsValid(Source) || Source->GetHealth() <= 0 || GetHealth() <= 0 || !FMath::IsFinite(Damage) || Damage <= 0) return 0;
    const float Before = GetHealth();
    auto* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UPGAtrributeSet::GetDamageTakenAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(Damage);
    Effect->Modifiers.Add(Modifier);
    ApplyGameplayEffectToSelf(Effect, 1.f, Source->MakeEffectContext());
    // Secondary damage deliberately bypasses all on-hit procs (no recursive explosions).
    return FMath::Max(0.f, Before - GetHealth());
}

void UPGAbilitySystemComponent::AddBleed(UPGAbilitySystemComponent* Source, float Damage)
{
    if (!GetWorld() || GetHealth() <= 0 || !Source) return;
    const auto* Tuning = Source->CombatTuning ? Source->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    BleedSource = Source;
    BleedStacks = FMath::Min(FMath::Max(1, Tuning->BleedMaxStacks), BleedStacks + 1);
    BleedDamage = Damage * BleedStacks * (1.f + Source->GetPerkPercent(EPGCombatPerk::BleedPotency) * .01f);
    BleedRemaining = FMath::Clamp(Tuning->BleedTicks, 1, 60);
    // Refresh duration without postponing the next tick on rapid attacks.
    if (!GetWorld()->GetTimerManager().IsTimerActive(BleedTimer))
        GetWorld()->GetTimerManager().SetTimer(BleedTimer, this, &ThisClass::TickBleed, FMath::Max(.1f, Tuning->BleedInterval), true);
}

void UPGAbilitySystemComponent::TickBleed()
{
    auto* Source = BleedSource.Get();
    if (!Source || Source->GetHealth() <= 0 || GetHealth() <= 0 || BleedRemaining <= 0)
    {
        GetWorld()->GetTimerManager().ClearTimer(BleedTimer); BleedRemaining = 0; BleedStacks = 0; return;
    }
    --BleedRemaining;
    const auto* Tuning = Source->CombatTuning ? Source->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    const FVector Center = GetAvatarActor()->GetActorLocation();
    ReceiveProcDamage(Source, BleedDamage);
    PlayBuildVFX(Tuning->BleedVFX, Center);
    if (GetHealth() <= 0 && Source->GetPerkPercent(EPGCombatPerk::BleedSpread) > 0)
        Source->Pulse(Center, BleedDamage * Source->GetPerkPercent(EPGCombatPerk::BleedSpread) * .01f, Tuning->ProcRadius, true);
}

void UPGAbilitySystemComponent::Pulse(const FVector& Center, float Damage, float Radius, bool bSpread)
{
    if (!GetWorld() || GetHealth() <= 0 || !Cast<APGCharacterPlayer>(GetAvatarActor())) return;
    TArray<TEnumAsByte<EObjectTypeQuery>> Types; Types.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    TArray<AActor*> Ignore; Ignore.Add(GetAvatarActor());
    TArray<AActor*> Targets;
    UKismetSystemLibrary::SphereOverlapActors(this, Center, FMath::Clamp(Radius, 1.f, 1500.f), Types, APGCharacterEnemy::StaticClass(), Ignore, Targets);
    const auto* Tuning = CombatTuning ? CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    PlayBuildVFX(bSpread ? Tuning->BleedVFX : Tuning->ShockVFX, Center);
    for (AActor* Actor : Targets)
    {
        auto* Enemy = Cast<APGCharacterEnemy>(Actor);
        auto* Target = Enemy ? Enemy->GetPGAbilitySystemComponent() : nullptr;
        if (!Target || Target->GetHealth() <= 0) continue;
        if (bSpread) Target->AddBleed(this, Damage);
        else
        {
            const float Bonus = Target->GetHealth() <= Target->GetCombatStat(EPGStatType::Health) * .35f ? GetPerkPercent(EPGCombatPerk::ShockExecute) * .01f : 0.f;
            Target->ReceiveProcDamage(this, Damage * (1.f + Bonus));
        }
    }
}

float UPGAbilitySystemComponent::GetFrenzyRate() const
{
    if (!GetWorld() || GetWorld()->GetTimeSeconds() >= FrenzyUntil || GetHealth() <= 0) return 1.f;
    return 1.f + FMath::Min(.75f, FrenzyStacks * GetPerkPercent(EPGCombatPerk::Frenzy) * .01f);
}

void UPGAbilitySystemComponent::PlayBuildVFX(const TSoftObjectPtr<UNiagaraSystem>& Effect, const FVector& Location)
{
    // Assets are preloaded before combat; never synchronously load from a hit callback.
    if (UNiagaraSystem* Asset = Effect.Get()) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Asset, Location);
}
