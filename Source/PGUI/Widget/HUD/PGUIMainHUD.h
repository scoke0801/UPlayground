#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "PGShared/Shared/Message/Combat/PGConsumablePresentation.h"
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
    void OnBossPresentation(const class IPGEventData* Event);
    FDelegateHandle BossPresentationHandle;
    double BossDefeatUntil = 0;
    bool bBossDefeated = false;
    FLinearColor BossStatusColor = FLinearColor::White;
    void Refresh();
    FReply ActivateSlot(int32 Index);
    TSharedRef<SWidget> MakeResource(bool bHealth);
    TSharedRef<SWidget> MakeHealingPotion();
    void RefreshHealingPotion();
    void OnConsumableChanged(const class IPGEventData* Event);
    FDelegateHandle ConsumableHandle;
    FPGConsumableState Potion;
    FText PotionKey;
    FText PotionNotice;
    bool bPotionHintShown = false;
    double PotionHintUntil = 0.;
    TSharedRef<SWidget> MakeSkill(int32 Index);
    TSharedRef<SWidget> MakeStagePanel();
    TSharedRef<SWidget> MakeActions();
    UPROPERTY(Transient) TObjectPtr<UTexture2D> OrbFrameTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> CombatPlateTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> PotionIconTexture;
    FSlateBrush OrbFrameBrush;
    FSlateBrush CombatPlateBrush;
    FSlateBrush PotionIconBrush;
    FButtonStyle ActionStyle;
    FText StagePhase;
    FTimerHandle RefreshTimer;
    TWeakObjectPtr<class APGStageManager> Stage;
    TWeakObjectPtr<AActor> Boss;
    float BossHealth = 0.f;
    bool bShowBoss = false;
    FText BossTitle;
    FText BossStatus;
    FProgressBarStyle ResourceStyle;
    FSlateBrush SkillBrushes[8];
    UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> SkillTextures;
    int32 SkillIds[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
    float Cooldowns[8] = {};
    FText SkillNames[8];
    FText SkillKeys[8];
    FText SkillReasons[8];
    FText Objective;
    FText StageTitle;
    FText HealthText;
    FText RageText;
    float HealthRatio = 0.f;
    float RageRatio = 0.f;
    bool bCanAct = false;
    bool bRogueHUD = false;
};
