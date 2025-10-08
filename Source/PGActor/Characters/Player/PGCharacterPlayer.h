// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGCharacterPlayer.generated.h"

class UPGUIPlayerHpBar;
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
	UPGUIPlayerHpBar* PlayerHpWidget;
	
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
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

public:
	virtual void OnHit(UPGStatComponent* StatComponent) override;
	
public:
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
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Zoom(const FInputActionValue& InputActionValue);
	
	void Input_AbilityInputPressed(FGameplayTag InInputTag);
	void input_AbilityInputReleased(FGameplayTag InInputTag);

private:
	void InitUIComponents();
	void UpdateHpComponent();
};
