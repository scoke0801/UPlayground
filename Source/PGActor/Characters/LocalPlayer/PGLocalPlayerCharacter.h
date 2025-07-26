// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "Characters/PGCharacterBase.h"
#include "PGLocalPlayerCharacter.generated.h"

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
class UPLAYGROUND_API APGLocalPlayerCharacter : public APGCharacterBase
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

public:
	APGLocalPlayerCharacter();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
	void StartSkillWindow();
	void EndSkillWindow();

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	EComboState GetCurrentComboState() const { return CurrentComboState; }

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	int32 GetCurrentComboCount() const { return CurrentComboCount; }

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	bool CanCombo() const;

private:
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Attack(const FInputActionValue& InputActionValue);
	void ExecuteAttack(int32 ComboIndex);
	void SetComboState(EComboState NewState);
	void ResetCombo();
};
