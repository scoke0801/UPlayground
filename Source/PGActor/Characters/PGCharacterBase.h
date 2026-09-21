// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "PGCharacterBase.generated.h"

enum class EPGDamageType : uint8;
class UPGSkillMontageController;
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category= "PG|Component")
	UPGSkillMontageController* SkillMontageController;
	
protected:
    // Native handler owned by this actor and released in EndPlay; it is not a UObject.
    FPGSkillHandler* SkillHandler = nullptr;
	
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

	virtual ECollisionChannel GetCollisionChannel() const { return ECC_Pawn; }
public:
	virtual void OnHit(UPGStatComponent* StatComponent, const UPGPawnCombatComponent* const OtherCombatComponent) {}
	virtual void OnHeal(UPGStatComponent* StatComponent, int32 HealAmount) {}

	virtual void OnDied();
    virtual void OnHealthChanged();
protected:
    UPROPERTY(EditDefaultsOnly, Category="PG|Feedback")
    TObjectPtr<class UPGCombatFeedbackData> FeedbackData;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Feedback", meta=(ClampMin="0", ClampMax="1"))
    float FeedbackIntensity = 1.f;
    FTimerHandle HitStopTimer;
    float SavedAnimRate = 1.f;
    double LastFeedbackTime = -1.;
    double LastCameraShakeTime = -1.;
    int32 SuppressedFeedbackRequests = 0;
    float TotalHitStopSeconds = 0.f;
    void EndHitStop();
    void ApplyHitStop(float Seconds);
    void PlayCombatFeedback(AActor* Source, EPGDamageType Type);
    bool bDeathStarted = false;
    bool bDeathFinished = false;
    FTimerHandle DeathFallbackTimer;
public:
	
public:
	int32 GetCharacterTID() const {return CharacterTID;}
    bool bPerformingHeavyAttack = false;
    const class UPGCombatFeedbackData* GetCombatFeedbackData() const;
    int32 GetSuppressedFeedbackRequests() const { return SuppressedFeedbackRequests; }
    float GetTotalHitStopSeconds() const { return TotalHitStopSeconds; }
    friend class FPGFeedbackBudgetTest;
	
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
	
	void UpdateMovementSpeed();
};
