#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"
#include "TimerManager.h"

void UPGAbilitySystemComponent::ProcessProfileProcs(UPGAbilitySystemComponent* Source, float Damage, float Applied)
{
    const auto Context = Source->ScopedCast;
    const auto Policy = Source->ScopedProcPolicy;
    const auto* Tuning = Source->CombatTuning ? Source->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    const double Now = GetWorld()->GetTimeSeconds();
    Source->LastBuildTarget = this;
    const bool bDirectKill = GetHealth() <= 0.f;
    const bool bWasBleeding = BleedRemaining > 0 && BleedSource == Source && BleedSourceGeneration == Source->BleedGeneration;
    const float SpreadDamage = BleedDamage;
    if (Policy.bFrenzy && Source->GetPerkPercent(EPGCombatPerk::Frenzy) > 0 && Context->FrenzyGranted < Context->FrenzyCap)
    {
        ++Context->FrenzyGranted;
        if (Now > Source->FrenzyUntil) Source->FrenzyStacks = 0;
        Source->FrenzyStacks = FMath::Min(Tuning->FrenzyMaxStacks, Source->FrenzyStacks + 1);
        Source->FrenzyUntil = Now + Tuning->FrenzySeconds * (1.f + Source->GetPerkPercent(EPGCombatPerk::FrenzyDuration) * .01f);
        Source->RestoreHealth(Applied * Source->GetPerkPercent(EPGCombatPerk::FrenzyLeech) * .01f);
    }
    // Direct damage -> bleed -> eligible burst. Direct kills cannot start new bleed/burst.
    if (!bDirectKill && Policy.bBleed && Source->GetPerkPercent(EPGCombatPerk::Bleed) > 0)
        AddBleed(Source, Damage * Source->GetPerkPercent(EPGCombatPerk::Bleed) * .01f);
    if (!bDirectKill && Policy.bBleedBurst && Source->GetPerkPercent(EPGCombatPerk::BleedBurst) > 0 &&
        BleedRemaining > 0 && BleedSource == Source && BleedSourceGeneration == Source->BleedGeneration &&
        !Context->BurstTargets.Contains(GetAvatarActor()))
    {
        Context->BurstTargets.Add(GetAvatarActor());
        const float Burst = BleedDamage * BleedRemaining * Source->GetPerkPercent(EPGCombatPerk::BleedBurst) * .01f;
        BleedRemaining = 0; BleedStacks = 0;
        GetWorld()->GetTimerManager().ClearTimer(BleedTimer);
        ReceiveProcDamage(Source, Burst, EPGDamageCause::BleedBurst);
    }
    if (bDirectKill && bWasBleeding && Source->GetPerkPercent(EPGCombatPerk::BleedSpread) > 0)
        Source->Pulse(GetAvatarActor()->GetActorLocation(), SpreadDamage * Source->GetPerkPercent(EPGCombatPerk::BleedSpread) * .01f, Tuning->ProcRadius, true);
    if (Policy.bShock && !Context->bShockUsed && Source->GetPerkPercent(EPGCombatPerk::Shockwave) > 0 && Now >= Source->NextShockAt)
    {
        Context->bShockUsed = true;
        Source->NextShockAt = Now + FMath::Max(.1f, Tuning->ShockCooldown);
        Source->ShockProcUntil = Now + Tuning->BuildProcDisplaySeconds;
        const FVector Center = GetAvatarActor()->GetActorLocation();
        const float Radius = Tuning->ProcRadius * (1.f + Source->GetPerkPercent(EPGCombatPerk::ShockRadius) * .01f);
        const float Power = Damage * Source->GetPerkPercent(EPGCombatPerk::Shockwave) * .01f;
        Source->Pulse(Center, Power, Radius, false);
        if (Source->GetPerkPercent(EPGCombatPerk::ShockEcho) > 0)
        {
            const float Echo = Power * Source->GetPerkPercent(EPGCombatPerk::ShockEcho) * .01f;
            const uint32 Generation = Source->ShockGeneration;
            GetWorld()->GetTimerManager().SetTimer(Source->ShockEchoTimer, FTimerDelegate::CreateWeakLambda(Source,
                [Source, Center, Echo, Radius, Generation]()
                { if (Generation == Source->ShockGeneration && Source->GetHealth() > 0 &&
                    Source->GetPerkPercent(EPGCombatPerk::Shockwave) > 0 && Source->GetPerkPercent(EPGCombatPerk::ShockEcho) > 0)
                    Source->Pulse(Center, Echo, Radius, false, EPGDamageCause::ShockEcho); }), .3f, false);
        }
    }
}
