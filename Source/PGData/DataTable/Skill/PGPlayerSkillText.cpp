#include "PGPlayerSkillText.h"
#include "PGSkillDataRow.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"
#include "Animation/AnimMontage.h"
PGPlayerSkillText::FView PGPlayerSkillText::MakeView(const FPGSkillDataRow& Row)
{
    FView View;
    View.Name = Row.Desc;
    const int32 Separator = View.Name.Find(TEXT("·"));
    if (Separator != INDEX_NONE) View.Name = View.Name.Left(Separator).TrimEnd();
    View.Cooldown = Row.SkillCoolTime > 0 ? FString::Printf(TEXT("기본 재사용  %d초"),Row.SkillCoolTime) : TEXT("재사용 대기 없음");
    const auto* Profile = Row.PlayerProfile.LoadSynchronous();
    if (!Profile || Profile->HitPhases.IsEmpty()) return View;
    const auto Phases = Profile->ResolveHitPhases(Cast<UAnimMontage>(Row.MontagePath.TryLoad()));
    const auto& Hit = Phases[0];
    const bool Leap = Profile->MovementSegments.ContainsByPredicate([](const auto& Move){return Move.Mode==EPGPlayerMoveMode::GroundLeap;});
    View.Description = Hit.Shape==EPGPlayerHitShape::Projectile ? TEXT("전방으로 검기를 날려 경로에 있는 적들을 관통합니다.") :
        Leap ? TEXT("전방으로 도약하며 주변의 적을 연속으로 베어냅니다.") :
        Hit.Shape==EPGPlayerHitShape::Disc ? TEXT("크게 원을 그리며 주변의 적을 베어냅니다.") :
        Phases.Num()>1 ? TEXT("전방의 적에게 연속 참격을 가합니다.") :
        Hit.bHeavyImpact ? TEXT("힘을 실은 참격으로 전방의 적을 베어냅니다.") : TEXT("전방의 적을 베어냅니다.");
    float Total = 0.f;
    for (const auto& Phase : Phases) Total += Phase.DamageMultiplier;
    View.Damage = FString::Printf(TEXT("%s  공격력의 %.0f%%"),Phases.Num()>1 ? TEXT("총 피해") : TEXT("피해"),Total*100);
    if (Phases.Num()>1) View.Damage += FString::Printf(TEXT(" · %d회 공격"),Phases.Num());
    return View;
}

FString PGPlayerSkillText::Describe(const FPGSkillDataRow& Row)
{
    const auto View = MakeView(Row);
    TArray<FString> Lines = {View.Name};
    if (!View.Description.IsEmpty()) Lines.Add(View.Description);
    if (!View.Damage.IsEmpty()) Lines.Add(View.Damage);
    Lines.Add(View.Cooldown);
    return FString::Join(Lines,TEXT("\n"));
}
