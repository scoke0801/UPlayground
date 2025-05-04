// Copyright (c) 2025. All Rights Reserved.

#include "IA_Look.h"
#include "EnhancedInput/Public/InputModifiers.h"
#include "EnhancedInput/Public/InputTriggers.h"

UIA_Look::UIA_Look()
{
    // 값 타입: 2D 벡터 (마우스/스틱 입력)
    ValueType = EInputActionValueType::Axis2D;
    
    // 트리거 설정: 입력이 있을 때마다 동작
    Triggers.Add(NewObject<UInputTriggerDown>());
    Triggers.Add(NewObject<UInputTriggerPressed>());
    
    // 수정자 설정: 감도 조절
    UInputModifierScalar* ScalarX = NewObject<UInputModifierScalar>();
    ScalarX->Scalar = FVector(0.5f, 0.0f, 0.0f);  // X축 감도
    Modifiers.Add(ScalarX);
    
    UInputModifierScalar* ScalarY = NewObject<UInputModifierScalar>();
    ScalarY->Scalar = FVector(0.0f, 0.5f, 0.0f);  // Y축 감도
    Modifiers.Add(ScalarY);
}
