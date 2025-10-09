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
class UBoxComponent;
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

protected:
	// 공격 충돌 영역
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "PG|Combat")
	FName LeftHandCollisionBoxAttachBoneName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category = "PG|Combat")
	UBoxComponent* LeftHandCollisionBox;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "PG|Combat")
	FName RightHandCollisionBoxAttachBoneName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category = "PG|Combat")
	UBoxComponent* RightHandCollisionBox;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "PG|Combat")
	FName RightFootCollisionBoxAttachBoneName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category = "PG|Combat")
	UBoxComponent* RightFootCollisionBox;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "PG|Combat")
	FName LeftFootCollisionBoxAttachBoneName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category = "PG|Combat")
	UBoxComponent* LeftFootCollisionBox;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category = "PG|Combat")
	UBoxComponent* TailCollisionBox;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "PG|Combat")
	FName TailCollisionBoxAttachBoneName;

	
private:
	UPROPERTY(Transient)
	UPGUIEnemyNamePlate* EnemyNamePlate;

	/** Dissolve 효과 지속 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Death", meta = (AllowPrivateAccess = true))
	float TotalDissolveTime = 2.0f;

	/** Dissolve Timeline용 커브 (0~1로 변화) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Death", meta = (AllowPrivateAccess = true))
	UCurveFloat* DissolveCurve;
	
	/** Dissolve 효과용 타임라인 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG|Death", meta = (AllowPrivateAccess = true))
	UTimelineComponent* DissolveTimeline;

public:
	APGCharacterEnemy();
	
protected:
	virtual void BeginPlay() override;

	virtual void PossessedBy(AController* NewController) override;

	virtual void OnHit(UPGStatComponent* StatComponent) override;
	virtual void OnDied() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
public:
	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	UPGEnemyCombatComponent* GetEnemyCombatComponent() const {return CombatComponent;}

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	virtual UPGPawnCombatComponent* GetCombatComponent() const override;

	virtual UPGStatComponent* GetStatComponent() const override;
	UPGEnemyStatComponent* GetEnemyStatComponent() const;

	
	FORCEINLINE UBoxComponent* GetLeftHandCollisionBox() const { return LeftHandCollisionBox; }
	FORCEINLINE UBoxComponent* GetRightHandCollisionBox() const { return RightHandCollisionBox; }
	
	FORCEINLINE UBoxComponent* GetLeftFootCollisionBox() const { return LeftFootCollisionBox; }
	FORCEINLINE UBoxComponent* GetRightFootCollisionBox() const { return RightFootCollisionBox; }
	
	FORCEINLINE UBoxComponent* GetTailCollisionBox() const { return TailCollisionBox; }
	
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
	
	UFUNCTION()
	void OnBodyCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
