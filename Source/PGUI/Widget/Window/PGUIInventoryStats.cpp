#include "PGUIInventory.h"
#include "PGInventoryPresentation.h"
#include "PGUI/Style/PGInventoryStyle.h"
#include "PGUI/Style/PGUIStyleSettings.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Stat/PGStatComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"
#include "PGData/DataTable/Reward/PGRewardText.h"
#include "PGActor/Components/Rendering/PGCharacterAppearanceComponent.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "EngineUtils.h"

void UPGUIInventory::SetStatsTab()
{
    if (!bStatsOpen) ToggleStatsPanel();
    bLastStats=true;
    ApplyPanelLayout();
    if (auto* Profile=UPGProfileSubsystem::Get(this)) Profile->AcknowledgeLevelNotice();
}

int32 UPGUIInventory::GrowthDraftPoints() const
{
    int32 Total=0;
    for (const auto& Pair : GrowthDraft) Total+=Pair.Value;
    return Total;
}
bool UPGUIInventory::CanAddGrowth(EPGStatType Stat) const
{
    const auto* P=UPGProfileSubsystem::Get(this);
    if (!P || !P->GetProfile() || !P->GetCatalog() || P->IsSaveBlocked() || P->GetProfile()->bRunEnded) return false;
    const auto* Rule=P->GetCatalog()->LevelRules.InvestmentRules.Find(Stat);
    return Rule && GrowthDraftPoints()<P->GetProfile()->Growth.UnspentPoints &&
        int64(P->GetProfile()->Growth.Investments.FindRef(Stat))+GrowthDraft.FindRef(Stat)<Rule->MaxPoints;
}
void UPGUIInventory::ResetGrowthDraft()
{
    GrowthDraft.Reset(); GrowthBase.Reset(); GrowthRun.Invalidate(); bConfirmGrowthClose=false; GrowthStatus=FText::GetEmpty();
}
FReply UPGUIInventory::ApplyGrowth()
{
    CheckGrowthContext();
    if (auto* P=UPGProfileSubsystem::Get(this); P && !GrowthDraft.IsEmpty())
    {
        const bool bClose=bConfirmGrowthClose;
        if (P->ApplyInvestments(GrowthRun,GrowthBase,GrowthDraft))
        {
            ResetGrowthDraft(); GrowthStatus=FText::FromString(TEXT("분배를 적용했습니다"));
            if (bClose) ToggleStatsPanel();
        }
        else GrowthStatus=FText::FromString(P->Status);
    }
    return FReply::Handled();
}
void UPGUIInventory::CheckGrowthContext()
{
    const auto* P=UPGProfileSubsystem::Get(this);
    int32 StageId=INDEX_NONE;
    if (GetWorld()) for (TActorIterator<APGStageManager> It(GetWorld());It;++It) { StageId=It->GetCurrentStageId(); break; }
    const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
    const auto* ASC=Player ? Player->GetPGAbilitySystemComponent() : nullptr;
    const bool bDead=ASC && ASC->GetHealth()<=0;
    if (!GrowthDraft.IsEmpty() && (!P || !P->GetProfile() || bDead || P->GetProfile()->bRunEnded ||
        P->GetProfile()->RunId!=GrowthRun || StageId!=GrowthStage ||
        !P->GetProfile()->Growth.Investments.OrderIndependentCompareEqual(GrowthBase))) ResetGrowthDraft();
    GrowthStage=StageId;
    if (bDead) if (auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->CloseInventory();
}

TSharedRef<SWidget> UPGUIInventory::BuildStatsTab()
{
    const auto& Style=PGInventoryStyle();
    auto Label=[&Style](const TCHAR* Value,int32 Size,FLinearColor Color)
    {
        return SNew(STextBlock).Text(FText::FromString(Value)).Font(Style.Font(Size,true))
            .ColorAndOpacity(Color).AutoWrapText(true);
    };
    auto Rows=SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight().Padding(0,0,0,12)
    [SNew(STextBlock).Tag(TEXT("PGGrowth")).Font(Style.Font(16,true)).ColorAndOpacity(Style.Text).AutoWrapText(true)
        .Text_Lambda([this]()
        {
            const auto* Profile = UPGProfileSubsystem::Get(this);
            if (!Profile || !Profile->GetProfile() || !Profile->GetCatalog()) return FText::GetEmpty();
            const auto& Growth = Profile->GetProfile()->Growth;
            const auto& Steps = Profile->GetCatalog()->LevelRules.Steps;
            const FString Experience = Steps.IsValidIndex(Growth.Level - 1)
                ? FString::Printf(TEXT("경험치 %lld / %lld"), Growth.Experience, Steps[Growth.Level - 1].RequiredExperience)
                : TEXT("최대 레벨");
            return FText::FromString(FString::Printf(TEXT("레벨 %d · %s\n미분배 포인트 %d"), Growth.Level, *Experience, Growth.UnspentPoints));
        })];
    auto Button=[&Style](const TCHAR* Text, TFunction<FReply()> Click)
    {
        return SNew(SButton).OnClicked_Lambda(MoveTemp(Click))
            [SNew(STextBlock).Text(FText::FromString(Text)).Font(Style.Font(14)).ColorAndOpacity(Style.Text)];
    };
    for (const auto Stat : {EPGStatType::Attack,EPGStatType::Defense,EPGStatType::Health})
    {
        auto Add=Button(TEXT("+"),[this,Stat]()
        {
            CheckGrowthContext();
            if (CanAddGrowth(Stat))
            {
                const auto* Save=UPGProfileSubsystem::Get(this)->GetProfile();
                if (GrowthDraft.IsEmpty()) { GrowthRun=Save->RunId; GrowthBase=Save->Growth.Investments; }
                ++GrowthDraft.FindOrAdd(Stat); GrowthStatus=FText::GetEmpty();
            }
            return FReply::Handled();
        });
        Add->SetEnabled(TAttribute<bool>::CreateLambda([this,Stat](){return CanAddGrowth(Stat);}));
        Add->SetTag(FName(*FString::Printf(TEXT("PGGrowth.Add.%d"),int32(Stat))));
        auto Remove=Button(TEXT("−"),[this,Stat]()
        {
            if (auto* Amount=GrowthDraft.Find(Stat); Amount && --*Amount<=0) GrowthDraft.Remove(Stat);
            return FReply::Handled();
        });
        Remove->SetEnabled(TAttribute<bool>::CreateLambda([this,Stat](){return GrowthDraft.FindRef(Stat)>0;}));
        Rows->AddSlot().AutoHeight().Padding(0,0,0,8)
        [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)
                [SNew(STextBlock).Font(Style.Font(14)).ColorAndOpacity(Style.Text).AutoWrapText(true)
                    .Text_Lambda([this,Stat]()
                    {
                        const auto* P=UPGProfileSubsystem::Get(this);
                        if (!P || !P->GetProfile() || !P->GetCatalog()) return FText::GetEmpty();
                        const auto* Rule=P->GetCatalog()->LevelRules.InvestmentRules.Find(Stat);
                        return Rule ? FText::FromString(FString::Printf(TEXT("%s · 투자 %d / %d · 초안 +%d\n1포인트당 +%d"),
                            *PGInventoryPresentation::StatName(Stat).ToString(),P->GetProfile()->Growth.Investments.FindRef(Stat),Rule->MaxPoints,GrowthDraft.FindRef(Stat),Rule->AmountPerPoint)) : FText::GetEmpty();
                    })]
                +SHorizontalBox::Slot().AutoWidth()[Remove]
                +SHorizontalBox::Slot().AutoWidth()[Add]]
            +SVerticalBox::Slot().AutoHeight()
            [SNew(STextBlock).Font(Style.Font(14)).ColorAndOpacity(Style.Muted).AutoWrapText(true)
                .Text_Lambda([this,Stat]()
                {
                    const auto* P=UPGProfileSubsystem::Get(this);
                    const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
                    auto* ASC=Player ? Player->GetPGAbilitySystemComponent() : nullptr;
                    if (!P || !P->GetCatalog() || !ASC) return FText::GetEmpty();
                    const auto* Rule=P->GetCatalog()->LevelRules.InvestmentRules.Find(Stat);
                    if (!Rule) return FText::GetEmpty();
                    const auto Current=ASC->GetStatDetails(Stat);
                    const auto Preview=ASC->GetStatDetails(Stat,Current.Profile+float(GrowthDraft.FindRef(Stat)*Rule->AmountPerPoint));
                    return FText::FromString(FString::Printf(TEXT("현재 %s → 적용 예상 %s"),
                        *PGInventoryPresentation::StatValue(Stat,Current.Current).ToString(),*PGInventoryPresentation::StatValue(Stat,Preview.Expected).ToString()));
                })]];
    }
    Rows->AddSlot().AutoHeight()[SNew(STextBlock).Font(Style.Font(13)).ColorAndOpacity(Style.Muted).AutoWrapText(true)
        .Text_Lambda([this]()
        {
            FString Reason;
            const auto* P=UPGProfileSubsystem::Get(this);
            if (P) P->CanChangeSkills(Reason);
            return FText::FromString(FString::Printf(TEXT("초안 사용 포인트 %d\n%s%s"),GrowthDraftPoints(),*Reason,
                bConfirmGrowthClose ? TEXT("\n분배 초안을 적용하거나 취소한 뒤 닫으세요.") : TEXT("")));
        })];
    auto Apply=Button(TEXT("적용"),[this](){return ApplyGrowth();});
    Apply->SetTag(TEXT("PGGrowth.Apply"));
    Apply->SetEnabled(TAttribute<bool>::CreateLambda([this](){ FString Reason; const auto* P=UPGProfileSubsystem::Get(this); return !GrowthDraft.IsEmpty() && P && P->CanChangeSkills(Reason); }));
    Rows->AddSlot().AutoHeight().Padding(0,6,0,8)
    [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()[Apply]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("초안 초기화"),[this](){ResetGrowthDraft();return FReply::Handled();})]];
    Rows->AddSlot().AutoHeight()
    [SNew(SHorizontalBox).Visibility_Lambda([this](){return bConfirmGrowthClose ? EVisibility::Visible : EVisibility::Collapsed;})
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("취소 후 닫기"),[this](){ResetGrowthDraft();ToggleStatsPanel();return FReply::Handled();})]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("계속 편집"),[this](){bConfirmGrowthClose=false;return FReply::Handled();})]];
    Rows->AddSlot().AutoHeight()[SNew(STextBlock).Font(Style.Font(13)).ColorAndOpacity(Style.Text).AutoWrapText(true).Text_Lambda([this](){return GrowthStatus;})];
    Rows->AddSlot().AutoHeight().Padding(0,0,0,16)
        [Label(TEXT("장비·런 강화·확정 분배가 반영된 능력치입니다. 초안은 적용 전까지 실제 능력치를 바꾸지 않습니다."),14,Style.Muted)];
    for (const auto Stat : {EPGStatType::Attack,EPGStatType::CriticalRate,EPGStatType::CriticalDamage,
        EPGStatType::Health,EPGStatType::Defense,EPGStatType::HealAmount,EPGStatType::MovementSpeed})
    {
        Rows->AddSlot().AutoHeight().Padding(0,0,0,12)
        [SNew(SBorder).BorderImage(&Style.Card).Padding(12)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Text(PGInventoryPresentation::StatName(Stat)).Font(Style.Font(14)).ColorAndOpacity(Style.Muted)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
                [SNew(STextBlock).Tag(FName(*FString::Printf(TEXT("PGStat.%d"),int32(Stat))))
                    .Font(Style.Font(22,true)).ColorAndOpacity(Style.Text)
                    .Text_Lambda([this,Stat]()
                    {
                        const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
                        const auto* ASC=Player ? Player->GetPGAbilitySystemComponent() : nullptr;
                        return ASC ? PGInventoryPresentation::StatValue(Stat,ASC->GetCombatStat(Stat)) : FText::FromString(TEXT("—"));
                    })]
                + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
                [SNew(STextBlock).Tag(FName(*FString::Printf(TEXT("PGStatSource.%d"),int32(Stat))))
                    .Font(Style.Font(12)).ColorAndOpacity(Style.Muted).AutoWrapText(true)
                    .Text_Lambda([this,Stat]()
                    {
                        using namespace PGInventoryPresentation;
                        const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
                        auto* ASC=Player ? Player->GetPGAbilitySystemComponent() : nullptr;
                        if (!ASC) return FText::GetEmpty();
                        const auto Details=ASC->GetStatDetails(Stat);
                        if (!Details.bValid) return FText::FromString(TEXT("능력치를 불러오는 중입니다"));
                        FString Value=TEXT("기본값 · ")+StatValue(Stat,Details.Base).ToString();
                        Value+=TEXT("\n무기 고유 보너스 · ")+StatValue(Stat,Details.Weapon,true).ToString();
                        const auto* Profile=UPGProfileSubsystem::Get(this);
                        const auto* Save=Profile ? Profile->GetProfile() : nullptr;
                        float SavedTotal=0;
                        if (Save && Profile->GetCatalog())
                        {
                            for (int32 Slot=0;Slot<int32(EPGEquipmentSlot::Max);++Slot)
                            {
                                const FGuid Guid=Save->Equipment.FindRef(EPGEquipmentSlot(Slot));
                                const auto* Item=Save->Items.FindByPredicate([Guid](const auto& I){return I.Guid==Guid;});
                                const auto* Def=Item ? Profile->GetCatalog()->FindItem(Item->DefinitionId) : nullptr;
                                if (!Def || !Item->Options.Contains(Stat)) continue;
                                const int32 Amount=Item->Options.FindRef(Stat);
                                SavedTotal+=Amount;
                                Value+=TEXT("\n")+SlotName(EPGEquipmentSlot(Slot)).ToString()+TEXT(" · ")+Def->DisplayName.ToString()+TEXT(" ")+StatValue(Stat,Amount,true).ToString();
                            }
                            const int32 Reward=Save->RewardBonuses.FindRef(Stat);
                            SavedTotal+=Reward;
                            Value+=TEXT("\n런 강화 · ")+StatValue(Stat,Reward,true).ToString();
                            const int32 Growth=Profile->GetCatalog()->LevelRules.InvestmentBonus(Save->Growth,Stat);
                            SavedTotal+=Growth;
                            Value+=TEXT("\n포인트 분배 · ")+StatValue(Stat,Growth,true).ToString();
                            if (!FMath::IsNearlyEqual(SavedTotal,Details.Profile)) Value+=TEXT("\n장비·강화 반영을 확인하고 있습니다");
                        }
                        Value+=TEXT("\n기타 효과·상한 보정 · ")+StatValue(Stat,Details.Other,true).ToString();
                        for (const auto& Modifier : Details.OtherModifiers)
                        {
                            const TCHAR* Operation=Modifier.Key==EGameplayModOp::Additive ? TEXT("가산") :
                                Modifier.Key==EGameplayModOp::Multiplicitive ? TEXT("배율") :
                                Modifier.Key==EGameplayModOp::Division ? TEXT("나눗셈") :
                                Modifier.Key==EGameplayModOp::Override ? TEXT("고정값") : TEXT("연산값");
                            Value+=FString::Printf(TEXT("\n적용 중인 추가 효과 · %s %.2f"),Operation,Modifier.Value);
                        }
                        const auto* Tuning=ASC->CombatTuning ? ASC->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
                        if (Stat==EPGStatType::CriticalRate) Value+=TEXT("\n치명타 확률 상한 100%");
                        if (Stat==EPGStatType::CriticalDamage) Value+=FString::Printf(TEXT("\n치명타 배율 · %.2f배"),FMath::Max(1.f,Tuning->BaseCriticalMultiplier+Details.Current/100.f));
                        if (Stat==EPGStatType::Defense) Value+=FString::Printf(TEXT("\n기본 피해 감소 · %.2f%%"),100.f*Details.Current/(Details.Current+FMath::Max(1.f,Tuning->DefenseConstant)));
                        if (Stat==EPGStatType::Defense) Value+=FString::Printf(TEXT("\n피해 감소 = 방어력 ÷ (방어력 + %.0f)"),FMath::Max(1.f,Tuning->DefenseConstant));
                        if (Stat==EPGStatType::Health) Value+=TEXT("\n최대치 증가로 현재 생명력이 회복되지는 않습니다");
                        if (Stat==EPGStatType::HealAmount) Value+=TEXT("\n회복 동작 1회 기준 수치입니다. 자동 재생·물약 회복량과는 별도입니다");
                        return FText::FromString(Value);
                    })]]];
    }
    Rows->AddSlot().AutoHeight()[Label(TEXT("조건부 피해·발동 효과는 공격 대상과 전투 상태에 따라 별도로 계산됩니다."),12,Style.Muted)];
    Rows->AddSlot().AutoHeight().Padding(0,8,0,0)
        [SNew(STextBlock).Font(Style.Font(12)).ColorAndOpacity(Style.Muted).AutoWrapText(true)
            .Text_Lambda([this]()
            {
                const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
                const auto* ASC=Player ? Player->GetPGAbilitySystemComponent() : nullptr;
                if (!ASC) return FText::GetEmpty();
                const auto* Tuning=ASC->CombatTuning ? ASC->CombatTuning.Get() : GetDefault<UPGCombatTuningData>();
                FString Value;
                for (int32 I=1;I<int32(EPGCombatPerk::Max);++I)
                {
                    const auto Perk=EPGCombatPerk(I);
                    const int32 Amount=ASC->GetPerkPercent(Perk);
                    if (Amount>0) Value+=TEXT("보유 · ")+PGRewardText::PerkName(Perk)+TEXT(" · ")+PGRewardText::Effect(Perk,Amount,*Tuning)+TEXT("\n");
                }
                const auto State=ASC->GetBuildCombatState();
                Value+=FString::Printf(TEXT("현재 전투 상태 · 격분 %d중첩 / %.1f초\n최근 대상 · 출혈 %d중첩 / 약화 %.1f초"),
                    State.FrenzyStacks,State.FrenzySeconds,State.BleedStacks,State.WeaknessSeconds);
                if (State.bShockProc) Value+=TEXT("\n발동 · 충격파");
                if (State.bRefundProc) Value+=TEXT("\n발동 · 재사용 회복");
                if (State.bAfterimageProc) Value+=TEXT("\n발동 · 잔상");
                return FText::FromString(Value);
            })];
    return SAssignNew(StatsScroll,SScrollBox)+SScrollBox::Slot()[Rows];
}
