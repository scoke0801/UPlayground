#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PGUIStyleSettings.generated.h"

/** Shared native UI theme. Applied when the next game process starts. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="PG UI Style"))
class PGUI_API UPGUIStyleSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }
    UPROPERTY(Config, EditAnywhere, Category="Colors") FLinearColor Accent = FLinearColor(.32f,.91f,.72f);
    UPROPERTY(Config, EditAnywhere, Category="Colors") FLinearColor Secondary = FLinearColor(.63f,.57f,1.f);
    UPROPERTY(Config, EditAnywhere, Category="Colors") FLinearColor Text = FLinearColor(.93f,.96f,1.f);
    UPROPERTY(Config, EditAnywhere, Category="Colors") FLinearColor Muted = FLinearColor(.61f,.69f,.79f);
    UPROPERTY(Config, EditAnywhere, Category="Colors") FLinearColor Danger = FLinearColor(1.f,.42f,.43f);
    UPROPERTY(Config, EditAnywhere, Category="Colors") FLinearColor Panel = FLinearColor(.009f,.012f,.030f,.92f);
    UPROPERTY(Config, EditAnywhere, Category="Colors") FLinearColor Card = FLinearColor(.018f,.025f,.052f,.88f);
    UPROPERTY(Config, EditAnywhere, Category="Colors") FLinearColor Rare = FLinearColor(1.f,.68f,.26f);
    UPROPERTY(Config, EditAnywhere, Category="Colors") FLinearColor Magic = FLinearColor(.35f,.76f,1.f);
    UPROPERTY(Config, EditAnywhere, Category="Layout", meta=(ClampMin="14", ClampMax="24")) int32 BodyFontSize = 16;
    UPROPERTY(Config, EditAnywhere, Category="Layout", meta=(ClampMin="960", ClampMax="1800")) float InventoryMaxWidth = 1540.f;
    UPROPERTY(Config, EditAnywhere, Category="Layout", meta=(ClampMin="8", ClampMax="48")) float ScreenMargin = 24.f;
    UPROPERTY(Config, EditAnywhere, Category="Rewards", meta=(ClampMin="960", ClampMax="1800")) float RewardMaxWidth = 1440.f;
    UPROPERTY(Config, EditAnywhere, Category="Rewards", meta=(ClampMin="0.05", ClampMax="1")) float RevealSeconds = .24f;
    UPROPERTY(Config, EditAnywhere, Category="Rewards", meta=(ClampMin="0", ClampMax="0.3")) float CardStaggerSeconds = .07f;
    UPROPERTY(Config, EditAnywhere, Category="Rewards", meta=(ClampMin="0.05", ClampMax="0.5")) float ConfirmSeconds = .18f;
    UPROPERTY(Config, EditAnywhere, Category="Art") TSoftObjectPtr<class UTexture2D> SanctuaryBackground;
    UPROPERTY(Config, EditAnywhere, Category="Art") TSoftObjectPtr<class UTexture2D> TrialFrame;
    UPROPERTY(Config, EditAnywhere, Category="Art") TSoftObjectPtr<class UTexture2D> HUDPlaque;
    UPROPERTY(Config, EditAnywhere, Category="Art") TSoftObjectPtr<class UTexture2D> CombatOrbFrame = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Combat/T_PGCombatOrbFrame.T_PGCombatOrbFrame")));
    UPROPERTY(Config, EditAnywhere, Category="Art") TSoftObjectPtr<class UTexture2D> CombatPlate = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Combat/T_PGCombatPlate.T_PGCombatPlate")));
    UPROPERTY(Config, EditAnywhere, Category="Art") TSoftObjectPtr<class UTexture2D> CombatPotionIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Combat/T_PGCombatPotion.T_PGCombatPotion")));
    UPROPERTY(Config, EditAnywhere, Category="Art") TSoftObjectPtr<class UTexture2D> MenuFrame;
    UPROPERTY(Config, EditAnywhere, Category="Rewards") TSoftObjectPtr<class USoundBase> RewardOpenSound;
    UPROPERTY(Config, EditAnywhere, Category="Rewards") TSoftObjectPtr<class USoundBase> RewardConfirmSound;
    UPROPERTY(Config, EditAnywhere, Category="Rewards") TSoftObjectPtr<class USoundBase> VictorySound;
    UPROPERTY(Config, EditAnywhere, Category="Loot", meta=(ClampMin="3", ClampMax="24")) int32 MaxLootLabels = 12;
    UPROPERTY(Config, EditAnywhere, Category="Loot", meta=(ClampMin="400", ClampMax="4000")) float LootLabelDistance = 1800.f;
    UPROPERTY(Config, EditAnywhere, Category="Loot", meta=(ClampMin="180", ClampMax="360")) float LootLabelWidth = 260.f;
};
