// Copyright (c) 2025. All Rights Reserved.

#include "IA_Move.h"
#include "EnhancedInput/Public/InputModifiers.h"
#include "EnhancedInput/Public/InputTriggers.h"

UIA_Move::UIA_Move()
{
    // 값 타입: 2D 벡터 (WASD/스틱 입력)
    ValueType = EInputActionValueType::Axis2D;
    
    // 트리거 설정: 눌렀을 때와 누르고 있는 동안 모두 동작
    Triggers.Add(NewObject<UInputTriggerDown>());
    Triggers.Add(NewObject<UInputTriggerPressed>());
    
    // 수정자 설정: 데드존 적용
    UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>();
    DeadZone->LowerThreshold = 0.2f;
    DeadZone->UpperThreshold = 1.0f;
    DeadZone->Type = EDeadZoneType::Radial;
    Modifiers.Add(DeadZone);
}
