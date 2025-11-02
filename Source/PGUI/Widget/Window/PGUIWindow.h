#pragma once

#include "CoreMinimal.h"
#include "../Base/PGWidgetBase.h"
#include "Components/Button.h"
#include "PGUIWindow.generated.h"

/**
 * 전체 화면을 가리는 윈도우 UI
 * 한 번에 하나의 윈도우만 활성화 가능
 */
UCLASS(BlueprintType, Blueprintable)
class PGUI_API UPGUIWindow : public UPGWidgetBase
{
    GENERATED_BODY()

protected:
    // ESC 키로 닫을 수 있는지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Window")
    bool bCanCloseWithEscape = true;

    // 배경 클릭으로 닫을 수 있는지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Window")
    bool bCanCloseWithBackgroundClick = false;

    // 윈도우가 열릴 때 다른 UI들을 숨길지 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Window")
    bool bHideOtherUIOnOpen = true;

private:
    bool bIsClosing = false;
    
public:
    UPGUIWindow(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    
public:
    virtual void ShowUI() override;
    virtual void HideUI() override;

    // 윈도우 닫기
    UFUNCTION(BlueprintCallable, Category = "PG|Window")
    virtual void CloseWindow();

    // 윈도우 강제 닫기 (애니메이션 없이)
    UFUNCTION(BlueprintCallable, Category = "PG|Window")
    virtual void ForceCloseWindow();

protected:
    // 입력 처리
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
};
