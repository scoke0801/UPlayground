#include "PGUIInventory.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "PGInventoryPresentation.h"
#include "PGUI/Style/PGUIStyle.h"
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
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

struct FPGInventoryCellView
{
    FGuid Guid;
    int32 DefinitionId = INDEX_NONE;
    bool bEquipped = false;
    FSlateBrush Brush;
    TSharedPtr<SButton> Button;
    TSharedPtr<STextBlock> Name, Badge;
};

namespace PGInventoryUI
{
    using namespace PGInventoryPresentation;
    TSharedRef<STextBlock> Text(const FText& Value, int32 Size = 0, FLinearColor Color = FLinearColor::White, bool bBold = false)
    {
        return SNew(STextBlock).Text(Value).Font(FPGUIStyle::Get().Font(Size>0 ? Size : 0,bBold)).ColorAndOpacity(Color).AutoWrapText(true);
    }
    TSharedRef<STextBlock> Text(const FString& Value, int32 Size = 0, FLinearColor Color = FLinearColor::White, bool bBold = false)
    { return Text(FText::FromString(Value),Size,Color,bBold); }
    TSharedRef<SButton> Action(const FString& Label, TFunction<FReply()> Callback)
    {
        return SNew(SButton).ButtonStyle(&FPGUIStyle::Get().Button).ContentPadding(FMargin(14,10))
            .OnClicked_Lambda(MoveTemp(Callback))
            [SNew(STextBlock).Text(FText::FromString(Label)).Font(FPGUIStyle::Get().Font(0,true))
                .ColorAndOpacity(FPGUIStyle::Get().Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)];
    }
    void Line(const TSharedPtr<SVerticalBox>& Rows, const FText& Value, FLinearColor Color, int32 Size = 0, bool bBold = false)
    { Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[Text(Value,Size,Color,bBold)]; }
}

UPGUIInventory::UPGUIInventory(const FObjectInitializer& Initializer) : Super(Initializer) { SetIsFocusable(true); }

TSharedRef<SWidget> UPGUIInventory::RebuildWidget()
{
    using namespace PGInventoryUI;
    const auto& Style=FPGUIStyle::Get();
    auto Equipment=SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("장착 장비"),18,Style.Text,true)]
        + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(EquippedRows,SVerticalBox)]];
    auto Bag=SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("가방"),18,Style.Text,true)]
            + SHorizontalBox::Slot().AutoWidth()[SAssignNew(CapacityText,STextBlock).Font(Style.Font()).ColorAndOpacity(Style.Muted)]]
        + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(BagGrid,SUniformGridPanel).SlotPadding(FMargin(3))]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)[Text(TEXT("아이템 선택 → 오른쪽에서 비교 · 장착"),12,Style.Muted)];
    auto Comparison=SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[Text(TEXT("아이템 비교"),18,Style.Text,true)]
        + SVerticalBox::Slot().FillHeight(1)[SAssignNew(ComparisonScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(ComparisonRows,SVerticalBox)]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
        [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(14,12))
            .IsEnabled_Lambda([this](){return bKnownSelection && !bSelectedEquipped && bCanWrite;})
            .OnClicked_Lambda([this](){EquipSelected();return FReply::Handled();})
            [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(bSelectedEquipped ? TEXT("장착 중") : TEXT("선택한 아이템 장착"));})
                .Font(Style.Font(0,true)).ColorAndOpacity(Style.Mint)]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
        [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(14,10))
            .IsEnabled_Lambda([this](){return SelectedItem.IsValid() && !bSelectedEquipped && bCanWrite;})
            .OnClicked_Lambda([this](){DiscardSelected();return FReply::Handled();})
            [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(PendingDiscard.IsValid() ? TEXT("버리기 확정 · 영구 삭제") : TEXT("아이템 버리기"));})
                .Font(Style.Font()).ColorAndOpacity(Style.Danger)]];
    auto EquipmentTab=SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(.24f).Padding(0,0,10,0)[SNew(SBorder).BorderImage(&Style.Card).Padding(16)[Equipment]]
        + SHorizontalBox::Slot().FillWidth(.36f).Padding(0,0,10,0)[SNew(SBorder).BorderImage(&Style.Card).Padding(16)[Bag]]
        + SHorizontalBox::Slot().FillWidth(.40f)[SNew(SBorder).BorderImage(&Style.Card).Padding(18)[Comparison]];
    auto BuildTab=SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(.28f).Padding(0,0,14,0)
        [SNew(SBorder).BorderImage(&Style.Card).Padding(18)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[Text(TEXT("01  /  검술 라이브러리"),18,Style.Text,true)]
                + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(SkillListRows,SVerticalBox)]]]]
        + SHorizontalBox::Slot().FillWidth(.39f).Padding(0,0,14,0)
        [SNew(SBorder).BorderImage(&Style.Card).Padding(22)
            [SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(SkillDetailRows,SVerticalBox)]]]
        + SHorizontalBox::Slot().FillWidth(.33f)
        [SNew(SBorder).BorderImage(&Style.Card).Padding(18)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SAssignNew(LoadoutRows,SVerticalBox)]
                + SVerticalBox::Slot().FillHeight(1).Padding(0,18,0,0)
                [SAssignNew(BuildScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(BuildRows,SVerticalBox)]]]];
    auto CharacterTab=SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(.57f).Padding(0,0,20,0)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[SAssignNew(CharacterRows,SVerticalBox)]
            + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()
                [SAssignNew(CharacterGrid,SUniformGridPanel).SlotPadding(FMargin(6))]]]
        + SHorizontalBox::Slot().FillWidth(.43f)
        [SNew(SBorder).BorderImage(&Style.Card).Padding(16)[SAssignNew(CharacterDetailRows,SVerticalBox)]];
    auto NearbyTab=SNew(SBorder).BorderImage(&Style.Card).Padding(20)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
            [Action(TEXT("등급 필터 변경 · 전체 / 마법 이상 / 희귀"),[this](){MinRarity=(MinRarity+1)%3;RefreshNearby();return FReply::Handled();})]
            + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(NearbyRows,SVerticalBox)]]];
    auto Root=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.004f,.008f,.018f,.85f)).Padding(0)
        [SNew(SSafeZone)
            [SNew(SOverlay)
                + SOverlay::Slot()[SNew(SBox).Clipping(EWidgetClipping::ClipToBounds).Visibility(EVisibility::HitTestInvisible)
                    [SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
                        [SNew(SImage).Image(Art(GetDefault<UPGUIStyleSettings>()->SanctuaryBackground.ToSoftObjectPath()))]]]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Fill).Padding(GetDefault<UPGUIStyleSettings>()->ScreenMargin)
                [SAssignNew(Frame,SBox).WidthOverride(GetDefault<UPGUIStyleSettings>()->InventoryMaxWidth)
                    [SNew(SBorder).BorderImage(&Style.Panel).BorderBackgroundColor(FLinearColor(1,1,1,.72f)).Padding(26)
                        [SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                            [SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1)
                                [SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight()[Text(TEXT("M O O N L I T   /   S A N C T U A R Y"),12,Style.Mint,true)]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)[Text(TEXT("전투 준비"),30,Style.Text,true)]]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Action(TEXT("닫기  ·  I / Esc"),[this](){return Close();})]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                            [SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
                                [SNew(SBorder).BorderImage_Lambda([this](){return Tabs && Tabs->GetActiveWidgetIndex()==0 ? &FPGUIStyle::Get().Selected : &FPGUIStyle::Get().Card;}).Padding(1)
                                    [Action(TEXT("01   장비"),[this](){SetBuildTab(false);return FReply::Handled();})]]
                                + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
                                [SNew(SBorder).BorderImage_Lambda([this](){return bBuildTab ? &FPGUIStyle::Get().Selected : &FPGUIStyle::Get().Card;}).Padding(1)
                                    [Action(TEXT("02   검술 · 강화"),[this](){SetBuildTab(true);return FReply::Handled();})]]
                                + SHorizontalBox::Slot().AutoWidth()
                                [SNew(SBorder).BorderImage_Lambda([this](){return Tabs && Tabs->GetActiveWidgetIndex()==2 ? &FPGUIStyle::Get().Selected : &FPGUIStyle::Get().Card;}).Padding(1)
                                    [Action(TEXT("03   전리품"),[this](){bBuildTab=false;PendingDiscard.Invalidate();Tabs->SetActiveWidgetIndex(2);RefreshNearby();return FReply::Handled();})]]
                                + SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)
                                [SNew(SBorder).BorderImage_Lambda([this](){return Tabs && Tabs->GetActiveWidgetIndex()==3 ? &FPGUIStyle::Get().Selected : &FPGUIStyle::Get().Card;}).Padding(1)
                                    [Action(TEXT("04   캐릭터"),[this](){SetCharacterTab();return FReply::Handled();})]]]
                            + SVerticalBox::Slot().FillHeight(1)
                            [SAssignNew(Tabs,SWidgetSwitcher)
                                + SWidgetSwitcher::Slot()[EquipmentTab]
                                + SWidgetSwitcher::Slot()[BuildTab]
                                + SWidgetSwitcher::Slot()[NearbyTab]
                                + SWidgetSwitcher::Slot()[CharacterTab]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
                            [SNew(SBorder).BorderImage(&Style.Selected).Padding(10)
                                .Visibility_Lambda([this](){return bConfirmClose ? EVisibility::Visible : EVisibility::Collapsed;})
                                [SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("적용하지 않은 검술 변경이 있습니다."),16,Style.Text)]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0)
                                    [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(14,10))
                .IsEnabled_Lambda([this](){FString Reason; auto* P=UPGProfileSubsystem::Get(this); return P && P->CanChangeSkills(Reason) && P->GetCatalog()->IsValidActiveSelection(DraftActiveSkills);})
                                        .OnClicked_Lambda([this](){ApplySkills(); if (!bActionFailed && !bDraftDirty) return Close(); return FReply::Handled();})
                                        [SNew(STextBlock).Text(FText::FromString(TEXT("적용 후 닫기"))).Font(Style.Font(16,true)).ColorAndOpacity(Style.Mint)]]
                                    + SHorizontalBox::Slot().AutoWidth()[Action(TEXT("계속 편집"),[this](){bConfirmClose=false;return FReply::Handled();})]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(10,0)[Action(TEXT("변경 취소 후 닫기"),[this](){bDraftDirty=false;return Close();})]]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
                            [SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                                [SAssignNew(StatusText,STextBlock).Font(Style.Font(16)).ColorAndOpacity(Style.Muted).AutoWrapText(true)]
                                + SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)
                                [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(12,8))
                                    .Visibility_Lambda([this](){return BoundProfile.IsValid() && BoundProfile->IsSaveBlocked() ? EVisibility::Visible : EVisibility::Collapsed;})
                                    .OnClicked_UObject(this,&ThisClass::RecoverSave)
                                    [SNew(STextBlock).Font(Style.Font(16)).Text_Lambda([this](){return FText::FromString(bConfirmRecovery ? TEXT("복구 확정 · Esc 취소") : TEXT("저장 복구"));})]]]
                        ]]]]];
    Refresh(); return Root;
}

void UPGUIInventory::NativeConstruct()
{
    Super::NativeConstruct(); BoundProfile=UPGProfileSubsystem::Get(this);
    if (BoundProfile.IsValid()) ProfileChangedHandle=BoundProfile->OnProfileChanged.AddUObject(this,&ThisClass::OnProfileChanged);
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
    const auto* Settings=GetDefault<UPGUIStyleSettings>();
    const float Width=FMath::Max(1.f,FMath::Min(Settings->InventoryMaxWidth,Geometry.GetLocalSize().X-Settings->ScreenMargin*2));
    if (Frame && !FMath::IsNearlyEqual(Width,LastWidth)) { Frame->SetWidthOverride(Width); LastWidth=Width; }
    if (bRefreshPending) Refresh();
}
void UPGUIInventory::OnProfileChanged() { bRefreshPending=true; }
FReply UPGUIInventory::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if (Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::I)
    {
        if (Event.IsRepeat()) return FReply::Handled();
        if (Event.GetKey()==EKeys::Escape && (PendingDiscard.IsValid() || bConfirmRecovery))
        { PendingDiscard.Invalidate(); bConfirmRecovery=false; Refresh(); return FReply::Handled(); }
        return Close();
    }
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
FReply UPGUIInventory::Close()
{
    if (bDraftDirty) { bConfirmClose=true; SetKeyboardFocus(); return FReply::Handled(); }
    if (auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->CloseInventory();
    return FReply::Handled();
}
void UPGUIInventory::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Cells.Empty(); BagGrid.Reset(); EquippedRows.Reset(); ComparisonRows.Reset(); BuildRows.Reset(); NearbyRows.Reset();
    CharacterRows.Reset(); CharacterGrid.Reset(); CharacterDetailRows.Reset();
    SkillListRows.Reset(); SkillDetailRows.Reset(); LoadoutRows.Reset();
    ComparisonScroll.Reset(); BuildScroll.Reset(); Tabs.Reset(); Frame.Reset(); CapacityText.Reset(); StatusText.Reset(); IconTextures.Empty(); ArtBrushes.Empty(); ArtTextures.Empty();
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
    if (!Save->Items.ContainsByPredicate([this](const auto& Item){return Item.Guid==SelectedItem;}))
        SelectedItem=Save->Items.IsEmpty() ? FGuid() : Save->Items[0].Guid;
    const FString Policy=Profile->GetCatalog()->bRoguelikeRuns ? TEXT("장비·강화는 이번 도전 전용") : TEXT("획득·장착 즉시 저장");
    StatusText->SetText(FText::FromString(bConfirmRecovery ? TEXT("원본을 보관하고 복구합니다. 유효한 백업이 없으면 빈 프로필로 시작합니다. Esc로 취소할 수 있습니다.") : Policy+TEXT("  ·  ")+Profile->Status));
    StatusText->SetColorAndOpacity(bActionFailed || Profile->IsSaveBlocked() ? FPGUIStyle::Get().Danger : FPGUIStyle::Get().Muted);
    RefreshBag(); RefreshEquipment(); RefreshComparison(); RefreshBuilds(); RefreshNearby(); RefreshCharacters();
}
void UPGUIInventory::SetCharacterTab()
{
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
    const auto& Style=FPGUIStyle::Get();
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
        CharacterGrid->AddSlot(Index%4,Index/4)
        [SNew(SBorder).BorderImage(Selected ? &Style.Selected : &Style.Card).Padding(2)
            [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(0)
                .OnClicked_Lambda([this,Id](){PreviewCharacter=Id;RefreshCharacters();SetKeyboardFocus();return FReply::Handled();})
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
    CharacterDetailRows->AddSlot().FillHeight(1)
        [SNew(SBox).Clipping(EWidgetClipping::ClipToBounds)
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
    const auto* Save=Profile->GetProfile(); const auto* Catalog=Profile->GetCatalog(); const auto& Style=FPGUIStyle::Get();
    CapacityText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),Save->Items.Num(),Catalog->BagCapacity)));
    CapacityText->SetColorAndOpacity(Save->Items.Num()>=Catalog->BagCapacity ? Style.Danger : Style.Muted);
    const int32 Count=FMath::Max(Catalog->BagCapacity,Save->Items.Num());
    if (Cells.Num()!=Count)
    {
        BagGrid->ClearChildren(); Cells.Empty();
        for (int32 Index=0;Index<Count;++Index)
        {
            auto Cell=MakeShared<FPGInventoryCellView>(); Cells.Add(Cell);
            BagGrid->AddSlot(Index%4,Index/4)
            [SNew(SBox).HeightOverride(124)
                [SNew(SBorder).Padding(2).BorderImage_Lambda([this,Index](){return Cells.IsValidIndex(Index) && Cells[Index]->Guid.IsValid() && Cells[Index]->Guid==SelectedItem ? &FPGUIStyle::Get().Selected : &FPGUIStyle::Get().Card;})
                    [SAssignNew(Cell->Button,SButton).ButtonStyle(&Style.Button).ContentPadding(6)
                        .OnClicked_Lambda([this,Index](){if(Cells.IsValidIndex(Index)) SelectItem(Cells[Index]->Guid);return FReply::Handled();})
                        [SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                            [SNew(SBox).WidthOverride(40).HeightOverride(40)[SNew(SImage).Image(&Cell->Brush)]]
                            + SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)
                            [SAssignNew(Cell->Name,STextBlock).Font(Style.Font(16)).Justification(ETextJustify::Center).AutoWrapText(true).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                            [SAssignNew(Cell->Badge,STextBlock).Font(Style.Font(14,true))]]]]];
        }
    }
    for (int32 Index=0;Index<Count;++Index)
    {
        auto& Cell=*Cells[Index];
        const auto* Item=Save->Items.IsValidIndex(Index) ? &Save->Items[Index] : nullptr;
        const auto* Def=Item ? Catalog->FindItem(Item->DefinitionId) : nullptr;
        Cell.Guid=Item ? Item->Guid : FGuid();
        const int32 DefinitionId=Item ? Item->DefinitionId : 0;
        const bool Equipped=Item && Save->Equipment.FindKey(Item->Guid)!=nullptr;
        if (Cell.DefinitionId==DefinitionId && Cell.bEquipped==Equipped) continue;
        Cell.DefinitionId=DefinitionId; Cell.bEquipped=Equipped;
        Cell.Button->SetEnabled(Item!=nullptr); Cell.Brush=FSlateBrush();
        auto* Icon=Def ? GetIcon(Def->Id) : nullptr;
        Cell.Brush.SetResourceObject(Icon); Cell.Brush.ImageSize=FVector2D(40,40);
        Cell.Brush.DrawAs=Icon ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
        if (Def && Def->IconPanel>=0 && Def->IconPanel<3) Cell.Brush.SetUVRegion(FBox2f(FVector2f(Def->IconPanel/3.f,0),FVector2f((Def->IconPanel+1)/3.f,1)));
        Cell.Name->SetText(Def ? Def->DisplayName : FText::FromString(Item ? TEXT("알 수 없는 장비") : TEXT("빈 슬롯")));
        Cell.Name->SetColorAndOpacity(Def ? Style.Text : Style.Muted);
        Cell.Badge->SetText(FText::FromString(Equipped ? TEXT("장착 중") : Def ? RarityName(Def->Rarity).ToString() : TEXT("—")));
        Cell.Badge->SetColorAndOpacity(Equipped ? Style.Mint : Def ? Style.RarityColor(Def->Rarity) : Style.Muted);
        Cell.Button->SetToolTipText(Def ? Def->DisplayName : FText::GetEmpty());
    }
}
void UPGUIInventory::SelectItem(FGuid Guid)
{
    auto* Profile=UPGProfileSubsystem::Get(this);
    if (!Profile || !Profile->GetProfile() || !Profile->GetProfile()->Items.ContainsByPredicate([Guid](const auto& Item){return Item.Guid==Guid;})) return;
    if (SelectedItem!=Guid && ComparisonScroll) ComparisonScroll->SetScrollOffset(0);
    SelectedItem=Guid; PendingDiscard.Invalidate(); RefreshComparison();
}
void UPGUIInventory::RefreshEquipment()
{
    using namespace PGInventoryUI;
    const auto* Profile=UPGProfileSubsystem::Get(this); const auto* Save=Profile->GetProfile(); const auto& Style=FPGUIStyle::Get();
    EquippedRows->ClearChildren();
    for (const auto EquipmentSlot : {EPGEquipmentSlot::Weapon,EPGEquipmentSlot::Accessory})
    {
        const auto Id=Save->Equipment.FindRef(EquipmentSlot);
        const auto* Item=Save->Items.FindByPredicate([Id](const auto& Entry){return Entry.Guid==Id;});
        const auto* Def=Item ? Profile->GetCatalog()->FindItem(Item->DefinitionId) : nullptr;
        Line(EquippedRows,SlotName(EquipmentSlot),Style.Muted,12,true);
        if (Item)
        {
            EquippedRows->AddSlot().AutoHeight().Padding(0,0,0,8)
                [Action(Def ? Def->DisplayName.ToString() : TEXT("정보를 찾을 수 없는 장비"),[this,Id](){SelectItem(Id);return FReply::Handled();})];
            auto Remove=Action(TEXT("장착 해제"),[this,EquipmentSlot](){return Unequip(uint8(EquipmentSlot));}); Remove->SetEnabled(bCanWrite);
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
    ComparisonRows->ClearChildren(); bKnownSelection=false; bSelectedEquipped=false;
    const auto* Profile=UPGProfileSubsystem::Get(this); if (!Profile || !Profile->GetProfile() || !Profile->GetCatalog()) return;
    const auto* Save=Profile->GetProfile(); const auto& Style=FPGUIStyle::Get();
    const auto* Item=Save->Items.FindByPredicate([this](const auto& Entry){return Entry.Guid==SelectedItem;});
    if (!Item) { Line(ComparisonRows,FText::FromString(TEXT("아직 획득한 장비가 없습니다.\n전투에서 얻은 아이템을 이곳에서 비교하세요.")),Style.Muted); return; }
    bSelectedEquipped=Save->Equipment.FindKey(Item->Guid)!=nullptr;
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
            + SVerticalBox::Slot().AutoHeight()[Text(RarityName(Def->Rarity).ToString()+TEXT(" · ")+SlotName(Def->Slot).ToString(),16,Style.RarityColor(Def->Rarity),true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[Text(Def->DisplayName,22,Style.Text,true)]]];
    const FGuid CurrentId=Save->Equipment.FindRef(Def->Slot);
    const auto* Current=Save->Items.FindByPredicate([CurrentId](const auto& Entry){return Entry.Guid==CurrentId;});
    const auto* CurrentDef=Current ? Profile->GetCatalog()->FindItem(Current->DefinitionId) : nullptr;
    Line(ComparisonRows,FText::FromString(bSelectedEquipped ? TEXT("현재 장착한 아이템입니다") : CurrentDef ? TEXT("비교 대상 · ")+CurrentDef->DisplayName.ToString() : TEXT("비교 대상 · 미장착")),Style.Muted,12);
    Line(ComparisonRows,FText::FromString(TEXT("능력치                 현재 → 선택")),Style.Muted,12);
    for (const auto& Row : Compare(*Item,Current))
    {
        const auto Delta=Row.Delta();
        ComparisonRows->AddSlot().AutoHeight().Padding(0,0,0,10)
        [SNew(SBorder).BorderImage(&Style.Panel).Padding(10)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Text(StatName(Row.Stat),12,Style.Muted)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
                [Text(FString::Printf(TEXT("%s → %s   (%s)"),*StatValue(Row.Stat,Row.Current).ToString(),*StatValue(Row.Stat,Row.Selected).ToString(),*StatValue(Row.Stat,Delta,true).ToString()),0,Delta>0 ? Style.Mint : Delta<0 ? Style.Danger : Style.Text,true)]]];
    }
    Line(ComparisonRows,FText::FromString(TEXT("아이템 옵션 차이입니다. 조건부 효과와 전투 중 버프는 별도로 적용됩니다.")),Style.Muted,12);
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
    PendingDiscard.Invalidate(); const bool Result=Profile->Equip(SelectedItem); bActionFailed=!Result; Refresh(); return Result;
}
bool UPGUIInventory::DiscardSelected()
{
    auto* Profile=UPGProfileSubsystem::Get(this);
    if (!Profile || !SelectedItem.IsValid() || bSelectedEquipped || !bCanWrite) return false;
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
    auto* Profile=UPGProfileSubsystem::Get(this); const auto* Save=Profile->GetProfile(); const auto* Catalog=Profile->GetCatalog(); const auto& Style=FPGUIStyle::Get();
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
            .OnClicked_Lambda([this,Id](){PreviewSkill=Id;RefreshBuilds();SetKeyboardFocus();return FReply::Handled();})
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
        Line(SkillDetailRows,FText::FromString(TEXT("S K I L L   /   검술 상세")),Style.Lavender,12,true);
        SkillDetailRows->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,16,0,24)
            [SNew(SBox).WidthOverride(128).HeightOverride(128)[SNew(SImage).Image(Art(Row->SkillIconPath))]];
        TArray<FString> Lines; PGPlayerSkillText::Describe(*Row).ParseIntoArrayLines(Lines);
        for (int32 I=0; I<Lines.Num(); ++I)
            Line(SkillDetailRows,FText::FromString(Lines[I]),I==0 ? Style.Text : Style.Muted,I==0 ? 23 : 16,I==0);
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
    Line(LoadoutRows,FText::FromString(bDraftDirty ? TEXT("02  /  적용 예정 슬롯") : TEXT("02  /  나의 장착 슬롯")),Style.Text,18,true);
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
    Line(BuildRows,FText::FromString(TEXT("현재 빌드 · 장착 효과 포함")),Style.Text,20,true);
    const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
    const auto* ASC=Player ? Player->GetPGAbilitySystemComponent() : nullptr;
    const auto* Tuning=ASC && ASC->CombatTuning ? ASC->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
    const EPGCombatPerk Roots[]={EPGCombatPerk::Bleed,EPGCombatPerk::Shockwave,EPGCombatPerk::Frenzy};
    const EPGCombatPerk Cores[]={EPGCombatPerk::BleedRecast,EPGCombatPerk::ShockFracture,EPGCombatPerk::FrenzyAfterimage};
    for (int32 I=0;I<3;++I)
    {
        int32 Branches=0; for (int32 N=1;N<=3;++N) if(Profile->GetEffectivePerk(EPGCombatPerk(uint8(Roots[I])+N))>0) ++Branches;
        const bool Active=Profile->GetEffectivePerk(Roots[I])>0;
        const FString Summary=FString::Printf(TEXT("%s  ·  %s  ·  전용 강화 %d/3  ·  %s"),*PGRewardText::PerkName(Roots[I]),Active ? TEXT("활성") : TEXT("미획득"),Branches,Profile->GetEffectivePerk(Cores[I])>0 ? TEXT("핵심 획득") : TEXT("핵심 미획득"));
        Line(BuildRows,FText::FromString(Summary),Active ? Style.Mint : Style.Muted);
    }
    Line(BuildRows,FText::FromString(TEXT("획득한 강화")),Style.Text,18,true);
    TArray<int32> RewardIds; Save->SelectedRewards.GetKeys(RewardIds); RewardIds.Sort();
    if (RewardIds.IsEmpty()) Line(BuildRows,FText::FromString(TEXT("아직 선택한 강화가 없습니다. 구간을 완료하고 빌드를 성장시키세요.")),Style.Muted);
    auto* Tables=UPGDataTableManager::Get(this);
    for (int32 Id : RewardIds)
        if (const auto* Reward=Tables ? Tables->GetRowData<FPGRewardStatDataRow>(Id) : nullptr)
        {
            const FString Effect=Reward->Perk==EPGCombatPerk::None ? Reward->PlaystyleDescription.ToString() : PGRewardText::Effect(Reward->Perk,Profile->GetEffectivePerk(Reward->Perk),*Tuning);
            Line(BuildRows,FText::FromString(FString::Printf(TEXT("%s ×%d%s"),*Reward->DisplayName.ToString(),Save->SelectedRewards.FindRef(Id),Reward->bKeystone ? TEXT("  ·  핵심 강화") : TEXT(""))),Style.Lavender,0,true);
            Line(BuildRows,FText::FromString(Effect),Style.Muted);
        }
    Line(BuildRows,FText::FromString(FString::Printf(TEXT("런 기록  ·  최고 구간 %d  ·  승리 %d회"),Save->BestStage,Save->CompletedRuns)),Style.Muted,12);
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
    const auto* Catalog=Profile->GetCatalog(); const auto& Style=FPGUIStyle::Get();
    Line(NearbyRows,FText::FromString(MinRarity==0 ? TEXT("표시: 전체") : MinRarity==1 ? TEXT("표시: 마법 이상") : TEXT("표시: 희귀")),Style.Mint,18,true);
    int32 Count=0;
    for (TActorIterator<APGLootDrop> It(GetWorld());It;++It)
    {
        if (!IsValid(*It) || !GetOwningPlayerPawn() || FVector::DistSquared(It->GetActorLocation(),GetOwningPlayerPawn()->GetActorLocation())>FMath::Square(Catalog->PickupRadius)) continue;
        const auto* Def=Catalog->FindItem(It->GetItem().DefinitionId);
        if (!Def || int32(Def->Rarity)<MinRarity) continue;
        ++Count; const TWeakObjectPtr<APGLootDrop> Drop(*It);
        auto Button=Action(RarityName(Def->Rarity).ToString()+TEXT(" · ")+Def->DisplayName.ToString()+TEXT("  /  획득"),[this,Drop]()
        { bActionFailed=!Drop.IsValid() || !Drop->TryPickup(GetOwningPlayerPawn());Refresh();SetKeyboardFocus();return FReply::Handled(); });
        Button->SetEnabled(bCanWrite); NearbyRows->AddSlot().AutoHeight().Padding(0,0,0,10)[Button];
    }
    if (Count==0) Line(NearbyRows,FText::FromString(TEXT("획득 범위에 해당 등급의 전리품이 없습니다.")),Style.Muted);
}
