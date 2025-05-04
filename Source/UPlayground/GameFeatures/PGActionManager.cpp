#include "PGActionManager.h"
#include "../Input/PGInputConfig.h"
#include "../Input/PGEnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"

UPGActionManager::UPGActionManager()
{
    ManagerName = FName("ActionManager");
    DefaultInputPriority = 0;
}

bool UPGActionManager::OnInitialize_Implementation()
{
    if (!Super::OnInitialize_Implementation())
    {
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("Action Manager Initialized"));
    
    // 입력 에셋 로드
    // 실제 구현에서는 애셋을 직접 로드하거나 참조를 설정해야 함
    // 예를 들어 데이터 에셋을 TSoftObjectPtr로 선언하고 여기서 로드
    
    return true;
}

void UPGActionManager::OnShutdown_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("Action Manager Shutdown"));
    Super::OnShutdown_Implementation();
}

void UPGActionManager::OnUpdate_Implementation(float DeltaTime)
{
    Super::OnUpdate_Implementation(DeltaTime);
    // 매니저 틱 로직 (필요한 경우)
}

void UPGActionManager::OnGameStateChange_Implementation(EPGGameStateType NewState)
{
    Super::OnGameStateChange_Implementation(NewState);
    
    // 게임 상태에 따른 입력 처리
    switch (NewState)
    {
        case EPGGameStateType::GamePaused:
            // 게임 일시정지 시 입력 처리
            break;
            
        case EPGGameStateType::GameRunning:
            // 게임 실행 중 입력 처리
            break;
            
        default:
            break;
    }
}

UPGInputConfig* UPGActionManager::GetDefaultInputConfig() const
{
    return DefaultInputConfig;
}

UInputMappingContext* UPGActionManager::GetDefaultMappingContext() const
{
    return DefaultMappingContext;
}

void UPGActionManager::SetupPlayerInput(APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return;
    }
    
    // 향상된 입력 서브시스템 가져오기
    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
    if (!Subsystem)
    {
        return;
    }
    
    // 기존 매핑 컨텍스트 제거 (있는 경우)
    if (DefaultMappingContext)
    {
        Subsystem->RemoveMappingContext(DefaultMappingContext);
    }
    
    // 기본 매핑 컨텍스트 추가
    if (DefaultMappingContext)
    {
        Subsystem->AddMappingContext(DefaultMappingContext, DefaultInputPriority);
    }
    
    // 입력 컴포넌트 설정
    UPGEnhancedInputComponent* EnhancedInputComponent = Cast<UPGEnhancedInputComponent>(PlayerController->InputComponent);
    if (EnhancedInputComponent && DefaultInputConfig)
    {
        // 향상된 입력 컴포넌트가 있고 기본 입력 설정이 있으면 여기서 입력 바인딩 설정 가능
        // 이 부분은 실제 구현에서 필요에 따라 추가
    }
}
