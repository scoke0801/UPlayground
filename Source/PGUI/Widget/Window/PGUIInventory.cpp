#include "PGUIInventory.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "PGInventoryPresentation.h"
#include "PGUI/Style/PGInventoryStyle.h"
#include "PGUI/Style/PGUIStyleSettings.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Stat/PGStatComponent.h"
#include "PGActor/Components/Rendering/PGCharacterAppearanceComponent.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGData/DataTable/Skill/PGPlayerSkillText.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/DataTable/Reward/PGRewardText.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SComboButton.h"
#include "PGData/DataAsset/Progression/PGEquipmentRules.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

class SPGInventoryPanel : public SBorder
{
public:
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
};

struct FPGInventoryCellView
{
    FGuid Guid;
    int32 DefinitionId = INDEX_NONE;
    bool bEquipped = false;
    EPGItemRarity Rarity = EPGItemRarity::Common;
    FSlateBrush Brush;
    TSharedPtr<SButton> Button;
    TSharedPtr<STextBlock> Name, Badge;
};

namespace PGInventoryUI
{
    using namespace PGInventoryPresentation;
    TSharedRef<STextBlock> Text(const FText& Value, int32 Size = 0, FLinearColor Color = FLinearColor::White, bool bBold = false)
    {
        return SNew(STextBlock).Text(Value).Font(PGInventoryStyle().Font(Size>0 ? Size : 0,bBold)).ColorAndOpacity(Color).AutoWrapText(true);
    }
    TSharedRef<STextBlock> Text(const FString& Value, int32 Size = 0, FLinearColor Color = FLinearColor::White, bool bBold = false)
    { return Text(FText::FromString(Value),Size,Color,bBold); }
    TSharedRef<SButton> Action(const FString& Label, TFunction<FReply()> Callback)
    {
        return SNew(SButton).ButtonStyle(&PGInventoryStyle().Button).ContentPadding(FMargin(14,10))
            .OnClicked_Lambda(MoveTemp(Callback))
            [SNew(STextBlock).Text(FText::FromString(Label)).Font(PGInventoryStyle().Font(0,true))
                .ColorAndOpacity(PGInventoryStyle().Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)];
    }
    void Line(const TSharedPtr<SVerticalBox>& Rows, const FText& Value, FLinearColor Color, int32 Size = 0, bool bBold = false)
    { Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[Text(Value,Size,Color,bBold)]; }
}

UPGUIInventory::UPGUIInventory(const FObjectInitializer& Initializer) : Super(Initializer) { SetIsFocusable(true); SetVisibility(ESlateVisibility::SelfHitTestInvisible); }

TSharedRef<SWidget> UPGUIInventory::RebuildWidget()
{
    using namespace PGInventoryUI;
    const auto& Style=PGInventoryStyle();
    auto MakeFrame=[this,&Style](const TSoftObjectPtr<UTexture2D>& Texture)
    {
        FSlateBrush Brush=*Art(Texture.ToSoftObjectPath());
        if (!Brush.GetResourceObject()) return FSlateBrush(Style.Panel);
        // Slate caches source-pixel corners; do not register the editor compilation placeholder.
#if WITH_EDITOR
        UTexture* FrameTexture=Cast<UTexture>(Brush.GetResourceObject());
        FTextureCompilingManager::Get().FinishCompilation({FrameTexture});
#endif
        Brush.DrawAs=ESlateBrushDrawType::Box;
        Brush.Margin=FMargin(.14f);
        return Brush;
    };
    InventoryFrameBrush=MakeFrame(GetDefault<UPGUIStyleSettings>()->InventoryFrame);
    StatsFrameBrush=MakeFrame(GetDefault<UPGUIStyleSettings>()->StatsFrame);
    auto Equipment=SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("장착 장비"),18,Style.Text,true)]
        + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(EquippedRows,SVerticalBox)]];
    auto Bag=SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("가방"),18,Style.Text,true)]
            + SHorizontalBox::Slot().AutoWidth()[SAssignNew(CapacityText,STextBlock).Font(Style.Font()).ColorAndOpacity(Style.Muted)]]
        + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(BagGrid,SUniformGridPanel).SlotPadding(FMargin(3))]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
        [SNew(STextBlock).Font(Style.Font(16)).ColorAndOpacity(Style.Muted).AutoWrapText(true)
            .Text_Lambda([this]()
            {
                const auto* P=UPGProfileSubsystem::Get(this);
                return FText::FromString(P && P->GetBagCount()==0 ? TEXT("가방이 비어 있습니다. 획득한 장비가 여기에 보관됩니다.") :
                    !Cells.ContainsByPredicate([](const auto& Cell){return Cell->Guid.IsValid();}) ? TEXT("필터에 맞는 장비가 없습니다.") : TEXT("장비를 선택하면 능력치를 비교하고 장착할 수 있습니다."));
            })];
    auto Comparison=SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[Text(TEXT("아이템 비교"),18,Style.Text,true)]
        + SVerticalBox::Slot().FillHeight(1)[SAssignNew(ComparisonScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(ComparisonRows,SVerticalBox)]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
        [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(14,12))
            .IsEnabled_Lambda([this](){return bKnownSelection && !bSelectedEquipped && bCanEquip;})
            .OnClicked_Lambda([this](){EquipSelected();return FReply::Handled();})
            [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(bSelectedEquipped ? TEXT("장착 중") : TEXT("선택한 아이템 장착"));})
                .Font(Style.Font(0,true)).ColorAndOpacity(Style.Mint)]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
        [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(14,10))
            .IsEnabled_Lambda([this](){return SelectedItem.IsValid() && !bSelectedEquipped && !bSelectedFavorite && bCanWrite;})
            .OnClicked_Lambda([this](){DiscardSelected();return FReply::Handled();})
            [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(PendingDiscard.IsValid() ? TEXT("버리기 확정 · 영구 삭제") : TEXT("아이템 버리기"));})
                .Font(Style.Font()).ColorAndOpacity(Style.Danger)]];
    Comparison->AddSlot().AutoHeight().Padding(0,6,0,0)
        [Action(TEXT("선택 장비 분해 미리보기"),[this](){PreviewSalvage(false);return FReply::Handled();})];
    Comparison->AddSlot().AutoHeight().Padding(0,6,0,0)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)
            [Action(TEXT("전설 각인"),[this](){CancelImprint();CancelSalvage();EquipmentPages->SetActiveWidgetIndex(4);RefreshImprint();return FReply::Handled();})]
            + SHorizontalBox::Slot().FillWidth(1).Padding(6,0,0,0)
            [Action(TEXT("담금질·명품화"),[this](){CancelImprint();CancelSalvage();EquipmentPages->SetActiveWidgetIndex(5);RefreshCraft();return FReply::Handled();})]];
    auto CraftPage=SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(CraftRows,SVerticalBox)];
    auto ImprintPage=SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(ImprintRows,SVerticalBox)];
    auto SalvagePage = SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(SalvageRows,SVerticalBox)];
    auto Filters = SNew(SWrapBox).UseAllottedSize(true);
    Filters->SetVisibility(TAttribute<EVisibility>::CreateLambda([this](){return EquipmentPages && EquipmentPages->GetActiveWidgetIndex()!=0 ? EVisibility::Collapsed : EVisibility::Visible;}));
    auto AddFilter = [this, &Filters](TFunction<FString()> Label, TArray<FString> Choices, TFunction<void(int32)> Select)
    {
        Filters->AddSlot().Padding(0,0,6,6)
        [SNew(SComboButton).ButtonStyle(&PGInventoryStyle().Button).ContentPadding(FMargin(8,6))
            .OnGetMenuContent_Lambda([Choices,Select]() -> TSharedRef<SWidget>
            {
                auto Rows = SNew(SVerticalBox);
                for (int32 I=0; I<Choices.Num(); ++I)
                    Rows->AddSlot().AutoHeight()[PGInventoryUI::Action(Choices[I],[Select,I]()
                    { Select(I); FSlateApplication::Get().DismissAllMenus(); return FReply::Handled(); })];
                return SNew(SBox).MaxDesiredHeight(360)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]];
            })
            .ButtonContent()[SNew(STextBlock).Text_Lambda([Label](){return FText::FromString(Label());})
                .Font(PGInventoryStyle().Font(15)).ColorAndOpacity(PGInventoryStyle().Text)]];
    };
    TArray<FString> Slots={TEXT("모든 부위")};
    for (uint8 I=0;I<uint8(EPGEquipmentSlot::Max);++I) Slots.Add(SlotName(EPGEquipmentSlot(I)).ToString());
    AddFilter([this,Slots](){return Slots[BagFilter.Slot+1];},Slots,[this](int32 I){BagFilter.Slot=I-1;Refresh();});
    TArray<FString> Grades={TEXT("모든 등급")};
    for (uint8 I=0;I<=uint8(EPGItemRarity::MythicUnique);++I) Grades.Add(RarityName(EPGItemRarity(I)).ToString());
    AddFilter([this,Grades](){return Grades[BagFilter.Rarity+1];},Grades,[this](int32 I){BagFilter.Rarity=I-1;Refresh();});
    const TArray<FString> Qualities={TEXT("모든 품질"),TEXT("기본 품질"),TEXT("선조")};
    AddFilter([this,Qualities](){return Qualities[BagFilter.Quality+1];},Qualities,[this](int32 I){BagFilter.Quality=I-1;Refresh();});
    const TArray<FString> Marks={TEXT("모든 표시"),TEXT("즐겨찾기"),TEXT("잡동사니")};
    AddFilter([this,Marks](){return Marks[BagFilter.Mark];},Marks,[this](int32 I){BagFilter.Mark=I;Refresh();});
    const TArray<FString> Sorts={TEXT("최근 획득순"),TEXT("부위순"),TEXT("등급순"),TEXT("위력순")};
    AddFilter([this,Sorts](){return Sorts[int32(BagSort)];},Sorts,[this](int32 I){BagSort=EPGInventorySort(I);Refresh();});
    auto EquipmentNavigation=SNew(SHorizontalBox);
    auto AddEquipmentPage=[this,&EquipmentNavigation](const TCHAR* Name,int32 Page)
    {
        EquipmentNavigation->AddSlot().FillWidth(1).Padding(2,0)
        [SNew(SButton).ButtonStyle(&PGInventoryTabStyle()).ContentPadding(FMargin(4,10)).HAlign(HAlign_Center)
            .OnClicked_Lambda([this,Page](){if (Page==3) { CancelSalvage(); RefreshSalvage(); } EquipmentPages->SetActiveWidgetIndex(Page);return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(Name)).Font(PGInventoryStyle().Font(16,true))
                .ColorAndOpacity_Lambda([this,Page](){return EquipmentPages && EquipmentPages->GetActiveWidgetIndex()==Page ? PGInventoryStyle().Lavender : PGInventoryStyle().Muted;})]];
    };
    AddEquipmentPage(TEXT("가방"),0);
    AddEquipmentPage(TEXT("장착"),1);
    AddEquipmentPage(TEXT("비교"),2);
    AddEquipmentPage(TEXT("재료"),3);
    auto EquipmentTab=SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
        [EquipmentNavigation]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[Filters]
        + SVerticalBox::Slot().FillHeight(1)
        [SAssignNew(EquipmentPages,SWidgetSwitcher)
            + SWidgetSwitcher::Slot()[Bag]
            + SWidgetSwitcher::Slot()[Equipment]
            + SWidgetSwitcher::Slot()[Comparison]
            + SWidgetSwitcher::Slot()[SalvagePage]
            + SWidgetSwitcher::Slot()[ImprintPage]
            + SWidgetSwitcher::Slot()[CraftPage]];
    auto BuildTab=SAssignNew(BuildScroll,SScrollBox)
        + SScrollBox::Slot()[SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                [SNew(SBorder).BorderImage(&Style.Card).Padding(12)[SAssignNew(SkillListRows,SVerticalBox)]]
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                [SNew(SBorder).BorderImage(&Style.Card).Padding(12)[SAssignNew(SkillDetailRows,SVerticalBox)]]
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                [SNew(SBorder).BorderImage(&Style.Card).Padding(12)[SAssignNew(LoadoutRows,SVerticalBox)]]
            + SVerticalBox::Slot().AutoHeight()
                [SNew(SBorder).BorderImage(&Style.Card).Padding(12)[SAssignNew(BuildRows,SVerticalBox)]]];
    auto CharacterTab=SAssignNew(CharacterScroll,SScrollBox)
        + SScrollBox::Slot()[SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SAssignNew(CharacterRows,SVerticalBox)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[SAssignNew(CharacterGrid,SUniformGridPanel).SlotPadding(FMargin(6))]
            + SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&Style.Card).Padding(12)[SAssignNew(CharacterDetailRows,SVerticalBox)]]];
    auto NearbyTab=SNew(SBorder).BorderImage(&Style.Card).Padding(20)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
            [Action(TEXT("등급 필터 변경 · 전체 / 마법 이상 / 희귀"),[this](){MinRarity=(MinRarity+1)%3;RefreshNearby();return FReply::Handled();})]
            + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(NearbyRows,SVerticalBox)]]];
    auto TabButtons=SNew(SWrapBox).UseAllottedSize(true);
    int32 TabIndex=0;
    auto AddTab=[&](const TCHAR* Name,TFunction<FReply()> ActionCallback)
    {
        const int32 Index=TabIndex++;
        TabButtons->AddSlot().Padding(0,0,6,12)
        [SNew(SButton).ButtonStyle(&PGInventoryTabStyle()).ContentPadding(FMargin(12,10))
            .OnClicked_Lambda(MoveTemp(ActionCallback))
            [SNew(STextBlock).Text(FText::FromString(Name)).Font(Style.Font(16,true))
                .ColorAndOpacity_Lambda([this,Index](){return Tabs && Tabs->GetActiveWidgetIndex()==Index ? PGInventoryStyle().Lavender : PGInventoryStyle().Muted;})]];
    };
    AddTab(TEXT("장비"),[this](){SetBuildTab(false);return FReply::Handled();});
    AddTab(TEXT("검술"),[this](){SetBuildTab(true);return FReply::Handled();});
    AddTab(TEXT("전리품"),[this](){bBuildTab=false;Tabs->SetActiveWidgetIndex(2);RefreshNearby();return FReply::Handled();});
    AddTab(TEXT("캐릭터"),[this](){SetCharacterTab();return FReply::Handled();});
    AddTab(TEXT("능력치"),[this](){ToggleStatsPanel();return FReply::Handled();});
    auto RightClose=Action(TEXT("닫기"),[this](){return Close();});
    RightClose->SetTag(TEXT("PGInventory.CloseRight"));
    auto Root=SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        + SOverlay::Slot().HAlign(HAlign_Left).Padding(12,24,0,190)
        [SAssignNew(StatsFrame,SBox).Tag(TEXT("PGInventory.Left")).WidthOverride(420)
            .Visibility_Lambda([this](){return bStatsOpen ? EVisibility::Visible : EVisibility::Collapsed;})
            [SNew(SPGInventoryPanel).BorderImage(&StatsFrameBrush).Padding(24)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("능력치"),28,Style.Text,true)]
                        + SHorizontalBox::Slot().AutoWidth()[Action(TEXT("닫기"),[this](){ToggleStatsPanel();return FReply::Handled();})]]
                    + SVerticalBox::Slot().FillHeight(1)[BuildStatsTab()]]]]
        + SOverlay::Slot().HAlign(HAlign_Right).Padding(0,24,12,190)
        [SAssignNew(Frame,SBox).Tag(TEXT("PGInventory.Right")).WidthOverride(420)
            .Visibility_Lambda([this](){return bRightOpen ? EVisibility::Visible : EVisibility::Collapsed;})
            [SNew(SPGInventoryPanel).BorderImage(&InventoryFrameBrush).Padding(24)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("인벤토리"),28,Style.Text,true)]
                        + SHorizontalBox::Slot().AutoWidth()[RightClose]]
                    + SVerticalBox::Slot().AutoHeight()[TabButtons]
                    + SVerticalBox::Slot().FillHeight(1)
                    [SAssignNew(Tabs,SWidgetSwitcher)
                        + SWidgetSwitcher::Slot()[EquipmentTab]
                        + SWidgetSwitcher::Slot()[BuildTab]
                        + SWidgetSwitcher::Slot()[NearbyTab]
                        + SWidgetSwitcher::Slot()[CharacterTab]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
                            [SNew(SBorder).BorderImage(&Style.Selected).Padding(10)
                                .Visibility_Lambda([this](){return bConfirmClose ? EVisibility::Visible : EVisibility::Collapsed;})
                                [SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight()[Text(TEXT("적용하지 않은 검술 변경이 있습니다."),16,Style.Text)]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0,0,10,0)
                                    [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(14,10))
                .IsEnabled_Lambda([this](){FString Reason; auto* P=UPGProfileSubsystem::Get(this); return P && P->CanChangeSkills(Reason) && P->GetCatalog()->IsValidActiveSelection(DraftActiveSkills);})
                                        .OnClicked_Lambda([this](){ApplySkills(); if (!bActionFailed && !bDraftDirty) return Close(); return FReply::Handled();})
                                        [SNew(STextBlock).Text(FText::FromString(TEXT("적용 후 닫기"))).Font(Style.Font(16,true)).ColorAndOpacity(Style.Mint)]]
                                    + SVerticalBox::Slot().AutoHeight()[Action(TEXT("계속 편집"),[this](){bConfirmClose=false;return FReply::Handled();})]
                                    + SVerticalBox::Slot().AutoHeight().Padding(10,0)[Action(TEXT("변경 취소 후 닫기"),[this](){bDraftDirty=false;return Close();})]]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
                            [SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                                [SAssignNew(StatusText,STextBlock).Font(Style.Font(16)).ColorAndOpacity(Style.Muted).AutoWrapText(true)]
                                + SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)
                                [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(12,8))
                                    .Visibility_Lambda([this](){return BoundProfile.IsValid() && BoundProfile->IsSaveBlocked() ? EVisibility::Visible : EVisibility::Collapsed;})
                                    .OnClicked_UObject(this,&ThisClass::RecoverSave)
                                    [SNew(STextBlock).Font(Style.Font(16)).Text_Lambda([this](){return FText::FromString(bConfirmRecovery ? TEXT("복구 확정 · Esc 취소") : TEXT("저장 복구"));})]]]
                ]]];
    Refresh(); return Root;
}

void UPGUIInventory::NativeConstruct()
{
    Super::NativeConstruct(); BoundProfile=UPGProfileSubsystem::Get(this);
    if (BoundProfile.IsValid()) ProfileChangedHandle=BoundProfile->OnProfileChanged.AddUObject(this,&ThisClass::OnProfileChanged);
    int32 X=0,Y=0; if (GetOwningPlayer()) GetOwningPlayer()->GetViewportSize(X,Y); bCompact=X<1600;
    Refresh();
}
void UPGUIInventory::NativeDestruct()
{
    if (auto* UI=UPGUIManager::Get(this)) UI->ReleaseModalInput(this);
    if (BoundProfile.IsValid()) BoundProfile->OnProfileChanged.Remove(ProfileChangedHandle);
    BoundProfile.Reset(); Super::NativeDestruct();
}
void UPGUIInventory::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    CheckGrowthContext();
    if (CraftRevision>=0)
    {
        auto* P=UPGProfileSubsystem::Get(this); FString Reason;
        if (!P || !P->GetProfile() || P->GetProfile()->Revision!=CraftRevision || !P->CanChangeSkills(Reason))
        { CancelCraft(); bRefreshPending=true; }
    }
    if (!ImprintEffect.IsNone())
    {
        auto* P=UPGProfileSubsystem::Get(this); FString Reason;
        if (!P || !P->GetProfile() || P->GetProfile()->Revision!=ImprintRevision || !P->CanChangeSkills(Reason))
        { CancelImprint(); bRefreshPending=true; }
    }
    if (!SalvageTargets.IsEmpty())
    {
        auto* P = UPGProfileSubsystem::Get(this); FString Reason;
        if (!P || !P->GetProfile() || P->GetProfile()->Revision != SalvageRevision || !P->CanChangeSkills(Reason))
        {
            CancelImprint(); CancelSalvage();
            if (P) P->Status=TEXT("상태가 변경되어 분해 미리보기를 취소했습니다");
            bRefreshPending=true;
        }
    }
    int32 PixelsX=0, PixelsY=0;
    if (GetOwningPlayer()) GetOwningPlayer()->GetViewportSize(PixelsX,PixelsY);
    bCompact=PixelsX<1600;
    ApplyPanelLayout();
    const float Width=FMath::Max(1.f,Geometry.GetLocalSize().X*(bCompact ? .46f : .29f));
    if (!FMath::IsNearlyEqual(Width,LastWidth))
    { if (Frame) Frame->SetWidthOverride(Width); if (StatsFrame) StatsFrame->SetWidthOverride(Width); LastWidth=Width; }
    if (bRefreshPending) Refresh();
}
void UPGUIInventory::OnProfileChanged() { bRefreshPending=true; }
FReply UPGUIInventory::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
    bLastStats=bStatsOpen && StatsFrame && StatsFrame->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition());
    if (auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->SuppressInventoryClick();
    return FReply::Unhandled();
}
FReply UPGUIInventory::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    const auto* PC=Cast<APGPlayerController>(GetOwningPlayer());
    if (Event.GetKey()==EKeys::Escape || (PC && (Event.GetKey()==PC->InventoryKey || Event.GetKey()==PC->StatsKey)))
    {
        if (Event.IsRepeat()) return FReply::Handled();
        if (Event.GetKey()==EKeys::Escape) return HandleEscape();
        if (PC && Event.GetKey()==PC->StatsKey) ToggleStatsPanel(); else ToggleRightPanel();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
FReply UPGUIInventory::Close()
{
    CancelImprint(); CancelSalvage();
    if (bDraftDirty) { bConfirmClose=true; SetKeyboardFocus(); return FReply::Handled(); }
    bRightOpen=false;
    if (!bStatsOpen && !GrowthDraft.IsEmpty()) { bStatsOpen=true; bLastStats=true; bConfirmGrowthClose=true; if (GrowthArea) GrowthArea->SetExpanded(true); if (StatsScroll) StatsScroll->ScrollToStart(); ApplyPanelLayout(); return FReply::Handled(); }
    if (!bStatsOpen) { if (auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->CloseInventory(); }
    else bLastStats=true;
    return FReply::Handled();
}
void UPGUIInventory::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Cells.Empty(); BagGrid.Reset(); EquippedRows.Reset(); ComparisonRows.Reset(); BuildRows.Reset(); NearbyRows.Reset();
    GrowthArea.Reset();
    SalvageRows.Reset(); ImprintRows.Reset(); CraftRows.Reset();
    CharacterRows.Reset(); CharacterGrid.Reset(); CharacterDetailRows.Reset();
    SkillListRows.Reset(); SkillDetailRows.Reset(); LoadoutRows.Reset();
    ComparisonScroll.Reset(); BuildScroll.Reset(); CharacterScroll.Reset(); StatsScroll.Reset(); Tabs.Reset(); Frame.Reset(); StatsFrame.Reset(); EquipmentPages.Reset(); CapacityText.Reset(); StatusText.Reset(); IconTextures.Empty(); ArtBrushes.Empty(); ArtTextures.Empty();
}
void UPGUIInventory::Refresh()
{
    bRefreshPending=false; auto* Profile=UPGProfileSubsystem::Get(this);
    if (!StatusText) return;
    if (!Profile || !Profile->GetCatalog() || !Profile->GetProfile())
    {
        bCanWrite=false;
        StatusText->SetText(FText::FromString(TEXT("장비 정보를 불러올 수 없습니다. 창을 닫고 다시 시도해 주세요."))); return;
    }
    const auto* Save=Profile->GetProfile(); bCanWrite=!Profile->IsSaveBlocked();
    FString LockReason; bCanEquip=Profile->CanChangeSkills(LockReason);
    if (!Save->Items.ContainsByPredicate([this](const auto& Item){return Item.Guid==SelectedItem;}))
    { SelectedItem.Invalidate(); if (EquipmentPages && EquipmentPages->GetActiveWidgetIndex()==2) EquipmentPages->SetActiveWidgetIndex(0); }
    const FString Policy=Profile->GetCatalog()->bRoguelikeRuns ? TEXT("장비·강화는 이번 도전 전용") : TEXT("획득·장착 즉시 저장");
    StatusText->SetText(FText::FromString(bConfirmRecovery ? TEXT("원본을 보관하고 복구합니다. 유효한 백업이 없으면 빈 프로필로 시작합니다. Esc로 취소할 수 있습니다.") : Policy+TEXT("  ·  ")+Profile->Status));
    StatusText->SetColorAndOpacity(bActionFailed || Profile->IsSaveBlocked() ? PGInventoryStyle().Danger : PGInventoryStyle().Muted);
    RefreshBag(); RefreshEquipment(); RefreshComparison(); RefreshBuilds(); RefreshNearby(); RefreshCharacters(); RefreshSalvage(); RefreshImprint(); RefreshCraft();
    if (!bCanEquip && !bActionFailed && !bConfirmRecovery) StatusText->SetText(FText::FromString(LockReason));
}
void UPGUIInventory::SetCharacterTab()
{
    CancelImprint(); CancelSalvage();
    bBuildTab=false;
    PendingDiscard.Invalidate();
    if (Tabs) Tabs->SetActiveWidgetIndex(3);
    RefreshCharacters();
}
const FSlateBrush* UPGUIInventory::Art(const FSoftObjectPath& Path)
{
    const FString Key=Path.ToString();
    if (const auto* Existing=ArtBrushes.Find(Key)) return Existing->Get();
    auto Brush=MakeShared<FSlateBrush>();
    auto* Texture=Cast<UTexture2D>(Path.TryLoad());
    ArtTextures.Add(Key,Texture);
    Brush->SetResourceObject(Texture);
    // GetSizeX/Y can still report the square placeholder during async texture compilation.
    // The imported dimensions preserve the artwork's aspect ratio in editor and cooked builds.
    const FIntPoint ImportedSize=Texture ? Texture->GetImportedSize() : FIntPoint::ZeroValue;
    Brush->ImageSize=ImportedSize.X>0 && ImportedSize.Y>0 ? FVector2D(ImportedSize) : FVector2D(64,64);
    Brush->DrawAs=Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
    ArtBrushes.Add(Key,Brush);
    return &Brush.Get();
}
void UPGUIInventory::RefreshCharacters()
{
    using namespace PGInventoryUI;
    if (!CharacterRows || !CharacterGrid || !CharacterDetailRows) return;
    CharacterRows->ClearChildren(); CharacterGrid->ClearChildren(); CharacterDetailRows->ClearChildren();
    auto* Profile=UPGProfileSubsystem::Get(this);
    if (!Profile || !Profile->GetCatalog() || !Profile->GetProfile()) return;
    const auto& Style=PGInventoryStyle();
    Line(CharacterRows,FText::FromString(TEXT("함께할 캐릭터")),Style.Text,24,true);
    Line(CharacterRows,FText::FromString(TEXT("초상화를 살펴보고 함께할 캐릭터를 선택하세요.")),Style.Muted,16);
    FName CurrentCharacter=Profile->GetProfile()->CharacterId;
    if (const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn()); Player && Player->AppearanceComponent)
        if (const auto* Appearance=Player->AppearanceComponent->GetAppearance()) CurrentCharacter=Appearance->Id;
    TArray<const UPGCharacterAppearance*> Appearances;
    for (const auto& Reference : Profile->GetCatalog()->PlayableCharacters)
        if (const auto* Appearance=Reference.LoadSynchronous()) Appearances.Add(Appearance);
    if (!Appearances.ContainsByPredicate([this](const auto* Entry){return Entry->Id==PreviewCharacter;}))
    {
        const auto* Current=Appearances.FindByPredicate([CurrentCharacter](const auto* Entry){return Entry->Id==CurrentCharacter;});
        PreviewCharacter=Current ? (*Current)->Id : Appearances.IsEmpty() ? NAME_None : Appearances[0]->Id;
    }
    if (Appearances.IsEmpty())
        Line(CharacterDetailRows,FText::FromString(TEXT("선택할 수 있는 캐릭터가 없습니다.")),Style.Muted);
    else if (!Appearances.ContainsByPredicate([CurrentCharacter](const auto* Entry){return Entry->Id==CurrentCharacter;}))
        Line(CharacterRows,FText::FromString(TEXT("현재 기본 외형 사용 중 · 캐릭터를 확정하면 다음 도전에도 유지됩니다.")),Style.Muted,13);
    const UPGCharacterAppearance* Preview=nullptr;
    int32 Index=0;
    for (const auto* Appearance : Appearances)
    {
        const FName Id=Appearance->Id;
        const bool Current=CurrentCharacter==Id;
        const bool Selected=PreviewCharacter==Id;
        if (Selected) Preview=Appearance;
        CharacterGrid->AddSlot(Index%2,Index/2)
        [SNew(SBorder).BorderImage(Selected ? &Style.Selected : &Style.Card).Padding(2)
            [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(0)
                .OnClicked_Lambda([this,Id](){PreviewCharacter=Id;RefreshCharacters();if (CharacterScroll) CharacterScroll->ScrollDescendantIntoView(CharacterDetailRows);SetKeyboardFocus();return FReply::Handled();})
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(SBox).HeightOverride(172).Clipping(EWidgetClipping::ClipToBounds)
                        [SNew(SScaleBox).Stretch(EStretch::ScaleToFill).VAlign(VAlign_Top)
                            [SNew(SImage).Tag(Appearance->Id).Image(Art(Appearance->Portrait.ToSoftObjectPath()))]]]
                    + SVerticalBox::Slot().AutoHeight().Padding(10,9,10,3)[Text(Appearance->DisplayName,16,Style.Text,true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(10,0,10,10)[Text(Current ? TEXT("사용 중") : Selected ? TEXT("미리보기") : TEXT("살펴보기"),12,Current ? Style.Mint : Style.Muted)]]]];
        ++Index;
    }
    if (!Preview) return;
    CharacterDetailRows->AddSlot().AutoHeight()
        [SNew(SBox).HeightOverride(220).Clipping(EWidgetClipping::ClipToBounds)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Tag(Preview->Id).Image(Art(Preview->Portrait.ToSoftObjectPath()))]]];
    CharacterDetailRows->AddSlot().AutoHeight().Padding(8,12)[Text(Preview->DisplayName,30,Style.Text,true)];
    Line(CharacterDetailRows,FText::FromString(TEXT("검술 · 장비 · 강화 공유  /  다음 도전에도 유지")),Style.Muted,14);
    FString Reason; const bool CanChange=Profile->CanChangeSkills(Reason);
    const bool Current=CurrentCharacter==Preview->Id;
    if (!CanChange) Line(CharacterDetailRows,FText::FromString(Reason),Style.Rare,14);
    auto Confirm=Action(Current ? TEXT("현재 함께하는 캐릭터") : TEXT("이 캐릭터와 함께하기  →"),[this,Id=Preview->Id]()
    {
        if (auto* P=UPGProfileSubsystem::Get(this)) bActionFailed=!P->SelectCharacter(Id);
        bRefreshPending=true; return FReply::Handled();
    });
    Confirm->SetEnabled(CanChange && !Current && bCanWrite);
    CharacterDetailRows->AddSlot().AutoHeight()[Confirm];
}
UTexture2D* UPGUIInventory::GetIcon(int32 DefinitionId)
{
    if (const auto* Found=IconTextures.Find(DefinitionId)) return Found->Get();
    auto* Profile=UPGProfileSubsystem::Get(this);
    const auto* Def=Profile && Profile->GetCatalog() ? Profile->GetCatalog()->FindItem(DefinitionId) : nullptr;
    auto* Texture=Def ? Def->Icon.LoadSynchronous() : nullptr; IconTextures.Add(DefinitionId,Texture); return Texture;
}
void UPGUIInventory::RefreshBag()
{
    using namespace PGInventoryUI;
    auto* Profile=UPGProfileSubsystem::Get(this);
    const auto* Save=Profile->GetProfile(); const auto* Catalog=Profile->GetCatalog(); const auto& Style=PGInventoryStyle();
    CapacityText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),Profile->GetBagCount(),Catalog->BagCapacity)));
    CapacityText->SetColorAndOpacity(Profile->GetBagCount()>=Catalog->BagCapacity ? Style.Danger : Style.Muted);
    TArray<const FPGItemInstance*> Bag;
    const auto Visible=GetVisibleItems();
    for (const auto& Id : Visible) Bag.Add(Save->Items.FindByPredicate([Id](const auto& I){return I.Guid==Id;}));
    if (SelectedItem.IsValid() && !Save->Equipment.FindKey(SelectedItem) && !Visible.Contains(SelectedItem))
    {
        SelectedItem.Invalidate(); PendingDiscard.Invalidate(); SelectedEquipmentSlot=255;
        StatusText->SetText(FText::FromString(TEXT("선택한 아이템이 필터에서 제외되어 선택을 해제했습니다.")));
        if (EquipmentPages && EquipmentPages->GetActiveWidgetIndex()==2) EquipmentPages->SetActiveWidgetIndex(0);
    }
    const int32 Count=FMath::Max(Catalog->BagCapacity,Bag.Num());
    if (Cells.Num()!=Count)
    {
        BagGrid->ClearChildren(); Cells.Empty();
        for (int32 Index=0;Index<Count;++Index)
        {
            auto Cell=MakeShared<FPGInventoryCellView>(); Cells.Add(Cell);
            BagGrid->AddSlot(Index%6,Index/6)
            [SNew(SBox).HeightOverride(92)
                [SNew(SBorder).Padding(2).BorderImage_Lambda([this,Index]()
                    { return Cells.IsValidIndex(Index) && Cells[Index]->Guid.IsValid() && Cells[Index]->Guid==SelectedItem
                        ? &PGInventoryStyle().Selected : FCoreStyle::Get().GetBrush("NoBorder"); })
                    [SAssignNew(Cell->Button,SButton).ButtonStyle(&Style.Button).ContentPadding(4)
                        .OnClicked_Lambda([this,Index](){if(Cells.IsValidIndex(Index)) SelectItem(Cells[Index]->Guid);return FReply::Handled();})
                        [SNew(SOverlay)
                            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                            [SNew(SBox).WidthOverride(60).HeightOverride(60)[SNew(SImage).Image(&Cell->Brush)]]
                            + SOverlay::Slot().VAlign(VAlign_Center)
                            [SAssignNew(Cell->Name,STextBlock).Font(Style.Font(13)).Justification(ETextJustify::Center).AutoWrapText(true)]
                            + SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Center)
                            [SAssignNew(Cell->Badge,STextBlock).Font(Style.Font(12,true))]]]]];
        }
    }
    for (int32 Index=0;Index<Count;++Index)
    {
        auto& Cell=*Cells[Index];
        const auto* Item=Bag.IsValidIndex(Index) ? Bag[Index] : nullptr;
        const auto* Def=Item ? Catalog->FindItem(Item->DefinitionId) : nullptr;
        Cell.Guid=Item ? Item->Guid : FGuid();
        const int32 DefinitionId=Item ? Item->DefinitionId : 0;
        const bool Equipped=Item && Save->Equipment.FindKey(Item->Guid)!=nullptr;
        const auto Rarity = Def && Item ? Def->GetRarity(*Item) : EPGItemRarity::Common;

        Cell.Rarity=Rarity;
        Cell.DefinitionId=DefinitionId; Cell.bEquipped=Equipped;
        Cell.Button->SetEnabled(Item!=nullptr); Cell.Brush=FSlateBrush();
        auto* Icon=Def ? GetIcon(Def->Id) : nullptr;
        Cell.Brush.SetResourceObject(Icon); Cell.Brush.ImageSize=FVector2D(40,40);
        Cell.Brush.DrawAs=Icon ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
        if (Def && Def->IconPanel>=0 && Def->IconPanel<3) Cell.Brush.SetUVRegion(FBox2f(FVector2f(Def->IconPanel/3.f,0),FVector2f((Def->IconPanel+1)/3.f,1)));
        Cell.Name->SetText(Def ? Def->DisplayName : FText::FromString(Item ? TEXT("알 수 없는 장비") : TEXT("")));
        Cell.Name->SetVisibility(Icon ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
        Cell.Name->SetColorAndOpacity(Def ? Style.Text : Style.Muted);
        Cell.Badge->SetText(FText::FromString(Equipped ? TEXT("장착 중") : Def ? (Item->bFavorite ? TEXT("★ ") : Item->bJunk ? TEXT("× ") : TEXT(""))+RarityName(Rarity).ToString() : TEXT("")));
        Cell.Badge->SetColorAndOpacity(Equipped ? Style.Mint : Def ? Style.RarityColor(Rarity) : Style.Muted);
        Cell.Button->SetToolTipText(Def ? FText::FromString(Def->DisplayName.ToString()+TEXT("\n")+RarityName(Rarity).ToString()+TEXT(" · 선택하여 비교")) : FText::GetEmpty());
    }
}
void UPGUIInventory::SelectItem(FGuid Guid)
{
    CancelImprint(); CancelSalvage();
    auto* Profile=UPGProfileSubsystem::Get(this);
    if (!Profile || !Profile->GetProfile() || !Profile->GetProfile()->Items.ContainsByPredicate([Guid](const auto& Item){return Item.Guid==Guid;})) return;
    if (SelectedItem!=Guid && ComparisonScroll) ComparisonScroll->SetScrollOffset(0);
    if (SelectedItem!=Guid) SelectedEquipmentSlot=255;
    SelectedItem=Guid; PendingDiscard.Invalidate(); RefreshComparison();
    if (EquipmentPages) EquipmentPages->SetActiveWidgetIndex(2);
}
void UPGUIInventory::RefreshEquipment()
{
    using namespace PGInventoryUI;
    const auto* Profile=UPGProfileSubsystem::Get(this); const auto* Save=Profile->GetProfile(); const auto& Style=PGInventoryStyle();
    EquippedRows->ClearChildren();
    for (uint8 SlotIndex=0;SlotIndex<uint8(EPGEquipmentSlot::Max);++SlotIndex)
    {
        const auto EquipmentSlot=EPGEquipmentSlot(SlotIndex);
        const auto Id=Save->Equipment.FindRef(EquipmentSlot);
        const auto* Item=Save->Items.FindByPredicate([Id](const auto& Entry){return Entry.Guid==Id;});
        const auto* Def=Item ? Profile->GetCatalog()->FindItem(Item->DefinitionId) : nullptr;
        Line(EquippedRows,SlotName(EquipmentSlot),Style.Muted,12,true);
        if (Item)
        {
            EquippedRows->AddSlot().AutoHeight().Padding(0,0,0,8)
                [Action(Def ? Def->DisplayName.ToString() : TEXT("정보를 찾을 수 없는 장비"),[this,Id](){SelectItem(Id);return FReply::Handled();})];
            auto Remove=Action(TEXT("장착 해제"),[this,EquipmentSlot](){return Unequip(uint8(EquipmentSlot));}); Remove->SetEnabled(bCanEquip);
            EquippedRows->AddSlot().AutoHeight().Padding(0,0,0,20)[Remove];
        }
        else Line(EquippedRows,FText::FromString(TEXT("장착한 장비 없음")),Style.Text);
    }
    Line(EquippedRows,FText::FromString(TEXT("현재 능력치")),Style.Text,16,true);
    const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
    if (Player && Player->GetStatComponent())
        for (const auto Stat : {EPGStatType::Health,EPGStatType::Attack,EPGStatType::Defense,EPGStatType::CriticalRate,EPGStatType::CriticalDamage})
            Line(EquippedRows,FText::FromString(StatName(Stat).ToString()+TEXT("  ")+StatValue(Stat,Player->GetStatComponent()->GetStat(Stat)).ToString()),Style.Muted,12);
}
void UPGUIInventory::RefreshComparison()
{
    using namespace PGInventoryUI;
    if (!ComparisonRows) return;
    ComparisonRows->ClearChildren(); bKnownSelection=false; bSelectedEquipped=false; bSelectedFavorite=false;
    const auto* Profile=UPGProfileSubsystem::Get(this); if (!Profile || !Profile->GetProfile() || !Profile->GetCatalog()) return;
    const auto* Save=Profile->GetProfile(); const auto& Style=PGInventoryStyle();
    const auto* Item=Save->Items.FindByPredicate([this](const auto& Entry){return Entry.Guid==SelectedItem;});
    if (!Item) { Line(ComparisonRows,FText::FromString(TEXT("가방 또는 장착 목록에서 비교할 아이템을 선택하세요.")),Style.Muted); return; }
    bSelectedEquipped=Save->Equipment.FindKey(Item->Guid)!=nullptr;
    bSelectedFavorite=Item->bFavorite;
    auto Favorite=Action(Item->bFavorite ? TEXT("즐겨찾기 해제") : TEXT("즐겨찾기 지정"),[this](){return MarkSelected(true);});
    Favorite->SetEnabled(bCanWrite);
    auto Junk=Action(Item->bJunk ? TEXT("잡동사니 해제") : TEXT("잡동사니 지정"),[this](){return MarkSelected(false);});
    Junk->SetEnabled(bCanWrite && !bSelectedEquipped);
    const auto* Def=Profile->GetCatalog()->FindItem(Item->DefinitionId);
    if (!Def) { Line(ComparisonRows,FText::FromString(TEXT("정보를 불러올 수 없는 장비입니다. 아이템은 가방에 보관되며 장착할 수 없습니다.")),Style.Muted); return; }
    bKnownSelection=true;
    SelectedBrush=FSlateBrush();
    auto* Icon=GetIcon(Def->Id);
    SelectedBrush.SetResourceObject(Icon); SelectedBrush.ImageSize=FVector2D(64,64);
    SelectedBrush.DrawAs=Icon ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
    if (Def->IconPanel>=0 && Def->IconPanel<3) SelectedBrush.SetUVRegion(FBox2f(FVector2f(Def->IconPanel/3.f,0),FVector2f((Def->IconPanel+1)/3.f,1)));
    ComparisonRows->AddSlot().AutoHeight().Padding(0,0,0,18)
    [SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)[SNew(SBox).WidthOverride(64).HeightOverride(64)[SNew(SImage).Image(&SelectedBrush)]]
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[Text(RarityName(Def->GetRarity(*Item)).ToString()+TEXT(" · ")+SlotName(Def->Slot).ToString(),16,Style.RarityColor(Def->GetRarity(*Item)),true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[Text(Def->DisplayName,22,Style.Text,true)]]];
    ComparisonRows->AddSlot().AutoHeight().Padding(0,0,0,16)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[Favorite]
            + SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[Junk]];
    const bool Ring=Def->Slot==EPGEquipmentSlot::RingLeft || Def->Slot==EPGEquipmentSlot::RingRight;
    if (!Ring) SelectedEquipmentSlot=uint8(Def->Slot);
    else if (bSelectedEquipped) SelectedEquipmentSlot=uint8(*Save->Equipment.FindKey(Item->Guid));
    else if (SelectedEquipmentSlot!=uint8(EPGEquipmentSlot::RingLeft) && SelectedEquipmentSlot!=uint8(EPGEquipmentSlot::RingRight))
        SelectedEquipmentSlot=uint8(!Save->Equipment.Contains(EPGEquipmentSlot::RingLeft) ? EPGEquipmentSlot::RingLeft :
            !Save->Equipment.Contains(EPGEquipmentSlot::RingRight) ? EPGEquipmentSlot::RingRight : Def->Slot);
    if (Ring)
        for (const auto Target : {EPGEquipmentSlot::RingLeft,EPGEquipmentSlot::RingRight})
            ComparisonRows->AddSlot().AutoHeight().Padding(0,0,0,8)
                [Action(SlotName(Target).ToString()+TEXT("과 비교"),[this,Target]()
                { SelectedEquipmentSlot=uint8(Target); RefreshComparison(); return FReply::Handled(); })];
    const FGuid CurrentId=Save->Equipment.FindRef(EPGEquipmentSlot(SelectedEquipmentSlot));
    Line(ComparisonRows,FText::FromString(TEXT("교체 부위 · ")+SlotName(EPGEquipmentSlot(SelectedEquipmentSlot)).ToString()),Style.Muted,12);
    Line(ComparisonRows,FText::FromString(FString::Printf(TEXT("%s · 위력 %d · 상위 옵션 %d개"),
        Item->Quality==EPGItemQuality::Ancestral ? TEXT("선조") : TEXT("기본 품질"),Item->Power,Item->GreaterOptions.Num())),Style.Muted,12);
    for (const auto& Roll : Item->OptionRolls)
    {
        const TCHAR* Source = Roll.Source==EPGItemOptionSource::Base ? TEXT("기본 성능") :
            Roll.Source==EPGItemOptionSource::Implicit ? TEXT("고정 특성") : Roll.bGreater ? TEXT("◆ 상위 옵션") : TEXT("일반 옵션");
        Line(ComparisonRows,FText::FromString(FString::Printf(TEXT("%s · %s %s (일반 범위 %s~%s)"),Source,
            *StatName(Roll.Stat).ToString(),*StatValue(Roll.Stat,Roll.Value).ToString(),
            *StatValue(Roll.Stat,Roll.Min).ToString(),*StatValue(Roll.Stat,Roll.Max).ToString())),Roll.bGreater ? Style.Lavender : Style.Muted,12);
    }
    if (!Item->Temper.Id.IsNone())
        Line(ComparisonRows,FText::FromString(TEXT("담금질 · ")+StatName(Item->Temper.Stat).ToString()+TEXT(" ")+StatValue(Item->Temper.Stat,Item->Temper.Value).ToString()),Style.Mint,12);
    if (Item->MasterworkLevel>0)
        Line(ComparisonRows,FText::FromString(FString::Printf(TEXT("명품화 %d단계 · 기본 성능 추가분"),Item->MasterworkLevel)),Style.Muted,14);
    for (const auto& Pair : Item->MasterworkBonuses)
        Line(ComparisonRows,FText::FromString(StatName(Pair.Key).ToString()+TEXT(" ")+StatValue(Pair.Key,Pair.Value).ToString()),Style.Mint,12);
    const auto* Current=Save->Items.FindByPredicate([CurrentId](const auto& Entry){return Entry.Guid==CurrentId;});
    const auto* CurrentDef=Current ? Profile->GetCatalog()->FindItem(Current->DefinitionId) : nullptr;
    Line(ComparisonRows,FText::FromString(bSelectedEquipped ? TEXT("현재 장착한 아이템입니다") : CurrentDef ? TEXT("비교 대상 · ")+CurrentDef->DisplayName.ToString() : TEXT("비교 대상 · 미장착")),Style.Muted,12);
    Line(ComparisonRows,FText::FromString(TEXT("최종 능력치 · 현재 → 교체 후 예상")),Style.Muted,12);
    TArray<FPGInventoryStatComparison> PreviewRows; FString PreviewReason;
    if (!PreviewEquip(Save->Items,Save->Equipment,*Profile->GetCatalog(),Item->Guid,
        EPGEquipmentSlot(SelectedEquipmentSlot),PreviewRows,PreviewReason))
        Line(ComparisonRows,FText::FromString(PreviewReason),Style.Danger,14);
    if (Def->EquipmentType==EPGEquipmentType::TwoHandSword && !bSelectedEquipped)
        Line(ComparisonRows,FText::FromString(TEXT("주무기와 보조무기를 함께 교체합니다")),Style.Muted,12);
    for (const auto& Row : PreviewRows)
    {
        ComparisonRows->AddSlot().AutoHeight().Padding(0,0,0,10)
        [SNew(SBorder).BorderImage(&Style.Card).Padding(10)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(StatName(Row.Stat),18,Style.Muted)]
                + SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)
                [SNew(STextBlock).Font(Style.Font(16,true)).ColorAndOpacity(Style.Text).AutoWrapText(true)
                    .Text_Lambda([this,Row]()
                    {
                        const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
                        auto* ASC=Player ? Player->GetPGAbilitySystemComponent() : nullptr;
                        const auto* CurrentProfile=UPGProfileSubsystem::Get(this);
                        const auto* CurrentSave=CurrentProfile ? CurrentProfile->GetProfile() : nullptr;
                        if (!ASC || !CurrentSave) return FText::FromString(TEXT("능력치를 불러오는 중입니다"));
                        const int32 Growth=CurrentProfile->GetCatalog()->LevelRules.InvestmentBonus(CurrentSave->Growth,Row.Stat);
                        const auto Details=ASC->GetStatDetails(Row.Stat,float(Row.Selected+CurrentSave->RewardBonuses.FindRef(Row.Stat)+Growth));
                        if (!Details.bValid) return FText::FromString(TEXT("교체 예상값을 확인할 수 없습니다"));
                        return FText::FromString(FString::Printf(TEXT("%s → %s   (%s)"),
                            *StatValue(Row.Stat,Details.Current).ToString(),*StatValue(Row.Stat,Details.Expected).ToString(),
                            *StatValue(Row.Stat,Details.Expected-Details.Current,true).ToString()));
                    })]]];
    }
    Line(ComparisonRows,FText::FromString(TEXT("현재 적용 중인 효과와 상한을 반영합니다. 조건부 피해·발동 효과는 별도이며, 버프가 종료되면 예상값도 달라집니다.")),Style.Muted,12);
    if (const auto* Effect=Profile->GetCatalog()->FindLegendary(Item->LegendaryEffect))
    {
        auto ExpectedEquipment=Save->Equipment; FString EffectReason;
        const bool PreviewValid=bSelectedEquipped || PGEquipmentRules::PlanEquip(*Profile->GetCatalog(),Save->Items,Save->Equipment,Item->Guid,EPGEquipmentSlot(SelectedEquipmentSlot),ExpectedEquipment,EffectReason);
        const auto Winners=PGLegendaryRules::Active(*Profile->GetCatalog(),Save->Items,ExpectedEquipment);
        const FString State=PreviewValid ? (Winners.FindRef(Effect->Id)==Item->Guid ? TEXT(" · 장착 시 활성") : TEXT(" · 중복 비활성")) : TEXT(" · 장착 불가");
        Line(ComparisonRows,FText::FromString(FString::Printf(TEXT("%s %d%%%s"),*Effect->DisplayName.ToString(),Item->LegendaryPercent,*State)),Style.Lavender,14,true);
        Line(ComparisonRows,Effect->Description,Style.Text);
    }
    if (!bSelectedEquipped && Current)
        if (const auto* Effect=Profile->GetCatalog()->FindLegendary(Current->LegendaryEffect))
        {
            Line(ComparisonRows,FText::FromString(FString::Printf(TEXT("현재 장비 · %s %d%%"),*Effect->DisplayName.ToString(),Current->LegendaryPercent)),Style.Muted,14,true);
            Line(ComparisonRows,Effect->Description,Style.Muted,12);
        }
    if (!Def->EffectDescription.IsEmpty())
    { Line(ComparisonRows,FText::FromString(TEXT("선택한 장비의 효과")),Style.Lavender,12,true); Line(ComparisonRows,Def->EffectDescription,Style.Text); }
    if (!bSelectedEquipped && CurrentDef && !CurrentDef->EffectDescription.IsEmpty())
    { Line(ComparisonRows,FText::FromString(TEXT("현재 장비의 효과")),Style.Muted,12,true); Line(ComparisonRows,CurrentDef->EffectDescription,Style.Muted); }
    if (PendingDiscard.IsValid()) Line(ComparisonRows,FText::FromString(TEXT("이 아이템을 영구 삭제합니다. 아래 버튼으로 확정하거나 Esc로 취소하세요.")),Style.Danger);
}
bool UPGUIInventory::EquipSelected()
{
    auto* Profile=UPGProfileSubsystem::Get(this);
    if (!Profile || !bKnownSelection || bSelectedEquipped || !bCanWrite) return false;
    PendingDiscard.Invalidate(); const bool Result=Profile->EquipToSlot(SelectedItem,EPGEquipmentSlot(SelectedEquipmentSlot)); bActionFailed=!Result; Refresh(); return Result;
}
bool UPGUIInventory::DiscardSelected()
{
    auto* Profile=UPGProfileSubsystem::Get(this);
    if (!Profile || !SelectedItem.IsValid() || bSelectedEquipped || bSelectedFavorite || !bCanWrite) return false;
    if (PendingDiscard!=SelectedItem) { PendingDiscard=SelectedItem; RefreshComparison(); return false; }
    const bool Result=Profile->Discard(SelectedItem); bActionFailed=!Result; PendingDiscard.Invalidate(); Refresh(); return Result;
}
FReply UPGUIInventory::Unequip(uint8 EquipmentSlot)
{
    if (auto* Profile=UPGProfileSubsystem::Get(this)) { bActionFailed=!Profile->Unequip(EPGEquipmentSlot(EquipmentSlot)); PendingDiscard.Invalidate(); Refresh(); }
    SetKeyboardFocus();
    return FReply::Handled();
}
FReply UPGUIInventory::RecoverSave()
{
    if (bConfirmRecovery) { if (auto* Profile=UPGProfileSubsystem::Get(this)) bActionFailed=!Profile->RecoverSave(); bConfirmRecovery=false; }
    else bConfirmRecovery=true;
    Refresh(); return FReply::Handled();
}
void UPGUIInventory::SetBuildTab(bool bBuild)
{
    CancelImprint(); CancelSalvage();
    bBuildTab=bBuild; PendingDiscard.Invalidate();
    if (Tabs) Tabs->SetActiveWidgetIndex(bBuild ? 1 : 0);
    if (bBuild) RefreshBuilds(); else RefreshComparison();
}
void UPGUIInventory::RefreshBuilds()
{
    using namespace PGInventoryUI;
    if (!BuildRows) return;
    const float Offset=BuildScroll->GetScrollOffset(); BuildRows->ClearChildren();
    if (!UPGProfileSubsystem::Get(this) || !UPGProfileSubsystem::Get(this)->GetProfile()) return;
    auto* Profile=UPGProfileSubsystem::Get(this); const auto* Save=Profile->GetProfile(); const auto* Catalog=Profile->GetCatalog(); const auto& Style=PGInventoryStyle();
    SkillListRows->ClearChildren(); SkillDetailRows->ClearChildren(); LoadoutRows->ClearChildren();
    FString Reason;
    const bool CanEdit=Profile->CanChangeSkills(Reason) && bCanWrite;
    const TArray<int32> EquippedSkills=GetEquippedActiveSkills();
    if (DraftActiveSkills.Num()!=PGPlayerSkillSlots::Count || !bDraftDirty) DraftActiveSkills=EquippedSkills;
    bDraftDirty=DraftActiveSkills!=EquippedSkills;
    if (!bDraftDirty) bConfirmClose=false;
    auto* TablesForSkills=UPGDataTableManager::Get(this);
    if (!Catalog->SelectableActiveSkills.Contains(PreviewSkill) && !Catalog->SelectableActiveSkills.IsEmpty()) PreviewSkill=Catalog->SelectableActiveSkills[0];
    for (int32 Id : Catalog->SelectableActiveSkills)
    {
        const auto* Row=TablesForSkills ? TablesForSkills->GetRowData<FPGSkillDataRow>(Id) : nullptr;
        if (!Row) continue;
        FString Name=Row->Desc;
        const int32 Break=Name.Find(TEXT("·"));
        if (Break!=INDEX_NONE) Name=Name.Left(Break).TrimEnd();
        const int32 AssignedSlot=DraftActiveSkills.Find(Id);
        const int32 EquippedSlot=EquippedSkills.Find(Id);
        FString Assignment;
        if (EquippedSlot!=INDEX_NONE) Assignment=FString::Printf(TEXT("슬롯 %d · 장착 중"),EquippedSlot+1);
        if (AssignedSlot!=INDEX_NONE && AssignedSlot!=EquippedSlot)
            Assignment+=(Assignment.IsEmpty() ? TEXT("") : TEXT("\n"))+FString::Printf(TEXT("슬롯 %d · 적용 예정"),AssignedSlot+1);
        else if (EquippedSlot!=INDEX_NONE && AssignedSlot==INDEX_NONE) Assignment+=TEXT("\n적용 시 해제");
        if (Assignment.IsEmpty()) Assignment=FString::Printf(TEXT("재사용 %d초"),Row->SkillCoolTime);
        auto Card=SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(8)
            .OnClicked_Lambda([this,Id](){PreviewSkill=Id;RefreshBuilds();if (BuildScroll) BuildScroll->ScrollDescendantIntoView(SkillDetailRows);SetKeyboardFocus();return FReply::Handled();})
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(SBox).WidthOverride(48).HeightOverride(48)[SNew(SImage).Image(Art(Row->SkillIconPath))]]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(12,0,0,0)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[Text(Name,16,Style.Text,true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)
                    [Text(Assignment,12,AssignedSlot!=INDEX_NONE ? Style.Mint : Style.Muted)]]];
        SkillListRows->AddSlot().AutoHeight().Padding(0,0,0,8)
            [SNew(SBorder).BorderImage(PreviewSkill==Id ? &Style.Selected : &Style.Card).Padding(2)[Card]];
    }
    Line(SkillListRows,FText::FromString(TEXT("검술 프리셋")),Style.Lavender,14,true);
    for (const auto& Build : Catalog->Builds)
    {
        const bool Unlocked=Save->ClearedStages>=Build.RequiredClears;
        const bool CanChange=!Catalog->bRoguelikeRuns || (Save->Checkpoint<=1 && Save->SelectedRewards.IsEmpty());
        const bool Current=Build.Id==Save->BuildId && Save->CustomActiveSkills.IsEmpty();
        auto Button=Action(Build.DisplayName.ToString(),[this,Id=Build.Id]()
        { if (auto* P=UPGProfileSubsystem::Get(this)) bActionFailed=!P->SelectBuild(Id); if (!bActionFailed) {DraftActiveSkills.Reset();bDraftDirty=false;bConfirmClose=false;} Refresh();return FReply::Handled(); });
        Button->SetEnabled(CanEdit && Unlocked && CanChange && !Current && !bDraftDirty);
        Button->SetToolTipText(FText::FromString(!Unlocked ? FString::Printf(TEXT("클리어 %d회 필요"),Build.RequiredClears) : bDraftDirty ? TEXT("먼저 변경 중인 스킬을 적용하거나 되돌려 주세요.") : !CanChange ? TEXT("다음 도전 시작 전에 변경할 수 있습니다.") : Current ? TEXT("사용 중인 프리셋") : TEXT("이 검술 프리셋을 즉시 적용합니다.")));
        SkillListRows->AddSlot().AutoHeight().Padding(0,0,0,6)[Button];
        if (!Unlocked || !CanChange) Line(SkillListRows,FText::FromString(!Unlocked ? FString::Printf(TEXT("클리어 %d회 필요"),Build.RequiredClears) : TEXT("다음 도전에서 변경")),Style.Muted,12);
    }
    if (const auto* Row=TablesForSkills ? TablesForSkills->GetRowData<FPGSkillDataRow>(PreviewSkill) : nullptr)
    {
        Line(SkillDetailRows,FText::FromString(TEXT("검술 상세")),Style.Lavender,12,true);
        SkillDetailRows->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,16,0,24)
            [SNew(SBox).WidthOverride(128).HeightOverride(128)[SNew(SImage).Image(Art(Row->SkillIconPath))]];
        const auto View=PGPlayerSkillText::MakeView(*Row);
        Line(SkillDetailRows,FText::FromString(View.Name),Style.Text,26,true);
        Line(SkillDetailRows,FText::FromString(View.Description),Style.Text,17);
        SkillDetailRows->AddSlot().AutoHeight().Padding(0,12,0,8)
            [SNew(SBorder).BorderImage(&Style.Panel).Padding(14)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[Text(View.Damage,17,Style.Mint,true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[Text(View.Cooldown,16,Style.Muted)]]];
        auto Choices=SNew(SVerticalBox);
        for (int32 I=0;I<PGPlayerSkillSlots::Count;++I)
        {
            const bool Selected=DraftActiveSkills.IsValidIndex(I) && DraftActiveSkills[I]==PreviewSkill;
            auto Button=Action(FString::Printf(TEXT("슬롯 %d%s"),I+1,Selected ? TEXT(" 선택됨") : TEXT("에 배치")),[this,I]()
            {
                if (DraftActiveSkills.Num()==PGPlayerSkillSlots::Count)
                {
                    const int32 Other=DraftActiveSkills.Find(PreviewSkill);
                    if (Other!=INDEX_NONE) Swap(DraftActiveSkills[I],DraftActiveSkills[Other]);
                    else DraftActiveSkills[I]=PreviewSkill;
                    bDraftDirty=true; bConfirmClose=false;
                }
                RefreshBuilds(); SetKeyboardFocus(); return FReply::Handled();
            });
            Button->SetEnabled(CanEdit && !Selected);
            Choices->AddSlot().AutoHeight().Padding(0,I==0 ? 0 : 6,0,0)[Button];
        }
        SkillDetailRows->AddSlot().AutoHeight().Padding(0,20,0,12)[Choices];
        Line(SkillDetailRows,FText::FromString(TEXT("다른 슬롯의 스킬을 선택하면 위치를 서로 바꿉니다.")),Style.Muted,13);
    }
    Line(LoadoutRows,FText::FromString(bDraftDirty ? TEXT("변경할 검술") : TEXT("장착한 검술")),Style.Text,18,true);
    for (int32 I=0;I<DraftActiveSkills.Num();++I)
    {
        const auto* Row=TablesForSkills ? TablesForSkills->GetRowData<FPGSkillDataRow>(DraftActiveSkills[I]) : nullptr;
        FString Name=Row ? Row->Desc : TEXT("미선택");
        const int32 Separator=Name.Find(TEXT("·"));
        if (Separator!=INDEX_NONE) Name=Name.Left(Separator).TrimEnd();
        LoadoutRows->AddSlot().AutoHeight().Padding(0,0,0,8)
            [SNew(SBorder).BorderImage(&Style.Selected).Padding(10)
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(40).HeightOverride(40)[SNew(SImage).Image(Row ? Art(Row->SkillIconPath) : nullptr)]]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(12,0,0,0)
                    [Text(FString::Printf(TEXT("%d  /  %s"),I+1,*Name),14,Style.Text,true)]]];
    }
    Line(LoadoutRows,FText::FromString(CanEdit ? bDraftDirty ? TEXT("변경한 검술을 적용해 주세요.") : TEXT("현재 장착 구성입니다.") : Reason),CanEdit ? Style.Muted : Style.Rare,13);
    auto Apply=Action(bDraftDirty ? TEXT("변경 적용 · 저장  →") : TEXT("장착 완료"),[this](){return ApplySkills();});
    Apply->SetEnabled(CanEdit && bDraftDirty && Catalog->IsValidActiveSelection(DraftActiveSkills));
    LoadoutRows->AddSlot().AutoHeight()[Apply];
    if (bDraftDirty)
        LoadoutRows->AddSlot().AutoHeight().Padding(0,6,0,0)[Action(TEXT("변경 되돌리기"),[this](){bDraftDirty=false;bConfirmClose=false;DraftActiveSkills.Reset();RefreshBuilds();SetKeyboardFocus();return FReply::Handled();})];
    Line(BuildRows,FText::FromString(TEXT("이번 도전의 힘")),Style.Text,20,true);
    const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
    const auto* ASC=Player ? Player->GetPGAbilitySystemComponent() : nullptr;
    const auto* Tuning=ASC && ASC->CombatTuning ? ASC->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    const EPGCombatPerk Roots[]={EPGCombatPerk::Bleed,EPGCombatPerk::Shockwave,EPGCombatPerk::Frenzy};
    int32 ActiveFamilies=0;
    for (int32 I=0;I<3;++I)
    {
        const int32 Value=Profile->GetEffectivePerk(Roots[I]);
        if (Value<=0) continue;
        ++ActiveFamilies;
        Line(BuildRows,FText::FromString(PGRewardText::PerkName(Roots[I])),Style.Mint,17,true);
        Line(BuildRows,FText::FromString(PGRewardText::Effect(Roots[I],Value,*Tuning)),Style.Muted,14);
    }
    if (ActiveFamilies==0)
    {
        Line(BuildRows,FText::FromString(TEXT("새로운 힘을 얻을 차례입니다")),Style.Lavender,16,true);
        Line(BuildRows,FText::FromString(TEXT("시련을 돌파하면 강화를 선택할 수 있습니다. 출혈, 충격파, 격분 중 원하는 전투 방식을 키워 보세요.")),Style.Muted,14);
    }
    TArray<int32> RewardIds; Save->SelectedRewards.GetKeys(RewardIds); RewardIds.Sort();
    if (!RewardIds.IsEmpty()) Line(BuildRows,FText::FromString(TEXT("선택한 강화")),Style.Text,18,true);
    auto* Tables=UPGDataTableManager::Get(this);
    for (int32 Id : RewardIds)
        if (const auto* Reward=Tables ? Tables->GetRowData<FPGRewardStatDataRow>(Id) : nullptr)
        {
            const FString Effect=Reward->Perk==EPGCombatPerk::None ? Reward->PlaystyleDescription.ToString() : PGRewardText::Effect(Reward->Perk,Profile->GetEffectivePerk(Reward->Perk),*Tuning);
            Line(BuildRows,FText::FromString(FString::Printf(TEXT("%s ×%d%s"),*Reward->DisplayName.ToString(),Save->SelectedRewards.FindRef(Id),Reward->bKeystone ? TEXT("  ·  핵심 강화") : TEXT(""))),Style.Lavender,0,true);
            Line(BuildRows,FText::FromString(Effect),Style.Muted);
        }
    if (Save->BestStage>0 || Save->CompletedRuns>0)
        Line(BuildRows,FText::FromString(FString::Printf(TEXT("최고 기록  %d구간  ·  돌파 %d회"),Save->BestStage,Save->CompletedRuns)),Style.Muted,12);
    BuildScroll->SetScrollOffset(Offset);
}
TArray<int32> UPGUIInventory::GetEquippedActiveSkills() const
{
    if (const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn()); Player && Player->GetSkillHandler())
    {
        TArray<int32> Skills;
        for (int32 Index=0;Index<PGPlayerSkillSlots::Count;++Index)
            if (const auto* Skill=Player->GetSkillHandler()->GetSkillData(PGPlayerSkillSlots::Get(Index))) Skills.Add(Skill->SkillId);
        if (Skills.Num()==PGPlayerSkillSlots::Count) return Skills;
    }
    const auto* Profile=UPGProfileSubsystem::Get(this);
    return Profile && Profile->GetProfile() && Profile->GetCatalog()
        ? Profile->GetCatalog()->ResolveActiveSkills(Profile->GetProfile()->CustomActiveSkills,Profile->GetProfile()->BuildId) : TArray<int32>();
}
FReply UPGUIInventory::ApplySkills()
{
    bActionFailed=true;
    if (auto* P=UPGProfileSubsystem::Get(this); P && DraftActiveSkills.Num()==PGPlayerSkillSlots::Count)
    {
        bActionFailed=!P->SelectActiveSkills(DraftActiveSkills);
        if (!bActionFailed) { bDraftDirty=false; bConfirmClose=false; }
    }
    Refresh(); SetKeyboardFocus(); return FReply::Handled();
}
void UPGUIInventory::RefreshNearby()
{
    using namespace PGInventoryUI;
    if (!NearbyRows) return;
    NearbyRows->ClearChildren(); auto* Profile=UPGProfileSubsystem::Get(this); if (!Profile || !Profile->GetCatalog()) return;
    const auto* Catalog=Profile->GetCatalog(); const auto& Style=PGInventoryStyle();
    Line(NearbyRows,FText::FromString(MinRarity==0 ? TEXT("표시: 전체") : MinRarity==1 ? TEXT("표시: 마법 이상") : TEXT("표시: 희귀")),Style.Mint,18,true);
    int32 Count=0;
    for (TActorIterator<APGLootDrop> It(GetWorld());It;++It)
    {
        if (!IsValid(*It) || !GetOwningPlayerPawn() || FVector::DistSquared(It->GetActorLocation(),GetOwningPlayerPawn()->GetActorLocation())>FMath::Square(Catalog->PickupRadius)) continue;
        const auto* Def=Catalog->FindItem(It->GetItem().DefinitionId);
        if (!Def || int32(Def->GetRarity(It->GetItem()))<MinRarity) continue;
        ++Count; const TWeakObjectPtr<APGLootDrop> Drop(*It);
        auto Button=Action(RarityName(Def->GetRarity(It->GetItem())).ToString()+TEXT(" · ")+Def->DisplayName.ToString()+TEXT("  /  획득"),[this,Drop]()
        { bActionFailed=!Drop.IsValid() || !Drop->TryPickup(GetOwningPlayerPawn());Refresh();SetKeyboardFocus();return FReply::Handled(); });
        Button->SetEnabled(bCanWrite); NearbyRows->AddSlot().AutoHeight().Padding(0,0,0,10)[Button];
    }
    if (Count==0) Line(NearbyRows,FText::FromString(TEXT("획득 범위에 해당 등급의 전리품이 없습니다.")),Style.Muted);
}


TArray<FGuid> UPGUIInventory::GetVisibleItems() const
{
    const auto* P=UPGProfileSubsystem::Get(this);
    return P && P->GetProfile() && P->GetCatalog() ? PGInventoryPresentation::BagItems(
        P->GetProfile()->Items,P->GetProfile()->Equipment,*P->GetCatalog(),BagFilter,BagSort) : TArray<FGuid>();
}
void UPGUIInventory::SetBagView(const FPGInventoryFilter& Filter, EPGInventorySort Sort)
{
    BagFilter=Filter;
    BagFilter.Slot=FMath::Clamp(BagFilter.Slot,-1,int32(EPGEquipmentSlot::Max)-1);
    BagFilter.Rarity=FMath::Clamp(BagFilter.Rarity,-1,int32(EPGItemRarity::MythicUnique));
    BagFilter.Quality=FMath::Clamp(BagFilter.Quality,-1,1); BagFilter.Mark=FMath::Clamp(BagFilter.Mark,0,2);
    BagSort=EPGInventorySort(FMath::Clamp(int32(Sort),0,3)); PendingDiscard.Invalidate(); Refresh();
}
FReply UPGUIInventory::MarkSelected(bool bFavorite)
{
    auto* P=UPGProfileSubsystem::Get(this);
    const auto* Item=P && P->GetProfile() ? P->GetProfile()->Items.FindByPredicate([this](const auto& I){return I.Guid==SelectedItem;}) : nullptr;
    if (Item) bActionFailed=!P->SetItemMark(SelectedItem,bFavorite && !Item->bFavorite,!bFavorite && !Item->bJunk);
    PendingDiscard.Invalidate(); Refresh(); return FReply::Handled();
}
void UPGUIInventory::ApplyPanelLayout()
{
    if (bCompact && bRightOpen && bStatsOpen)
    { if (bLastStats) bRightOpen=false; else bStatsOpen=false; }
}
void UPGUIInventory::ToggleRightPanel()
{
    if (bRightOpen) { Close(); return; }
    bRightOpen=true; bLastStats=false; ApplyPanelLayout();
}
void UPGUIInventory::ToggleStatsPanel()
{
    if (bStatsOpen && !GrowthDraft.IsEmpty()) { bConfirmGrowthClose=true; if (GrowthArea) GrowthArea->SetExpanded(true); if (StatsScroll) StatsScroll->ScrollToStart(); return; }
    bStatsOpen=!bStatsOpen; bLastStats=bStatsOpen;
    if (bStatsOpen) if (auto* Profile=UPGProfileSubsystem::Get(this))
    {
        Profile->AcknowledgeLevelNotice();
        if (GrowthArea && Profile->GetProfile() && Profile->GetProfile()->Growth.UnspentPoints>0) GrowthArea->SetExpanded(true);
    }
    ApplyPanelLayout();
    if (!bStatsOpen && !bRightOpen)
    {
        if (bDraftDirty) { bRightOpen=true; Close(); }
        else if (auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->CloseInventory();
    }
}
FReply UPGUIInventory::HandleEscape()
{
    if (CraftRevision>=0) { CancelCraft(); RefreshCraft(); return FReply::Handled(); }
    if (!ImprintEffect.IsNone()) { CancelImprint(); RefreshImprint(); return FReply::Handled(); }
    if (!SalvageTargets.IsEmpty()) { CancelSalvage(); RefreshSalvage(); return FReply::Handled(); }
    if (bConfirmGrowthClose) { bConfirmGrowthClose=false; return FReply::Handled(); }
    if (PendingDiscard.IsValid() || bConfirmRecovery || bConfirmClose)
    { PendingDiscard.Invalidate(); bConfirmRecovery=false; bConfirmClose=false; Refresh(); return FReply::Handled(); }
    if (bRightOpen && Tabs && Tabs->GetActiveWidgetIndex()==0 && EquipmentPages && EquipmentPages->GetActiveWidgetIndex()>=2)
    { EquipmentPages->SetActiveWidgetIndex(0); return FReply::Handled(); }
    if (bStatsOpen && (bLastStats || !bRightOpen)) { ToggleStatsPanel(); return FReply::Handled(); }
    return Close();
}

void UPGUIInventory::CancelCraft()
{
    CraftRecipe=NAME_None; CraftRevision=-1; CraftCost.Reset(); CraftResult=FPGItemInstance();
}

bool UPGUIInventory::PreviewCraft(FName Recipe)
{
    CancelImprint(); CancelSalvage(); PendingDiscard.Invalidate();
    auto* P=UPGProfileSubsystem::Get(this);
    if (!P || !P->GetProfile()) return false;
    bActionFailed=!P->PreviewCraft(SelectedItem,Recipe,CraftResult,CraftCost,P->Status);
    if (!bActionFailed) { CraftRecipe=Recipe; CraftRevision=P->GetProfile()->Revision; }
    EquipmentPages->SetActiveWidgetIndex(5); Refresh(); return !bActionFailed;
}

bool UPGUIInventory::ConfirmCraft()
{
    auto* P=UPGProfileSubsystem::Get(this);
    if (!P || CraftRevision<0) return false;
    bActionFailed=!P->Craft(CraftResult.Guid,CraftRecipe,CraftRevision,CraftResult,CraftCost);
    if (!bActionFailed) CancelCraft();
    Refresh(); return !bActionFailed;
}

void UPGUIInventory::RefreshCraft()
{
    using namespace PGInventoryUI;
    if (!CraftRows) return;
    CraftRows->ClearChildren();
    auto* P=UPGProfileSubsystem::Get(this);
    if (!P || !P->GetProfile() || !P->GetCatalog()) return;
    const auto* Save=P->GetProfile(); const auto* Catalog=P->GetCatalog(); const auto& Style=PGInventoryStyle();
    const auto* Item=Save->Items.FindByPredicate([this](const auto& I){return I.Guid==SelectedItem;});
    const auto* Def=Item ? Catalog->FindItem(Item->DefinitionId) : nullptr;
    Line(CraftRows,FText::FromString(TEXT("담금질 · 명품화")),Style.Text,18,true);
    if (!Def) { Line(CraftRows,FText::FromString(TEXT("강화할 장비를 먼저 선택하세요")),Style.Muted); return; }
    Line(CraftRows,FText::FromString(Def->DisplayName.ToString()+FString::Printf(TEXT(" · 명품화 %d단계"),Item->MasterworkLevel)),Style.Text,16,true);
    if (CraftRevision<0)
    {
        Line(CraftRows,FText::FromString(TEXT("담금질은 확정 옵션 1개를 부여·교체합니다. 명품화는 기본 성능만 강화합니다.")),Style.Muted,12);
        auto Choice=[&](FName Id,const FString& Label)
        {
            FPGItemInstance Result; TMap<FName,int64> Cost; FString Reason;
            const bool Available=P->PreviewCraft(Item->Guid,Id,Result,Cost,Reason);
            auto Button=Action(Label,[this,Id](){PreviewCraft(Id);return FReply::Handled();});
            Button->SetEnabled(Available && bCanEquip); CraftRows->AddSlot().AutoHeight().Padding(0,0,0,6)[Button];
            if (!Available) Line(CraftRows,FText::FromString(Reason),Style.Muted,12);
        };
        Choice(NAME_None,TEXT("다음 명품화 단계 미리보기"));
        for (const auto& Recipe : Catalog->TemperRecipes)
            if (Recipe.Slots.Contains(Def->Slot)) Choice(Recipe.Id,Recipe.DisplayName.ToString()+TEXT(" · 담금질 미리보기"));
        Line(CraftRows,FText::FromString(TEXT("분해 시 강화 투자 비용은 반환되지 않습니다.")),Style.Muted,12);
        return;
    }
    Line(CraftRows,FText::FromString(CraftRecipe.IsNone() ? FString::Printf(TEXT("명품화 %d → %d단계"),Item->MasterworkLevel,CraftResult.MasterworkLevel) : TEXT("담금질 교체 · 기존 담금질은 사라집니다")),Style.Mint,16,true);
    if (!CraftRecipe.IsNone())
    {
        if (!Item->Temper.Id.IsNone()) Line(CraftRows,FText::FromString(TEXT("현재 담금질: ")+StatName(Item->Temper.Stat).ToString()+TEXT(" ")+StatValue(Item->Temper.Stat,Item->Temper.Value).ToString()),Style.Muted,12);
        Line(CraftRows,FText::FromString(TEXT("새 담금질: ")+StatName(CraftResult.Temper.Stat).ToString()+TEXT(" ")+StatValue(CraftResult.Temper.Stat,CraftResult.Temper.Value).ToString()),Style.Mint,13);
    }
    Line(CraftRows,FText::FromString(TEXT("장비 능력치 합계 · 현재 → 강화 후")),Style.Muted,12);
    for (const auto& Row : Compare(CraftResult,Item))
        if (Row.Delta()!=0) Line(CraftRows,FText::FromString(StatName(Row.Stat).ToString()+TEXT(" ")+StatValue(Row.Stat,Row.Current).ToString()+TEXT(" → ")+StatValue(Row.Stat,Row.Selected).ToString()),Style.Text,14);
    TArray<FName> Keys; CraftCost.GetKeys(Keys); Keys.Sort(FNameLexicalLess());
    for (const auto Key : Keys)
        Line(CraftRows,FText::FromString(FString::Printf(TEXT("%s −%lld  (보유 %lld → %lld)"),*Catalog->SalvageRules.MaterialNames.FindRef(Key).ToString(),CraftCost[Key],Save->Materials.FindRef(Key),Save->Materials.FindRef(Key)-CraftCost[Key])),Style.Text,13);
    auto Confirm=Action(TEXT("강화 확정"),[this](){ConfirmCraft();return FReply::Handled();});
    Confirm->SetEnabled(bCanEquip); Confirm->SetTag(TEXT("PGInventory.CraftConfirm")); CraftRows->AddSlot().AutoHeight()[Confirm];
    CraftRows->AddSlot().AutoHeight().Padding(0,6)[Action(TEXT("취소"),[this](){CancelCraft();bRefreshPending=true;return FReply::Handled();})];
}

void UPGUIInventory::CancelSalvage()
{
    SalvageTargets.Reset(); SalvageRewards.Reset(); SalvageRevision=-1;
}
void UPGUIInventory::CancelImprint()
{
    CancelCraft();
    ImprintEffect=NAME_None; ImprintRevision=-1; ImprintCost.Reset(); ImprintResult=FPGItemInstance();
}
bool UPGUIInventory::PreviewImprint(FName Effect)
{
    CancelImprint(); CancelSalvage(); PendingDiscard.Invalidate();
    auto* P=UPGProfileSubsystem::Get(this);
    if (!P) return false;
    bActionFailed=!P->PreviewImprint(SelectedItem,Effect,ImprintResult,ImprintCost,P->Status);
    if (!bActionFailed) { ImprintEffect=Effect; ImprintRevision=P->GetProfile()->Revision; }
    if (EquipmentPages) EquipmentPages->SetActiveWidgetIndex(4);
    Refresh(); return !bActionFailed;
}
bool UPGUIInventory::ConfirmImprint()
{
    auto* P=UPGProfileSubsystem::Get(this);
    if (!P || ImprintEffect.IsNone()) return false;
    bActionFailed=!P->Imprint(ImprintResult.Guid,ImprintEffect,ImprintRevision,ImprintResult.LegendaryPercent,ImprintCost);
    if (!bActionFailed) CancelImprint();
    Refresh(); return !bActionFailed;
}
void UPGUIInventory::RefreshImprint()
{
    using namespace PGInventoryUI;
    if (!ImprintRows) return;
    ImprintRows->ClearChildren();
    auto* P=UPGProfileSubsystem::Get(this);
    if (!P || !P->GetProfile() || !P->GetCatalog()) return;
    const auto* Save=P->GetProfile(); const auto* Catalog=P->GetCatalog(); const auto& Style=PGInventoryStyle();
    const auto* Item=Save->Items.FindByPredicate([this](const auto& I){return I.Guid==SelectedItem;});
    const auto* Def=Item ? Catalog->FindItem(Item->DefinitionId) : nullptr;
    Line(ImprintRows,FText::FromString(TEXT("전설 각인")),Style.Text,18,true);
    Line(ImprintRows,FText::FromString(TEXT("전설 분해로 제작법 영구 해금 · 같은 효과는 가장 강한 하나만 활성화")),Style.Muted,12);
    if (!Def) { Line(ImprintRows,FText::FromString(TEXT("각인할 장비를 먼저 선택하세요")),Style.Muted); return; }
    Line(ImprintRows,Def->DisplayName,Style.Text,16,true);
    if (const auto* Old=Catalog->FindLegendary(Item->LegendaryEffect))
        Line(ImprintRows,FText::FromString(FString::Printf(TEXT("현재: %s %d%%"),*Old->DisplayName.ToString(),Item->LegendaryPercent)),Style.Muted,13);
    if (ImprintEffect.IsNone())
    {
        for (const auto& Effect : Catalog->LegendaryEffects)
        {
            const bool Unlocked=Save->LegendaryRecipes.Contains(Effect.Id);
            Line(ImprintRows,FText::FromString(Effect.DisplayName.ToString()+(Unlocked ? TEXT(" · 해금됨") : TEXT(" · 미해금"))),Style.Lavender,14,true);
            Line(ImprintRows,Effect.Description,Style.Text,12);
            TArray<FName> MaterialKeys; Effect.Cost.GetKeys(MaterialKeys); MaterialKeys.Sort(FNameLexicalLess());
            for (const FName Key : MaterialKeys)
                Line(ImprintRows,FText::FromString(FString::Printf(TEXT("%s %lld 필요 · 보유 %lld"),*Catalog->SalvageRules.MaterialNames.FindRef(Key).ToString(),Effect.Cost[Key],Save->Materials.FindRef(Key))),Style.Muted,12);
            FString Slots; TArray<EPGEquipmentSlot> Keys; Effect.SlotMultipliers.GetKeys(Keys); Keys.Sort();
            for (const auto EquipmentSlot : Keys) { if (!Slots.IsEmpty()) Slots+=TEXT(" · "); Slots+=SlotName(EquipmentSlot).ToString(); }
            Line(ImprintRows,FText::FromString(Slots),Style.Muted,12);
            FPGItemInstance Result; TMap<FName,int64> Cost; FString Reason;
            const bool Available=P->PreviewImprint(SelectedItem,Effect.Id,Result,Cost,Reason);
            auto Button=Action(TEXT("효과 · 비용 미리보기"),[this,Id=Effect.Id](){PreviewImprint(Id);return FReply::Handled();});
            Button->SetEnabled(Available && bCanEquip); ImprintRows->AddSlot().AutoHeight().Padding(0,0,0,10)[Button];
            if (!Available) Line(ImprintRows,FText::FromString(Reason),Style.Muted,12);
        }
        return;
    }
    const auto* Effect=Catalog->FindLegendary(ImprintEffect);
    if (!Effect) return;
    Line(ImprintRows,FText::FromString(FString::Printf(TEXT("각인 후: 전설 · %s %d%%"),*Effect->DisplayName.ToString(),ImprintResult.LegendaryPercent)),Style.Mint,16,true);
    Line(ImprintRows,Effect->Description,Style.Text,13);
    if (Save->Equipment.FindKey(ImprintResult.Guid))
    {
        auto ExpectedItems=Save->Items;
        *ExpectedItems.FindByPredicate([this](const auto& I){return I.Guid==ImprintResult.Guid;})=ImprintResult;
        const bool Active=PGLegendaryRules::Active(*Catalog,ExpectedItems,Save->Equipment).FindRef(ImprintEffect)==ImprintResult.Guid;
        Line(ImprintRows,FText::FromString(Active ? TEXT("각인 후 이 장비의 효과가 활성화됩니다") : TEXT("다른 장비의 같은 효과가 우선되어 이 효과는 비활성입니다")),Active ? Style.Mint : Style.Danger,12);
    }
    Line(ImprintRows,FText::FromString(TEXT("기존 효과는 교체됩니다. 위력·품질·옵션은 유지됩니다.")),Style.Muted,12);
    TArray<FName> Keys; ImprintCost.GetKeys(Keys); Keys.Sort(FNameLexicalLess());
    for (const FName Key : Keys)
        Line(ImprintRows,FText::FromString(FString::Printf(TEXT("%s −%lld  (보유 %lld → %lld)"),*Catalog->SalvageRules.MaterialNames.FindRef(Key).ToString(),ImprintCost[Key],Save->Materials.FindRef(Key),Save->Materials.FindRef(Key)-ImprintCost[Key])),Style.Text,13);
    auto Confirm=Action(TEXT("각인 확정"),[this](){ConfirmImprint();return FReply::Handled();});
    Confirm->SetEnabled(bCanEquip); Confirm->SetTag(TEXT("PGInventory.ImprintConfirm")); ImprintRows->AddSlot().AutoHeight()[Confirm];
    ImprintRows->AddSlot().AutoHeight().Padding(0,6)[Action(TEXT("취소"),[this](){CancelImprint();bRefreshPending=true;return FReply::Handled();})];
}
bool UPGUIInventory::PreviewSalvage(bool bJunkOnly)
{
    CancelImprint(); CancelSalvage(); PendingDiscard.Invalidate();
    auto* P=UPGProfileSubsystem::Get(this);
    if (!P || !P->GetProfile() || !P->CanChangeSkills(P->Status)) { bActionFailed=true; Refresh(); return false; }
    TArray<FGuid> Targets;
    if (bJunkOnly)
    {
        for (const auto& Item : P->GetProfile()->Items)
            if (Item.bJunk && !Item.bFavorite && !P->GetProfile()->Equipment.FindKey(Item.Guid)) Targets.Add(Item.Guid);
    }
    else Targets.Add(SelectedItem);
    bActionFailed=!P->PreviewSalvage(Targets,bJunkOnly,SalvageRewards,P->Status);
    if (!bActionFailed)
    {
        SalvageTargets=MoveTemp(Targets); SalvageRevision=P->GetProfile()->Revision; bSalvageJunkOnly=bJunkOnly;
        Tabs->SetActiveWidgetIndex(0); EquipmentPages->SetActiveWidgetIndex(3);
    }
    Refresh(); return !bActionFailed;
}
bool UPGUIInventory::ConfirmSalvage()
{
    auto* P=UPGProfileSubsystem::Get(this);
    if (!P || SalvageTargets.IsEmpty()) return false;
    bActionFailed=!P->Salvage(SalvageTargets,bSalvageJunkOnly,SalvageRevision,SalvageRewards);
    if (!bActionFailed) CancelSalvage();
    bRefreshPending=true; return !bActionFailed;
}
void UPGUIInventory::RefreshSalvage()
{
    using namespace PGInventoryUI;
    if (!SalvageRows) return;
    SalvageRows->ClearChildren();
    const auto* P=UPGProfileSubsystem::Get(this);
    if (!P || !P->GetCatalog() || !P->GetProfile()) return;
    const auto& Style=PGInventoryStyle();
    const auto& Names=P->GetCatalog()->SalvageRules.MaterialNames;
    TArray<FName> Keys; Names.GetKeys(Keys); Keys.Sort(FNameLexicalLess());
    if (SalvageTargets.IsEmpty())
    {
        Line(SalvageRows,FText::FromString(TEXT("보유 재료")),Style.Text,18,true);
        for (const auto Key : Keys)
            Line(SalvageRows,FText::FromString(FString::Printf(TEXT("%s  %lld"),*Names[Key].ToString(),P->GetProfile()->Materials.FindRef(Key))),Style.Text);
        Line(SalvageRows,FText::FromString(TEXT("재료는 가방 칸을 차지하지 않습니다. 장착·즐겨찾기 장비는 보호됩니다.")),Style.Muted,12);
        if (P->GetCatalog()->bRoguelikeRuns)
            Line(SalvageRows,FText::FromString(TEXT("이번 도전 전용 · 새 도전에서 재료 초기화")),Style.Muted,12);
        auto Preview=Action(TEXT("모든 잡동사니 분해 미리보기"),[this](){PreviewSalvage(true);return FReply::Handled();});
        Preview->SetTag(TEXT("PGInventory.SalvagePreview"));
        Preview->SetEnabled(bCanEquip);
        SalvageRows->AddSlot().AutoHeight()[Preview];
        Line(SalvageRows,FText::FromString(TEXT("가방 필터와 관계없이 잡동사니로 표시한 모든 장비를 확인합니다.")),Style.Muted,12);
        return;
    }
    Line(SalvageRows,FText::FromString(FString::Printf(TEXT("분해 대상 %d개 · 되돌릴 수 없습니다"),SalvageTargets.Num())),Style.Danger,16,true);
    for (const FGuid Guid : SalvageTargets)
    {
        const auto* Item=P->GetProfile()->Items.FindByPredicate([Guid](const auto& I){return I.Guid==Guid;});
        const auto* Def=Item ? P->GetCatalog()->FindItem(Item->DefinitionId) : nullptr;
        if (Item && !Item->LegendaryEffect.IsNone())
            if (const auto* Effect=P->GetCatalog()->FindLegendary(Item->LegendaryEffect))
                Line(SalvageRows,FText::FromString(Effect->DisplayName.ToString()+(P->GetProfile()->LegendaryRecipes.Contains(Effect->Id) ? TEXT(" · 제작법 보유") : TEXT(" · 제작법 영구 해금 예정"))),Style.Mint,12);
        if (Def) Line(SalvageRows,FText::FromString(Def->DisplayName.ToString()+TEXT(" · ")+RarityName(Item->Rarity).ToString()),Style.Text,13);
    }
    Line(SalvageRows,FText::FromString(TEXT("획득 예정 재료")),Style.Text,16,true);
    for (const auto Key : Keys)
        if (const auto* Amount=SalvageRewards.Find(Key))
            Line(SalvageRows,FText::FromString(FString::Printf(TEXT("%s +%lld  (보유 %lld)"),*Names[Key].ToString(),*Amount,P->GetProfile()->Materials.FindRef(Key))),Style.Mint);
    auto Confirm=Action(TEXT("분해 확정"),[this](){ConfirmSalvage();return FReply::Handled();});
    Confirm->SetTag(TEXT("PGInventory.SalvageConfirm")); Confirm->SetEnabled(bCanEquip);
    SalvageRows->AddSlot().AutoHeight()[Confirm];
    SalvageRows->AddSlot().AutoHeight().Padding(0,6)[Action(TEXT("취소"),[this](){CancelSalvage();bRefreshPending=true;return FReply::Handled();})];
}
