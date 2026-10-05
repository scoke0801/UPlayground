// Fill out your copyright notice in the Description page of Project Settings.


#include "PGPlayerSkillHandler.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "PGActor/Characters/PGCharacterBase.h"

#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"

double FPGPlayerSkillHandler::GetComboTime() const
{
    const UWorld* World = Context.IsValid() ? Context->GetWorld() : nullptr;
    return World ? World->GetTimeSeconds() : FPlatformTime::Seconds();
}

int32 FPGPlayerSkillHandler::GetComboIndex(EPGSkillSlot Slot, const FPGSkillDataRow& BaseSkill, double Now) const
{
    return Slot == LastUsedSlot && BaseSkill.SkillID == ComboBaseSkill && Now <= ComboExpiresAt &&
        BaseSkill.ChainSkillIdList.IsValidIndex(ComboCount - 1) ? ComboCount : 0;
}

void FPGPlayerSkillHandler::AdvanceCombo(EPGSkillSlot Slot, const FPGSkillDataRow& BaseSkill, double Now, float MontageSeconds)
{
    const int32 Current = GetComboIndex(Slot, BaseSkill, Now);
    ComboCount = Current < BaseSkill.ChainSkillIdList.Num() ? Current + 1 : 0;
    ComboBaseSkill = BaseSkill.SkillID;
    LastUsedSlot = Slot;
    ComboExpiresAt = Now + FMath::Max(0.f, MontageSeconds) + FMath::Clamp(BaseSkill.ComboResetSeconds, 0.f, 2.f);
}

void FPGPlayerSkillHandler::UseSkill(const EPGSkillSlot InSlotId)
{
    UPGDataTableManager* Manager = UPGDataTableManager::Get(Context.Get());
    if (!Manager) return;
    const PGSkillId SkillId = GetSkillID(InSlotId);
    if (InSlotId == EPGSkillSlot::SkillSlot_Roll && bProfileCombo)
    {
        // Preserve the original deadline. Dodge cannot extend the combo.
    }
    else if (const auto* BaseSkill = Manager->GetRowData<FPGSkillDataRow>(Super::GetSkillID(InSlotId)))
    {
        float MontageSeconds = 0.f;
        const auto* Character = Cast<APGCharacterBase>(Context.Get());
        const auto* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
        if (const auto* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr)
            MontageSeconds = FMath::Max(0.f, Montage->GetPlayLength() - Anim->Montage_GetPosition(Montage)) /
                FMath::Max(.01f, FMath::Abs(Anim->Montage_GetPlayRate(Montage) * Montage->RateScale));
        const auto* Player = Cast<APGCharacterPlayer>(Character);
        if (Player && Player->GetPlayerAttackComponent()->IsRunning())
            MontageSeconds = Player->GetPlayerAttackComponent()->GetExpectedSeconds();
        AdvanceCombo(InSlotId, *BaseSkill, GetComboTime(), MontageSeconds);
        bProfileCombo = InSlotId == EPGSkillSlot::NormalAttack && !BaseSkill->PlayerProfile.IsNull();
    }
    else ComboCount = 0;

	if (FPGSkillData* Data = SkillDataMap.Find(InSlotId))
	{
        Data->LastSkillUsedTime = FMath::Max(UE_DOUBLE_SMALL_NUMBER, Data->GetTime());
        Data->InheritedCooldownUntil = -1.e30;
	}
	
	FPGEventDataTwoParam<PGSkillId, EPGSkillSlot> ToSendData(SkillId, InSlotId);
    if (auto* Messages = UPGMessageManager::Get(Context.Get())) Messages->SendMessage(EPGPlayerMessageType::UseSkill, &ToSendData);
}

PGSkillId FPGPlayerSkillHandler::GetSkillID(const EPGSkillSlot InSlotId)
{
	if (false == SkillDataMap.Contains(InSlotId))
	{
		return INVALID_SKILL_ID;
	}
	
	PGSkillId SkillId = Super::GetSkillID(InSlotId);
	
    UPGDataTableManager* Manager = UPGDataTableManager::Get(Context.Get());
    if (!Manager) return SkillId;
	if (FPGSkillDataRow* SkillData = Manager->GetRowData<FPGSkillDataRow>(SkillId))
	{
        const int32 Index = GetComboIndex(InSlotId, *SkillData, GetComboTime());
		if (Index > 0)
		{
			return SkillData->ChainSkillIdList[Index - 1];
		}
	}

	return SkillId;
}
