#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "PGInventoryPresentation.h"
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
    bool PreviewSalvage(bool bJunkOnly);
    bool ConfirmSalvage();
    bool PreviewImprint(FName Effect);
    bool ConfirmImprint();
    bool PreviewCraft(FName Recipe);
    bool ConfirmCraft();
    void SetBuildTab(bool bBuild);
    void SetCharacterTab();
    void SetStatsTab();
    void ToggleRightPanel();
    void ToggleStatsPanel();
    FReply HandleEscape();
    void SetBagView(const FPGInventoryFilter& Filter, EPGInventorySort Sort);
    TArray<FGuid> GetVisibleItems() const;
    bool IsRightPanelOpen() const { return bRightOpen; }
    bool IsStatsPanelOpen() const { return bStatsOpen; }
    bool HasPendingEdit() const { return CraftRevision>=0 || !ImprintEffect.IsNone() || !SalvageTargets.IsEmpty() || bDraftDirty || bConfirmClose || bConfirmGrowthClose || PendingDiscard.IsValid() || bConfirmRecovery || !GrowthDraft.IsEmpty(); }
    FGuid GetSelectedItem() const { return SelectedItem; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    FName ImprintEffect;
    FName CraftRecipe;
    FPGItemInstance CraftResult;
    TMap<FName,int64> CraftCost;
    int64 CraftRevision=-1;
    TSharedPtr<class SVerticalBox> CraftRows;
    void CancelCraft();
    void RefreshCraft();
    FPGItemInstance ImprintResult;
    TMap<FName,int64> ImprintCost;
    int64 ImprintRevision=-1;
    TSharedPtr<class SVerticalBox> ImprintRows;
    void CancelImprint();
    void RefreshImprint();
    TArray<FGuid> SalvageTargets;
    TMap<FName, int64> SalvageRewards;
    int64 SalvageRevision = -1;
    bool bSalvageJunkOnly = false;
    TSharedPtr<class SVerticalBox> SalvageRows;
    void RefreshSalvage();
    void CancelSalvage();
    TMap<EPGStatType, int32> GrowthDraft, GrowthBase;
    FGuid GrowthRun;
    int32 GrowthStage = INDEX_NONE;
    bool bConfirmGrowthClose = false;
    FText GrowthStatus;
    bool CanAddGrowth(EPGStatType Stat) const;
    int32 GrowthDraftPoints() const;
    FReply ApplyGrowth();
    void ResetGrowthDraft();
    void CheckGrowthContext();
    TWeakObjectPtr<UPGProfileSubsystem> BoundProfile;
    FDelegateHandle ProfileChangedHandle;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UTexture2D>> IconTextures;
    TArray<TSharedPtr<FPGInventoryCellView>> Cells;
    TSharedPtr<class SUniformGridPanel> BagGrid;
    TSharedPtr<class SVerticalBox> EquippedRows, ComparisonRows, BuildRows, NearbyRows, CharacterRows;
    TSharedPtr<class SScrollBox> ComparisonScroll, BuildScroll, CharacterScroll, StatsScroll;
    TSharedPtr<class SExpandableArea> GrowthArea;
    TSharedPtr<class SWidgetSwitcher> Tabs;
    TSharedPtr<class SBox> Frame, StatsFrame;
    TSharedPtr<class SWidgetSwitcher> EquipmentPages;
    bool bRightOpen = true, bStatsOpen = false, bLastStats = false;
    bool bCompact = true;
    FPGInventoryFilter BagFilter;
    EPGInventorySort BagSort = EPGInventorySort::Recent;
    bool bCanEquip = false;
    bool bSelectedFavorite = false;
    FReply MarkSelected(bool bFavorite);
    void ApplyPanelLayout();
    TSharedPtr<class STextBlock> CapacityText;
    TSharedPtr<class STextBlock> StatusText;
    FGuid SelectedItem;
    uint8 SelectedEquipmentSlot = 255;
    FSlateBrush SelectedBrush;
    FSlateBrush InventoryFrameBrush, StatsFrameBrush;
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
    TSharedRef<SWidget> BuildStatsTab();
    FReply Close();
    FReply RecoverSave();
    FReply Unequip(uint8 EquipmentSlot);
    UTexture2D* GetIcon(int32 DefinitionId);
};
