// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGCharacterEnemy.generated.h"
class UPGUIEnemyNamePlate;
class UPGEnemyStatComponent;
class UPGEnemyCombatComponent;
class UPGWidgetComponentBase;
class UUserWidget;
class UTimelineComponent;
class APGWeaponBase;
/**
 * 
 */
UCLASS()
class PGACTOR_API APGCharacterEnemy : public APGCharacterBase
{
	GENERATED_BODY()

protected:
	/** 컴뱃 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Combat", meta = (AllowPrivateAccess = true))
	UPGEnemyCombatComponent* CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Stat", meta = (AllowPrivateAccess = true))
	UPGEnemyStatComponent* EnemyStatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|UI", meta = (AllowPrivateAccess = true))
	UPGWidgetComponentBase* EnemyNameplateWidgetComponent;

	/** Dissolve 효과용 타임라인 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG|Death", meta = (AllowPrivateAccess = true))
	UTimelineComponent* DissolveTimeline;

private:
	UPROPERTY(Transient)
	UPGUIEnemyNamePlate* EnemyNamePlate;

	/** Dissolve 효과 지속 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Death", meta = (AllowPrivateAccess = true))
	float TotalDissolveTime = 2.0f;

	/** Dissolve Timeline용 커브 (0~1로 변화) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Death", meta = (AllowPrivateAccess = true))
	UCurveFloat* DissolveCurve;
	
public:
	APGCharacterEnemy();
	
protected:
	virtual void BeginPlay() override;

	virtual void PossessedBy(AController* NewController) override;

	virtual void OnHit(UPGStatComponent* StatComponent) override;
	virtual void OnDied() override;
	
public:
	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	UPGEnemyCombatComponent* GetEnemyCombatComponent() const {return CombatComponent;}

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	virtual UPGPawnCombatComponent* GetCombatComponent() const override;

	virtual UPGStatComponent* GetStatComponent() const override;
	UPGEnemyStatComponent* GetEnemyStatComponent() const;
	
private:
	void InitEnemyStartUpData();
	void InitUIComponents();
	
	void UpdateHpBar();
	
	/** Dissolve 효과 관련 함수들 */
	void StartDissolveEffect();
	
	UFUNCTION()
	void OnDissolveTimelineUpdate(float Value);
	
	UFUNCTION()
	void OnDissolveTimelineFinished();
	
};
