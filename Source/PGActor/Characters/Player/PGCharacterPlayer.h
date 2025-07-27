// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGCharacterPlayer.generated.h"

struct FGameplayTag;
class UPGPlayerCombatComponent;
class UDataAsset_InputConfig;
class USpringArmComponent;
class UCameraComponent;

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

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Camera", meta = (AllowPrivateAccess = true))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Camera", meta = (AllowPrivateAccess = true))
	UCameraComponent* FollowCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category= "PG|Character", meta = (AllowPrivateAccess = "true"))
	UDataAsset_InputConfig* InputConfigDataAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG|Combat", meta = (AllowPrivateAccess = "true"))
	EComboState CurrentComboState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG|Combat", meta = (AllowPrivateAccess = "true"))
	int32 CurrentComboCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PG|Combat", meta = (AllowPrivateAccess = "true"))
	int32 MaxComboCount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG|Combat", meta = (AllowPrivateAccess = "true"))
	bool bHasQueuedInput;

	/** 플레이어 컴뱃 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Combat", meta = (AllowPrivateAccess = true))
	UPGPlayerCombatComponent* CombatComponent;
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG|Combat")
	bool bIsJump = false;
	
public:
	APGCharacterPlayer();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	
public:
	void StartSkillWindow();
	void EndSkillWindow();

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	EComboState GetCurrentComboState() const { return CurrentComboState; }

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	int32 GetCurrentComboCount() const { return CurrentComboCount; }

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	bool CanCombo() const;

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	UPGPlayerCombatComponent* GetPlayerCombatComponent() const {return CombatComponent;}

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	virtual UPGPawnCombatComponent* GetCombatComponent() const override;
	
	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	bool GetIsJumping() const {return bIsJump;}
private:
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Attack(const FInputActionValue& InputActionValue);
	void Input_Jump(const FInputActionValue& InputActionValue);
	
	void Input_AbilityInputPressed(FGameplayTag InInputTag);
	void input_AbilityInputReleased(FGameplayTag InInputTag);
	
	void ExecuteAttack(int32 ComboIndex);
	void SetComboState(EComboState NewState);
	void ResetCombo();
};
