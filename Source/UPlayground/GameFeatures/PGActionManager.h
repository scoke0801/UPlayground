#pragma once

#include "CoreMinimal.h"
#include "../Managers/PGBaseManager.h"
#include "PGActionManager.generated.h"

/**
 * 플레이어 액션과 입력 처리를 위한 매니저
 */
UCLASS(Blueprintable)
class UPLAYGROUND_API UPGActionManager : public UPGBaseManager
{
    GENERATED_BODY()
    
public:
    UPGActionManager();
    
    //~Begin UPGBaseManager interface
    virtual bool OnInitialize_Implementation() override;
    virtual void OnShutdown_Implementation() override;
    virtual void OnUpdate_Implementation(float DeltaTime) override;
    virtual void OnGameStateChange_Implementation(EPGGameStateType NewState) override;
    //~End UPGBaseManager interface
    
    // 입력 설정 액세스
    UFUNCTION(BlueprintCallable, Category = "Input")
    class UPGInputConfig* GetDefaultInputConfig() const;
    
    // 입력 매핑 컨텍스트 액세스
    UFUNCTION(BlueprintCallable, Category = "Input")
    class UInputMappingContext* GetDefaultMappingContext() const;
    
    // 입력 처리 설정
    UFUNCTION(BlueprintCallable, Category = "Input")
    void SetupPlayerInput(class APlayerController* PlayerController);
    
protected:
    // 기본 입력 설정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    class UPGInputConfig* DefaultInputConfig;
    
    // 기본 매핑 컨텍스트
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    class UInputMappingContext* DefaultMappingContext;
    
    // 기본 우선순위
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    int32 DefaultInputPriority;
};
