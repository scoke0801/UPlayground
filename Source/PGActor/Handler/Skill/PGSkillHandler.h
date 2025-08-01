// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Handler/PGHandler.h"
#include "PGShared/Shared/Define/PGSkillDefine.h"
#include "PGSkillHandler.generated.h"

enum class EPGSkillSlot : uint8;

USTRUCT(BlueprintType)
struct FPGSkillData
{
	GENERATED_BODY()

public:
	PGSkillId SkillId = 0;				// 스킬 ID
	
	float LastSkillUsedTime = 0.0f;	// 마지막 스킬 사용 시간, 쿨타임 계산 용
	float CoolTime = 0.0f;
	
public:
	FPGSkillData() = default;
	FPGSkillData(const PGSkillId InSkillId);
	void Init(const PGSkillId InSkillId);
};
/**
 * 
 */

class PGACTOR_API FPGSkillHandler : public FPGHandler
{
protected:
	TMap<EPGSkillSlot, FPGSkillData> SkillDataMap;
	
public:
	virtual ~FPGSkillHandler();

public:
	// FPGHandler override
	virtual void Initialize() override;
	virtual void Finalize() override;
	
public:
	void AddSkill(const EPGSkillSlot InSlotId, const PGSkillId InSkillId);
	void RemoveSkill(const EPGSkillSlot InSlotID);
	
	bool IsCanUseSkill(const EPGSkillSlot InSlotId);

public:
	virtual void UseSkill(const EPGSkillSlot InSlotId) {}
	virtual PGSkillId GetSkillID(const EPGSkillSlot InSlotId);
};
