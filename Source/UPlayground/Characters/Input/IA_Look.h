// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "IA_Look.generated.h"

/**
 * 카메라/시점 조작 입력 액션
 */
UCLASS()
class UPLAYGROUND_API UIA_Look : public UInputAction
{
    GENERATED_BODY()
    
public:
    UIA_Look();
};
