#include "PGRewardText.h"
#include "PGRewardStatDataRow.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"

FString PGRewardText::PerkName(EPGCombatPerk Perk)
{
    switch (Perk)
    {
    case EPGCombatPerk::Bleed: return TEXT("출혈");
    case EPGCombatPerk::BleedPotency: return TEXT("출혈 증폭");
    case EPGCombatPerk::BleedSpread: return TEXT("처치 확산");
    case EPGCombatPerk::BleedBurst: return TEXT("출혈 폭발");
    case EPGCombatPerk::Shockwave: return TEXT("충격파");
    case EPGCombatPerk::ShockRadius: return TEXT("파동 반경");
    case EPGCombatPerk::ShockEcho: return TEXT("메아리");
    case EPGCombatPerk::ShockExecute: return TEXT("파동 마무리");
    case EPGCombatPerk::Frenzy: return TEXT("격분");
    case EPGCombatPerk::FrenzyDuration: return TEXT("격분 지속");
    case EPGCombatPerk::FrenzyLeech: return TEXT("격분 회복");
    case EPGCombatPerk::FrenzyGuard: return TEXT("격분 방어");
    case EPGCombatPerk::LifeSteal: return TEXT("흡혈");
    case EPGCombatPerk::Cooldown: return TEXT("쿨다운 감소");
    case EPGCombatPerk::Execution: return TEXT("마무리");
    case EPGCombatPerk::Counter: return TEXT("빈틈 공략");
    case EPGCombatPerk::BleedRecast: return TEXT("핏빛 순환");
    case EPGCombatPerk::ShockFracture: return TEXT("공명 균열");
    case EPGCombatPerk::FrenzyAfterimage: return TEXT("격분의 잔상");
    default: return TEXT("없음");
    }
}

FString PGRewardText::Effect(EPGCombatPerk Perk, int32 Value, const UPGCombatTuningData& T)
{
    switch (Perk)
    {
    case EPGCombatPerk::Bleed: return FString::Printf(TEXT("공격한 적에게 출혈을 남깁니다. 적중 피해의 %d%%를 %.1f초마다 추가로 가하며, %.1f초 동안 최대 %d중첩됩니다."), Value,T.BleedInterval,T.BleedInterval*T.BleedTicks,T.BleedMaxStacks);
    case EPGCombatPerk::BleedPotency: return FString::Printf(TEXT("출혈로 주는 지속 피해가 %d%% 증가합니다."),Value);
    case EPGCombatPerk::BleedSpread: return FString::Printf(TEXT("출혈 중인 적을 처치하면 %.1fm 안의 적에게 출혈이 번집니다. 퍼진 출혈은 기존 지속 피해의 %d%%를 줍니다."),T.ProcRadius/100,Value);
    case EPGCombatPerk::BleedBurst: return FString::Printf(TEXT("검술로 적을 맞히면 남은 출혈 피해의 %d%%가 한 번에 터지고, 새 출혈을 남깁니다."),Value);
    case EPGCombatPerk::Shockwave: return FString::Printf(TEXT("검술이 적중하면 %.1fm 안에 충격파가 퍼져 적중 피해의 %d%%를 추가로 줍니다. %.1f초마다 발동합니다."),T.ProcRadius/100,Value,T.ShockCooldown);
    case EPGCombatPerk::ShockRadius: return FString::Printf(TEXT("충격파와 메아리 반경 +%d%%."),Value);
    case EPGCombatPerk::ShockEcho: return FString::Printf(TEXT("0.3초 뒤 최초 파동 피해의 %d%%로 메아리."),Value);
    case EPGCombatPerk::ShockExecute: return FString::Printf(TEXT("생명력이 35%% 이하인 적에게 충격파와 메아리가 %d%% 더 큰 피해를 줍니다."),Value);
    case EPGCombatPerk::Frenzy: return FString::Printf(TEXT("공격을 맞힐수록 공격 속도가 %d%%씩 빨라집니다. %.1f초 동안 최대 %d중첩되며, 최대 75%%까지 증가합니다."),Value,T.FrenzySeconds,T.FrenzyMaxStacks);
    case EPGCombatPerk::FrenzyDuration: return FString::Printf(TEXT("격분 유지 시간 +%d%% (%.1f초)."),Value,T.FrenzySeconds*(1+Value*.01f));
    case EPGCombatPerk::FrenzyLeech: return FString::Printf(TEXT("격분을 쌓는 직접 피해의 %d%%만큼 회복."),Value);
    case EPGCombatPerk::FrenzyGuard: return FString::Printf(TEXT("격분 유지 중 받는 직접 피해 %d%% 감소."),FMath::Min(60,Value));
    case EPGCombatPerk::LifeSteal: return FString::Printf(TEXT("실제 가한 직접 피해의 %d%% 회복. 과잉 피해 제외."),Value);
    case EPGCombatPerk::Cooldown: return FString::Printf(TEXT("검술과 대시의 재사용 대기시간이 %d%% 감소합니다."),FMath::Min(50,Value));
    case EPGCombatPerk::BleedRecast: return FString::Printf(TEXT("출혈 폭발로 적을 처치하면 사용한 검술의 남은 대기시간이 %.0f%% 줄어듭니다. 검술 사용마다 한 번 발동합니다."),T.BleedRefundFraction*100);
    case EPGCombatPerk::ShockFracture: return FString::Printf(TEXT("같은 적에게 %.1f초 내 파동·메아리 %d회 적중 시 %.1f초간 방어력 %.0f%% 감소."),T.ShockStackSeconds,T.ShockFractureHits,T.ShockWeaknessSeconds,T.ShockDefenseReduction*100);
    case EPGCombatPerk::FrenzyAfterimage: return FString::Printf(TEXT("격분이 가득 찼을 때 대시하면 격분을 모두 소모하고 잔상을 남깁니다. 잔상이 출발점 %.1fm 안의 적에게 공격력의 %.0f%% 피해를 줍니다."),T.AfterimageRadius/100,T.AfterimageAttackMultiplier*100);
    case EPGCombatPerk::Execution: return FString::Printf(TEXT("생명력이 %.0f%% 이하인 적을 직접 공격하면 피해가 %d%% 증가합니다."),T.ExecutionHealthThreshold*100,Value);
    case EPGCombatPerk::Counter: return FString::Printf(TEXT("회복 중인 적에게 직접 피해 +%d%%."),Value);
    default: return FString();
    }
}

FString PGRewardText::Requirements(const FPGRewardStatDataRow& Reward)
{
    TArray<FString> All;
    if (Reward.RequiredPerk != EPGCombatPerk::None) All.Add(PerkName(Reward.RequiredPerk));
    for (auto Perk : Reward.RequiredPerks) All.AddUnique(PerkName(Perk));
    if (!Reward.RequiredAnyPerks.IsEmpty())
    {
        TArray<FString> Any;
        for (auto Perk : Reward.RequiredAnyPerks) Any.Add(PerkName(Perk));
        All.Add(TEXT("(") + FString::Join(Any,TEXT(" / ")) + TEXT(" 중 하나)"));
    }
    return All.IsEmpty() ? TEXT("선행 조건 없음") : TEXT("필요: ") + FString::Join(All,TEXT(" + ")) + TEXT(" · 장착 효과 포함");
}
