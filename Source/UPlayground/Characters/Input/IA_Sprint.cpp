// Copyright (c) 2025. All Rights Reserved.

#include "IA_Sprint.h"
#include "EnhancedInput/Public/InputTriggers.h"

UIA_Sprint::UIA_Sprint()
{
    // 값 타입: 디지털 (버튼 입력)
    ValueType = EInputActionValueType::Boolean;
    
    // 트리거 설정: 눌렀을 때와 뗐을 때
    Triggers.Add(NewObject<UInputTriggerDown>());
    Triggers.Add(NewObject<UInputTriggerReleased>());
}
