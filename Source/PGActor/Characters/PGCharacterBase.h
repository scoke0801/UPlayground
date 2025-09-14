// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "PGCharacterBase.generated.h"

class UNiagaraSystem;
class FPGSkillHandler;
class UPGDataAsset_StartUpDataBase;
class UPGPawnCombatComponent;
class UPGAbilitySystemComponent;
class UMotionWarpingComponent;
class UPGStatComponent;

UCLASS()
class PGACTOR_API APGCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category= "PG")
	int32 CharacterTID = 0;
	
	/**
	 * 어빌리티 시스템 컴포넌트
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category= "PG|AbilitySystem")
	UPGAbilitySystemComponent* AbilitySystemComponent;

	/**
	 * 캐릭터 시작 데이터 에셋
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PG|CharacterData")
	TSoftObjectPtr<UPGDataAsset_StartUpDataBase> CharacterStartUpData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category= "PG|MotionWarping")
	UMotionWarpingComponent* MotionWarpingComponent;
	
protected:
	TObjectPtr<FPGSkillHandler> SkillHandler;
	
public:	
	// Sets default values for this actor's properties
	APGCharacterBase();

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/**
	 * 컨트롤러에 의해 소유될 때 호출되는 오버라이드 함수
	 * @param NewController 새로운 컨트롤러
	 */
	virtual void PossessedBy(AController* NewController) override;

public:
	virtual UPGPawnCombatComponent* GetCombatComponent() const {return nullptr;}
	virtual FPGSkillHandler* GetSkillHandler() const {return SkillHandler;}
	virtual UPGStatComponent* GetStatComponent() const { return nullptr; }

public:
	virtual void OnHit(UPGStatComponent* StatComponent) {}

	virtual void OnDied();
	
public:
	int32 GetCharacterTID() const {return CharacterTID;}
	
public:
	/**
	 * 어빌리티 시스템 컴포넌트를 반환하는 인터페이스 구현 함수
	 * @return 어빌리티 시스템 컴포넌트
	 * IAbilitySystemInterface
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	/**
	 * PG 커스텀 어빌리티 시스템 컴포넌트를 반환하는 인라인 함수
	 * @return PG 어빌리티 시스템 컴포넌트
	 */
	UPGAbilitySystemComponent* GetPGAbilitySystemComponent() const;

protected:
	void PlayVFX(UNiagaraSystem* ToPlayTemplate);

	void PlayDeathDissolveVFX(UNiagaraSystem* ToPlayTemplate);
};
