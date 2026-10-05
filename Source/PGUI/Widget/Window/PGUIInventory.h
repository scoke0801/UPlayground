#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "PGUIInventory.generated.h"
struct FPGInventoryCellView;
class UPGProfileSubsystem;
UCLASS()
class PGUI_API UPGUIInventory : public UUserWidget
{
    GENERATED_BODY()
public:
    UPGUIInventory(const FObjectInitializer& Initializer);
    void SelectItem(FGuid Guid);
    bool EquipSelected();
    bool DiscardSelected();
    void SetBuildTab(bool bBuild);
    void SetCharacterTab();
    FGuid GetSelectedItem() const { return SelectedItem; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    TWeakObjectPtr<UPGProfileSubsystem> BoundProfile;
    FDelegateHandle ProfileChangedHandle;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UTexture2D>> IconTextures;
    TArray<TSharedPtr<FPGInventoryCellView>> Cells;
    TSharedPtr<class SUniformGridPanel> BagGrid;
    TSharedPtr<class SVerticalBox> EquippedRows, ComparisonRows, BuildRows, NearbyRows, CharacterRows;
    TSharedPtr<class SScrollBox> ComparisonScroll, BuildScroll;
    TSharedPtr<class SWidgetSwitcher> Tabs;
    TSharedPtr<class SBox> Frame;
    TSharedPtr<class STextBlock> CapacityText;
    TSharedPtr<class STextBlock> StatusText;
    FGuid SelectedItem;
    FSlateBrush SelectedBrush;
    UPROPERTY(Transient) TMap<FString, TObjectPtr<UTexture2D>> ArtTextures;
    TMap<FString, TSharedPtr<FSlateBrush>> ArtBrushes;
    TSharedPtr<class SVerticalBox> SkillListRows, SkillDetailRows, LoadoutRows, CharacterDetailRows;
    TSharedPtr<class SUniformGridPanel> CharacterGrid;
    FName PreviewCharacter;
    int32 PreviewSkill = 0;
    bool bDraftDirty = false;
    bool bConfirmClose = false;
    const FSlateBrush* Art(const FSoftObjectPath& Path);
    TArray<int32> GetEquippedActiveSkills() const;
    FReply ApplySkills();
    int32 MinRarity = 0;
    FGuid PendingDiscard;
    bool bConfirmRecovery = false;
    bool bBuildTab = false;
    TArray<int32> DraftActiveSkills;
    bool bRefreshPending = false;
    bool bKnownSelection = false;
    bool bSelectedEquipped = false;
    bool bCanWrite = false;
    bool bActionFailed = false;
    float LastWidth = 0.f;
    void OnProfileChanged();
    void Refresh();
    void RefreshBag();
    void RefreshEquipment();
    void RefreshComparison();
    void RefreshBuilds();
    void RefreshNearby();
    void RefreshCharacters();
    FReply Close();
    FReply RecoverSave();
    FReply Unequip(uint8 EquipmentSlot);
    UTexture2D* GetIcon(int32 DefinitionId);
};
