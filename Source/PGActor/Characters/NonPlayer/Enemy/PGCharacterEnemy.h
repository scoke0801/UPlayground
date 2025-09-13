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

private:
	UPROPERTY(Transient)
	UPGUIEnemyNamePlate* EnemyNamePlate;
	
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
};
