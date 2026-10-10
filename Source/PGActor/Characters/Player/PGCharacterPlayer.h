// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "PGData/DataAsset/Input/PGCameraSettings.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGCharacterPlayer.generated.h"

class UPGWidgetComponentBase;
struct FGameplayTag;
class UPGPlayerCombatComponent;
class UDataAsset_InputConfig;
class USpringArmComponent;
class UCameraComponent;
class UPGPlayerStatComponent;
class UPGStatComponent;

UENUM(BlueprintType)
enum class EComboState : uint8
{
	None,
	Attacking,
	ComboWindow,
	ComboEnd
};

/**
 * 
 */
UCLASS()
class PGACTOR_API APGCharacterPlayer : public APGCharacterBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PG|QuarterView")
    bool bUseQuarterView = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PG|QuarterView")
    TObjectPtr<class UPGQuarterViewData> QuarterViewData;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PG|Movement", meta=(ClampMin="0.1"))
    float MovementFacingInterpSpeed = 8.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PG|Movement")
    TSoftObjectPtr<class UPGPlayerLocomotionData> LocomotionDataAsset;
    FVector LastAimDirection = FVector::ForwardVector;
    FVector LastAimPoint = FVector::ZeroVector;
    bool bHasAimPoint = false;
    FTimerHandle AimTimer;
    void UpdateAim();
    void ConfigureQuarterView();
    EPGCameraMode CameraMode = EPGCameraMode::QuarterView;
    float QuarterViewDistance = 1200.f;
    float ActionCameraDistance = 450.f;
    FRotator ActionCameraRotation = FRotator::ZeroRotator;
    void UpdatePlayerCamera(float DeltaSeconds);
    void ApplyActionCameraMouseDelta(float MouseX, float MouseY);
    float TargetCameraDistance = 1200.f;
    float CameraPitchOffset = 0.f;
    FVector CombatCameraTargetOffset = FVector::ZeroVector;
public:
    void SetCameraMode(EPGCameraMode Mode);
    bool ShouldHideForCamera(const FVector& ViewLocation, bool bWasHidden) const;
    float GetCameraBodyFade(const FVector& ViewLocation, const class UCapsuleComponent* Body = nullptr) const;
    EPGCameraMode GetCameraMode() const { return CameraMode; }
    bool IsGameplayInputAllowed() const;
    bool IsConsumableInputAllowed() const;
    class UPGConsumableComponent* GetConsumableComponent() const { return ConsumableComponent; }
    const UDataAsset_InputConfig* GetInputConfig() const { return InputConfigDataAsset; }
    void FaceAimDirection();
    class UAnimSequence* GetLocomotionTurnAnimation() const { return TurnAnimation; }
    float GetLocomotionTurnTime() const { return TurnAnimationTime; }
    float GetLocomotionTurnPlayRate() const { return TurnEffectivePlayRate; }
    bool IsLocomotionTurning() const { return bLocomotionTurning; }
    void CancelLocomotionTurn();
    const class UPGPlayerLocomotionData* GetLocomotionData() const { return LocomotionData; }
    bool GetGroundAimPoint(FVector& Point) const { Point = LastAimPoint; return bHasAimPoint; }
    void FaceDodgeDirection();
    FVector GetDodgeDirection();
    void SetAttackAimTracking(bool bEnabled) { bTrackAttackAim = bEnabled; }
    bool CanStartSkill(bool bDodge) const;
    void SetSkillCancelPolicy(float AttackFraction, float DodgeFraction);
    class UPGPlayerAttackComponent* GetPlayerAttackComponent() const { return PlayerAttackComponent; }
    class UPGPlayerDashComponent* GetPlayerDashComponent() const { return PlayerDashComponent; }
    void ResetAttackHitStop() { EndHitStop(); }
    void ApplyProfileHitStop(float Seconds) { ApplyHitStop(Seconds); }
private:
    UPROPERTY(VisibleAnywhere, Category="PG|Consumables")
    TObjectPtr<class UPGConsumableComponent> ConsumableComponent;
    UPROPERTY(VisibleAnywhere, Category="PG|Combat")
    TObjectPtr<class UPGPlayerDashComponent> PlayerDashComponent;
    UPROPERTY(VisibleAnywhere, Category="PG|Combat")
    TObjectPtr<class UPGPlayerAttackComponent> PlayerAttackComponent;
    friend class FPGDodgeDirectionTest;
    friend class FPGCameraModesTest;
    friend class FPGCameraCollisionTest;
    FVector MoveInputDirection = FVector::ZeroVector;
    bool bTrackAttackAim = false;
    UPROPERTY(Transient) TObjectPtr<class UPGPlayerLocomotionData> LocomotionData;
    UPROPERTY(Transient) TObjectPtr<class UAnimSequence> TurnAnimation;
    bool bLocomotionTurning = false;
    bool bCanStartStationaryTurn = true;
    int32 TurnMotionIndex = INDEX_NONE;
    float TurnAnimationTime = 0.f;
    float TurnEffectivePlayRate = 1.f;
    float TurnStartYaw = 0.f;
    float TurnAngle = 0.f;
    float TurnCooldown = 0.f;
    float LastTurnSign = 1.f;
    float CombatFacingUntil = 0.f;
    float UpdateMovementFacing(const FVector& Direction, float DeltaSeconds);
    void RefreshCursorAim();
    bool bSkillWindowOpen = false;
    float AttackCancelFraction = 0.2f;
    float DodgeCancelFraction = 0.5f;
public:
protected:
    // 카메라 회전 민감도 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Camera Settings", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float MouseSensitivityX = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Camera Settings", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float MouseSensitivityY = 1.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Camera Settings")
	float CameraMinOffset = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Camera Settings")
	float CameraMaxOffset = 600.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Camera Settings")
	float CameraUpdateSpeed = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Camera Settings")
	float CameraMinPitch = -60.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Camera Settings")
	float CameraMaxPitch = 30.0f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Camera", meta = (AllowPrivateAccess = true))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Camera", meta = (AllowPrivateAccess = true))
	UCameraComponent* FollowCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category= "PG|Character", meta = (AllowPrivateAccess = "true"))
	UDataAsset_InputConfig* InputConfigDataAsset;

	/** 플레이어 컴뱃 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Combat", meta = (AllowPrivateAccess = true))
	UPGPlayerCombatComponent* CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG|Stat", meta = (AllowPrivateAccess = true))
	UPGPlayerStatComponent* PlayerStatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG|UI", meta = (AllowPrivateAccess = true))
	UPGWidgetComponentBase* PlayerHpWidgetComponent;

private:
	
	UPROPERTY(Transient)
	bool bIsJump = false;
	
	UPROPERTY(Transient)
	bool bIsCanControl = true;
	
	FTimerHandle MeshCheckTimerHandle;
	
	bool bIsAllMeshLoaded = false;
	
public:
	APGCharacterPlayer();
	
protected:
	virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

public:
	virtual void OnHit(UPGStatComponent* StatComponent, const UPGPawnCombatComponent* const OtherCombatComponent) override;
	
public:
	virtual void OnHealthChanged() override;
    void StartSkillWindow();
	void EndSkillWindow();

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	UPGPlayerCombatComponent* GetPlayerCombatComponent() const {return CombatComponent;}

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	virtual UPGPawnCombatComponent* GetCombatComponent() const override;

	virtual UPGStatComponent* GetStatComponent() const override;
	UPGPlayerStatComponent* GetPlayerStatComponent() const;
	
	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	bool GetIsJumping() const {return bIsJump;}

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	bool GetIsCacControl()const { return bIsCanControl;}
	
	void SetIsCanControl(bool IsCanControl) {bIsCanControl = IsCanControl;}
	void SetIsJump(bool IsJump);
	bool IsCanJump() const;

public:
	void CheckAllMeshesLoaded();

private:
	void Input_Move(const FInputActionValue& InputActionValue);
    void Input_HealingPotion(const FInputActionValue& InputActionValue);
    void Input_MoveReleased(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Zoom(const FInputActionValue& InputActionValue);
	
	void Input_AbilityInputPressed(FGameplayTag InInputTag);
    void Input_AbilityInputHeld(FGameplayTag InInputTag);
	void input_AbilityInputReleased(FGameplayTag InInputTag);

private:
	void InitUIComponents();
};
