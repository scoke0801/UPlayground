#include "PGUIWindowRewardSelect.h"
#include "PGActor/Manager/PGStageManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "RewardSelect/PGUIRewardCard.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Components/Stat/PGStatComponent.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "Components/VerticalBoxSlot.h"

void UPGUIWindowRewardSelect::SetRewardId(int StageId)
{
    if (PGData()) if (const auto* Stage = PGData()->GetRowData<FPGStageDataRow>(StageId)) Choices = Stage->RewardPool;
}
void UPGUIWindowRewardSelect::SetChoices(FGuid InToken, const TArray<FPGStageReward>& InChoices)
{
    Token = InToken; Choices = InChoices; bIsStatus = false;
}
TSharedRef<SWidget> UPGUIWindowRewardSelect::RebuildWidget()
{
    bCanCloseWithEscape = false; bCanCloseWithBackgroundClick = false; SetIsFocusable(true);
    if (!WidgetTree->RootWidget)
    {
        auto* Backdrop = WidgetTree->ConstructWidget<UBorder>();
        Backdrop->SetBrushColor(FLinearColor(.015f,.02f,.035f,.96f));
        Backdrop->SetHorizontalAlignment(HAlign_Center); Backdrop->SetVerticalAlignment(VAlign_Center);
        WidgetTree->RootWidget = Backdrop;
        auto* List = WidgetTree->ConstructWidget<UVerticalBox>(); Backdrop->SetContent(List);
        auto* Title = WidgetTree->ConstructWidget<UTextBlock>();
        Title->SetText(bIsStatus ? StatusText : NSLOCTEXT("PG", "ChooseReward", "다음 전투의 힘을 선택하세요"));
        Title->SetJustification(ETextJustify::Center); auto TitleFont = Title->GetFont(); TitleFont.Size = 30; Title->SetFont(TitleFont); List->AddChild(Title);
        auto* Hint = WidgetTree->ConstructWidget<UTextBlock>();
        Hint->SetText(NSLOCTEXT("PG", "BoonHint", "선택한 효과는 이번 런 동안 누적됩니다 · 정예의 푸른 빈틈을 노리세요"));
        Hint->SetJustification(ETextJustify::Center); Hint->SetColorAndOpacity(FLinearColor(.6f,.65f,.72f));
        auto HintFont = Hint->GetFont(); HintFont.Size = 15; Hint->SetFont(HintFont); List->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0,12));
        if (!bIsStatus)
        {
            BuildCountdown = WidgetTree->ConstructWidget<UTextBlock>();
            BuildCountdown->SetJustification(ETextJustify::Center);
            List->AddChild(BuildCountdown);
        }
        auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>(); List->AddChild(Row);
        const int32 Count = bIsStatus ? 1 : FMath::Max(1, Choices.Num());
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FText Name = bIsStatus ? NSLOCTEXT("PG", "RestartRunRogue", "다시 도전하기") : NSLOCTEXT("PG", "ContinueBuild", "빌드 준비하기");
            FText Desc = NSLOCTEXT("PG", "ContinueHint", "준비되면 선택하세요");
            EPGRewardGrade Grade = EPGRewardGrade::Normal;
            UTexture2D* Icon = nullptr;
            int32 IconPanel = -1;
            if (!bIsStatus && Choices.IsValidIndex(Index) && PGData())
                if (const auto* Reward = PGData()->GetRowData<FPGRewardStatDataRow>(Choices[Index].RewardId))
                {
                    Name = Reward->DisplayName; Grade = Reward->Grade; Icon = Reward->Icon.LoadSynchronous(); IconPanel = Reward->IconPanel;
                    const auto* Player = Cast<APGCharacterBase>(GetOwningPlayerPawn());
                    if (Player && Player->GetStatComponent())
                    {
                        const int32 Before = Player->GetStatComponent()->GetStat(Reward->StatType);
                        Desc = FText::Format(NSLOCTEXT("PG", "RewardDelta", "{0} → {1}\n+{2}"), FText::AsNumber(Before), FText::AsNumber(Before + Reward->Amount), FText::AsNumber(Reward->Amount));
                    }
                    else Desc = FText::Format(NSLOCTEXT("PG", "RewardAmount", "+{0}"), FText::AsNumber(Reward->Amount));
                    if (!Reward->PlaystyleDescription.IsEmpty() && Reward->Perk == EPGCombatPerk::None)
                        Desc = FText::Format(NSLOCTEXT("PG", "RewardExplained", "{0}\n\n{1}"), Reward->PlaystyleDescription, Desc);
                    if (Reward->Perk != EPGCombatPerk::None)
                    {
                        const auto* Profile = UPGProfileSubsystem::Get(this);
                        const int32 Before = Profile ? Profile->GetProfile()->CombatPerks.FindRef(Reward->Perk) : 0;
                        Desc = FText::Format(NSLOCTEXT("PG", "PerkDelta", "{0}\n\n효과 {1}% → {2}%"), Reward->PlaystyleDescription,
                            FText::AsNumber(Before), FText::AsNumber(FMath::Min(100, Before + Reward->PerkPercent)));
                    }
                }
            auto* Card = CreateWidget<UPGUIRewardCard>(GetOwningPlayer(), UPGUIRewardCard::StaticClass());
            Card->Configure(Index, Name, Desc, Grade, Icon, IconPanel);
            Card->OnSelected.BindUObject(this, &ThisClass::BeginChoice);
            Card->SetRenderOpacity(0); Card->SetIsEnabled(false);
            Row->AddChildToHorizontalBox(Card)->SetPadding(FMargin(12,28)); Cards.Add(Card);
        }
    }
    return Super::RebuildWidget();
}
void UPGUIWindowRewardSelect::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (BuildCountdown && StageOwner.IsValid())
        BuildCountdown->SetText(StageOwner->IsManualReady() ? NSLOCTEXT("PG", "ManualBuild", "강화 선택 → 장비 정비 → 우측 준비 완료") :
            FText::Format(NSLOCTEXT("PG", "BuildCountdown", "빌드 시간 {0}초 · 시간 종료 시 첫 번째 보상 자동 선택"),
            FText::AsNumber(FMath::CeilToInt(StageOwner->GetBuildTimeRemaining()))));
    PresentationTime += DeltaTime;
    if (bConfirming)
    {
        ConfirmTime += DeltaTime;
        if (ConfirmTime >= .22f)
        {
            bConfirming = false;
            if (bIsStatus) { const FSimpleDelegate Retry = OnRetry; Retry.ExecuteIfBound(); return; }
            if (!SubmitChoice(PendingIndex))
                for (const auto& Card : Cards) { Card->SetIsEnabled(true); Card->SetRenderOpacity(1); Card->SetRenderScale(FVector2D(1)); }
        }
        return;
    }
    for (int32 I = 0; I < Cards.Num(); ++I)
    {
        const float Alpha = FMath::Clamp((PresentationTime - I * .08f) / .18f, 0.f, 1.f);
        Cards[I]->SetRenderOpacity(Alpha); Cards[I]->SetRenderTranslation(FVector2D(0, (1-Alpha)*20)); Cards[I]->SetIsEnabled(Alpha >= 1.f);
    }
}
void UPGUIWindowRewardSelect::BeginChoice(int32 Index)
{
    if (bConfirming || (!bIsStatus && !Token.IsValid())) return;
    PendingIndex = Choices.IsEmpty() ? INDEX_NONE : Index; ConfirmTime = 0; bConfirming = true;
    for (int32 I = 0; I < Cards.Num(); ++I) Cards[I]->SetConfirmed(I == Index);
}
bool UPGUIWindowRewardSelect::SubmitChoice(int32 Index)
{
    if (!Token.IsValid() || !OnSubmit.IsBound()) return false;
    const FPGSubmitReward Submit = OnSubmit;
    const bool bApplied = Submit.Execute(Token, Index);
    if (bApplied) Token.Invalidate();
    return bApplied;
}
void UPGUIWindowRewardSelect::SelectFirst() { BeginChoice(0); }
void UPGUIWindowRewardSelect::SelectSecond() { BeginChoice(1); }
void UPGUIWindowRewardSelect::SelectThird() { BeginChoice(2); }
