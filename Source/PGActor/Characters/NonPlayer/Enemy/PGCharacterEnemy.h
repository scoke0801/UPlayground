// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Interface/PGClickableInterface.h"
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
class PGACTOR_API APGCharacterEnemy : public APGCharacterBase, public IPGClickableInterface
{
	GENERATED_BODY()
    friend class FPGBossPhaseLifecycleTest;

protected:
	/** 컴뱃 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Combat", meta = (AllowPrivateAccess = true))
	UPGEnemyCombatComponent* CombatComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PG|Presentation")
    TObjectPtr<class UPGEnemyPresentationComponent> EnemyPresentation;

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
	TObjectPtr<class UDecalComponent> GuardDecal;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> PreparedPatternAssets;
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
    /** Locomotion asset for the native imported-creature animation graph. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PG|Motion")
    TObjectPtr<class UBlendSpace> CreatureLocomotion;
    // Assigned once by the stage before combat; independent of kill order and spawn retries.
    int32 LootSeed = 0;
    FGuid LootGuid;
    bool bLootResolved = false;
    bool bCanDropLoot = true;
    // Scoped activation request shared by BT and role AI. Zero means legacy tag selection.
    UPROPERTY(BlueprintReadOnly, Category="PG|Skill")
    int32 RequestedSkillID = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="PG|Boss")
    int32 BossPhase = 1;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="PG|Boss")
    double PhaseTransitionUntil = 0;
    UFUNCTION(BlueprintPure, Category="PG|Boss")
    bool IsBossTransitioning() const;
    void PublishBossPresentation(bool bHidePresentation = false) const;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="PG|Pattern")
    bool bPatternActive = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="PG|Pattern")
    bool bPatternRecovering = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="PG|Pattern")
    bool bPatternStriking = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="PG|Pattern")
    bool bGuarding = false;
    int32 ActivePatternID = 0;
    float GetDirectionalDamageScale(const AActor* Attacker) const;
    void ClearPatternHitboxes();
    void SetGuarding(bool bEnabled);
    UPGEnemyPresentationComponent* GetEnemyPresentation() const { return EnemyPresentation; }
	
protected:
	virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	virtual void PossessedBy(AController* NewController) override;

	virtual void OnHit(UPGStatComponent* StatComponent,const UPGPawnCombatComponent* const OtherCombatComponent) override;
	virtual void OnHeal(UPGStatComponent* StatComponent, int32 HealAmount) override;
	virtual void OnDied() override;
    virtual void OnHealthChanged() override;

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
	
	virtual ECollisionChannel GetCollisionChannel() const override { return ECC_GameTraceChannel1; }

public:
	FORCEINLINE UBoxComponent* GetLeftHandCollisionBox() const { return LeftHandCollisionBox; }
	FORCEINLINE UBoxComponent* GetRightHandCollisionBox() const { return RightHandCollisionBox; }
	
	FORCEINLINE UBoxComponent* GetLeftFootCollisionBox() const { return LeftFootCollisionBox; }
	FORCEINLINE UBoxComponent* GetRightFootCollisionBox() const { return RightFootCollisionBox; }
	
	FORCEINLINE UBoxComponent* GetTailCollisionBox() const { return TailCollisionBox; }

public:
	// IPGClickableInterface 구현
	virtual void OnClicked_Implementation(AActor* ClickedActor, const FVector& ClickLocation) override;
	virtual void OnClickCancelled_Implementation() override;
	virtual bool IsClickable_Implementation() const override;
	
private:
    FTimerHandle BossTransitionTimer;
    bool bBossPresentationClosed = false;
    bool TryBeginBossPhase(const struct FPGEnemyDataRow& Row);
    void FinishBossTransition();
	void InitEnemyStartUpData();
	void InitUIComponents();
	
	void UpdateHpBar();
	
	/** 스테이지 매니저에 처치 알림 */
	void NotifyStageManagerOnDeath();
	
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
