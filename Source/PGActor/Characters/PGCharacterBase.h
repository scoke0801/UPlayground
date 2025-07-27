// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "PGCharacterBase.generated.h"

class UPGDataAsset_StartUpDataBase;
class UPGPawnCombatComponent;
class UPGAbilitySystemComponent;

UCLASS()
class PGACTOR_API APGCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
protected:
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
	
public:	
	// Sets default values for this actor's properties
	APGCharacterBase();
	
protected:
	/**
	 * 컨트롤러에 의해 소유될 때 호출되는 오버라이드 함수
	 * @param NewController 새로운 컨트롤러
	 */
	virtual void PossessedBy(AController* NewController) override;
	
public:
	virtual UPGPawnCombatComponent* GetCombatComponent() const {return nullptr;}
	
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
};
