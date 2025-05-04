// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputMappingContext.h"
#include "IMC_PlayerMovement.generated.h"

/**
 * 플레이어 이동을 위한 입력 매핑 컨텍스트
 */
UCLASS()
class UPLAYGROUND_API UIMC_PlayerMovement : public UInputMappingContext
{
    GENERATED_BODY()
    
public:
    UIMC_PlayerMovement();
    
    // 입력 액션 참조들 (에디터에서 설정 가능)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Actions")
    class UInputAction* MoveAction;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Actions")
    class UInputAction* LookAction;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Actions")
    class UInputAction* JumpAction;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Actions")
    class UInputAction* SprintAction;
};
