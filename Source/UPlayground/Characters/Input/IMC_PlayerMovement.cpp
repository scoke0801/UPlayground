// Copyright (c) 2025. All Rights Reserved.

#include "IMC_PlayerMovement.h"
#include "InputAction.h"
#include "IA_Move.h"
#include "IA_Look.h"
#include "IA_Jump.h"
#include "IA_Sprint.h"

UIMC_PlayerMovement::UIMC_PlayerMovement()
{
    // 기본 입력 액션들을 생성 (에디터에서 재설정 가능)
    MoveAction = CreateDefaultSubobject<UIA_Move>(TEXT("MoveAction"));
    LookAction = CreateDefaultSubobject<UIA_Look>(TEXT("LookAction"));
    JumpAction = CreateDefaultSubobject<UIA_Jump>(TEXT("JumpAction"));
    SprintAction = CreateDefaultSubobject<UIA_Sprint>(TEXT("SprintAction"));
}
