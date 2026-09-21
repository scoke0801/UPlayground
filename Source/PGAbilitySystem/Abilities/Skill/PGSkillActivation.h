#pragma once
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
namespace PGSkillActivation
{
inline bool IsReady(const FGameplayAbilityActorInfo* Info, EPGSkillSlot Slot)
{
    APGCharacterBase* Character = Info ? Cast<APGCharacterBase>(Info->AvatarActor.Get()) : nullptr;
    if (!Character || !Character->GetSkillHandler() || !PGData()) return false;
    if (!Character->GetSkillHandler()->IsCanUseSkill(Slot)) return false;
    const auto* Row = PGData()->GetRowData<FPGSkillDataRow>(Character->GetSkillHandler()->GetSkillID(Slot));
    if (!Row || !Row->MontagePath.IsValid()) return false;
    if (const APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(Character))
        return Player->CanStartSkill(Slot == EPGSkillSlot::SkillSlot_Roll);
    return true;
}
}