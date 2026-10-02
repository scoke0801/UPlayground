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
    case EPGCombatPerk::Bleed: return FString::Printf(TEXT("직접 적중 피해의 %d%%씩 %.1f초마다 출혈. %.1f초, 최대 %d중첩."), Value,T.BleedInterval,T.BleedInterval*T.BleedTicks,T.BleedMaxStacks);
    case EPGCombatPerk::BleedPotency: return FString::Printf(TEXT("출혈 틱 피해 +%d%%."),Value);
    case EPGCombatPerk::BleedSpread: return FString::Printf(TEXT("출혈 대상 처치 시 틱 피해의 %d%%를 반경 %.0f에 출혈로 확산."),Value,T.ProcRadius);
    case EPGCombatPerk::BleedBurst: return FString::Printf(TEXT("액티브 직접 적중 시 남은 출혈의 %d%%를 폭발시키고 새 출혈 부여."),Value);
    case EPGCombatPerk::Shockwave: return FString::Printf(TEXT("액티브 직접 적중 피해의 %d%%로 반경 %.0f 파동. 간격 %.1f초."),Value,T.ProcRadius,T.ShockCooldown);
    case EPGCombatPerk::ShockRadius: return FString::Printf(TEXT("충격파와 메아리 반경 +%d%%."),Value);
    case EPGCombatPerk::ShockEcho: return FString::Printf(TEXT("0.3초 뒤 최초 파동 피해의 %d%%로 메아리."),Value);
    case EPGCombatPerk::ShockExecute: return FString::Printf(TEXT("HP 35%% 이하 적에게 충격파·메아리 피해 +%d%%."),Value);
    case EPGCombatPerk::Frenzy: return FString::Printf(TEXT("직접 적중마다 공격 속도 +%d%%. %d중첩·%.1f초, 속도 상한 +75%%."),Value,T.FrenzyMaxStacks,T.FrenzySeconds);
    case EPGCombatPerk::FrenzyDuration: return FString::Printf(TEXT("격분 유지 시간 +%d%% (%.1f초)."),Value,T.FrenzySeconds*(1+Value*.01f));
    case EPGCombatPerk::FrenzyLeech: return FString::Printf(TEXT("격분을 쌓는 직접 피해의 %d%%만큼 회복."),Value);
    case EPGCombatPerk::FrenzyGuard: return FString::Printf(TEXT("격분 유지 중 받는 직접 피해 %d%% 감소."),FMath::Min(60,Value));
    case EPGCombatPerk::LifeSteal: return FString::Printf(TEXT("실제 가한 직접 피해의 %d%% 회복. 과잉 피해 제외."),Value);
    case EPGCombatPerk::Cooldown: return FString::Printf(TEXT("장착 스킬·회피 쿨다운 %d%% 감소."),FMath::Min(50,Value));
    case EPGCombatPerk::BleedRecast: return FString::Printf(TEXT("출혈 폭발로 처치하면 사용한 액티브의 남은 쿨다운 %.0f%% 반환. 스킬 사용당 1회."),T.BleedRefundFraction*100);
    case EPGCombatPerk::ShockFracture: return FString::Printf(TEXT("같은 적에게 %.1f초 내 파동·메아리 %d회 적중 시 %.1f초간 방어력 %.0f%% 감소."),T.ShockStackSeconds,T.ShockFractureHits,T.ShockWeaknessSeconds,T.ShockDefenseReduction*100);
    case EPGCombatPerk::FrenzyAfterimage: return FString::Printf(TEXT("최대 격분에서 회피하면 전 중첩 소비. 출발점 반경 %.0f에 공격력 %.0f%% 잔상 타격."),T.AfterimageRadius,T.AfterimageAttackMultiplier*100);
    case EPGCombatPerk::Execution: return FString::Printf(TEXT("HP %.0f%% 이하 대상에게 직접 피해 +%d%%."),T.ExecutionHealthThreshold*100,Value);
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
