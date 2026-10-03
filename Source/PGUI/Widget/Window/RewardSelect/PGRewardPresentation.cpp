#include "PGRewardPresentation.h"

FText PGRewardPresentation::BuildContext(const FPGRewardStatDataRow& Reward, TFunctionRef<int32(EPGCombatPerk)> GetPerk)
{
    // BuildFamily is authored on the reward; prerequisites include equipped item effects.
    FString Family;
    EPGCombatPerk Root = EPGCombatPerk::None;
    if (Reward.BuildFamily == TEXT("Bleed")) { Family = TEXT("출혈"); Root = EPGCombatPerk::Bleed; }
    else if (Reward.BuildFamily == TEXT("Shock")) { Family = TEXT("충격파"); Root = EPGCombatPerk::Shockwave; }
    else if (Reward.BuildFamily == TEXT("Frenzy")) { Family = TEXT("격분"); Root = EPGCombatPerk::Frenzy; }
    else Family = Reward.BuildFamily.IsNone() ? TEXT("공용") : Reward.BuildFamily.ToString();
    if (!Reward.MeetsRequirements(GetPerk)) return FText::FromString(Family + TEXT(" · 선행 강화 필요"));
    if (Reward.bKeystone) return FText::FromString(Family + TEXT(" · 핵심 강화"));
    if (Root == EPGCombatPerk::None) return FText::FromString(Family + TEXT(" · 모든 빌드에 적용"));
    return FText::FromString(Family + (GetPerk(Root) > 0 ? TEXT(" · 현재 빌드 연계") : TEXT(" · 새 빌드 시작")));
}

bool PGRewardPresentation::IsValidChoice(int32 Index, int32 Count)
{
    return Count == 0 ? Index == INDEX_NONE : Index >= 0 && Index < Count;
}
