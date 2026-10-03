#include "PGUIWindowRewardSelect.h"
#include "RewardSelect/PGUIRewardCard.h"
#include "RewardSelect/PGRewardPresentation.h"
#include "PGInventoryPresentation.h"
#include "PGUI/Style/PGUIStyle.h"
#include "PGUI/Style/PGUIStyleSettings.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGData/DataTable/Reward/PGRewardText.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Components/Stat/PGStatComponent.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

namespace PGRewardUI
{
    TSharedRef<STextBlock> Text(const FText& Value, int32 Size, FLinearColor Color, bool Bold = false)
    { return SNew(STextBlock).Text(Value).Font(FPGUIStyle::Get().Font(Size,Bold)).ColorAndOpacity(Color).AutoWrapText(true); }
    TSharedRef<STextBlock> Text(const FString& Value, int32 Size, FLinearColor Color, bool Bold = false)
    { return Text(FText::FromString(Value),Size,Color,Bold); }
    void Sound(const UObject* Context, TSoftObjectPtr<USoundBase> Asset)
    { if (auto* Loaded = Asset.LoadSynchronous()) UGameplayStatics::PlaySound2D(Context,Loaded); }
}

void UPGUIWindowRewardSelect::SetRewardId(int StageId)
{
    if (PGData()) if (const auto* Stage = PGData()->GetRowData<FPGStageDataRow>(StageId)) Choices = Stage->RewardPool;
}
void UPGUIWindowRewardSelect::SetChoices(FGuid InToken, const TArray<FPGStageReward>& InChoices)
{
    Token = InToken; Choices = InChoices; bIsStatus = bIsResult = false;
}
TSharedRef<SWidget> UPGUIWindowRewardSelect::RebuildWidget()
{
    using namespace PGRewardUI;
    const auto& Style = FPGUIStyle::Get();
    bCanCloseWithEscape = bCanCloseWithBackgroundClick = false; SetIsFocusable(true);
    Cards.Empty(); PresentationTime = ConfirmTime = 0; bConfirming = false;
    const FText Title = bIsResult ? FText::FromString(Result.bSavePending ? TEXT("시련 돌파 · 저장 대기") : TEXT("시련 돌파!")) :
        bIsStatus ? FText::FromString(TEXT("도전 종료")) : FText::FromString(TEXT("다음 전투의 힘을 선택하세요"));
    auto Content = bIsResult ? MakeResult() : bIsStatus ? StaticCastSharedRef<SWidget>(
        SNew(SScrollBox) + SScrollBox::Slot()[Text(StatusText,24,Style.Text)]) : MakeChoices();
    auto Layout = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[Text(bIsStatus ? TEXT("T R I A L  /  도전 결과") : TEXT("A U G M E N T  /  강화 선택"),16,Style.Mint,true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,10)[Text(Title,32,Style.Text,true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
        [SNew(STextBlock).Text_Lambda([this](){return CountdownText();}).Font(Style.Font(16)).ColorAndOpacity(Style.Muted).AutoWrapText(true)]
        + SVerticalBox::Slot().FillHeight(1)[Content]
        + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
        [SNew(STextBlock).Text_Lambda([this](){return Feedback;}).Font(Style.Font(16)).ColorAndOpacity(Style.Danger).AutoWrapText(true)];
    if (bIsStatus)
    {
        Layout->AddSlot().AutoHeight().Padding(0,16,0,0)
            [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(20,14)).HAlign(HAlign_Center)
                .IsEnabled_Lambda([this](){return !bConfirming && PresentationTime >= GetDefault<UPGUIStyleSettings>()->RevealSeconds;})
                .OnClicked_Lambda([this](){RetryRun();return FReply::Handled();})
                [SNew(STextBlock).Text(bIsResult ? FText::FromString(Result.bSavePending ? TEXT("저장 다시 시도") : TEXT("새 도전 시작")) :
                    StatusAction.IsEmpty() ? FText::FromString(TEXT("다시 도전하기")) : StatusAction).Font(Style.Font(20,true)).ColorAndOpacity(Style.Mint)]];
    }
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.004f,.008f,.018f,.90f)).Padding(0)
        [SNew(SSafeZone)[SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(GetDefault<UPGUIStyleSettings>()->ScreenMargin)
            [SAssignNew(Frame,SBox).WidthOverride(1200).HeightOverride(780)
                [SNew(SBorder).BorderImage(&Style.Panel).Padding(24)[Layout]]]]];
}

TSharedRef<SWidget> UPGUIWindowRewardSelect::MakeResult()
{
    using namespace PGRewardUI;
    using namespace PGInventoryPresentation;
    const auto& Style = FPGUIStyle::Get();
    const auto* Profile = UPGProfileSubsystem::Get(this);
    const auto* Def = Profile && Profile->GetCatalog() && Result.Loot.Guid.IsValid() ? Profile->GetCatalog()->FindItem(Result.Loot.DefinitionId) : nullptr;
    auto Loot = SNew(SVerticalBox);
    Loot->AddSlot().AutoHeight()[Text(TEXT("B O S S  L O O T  /  보스 전리품"),16,Style.Mint,true)];
    if (Def)
    {
        LootIcon = Def->Icon.LoadSynchronous(); LootBrush.SetResourceObject(LootIcon); LootBrush.ImageSize = FVector2D(112);
        if (Def->IconPanel >= 0 && Def->IconPanel < 3)
            LootBrush.SetUVRegion(FBox2f(FVector2f(Def->IconPanel/3.f,0),FVector2f((Def->IconPanel+1)/3.f,1)));
        const auto Color = Style.RarityColor(Def->Rarity);
        Loot->AddSlot().AutoHeight().Padding(0,18)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(112).HeightOverride(112)[SNew(SImage).Image(&LootBrush)]]
                + SHorizontalBox::Slot().FillWidth(1).Padding(20,0,0,0).VAlign(VAlign_Center)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[Text(RarityName(Def->Rarity).ToString()+TEXT(" · ")+SlotName(Def->Slot).ToString(),16,Color,true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[Text(Def->DisplayName,26,Style.Text,true)]]];
        for (const auto& Option : Compare(Result.Loot,nullptr))
            Loot->AddSlot().AutoHeight().Padding(0,0,0,10)
                [Text(StatName(Option.Stat).ToString()+TEXT("  ")+StatValue(Option.Stat,Option.Selected,true).ToString(),20,Style.Text)];
        if (!Def->EffectDescription.IsEmpty())
            Loot->AddSlot().AutoHeight().Padding(0,10,0,0)[Text(Def->EffectDescription,18,Style.Muted)];
    }
    else Loot->AddSlot().AutoHeight().Padding(0,24)[Text(TEXT("기록된 보스 전리품이 없습니다."),20,Style.Muted)];
    Loot->AddSlot().AutoHeight().Padding(0,22,0,0)[Text(Result.bSavePending ? TEXT("저장되지 않았습니다. 전리품을 유지한 채 다시 시도합니다.") :
        TEXT("저장 완료 · 가방과 별도의 결과 슬롯에 보관"),16,Result.bSavePending ? Style.Danger : Style.Mint,true)];
    auto Build = SNew(SVerticalBox);
    int32 Selections = 0; for (const auto& Entry : Result.SelectedRewards) Selections += Entry.Value;
    Build->AddSlot().AutoHeight()[Text(FString::Printf(TEXT("%d구간 돌파   ·   강화 %d회"),Result.CompletedStages,Selections),22,Style.Text,true)];
    Build->AddSlot().AutoHeight().Padding(0,10,0,22)[Text(FString::Printf(TEXT("누적 승리 %d회%s"),Result.CompletedRuns,
        Result.bSavePending ? TEXT(" · 이번 승리 저장 대기") : TEXT("")),16,Style.Muted)];
    Build->AddSlot().AutoHeight().Padding(0,0,0,14)[Text(TEXT("이번 도전의 강화"),18,Style.Lavender,true)];
    TArray<int32> Ids; Result.SelectedRewards.GetKeys(Ids); Ids.Sort();
    for (int32 Id : Ids)
    {
        const auto* Reward = PGData() ? PGData()->GetRowData<FPGRewardStatDataRow>(Id) : nullptr;
        Build->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(FString::Printf(TEXT("%s  ×%d"),
            Reward ? *Reward->DisplayName.ToString() : TEXT("알 수 없는 강화"),Result.SelectedRewards[Id]),18,Reward && Reward->bKeystone ? Style.Rare : Style.Text)];
    }
    if (Ids.IsEmpty()) Build->AddSlot().AutoHeight()[Text(TEXT("선택한 강화 없음"),18,Style.Muted)];
    return SAssignNew(ResultBody,SBorder).BorderImage(&Style.Card).Padding(20)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(.58f).Padding(0,0,24,0)[SNew(SScrollBox)+SScrollBox::Slot()[Loot]]
            + SHorizontalBox::Slot().FillWidth(.42f)[SNew(SScrollBox)+SScrollBox::Slot()[Build]]];
}

FText UPGUIWindowRewardSelect::CountdownText() const
{
    if (bIsResult) return FText::FromString(TEXT("전리품 · 장비 · 강화는 이번 도전 전용입니다. 새 도전에서 초기화됩니다."));
    if (bIsStatus) return FText::FromString(TEXT("장비와 강화는 새 도전에서 다시 구성됩니다."));
    if (!StageOwner.IsValid() || StageOwner->IsManualReady()) return FText::FromString(TEXT("한 가지를 선택하세요 · 이번 도전 동안 누적 · 선택 후 장비 정비"));
    return FText::Format(NSLOCTEXT("PG", "BuildCountdown", "빌드 시간 {0}초 · 시간 종료 시 첫 번째 보상 자동 선택"),
        FText::AsNumber(FMath::CeilToInt(StageOwner->GetBuildTimeRemaining())));
}

void UPGUIWindowRewardSelect::NativeConstruct()
{
    Super::NativeConstruct();
    const auto* Settings = GetDefault<UPGUIStyleSettings>();
    if (bIsResult && !Result.bSavePending) PGRewardUI::Sound(this,Settings->VictorySound);
    else if (!bIsStatus) PGRewardUI::Sound(this,Settings->RewardOpenSound);
}
void UPGUIWindowRewardSelect::NativeDestruct()
{
    if (auto* UI = UPGUIManager::Get(this)) UI->ReleaseModalInput(this);
    bConfirming = false; OnSubmit.Unbind(); OnRetry.Unbind();
    Super::NativeDestruct();
}
void UPGUIWindowRewardSelect::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren); Frame.Reset(); ResultBody.Reset(); Cards.Empty(); LootIcon = nullptr;
}
void UPGUIWindowRewardSelect::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    const auto* Settings = GetDefault<UPGUIStyleSettings>();
    if (Frame)
    {
        Frame->SetWidthOverride(FMath::Max(1.f,FMath::Min(Settings->RewardMaxWidth,Geometry.GetLocalSize().X-Settings->ScreenMargin*2)));
        Frame->SetHeightOverride(FMath::Max(1.f,FMath::Min(820.f,Geometry.GetLocalSize().Y-Settings->ScreenMargin*2)));
    }
    PresentationTime += DeltaTime;
    if (ResultBody) ResultBody->SetRenderOpacity(FMath::Clamp(PresentationTime/FMath::Max(.05f,Settings->RevealSeconds),0.f,1.f));
    if (bConfirming)
    {
        ConfirmTime += DeltaTime;
        if (ConfirmTime >= Settings->ConfirmSeconds)
        {
            // The callback may synchronously destroy this widget and create another stage window.
            if (bIsStatus)
            {
                const FSimpleDelegate Retry = OnRetry; Retry.ExecuteIfBound();
                if (IsInViewport())
                {
                    bConfirming = false;
                    if (const auto* Profile = UPGProfileSubsystem::Get(this)) Feedback = FText::FromString(Profile->Status);
                }
                return;
            }
            bConfirming = false;
            if (!SubmitChoice(PendingIndex))
            {
                Feedback = FText::FromString(TEXT("강화를 저장하지 못했습니다. 같은 카드를 다시 선택해 주세요."));
                for (const auto& Card : Cards) Card->ResetConfirmation();
            }
        }
        return;
    }
    for (int32 I = 0; I < Cards.Num(); ++I)
    {
        const float Alpha = FMath::Clamp((PresentationTime-I*Settings->CardStaggerSeconds)/FMath::Max(.05f,Settings->RevealSeconds),0.f,1.f);
        Cards[I]->SetRenderOpacity(Alpha); Cards[I]->SetRenderTranslation(FVector2D(0,(1-Alpha)*12)); Cards[I]->SetIsEnabled(Alpha>=1.f);
    }
}
void UPGUIWindowRewardSelect::BeginChoice(int32 Index)
{
    const int32 Choice = Choices.IsEmpty() ? INDEX_NONE : Index;
    if (bConfirming || !Token.IsValid() || !PGRewardPresentation::IsValidChoice(Choice,Choices.Num()) ||
        !Cards.IsValidIndex(Index) || !Cards[Index]->GetIsEnabled()) return;
    PendingIndex = Choice; ConfirmTime = 0; bConfirming = true; Feedback = FText();
    for (int32 I = 0; I < Cards.Num(); ++I) Cards[I]->SetConfirmed(I==Index);
    PGRewardUI::Sound(this,GetDefault<UPGUIStyleSettings>()->RewardConfirmSound);
}
bool UPGUIWindowRewardSelect::SubmitChoice(int32 Index)
{
    if (bSubmitting || !Token.IsValid() || !OnSubmit.IsBound() || !PGRewardPresentation::IsValidChoice(Index,Choices.Num())) return false;
    TGuardValue<bool> Guard(bSubmitting,true);
    const FPGSubmitReward Submit = OnSubmit;
    const bool Applied = Submit.Execute(Token,Index);
    if (Applied) Token.Invalidate();
    return Applied;
}
void UPGUIWindowRewardSelect::RetryRun()
{
    if (bConfirming || !OnRetry.IsBound()) return;
    bConfirming = true; ConfirmTime = 0; Feedback = FText();
}
FReply UPGUIWindowRewardSelect::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // Escape cannot dismiss an uncommitted reward or accidentally restart a finished run.
    if (Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::I) return FReply::Handled();
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}

TSharedRef<SWidget> UPGUIWindowRewardSelect::MakeChoices()
{
    auto Row = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < FMath::Max(1,Choices.Num()); ++Index)
    {
        FText Name = NSLOCTEXT("PG", "ContinueBuild", "빌드 준비하기");
        FText Desc = NSLOCTEXT("PG", "NoRewardsContinue", "선택할 강화가 없습니다. 장비를 정비한 뒤 다음 전투를 준비하세요.");
        EPGRewardGrade Grade = EPGRewardGrade::Normal;
        UTexture2D* Icon = nullptr;
        int32 IconPanel = -1;
        if (Choices.IsValidIndex(Index) && PGData())
            if (const auto* Reward = PGData()->GetRowData<FPGRewardStatDataRow>(Choices[Index].RewardId))
            {
                Name = Reward->DisplayName; Grade = Reward->Grade; Icon = Reward->Icon.LoadSynchronous(); IconPanel = Reward->IconPanel;
                const auto* Player = Cast<APGCharacterBase>(GetOwningPlayerPawn());
                const auto* Profile = UPGProfileSubsystem::Get(this);
                if (Reward->Perk == EPGCombatPerk::None)
                {
                    using namespace PGInventoryPresentation;
                    const int32 Before = Player && Player->GetStatComponent() ? Player->GetStatComponent()->GetStat(Reward->StatType) : 0;
                    const int32 After = Reward->StatType == EPGStatType::CriticalRate ? FMath::Min(10000,Before+Reward->Amount) : Before+Reward->Amount;
                    Desc = FText::FromString(Reward->PlaystyleDescription.ToString()+TEXT("\n\n")+StatName(Reward->StatType).ToString()+TEXT("\n")+
                        StatValue(Reward->StatType,Before).ToString()+TEXT(" → ")+StatValue(Reward->StatType,After).ToString()+TEXT("  (")+
                        StatValue(Reward->StatType,After-Before,true).ToString()+TEXT(")"));
                }
                else
                {
                    const int32 Before = Profile ? Profile->GetEffectivePerk(Reward->Perk) : 0;
                    const auto* ASC = Player ? Player->GetPGAbilitySystemComponent() : nullptr;
                    const auto* Tuning = ASC && ASC->CombatTuning ? ASC->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
                    const int32 EffectCap = Reward->Perk == EPGCombatPerk::Cooldown ? 50 : Reward->Perk == EPGCombatPerk::FrenzyGuard ? 60 : 100;
                    const int32 EffectiveBefore = FMath::Min(EffectCap,Before);
                    const int32 EffectiveAfter = FMath::Min(EffectCap,Before+Reward->PerkPercent);
                    const FString Change = Reward->bKeystone ? TEXT("미획득 → 핵심 강화 획득") :
                        FString::Printf(TEXT("%s %d%% → %d%%"),*PGRewardText::PerkName(Reward->Perk),EffectiveBefore,EffectiveAfter);
                    Desc = FText::FromString(PGRewardText::Effect(Reward->Perk,EffectiveAfter,*Tuning)+TEXT("\n\n")+Change);
                }
                const int32 Chosen = Profile ? Profile->GetProfile()->SelectedRewards.FindRef(Reward->StatId) : 0;
                const FString Limit = Reward->MaxSelections > 0 ? FString::Printf(TEXT("선택 %d/%d회"),Chosen,Reward->MaxSelections) : TEXT("반복 선택 가능");
                Desc = FText::FromString(Desc.ToString()+TEXT("\n\n")+PGRewardText::Requirements(*Reward)+TEXT("\n")+Limit);
            }

        auto* Card = CreateWidget<UPGUIRewardCard>(GetOwningPlayer());
        Card->Configure(Index,Name,Desc,Grade,Icon,IconPanel);
        Card->SetContinueOnly(Choices.IsEmpty());
        if (Choices.IsValidIndex(Index) && PGData())
            if (const auto* Reward = PGData()->GetRowData<FPGRewardStatDataRow>(Choices[Index].RewardId))
            {
                const auto* Profile = UPGProfileSubsystem::Get(this);
                Card->SetBuildContext(PGRewardPresentation::BuildContext(*Reward,[Profile](EPGCombatPerk Perk){return Profile ? Profile->GetEffectivePerk(Perk) : 0;}));
            }
        Card->OnSelected.BindUObject(this,&ThisClass::BeginChoice);
        Card->SetRenderOpacity(0); Card->SetIsEnabled(false);
        Row->AddSlot().FillWidth(1).Padding(Index==0 ? 0.f : 8.f,0,Index==Choices.Num()-1 ? 0.f : 8.f,0)[Card->TakeWidget()];
        Cards.Add(Card);
    }
    return Row;
}
