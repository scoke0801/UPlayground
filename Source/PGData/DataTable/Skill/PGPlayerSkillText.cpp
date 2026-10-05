#include "PGPlayerSkillText.h"
#include "PGSkillDataRow.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"
FString PGPlayerSkillText::Describe(const FPGSkillDataRow& Row)
{
    const auto* Profile=Row.PlayerProfile.LoadSynchronous();
    if (!Profile) return Row.Desc;
    TArray<FString> Hits; float Total=0;
    for (const auto& Hit : Profile->HitPhases) { Hits.Add(FString::Printf(TEXT("%.0f%%"),Hit.DamageMultiplier*100)); Total+=Hit.DamageMultiplier; }
    FString Result=Row.Desc+TEXT("\n공격력 ")+FString::Join(Hits,TEXT(" + "));
    if (Hits.Num()>1) Result+=FString::Printf(TEXT(" · %d회 합계 %.0f%%"),Hits.Num(),Total*100);
    Result+=FString::Printf(TEXT("\n기본 재사용 %.1f초 · 회피 취소 %.2f초 이후"),float(Row.SkillCoolTime),Profile->DodgeCancel);
    if (Profile->EarlyDodgeUntil>0) Result+=FString::Printf(TEXT(" / %.2f초 이전"),Profile->EarlyDodgeUntil);
    if (!Profile->HitPhases.IsEmpty())
    {
        const auto& Hit=Profile->HitPhases[0];
        if (Hit.Shape==EPGPlayerHitShape::Projectile) Result+=FString::Printf(TEXT("\n관통 검기 · 사거리 %.1fm · 폭 %.1fm · 대상당 1회"),Profile->ProjectileRange/100,Hit.Radius/50);
        else Result+=FString::Printf(TEXT("\n%s · 반경 %.1fm · %.0f도"),Hit.Shape==EPGPlayerHitShape::Disc ? TEXT("주변 원판") : TEXT("전방 부채꼴"),Hit.Radius/100,Hit.Shape==EPGPlayerHitShape::Disc ? 360.f : Hit.FullAngleDegrees);
    }
    float Distance=0; for (const auto& Move : Profile->MovementSegments) Distance+=Move.Distance;
    if (Distance>0) Result+=FString::Printf(TEXT("\n최대 %.1fm 이동 · 이동 중 무적 없음"),Distance/100);
    return Result;
}
