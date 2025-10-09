// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Handler/PGHandler.h"
#include "PGShared/Shared/Define/PGSkillDefine.h"
#include "PGSkillHandler.generated.h"

enum class EPGSkillType : uint8;
enum class EPGSkillSlot : uint8;

USTRUCT(BlueprintType)
struct FPGSkillData
{
	GENERATED_BODY()

public:
	PGSkillId SkillId = 0;				// 스킬 ID
	
	float LastSkillUsedTime = 0.0f;	// 마지막 스킬 사용 시간, 쿨타임 계산 용
	float CoolTime = 0.0f;

	EPGSkillType SkillType;

	int32 Priority = 1;		// 우선순위, 한 번 사용한 이후 다른 스킬이 먼저 사용되도록 하던가 하는 용도로 사용
	
public:
	FPGSkillData() = default;
	FPGSkillData(const PGSkillId InSkillId);
	void Init(const PGSkillId InSkillId);

public:
	// 쿨다운 체크
	bool IsOnCooldown() const;

	// 남은 쿨타임 반환
	float GetRemainingCooldown() const;
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

	// 스킬 ID로 쿨타임 체크
	bool IsSkillReadyByID(int32 SkillID) const;

	// 스킬 ID로 남은 쿨타임 조회
	float GetRemainingCooldownByID(int32 SkillID) const;

public:
	virtual void UseSkill(const EPGSkillSlot InSlotId);
	virtual PGSkillId GetSkillID(const EPGSkillSlot InSlotId);

	FPGSkillData* GetSkillData(const EPGSkillSlot InSlotId);

	// 스킬 ID로 슬롯 찾기
	EPGSkillSlot FindSlotBySkillID(int32 SkillID) const;
	
public:
	virtual EPGSkillSlot GetRandomSkillSlot() const;

	// 모든 스킬 데이터 맵 접근 (읽기 전용)
	const TMap<EPGSkillSlot, FPGSkillData>& GetAllSkillData() const { return SkillDataMap; }
};
