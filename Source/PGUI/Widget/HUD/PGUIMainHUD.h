#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "PGUIMainHUD.generated.h"

/** Native gameplay HUD; no legacy widget blueprint bindings required. */
UCLASS()
class PGUI_API UPGUIMainHUD : public UUserWidget
{
    GENERATED_BODY()
public:
    UPGUIMainHUD(const FObjectInitializer& Initializer);
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(EditDefaultsOnly, Category="PG|HUD", meta=(ClampMin="0.05", ClampMax="1"))
    float RefreshInterval = .1f;
private:
    void Refresh();
    FReply ActivateSlot(int32 Index);
    TSharedRef<SWidget> MakeResource(bool bHealth);
    TSharedRef<SWidget> MakeSkill(int32 Index);
    FTimerHandle RefreshTimer;
    TWeakObjectPtr<class APGStageManager> Stage;
    FProgressBarStyle ResourceStyle;
    FSlateBrush SkillBrushes[8];
    UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> SkillTextures;
    int32 SkillIds[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
    float Cooldowns[8] = {};
    FText SkillNames[8];
    FText Objective;
    FText StageTitle;
    FText HealthText;
    FText RageText;
    float HealthRatio = 0.f;
    float RageRatio = 0.f;
    bool bCanAct = false;
    bool bRogueHUD = false;
};
