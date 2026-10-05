#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Progression/PGRunTelemetrySubsystem.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGData/PGDataTableManager.h"

float UPGAbilitySystemComponent::ReceiveProcDamage(UPGAbilitySystemComponent* Source, float Damage, EPGDamageCause Cause)
{
    if (!IsValid(Source) || Source->GetHealth() <= 0 || GetHealth() <= 0 || !FMath::IsFinite(Damage) || Damage <= 0) return 0;
    TGuardValue<int32> Processing(DamageProcessingDepth, DamageProcessingDepth + 1);
    TGuardValue<int32> SourceProcessing(Source->DamageProcessingDepth, Source->DamageProcessingDepth + 1);
    const float Before = GetHealth();
    const bool bWasBleeding = BleedRemaining > 0 && BleedSource == Source && BleedSourceGeneration == Source->BleedGeneration;
    const float SpreadDamage = BleedDamage;
    auto* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UPGAtrributeSet::GetDamageTakenAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(Damage);
    Effect->Modifiers.Add(Modifier);
    auto* Telemetry = UPGRunTelemetrySubsystem::Get(this);
    const int32 TelemetrySample = Telemetry ? Telemetry->GetSampleIndex() : INDEX_NONE;
    ApplyGameplayEffectToSelf(Effect, 1.f, Source->MakeEffectContext());
    const float Applied = FMath::Max(0.f, Before - GetHealth());
    const bool bPlayerSource = Cast<APGCharacterPlayer>(Source->GetAvatarActor()) != nullptr;
    const bool bPlayerTarget = Cast<APGCharacterPlayer>(GetAvatarActor()) != nullptr;
    if (Telemetry && ((bPlayerSource && Cast<APGCharacterEnemy>(GetAvatarActor())) ||
        (bPlayerTarget && Cast<APGCharacterEnemy>(Source->GetAvatarActor()))))
        Telemetry->RecordDamage(TelemetrySample, Applied, bPlayerSource, bPlayerTarget, true);
    // Secondary damage deliberately bypasses all on-hit procs (no recursive explosions).
    if (Applied > 0 && bPlayerSource && Cast<APGCharacterEnemy>(GetAvatarActor()))
    {
        const auto* Tuning = Source->CombatTuning ? Source->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
        Source->LastBuildTarget = this;
        if (GetHealth() > 0 && (Cause == EPGDamageCause::Shockwave || Cause == EPGDamageCause::ShockEcho)) RegisterShockHit(Source);
        const auto Context = Source->ScopedCast;
        const bool bCanRefund = Context ? Context->bRefundEligible && !Context->bRefundUsed : Source->bHeavySkill && !Source->bRefundUsed;
        if (GetHealth() <= 0 && Cause == EPGDamageCause::BleedBurst && bCanRefund &&
            Source->GetPerkPercent(EPGCombatPerk::BleedRecast) > 0)
        {
            // Claim before callbacks/another target in the same montage can also die.
            Source->bRefundUsed = true;
            if (Context) Context->bRefundUsed = true;
            auto* Player = Cast<APGCharacterPlayer>(Source->GetAvatarActor());
            if (auto* Handler = Player->GetSkillHandler())
                if ((Context ? Handler->RefundRemainingCooldownByID(Context->SkillID, Tuning->BleedRefundFraction) :
                    Handler->RefundRemainingCooldown(Source->ActiveCombatSlot, Tuning->BleedRefundFraction)) > 0)
                {
                    Source->RefundProcUntil = GetWorld()->GetTimeSeconds() + Tuning->BuildProcDisplaySeconds;
                    Source->RefundSkillID = Context ? Context->SkillID : Handler->GetSkillID(Source->ActiveCombatSlot);
                }
        }
        if (GetHealth() <= 0 && bWasBleeding && Cause != EPGDamageCause::External && Source->GetPerkPercent(EPGCombatPerk::BleedSpread) > 0)
            Source->Pulse(GetAvatarActor()->GetActorLocation(), SpreadDamage * Source->GetPerkPercent(EPGCombatPerk::BleedSpread) * .01f, Tuning->ProcRadius, true);
    }
    return Applied;
}

void UPGAbilitySystemComponent::AddBleed(UPGAbilitySystemComponent* Source, float Damage, bool bSnapshot)
{
    if (!GetWorld() || GetHealth() <= 0 || !Source || !FMath::IsFinite(Damage) || Damage <= 0) return;
    const auto* Tuning = Source->CombatTuning ? Source->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    if (BleedRemaining <= 0 || BleedSource != Source || BleedSourceGeneration != Source->BleedGeneration) { BleedStacks = 0; BleedDamage = 0; }
    BleedSource = Source;
    BleedSourceGeneration = Source->BleedGeneration;
    if (bSnapshot)
    {
        // Spread already contains stacks and potency. Do not multiply them again along kill chains.
        BleedStacks = FMath::Max(1,BleedStacks);
        BleedDamage = FMath::Max(BleedDamage,Damage);
    }
    else
    {
        BleedStacks = FMath::Min(FMath::Max(1, Tuning->BleedMaxStacks), BleedStacks + 1);
        BleedDamage = Damage * BleedStacks * (1.f + Source->GetPerkPercent(EPGCombatPerk::BleedPotency) * .01f);
    }
    BleedRemaining = FMath::Clamp(Tuning->BleedTicks, 1, 60);
    // Refresh duration without postponing the next tick on rapid attacks.
    if (!GetWorld()->GetTimerManager().IsTimerActive(BleedTimer))
        GetWorld()->GetTimerManager().SetTimer(BleedTimer, this, &ThisClass::TickBleed, FMath::Max(.1f, Tuning->BleedInterval), true);
}

void UPGAbilitySystemComponent::TickBleed()
{
    auto* Source = BleedSource.Get();
    if (!Source || BleedSourceGeneration != Source->BleedGeneration || Source->GetHealth() <= 0 || Source->GetPerkPercent(EPGCombatPerk::Bleed) <= 0 || GetHealth() <= 0 || BleedRemaining <= 0)
    {
        GetWorld()->GetTimerManager().ClearTimer(BleedTimer); BleedRemaining = 0; BleedStacks = 0; return;
    }
    const auto* Tuning = Source->CombatTuning ? Source->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    const FVector Center = GetAvatarActor()->GetActorLocation();
    ReceiveProcDamage(Source, BleedDamage, EPGDamageCause::BleedTick);
    --BleedRemaining;
    PlayBuildVFX(Tuning->BleedVFX, Center);
    if (GetHealth() <= 0 || BleedRemaining <= 0)
    { GetWorld()->GetTimerManager().ClearTimer(BleedTimer); BleedRemaining = 0; BleedStacks = 0; }
}

void UPGAbilitySystemComponent::Pulse(const FVector& Center, float Damage, float Radius, bool bSpread, EPGDamageCause Cause)
{
    if (!GetWorld() || GetHealth() <= 0 || !Cast<APGCharacterPlayer>(GetAvatarActor())) return;
    TArray<TEnumAsByte<EObjectTypeQuery>> Types;
    Types.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel1)); // EnemyCharacter in DefaultEngine.ini
    Types.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn)); // Legacy actors
    TArray<AActor*> Ignore; Ignore.Add(GetAvatarActor());
    TArray<AActor*> Targets;
    UKismetSystemLibrary::SphereOverlapActors(this, Center, FMath::Clamp(Radius, 1.f, 1500.f), Types, APGCharacterEnemy::StaticClass(), Ignore, Targets);
    const auto* Tuning = CombatTuning ? CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    PlayBuildVFX(bSpread ? Tuning->BleedVFX : Cause == EPGDamageCause::FrenzyAfterimage ? Tuning->AfterimageVFX : Tuning->ShockVFX, Center);
    for (AActor* Actor : Targets)
    {
        auto* Enemy = Cast<APGCharacterEnemy>(Actor);
        auto* Target = Enemy ? Enemy->GetPGAbilitySystemComponent() : nullptr;
        if (!Target || Target->GetHealth() <= 0) continue;
        if (bSpread) Target->AddBleed(this, Damage, true);
        else
        {
            const bool bShock = Cause == EPGDamageCause::Shockwave || Cause == EPGDamageCause::ShockEcho;
            const float Bonus = bShock && Target->GetHealth() <= Target->GetCombatStat(EPGStatType::Health) * .35f ? GetPerkPercent(EPGCombatPerk::ShockExecute) * .01f : 0.f;
            Target->ReceiveProcDamage(this, Damage * (1.f + Bonus), Cause);
        }
    }
}

float UPGAbilitySystemComponent::GetFrenzyRate() const
{
    if (!GetWorld() || GetWorld()->GetTimeSeconds() >= FrenzyUntil || GetHealth() <= 0 || GetPerkPercent(EPGCombatPerk::Frenzy) <= 0) return 1.f;
    return 1.f + FMath::Min(.75f, FrenzyStacks * GetPerkPercent(EPGCombatPerk::Frenzy) * .01f);
}

void UPGAbilitySystemComponent::BeginCombatSkill(EPGSkillSlot Slot)
{
    ActiveCombatSlot = Slot;
    bHeavySkill = Slot >= EPGSkillSlot::SkillSlot_1 && Slot <= EPGSkillSlot::SkillSlot_6;
    bRefundUsed = false;
}

void UPGAbilitySystemComponent::OnDodgeCommitted()
{
    const auto* Tuning = CombatTuning ? CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    if (!GetWorld() || GetHealth() <= 0 || GetPerkPercent(EPGCombatPerk::FrenzyAfterimage) <= 0 ||
        GetFrenzyRate() <= 1.f || FrenzyStacks < FMath::Max(1, Tuning->FrenzyMaxStacks)) return;
    const FVector Origin = GetAvatarActor()->GetActorLocation();
    FrenzyStacks = 0; FrenzyUntil = 0.;
    AfterimageProcUntil = GetWorld()->GetTimeSeconds() + Tuning->BuildProcDisplaySeconds;
    Pulse(Origin, GetCombatStat(EPGStatType::Attack) * FMath::Max(0.f, Tuning->AfterimageAttackMultiplier),
        Tuning->AfterimageRadius, false, EPGDamageCause::FrenzyAfterimage);
}

void UPGAbilitySystemComponent::RegisterShockHit(UPGAbilitySystemComponent* Source)
{
    if (Source->GetPerkPercent(EPGCombatPerk::ShockFracture) <= 0 || Source->GetPerkPercent(EPGCombatPerk::Shockwave) <= 0) return;
    const auto* Tuning = Source->CombatTuning ? Source->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    const double Now = GetWorld()->GetTimeSeconds();
    if (ShockSource != Source || ShockSourceGeneration != Source->ShockGeneration || Now >= ShockStackUntil) ShockHits = 0;
    if (ShockSource != Source || ShockSourceGeneration != Source->ShockGeneration) WeaknessUntil = 0;
    ShockSource = Source;
    ShockSourceGeneration = Source->ShockGeneration;
    ShockStackUntil = Now + FMath::Max(.1f, Tuning->ShockStackSeconds);
    if (++ShockHits >= FMath::Max(1, Tuning->ShockFractureHits))
    {
        ShockHits = 0;
        WeaknessUntil = Now + FMath::Max(.1f, Tuning->ShockWeaknessSeconds);
    }
}

bool UPGAbilitySystemComponent::HasShockWeakness() const
{
    const auto* Source = ShockSource.Get();
    return GetWorld() && GetHealth() > 0 && GetWorld()->GetTimeSeconds() < WeaknessUntil && Source && Source->GetHealth() > 0 && ShockSourceGeneration == Source->ShockGeneration &&
        Source->GetPerkPercent(EPGCombatPerk::Shockwave) > 0 && Source->GetPerkPercent(EPGCombatPerk::ShockFracture) > 0;
}

float UPGAbilitySystemComponent::GetEffectiveDefense() const
{
    if (!HasShockWeakness()) return GetCombatStat(EPGStatType::Defense);
    const auto* Source = ShockSource.Get();
    const auto* Tuning = Source->CombatTuning ? Source->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    return GetCombatStat(EPGStatType::Defense) * (1.f - FMath::Clamp(Tuning->ShockDefenseReduction, 0.f, 1.f));
}

FPGBuildCombatState UPGAbilitySystemComponent::GetBuildCombatState() const
{
    FPGBuildCombatState State;
    if (!GetWorld() || GetHealth() <= 0) return State;
    const double Now = GetWorld()->GetTimeSeconds();
    const auto* Tuning = CombatTuning ? CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    State.FrenzyMaxStacks = FMath::Max(1, Tuning->FrenzyMaxStacks);
    State.FrenzyStacks = GetFrenzyRate() > 1.f ? FrenzyStacks : 0;
    State.FrenzySeconds = State.FrenzyStacks > 0 ? FMath::Max(0.f, float(FrenzyUntil - Now)) : 0;
    State.ShockHitsRequired = FMath::Max(1, Tuning->ShockFractureHits);
    State.bShockProc = Now < ShockProcUntil;
    State.bRefundProc = Now < RefundProcUntil;
    State.RefundSkillID = State.bRefundProc ? RefundSkillID : 0;
    State.bAfterimageProc = Now < AfterimageProcUntil;
    if (const auto* Target = LastBuildTarget.Get(); Target && Target->GetHealth() > 0)
    {
        State.TargetName = TEXT("최근 적중 대상");
        if (const auto* Enemy = Cast<APGCharacterEnemy>(Target->GetAvatarActor()))
            if (auto* Tables = UPGDataTableManager::Get(this))
                if (const auto* Row = Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID())) State.TargetName = Row->EnemyName.ToString();
        if (Target->BleedSource == this && Target->BleedSourceGeneration == BleedGeneration && Target->BleedRemaining > 0 && GetPerkPercent(EPGCombatPerk::Bleed) > 0)
        {
            State.BleedStacks = Target->BleedStacks;
            State.BleedSeconds = FMath::Max(0.f, GetWorld()->GetTimerManager().GetTimerRemaining(Target->BleedTimer)) +
                (Target->BleedRemaining - 1) * FMath::Max(.1f, Tuning->BleedInterval);
        }
        if (Target->ShockSource == this && Target->ShockSourceGeneration == ShockGeneration && GetPerkPercent(EPGCombatPerk::ShockFracture) > 0 && GetPerkPercent(EPGCombatPerk::Shockwave) > 0)
        {
            State.ShockHits = Now < Target->ShockStackUntil ? Target->ShockHits : 0;
            State.WeaknessSeconds = Target->HasShockWeakness() ? float(Target->WeaknessUntil - Now) : 0;
        }
    }
    return State;
}

void UPGAbilitySystemComponent::PlayBuildVFX(const TSoftObjectPtr<UNiagaraSystem>& Effect, const FVector& Location)
{
    // Assets are preloaded before combat; never synchronously load from a hit callback.
    if (UNiagaraSystem* Asset = Effect.Get()) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Asset, Location);
}
