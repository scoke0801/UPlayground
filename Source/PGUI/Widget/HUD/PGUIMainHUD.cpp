#include "PGUIMainHUD.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace PGMainHUD
{
    const FLinearColor Mint(.32f, .91f, .72f);
    const FLinearColor Lavender(.63f, .57f, 1.f);
    const FLinearColor Text(.93f, .96f, 1.f);
    const FLinearColor Muted(.55f, .64f, .75f);
    const FSlateRoundedBoxBrush Panel(FLinearColor(.016f, .025f, .047f, .92f), 12.f, FLinearColor(.25f, .34f, .46f, .5f), 1.f);
    const FSlateRoundedBoxBrush Card(FLinearColor(.035f, .052f, .085f, .96f), 10.f, FLinearColor(.28f, .38f, .49f, .7f), 1.f);
    const FSlateRoundedBoxBrush Hover(FLinearColor(.065f, .13f, .16f), 10.f, Mint, 1.5f);
    const FSlateRoundedBoxBrush Pressed(FLinearColor(.025f, .075f, .09f), 10.f, Mint, 2.f);
    const FSlateRoundedBoxBrush Cooldown(FLinearColor(.008f, .012f, .03f, .78f), 8.f);
    const FButtonStyle Button = FButtonStyle().SetNormal(Card).SetHovered(Hover).SetPressed(Pressed)
        .SetDisabled(Card).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    EPGSkillSlot Slot(int32 Index) { return Index == 7 ? EPGSkillSlot::SkillSlot_Roll : static_cast<EPGSkillSlot>(Index); }
    FGameplayTag Tag(int32 Index)
    {
        const FGameplayTag Tags[] = {PGGamePlayTags::InputTag_Skill_Normal, PGGamePlayTags::InputTag_Skill_Slot1,
            PGGamePlayTags::InputTag_Skill_Slot2, PGGamePlayTags::InputTag_Skill_Slot3, PGGamePlayTags::InputTag_Skill_Slot4,
            PGGamePlayTags::InputTag_Skill_Slot5, PGGamePlayTags::InputTag_Skill_Slot6};
        return Index < 7 ? Tags[Index] : PGGamePlayTags::InputTag_Roll;
    }
}

UPGUIMainHUD::UPGUIMainHUD(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(false);
    SkillTextures.SetNum(8);
    ResourceStyle.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(.07f,.10f,.15f), 3.f))
        .SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 3.f));
}

TSharedRef<SWidget> UPGUIMainHUD::MakeResource(bool bHealth)
{
    return SNew(SBox).WidthOverride(bHealth ? 300.f : 180.f)
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,7)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(STextBlock).Text_Lambda([this,bHealth](){return FText::FromString(bHealth ? TEXT("생명력") : bRogueHUD ? TEXT("격분") : TEXT("분노"));})
                .ColorAndOpacity(bHealth ? PGMainHUD::Mint : PGMainHUD::Lavender).Font(FCoreStyle::GetDefaultFontStyle("Bold",11))]
            + SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right)
            [SNew(STextBlock).ColorAndOpacity(PGMainHUD::Text).Font(FCoreStyle::GetDefaultFontStyle("Bold",13))
                .Text_Lambda([this,bHealth](){return bHealth ? HealthText : RageText;})]]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SBox).HeightOverride(6)
            [SNew(SProgressBar).Style(&ResourceStyle).BorderPadding(FVector2D::ZeroVector)
                .FillColorAndOpacity_Lambda([this,bHealth]()
                {return bHealth ? (HealthRatio <= .25f ? FLinearColor(1.f,.22f,.28f) : PGMainHUD::Mint) : PGMainHUD::Lavender;})
                .Percent_Lambda([this,bHealth](){return bHealth ? HealthRatio : RageRatio;})]]
    ];
}

TSharedRef<SWidget> UPGUIMainHUD::MakeSkill(int32 Index)
{
    const FLinearColor Accent = Index == 7 ? PGMainHUD::Lavender : PGMainHUD::Mint;
    return SNew(SBox).WidthOverride(62).HeightOverride(64)
    .Visibility_Lambda([this,Index](){return SkillIds[Index] > 0 ? EVisibility::Visible : EVisibility::Collapsed;})
    [SNew(SButton).ButtonStyle(&PGMainHUD::Button).IsFocusable(false).ContentPadding(3)
        .IsEnabled_Lambda([this,Index](){return bCanAct && SkillIds[Index] > 0 && Cooldowns[Index] <= 0.f;})
        .ToolTipText_Lambda([this,Index](){return SkillNames[Index];})
        .OnClicked_Lambda([this,Index](){return ActivateSlot(Index);})
        [SNew(SOverlay)
            + SOverlay::Slot().Padding(1,1,1,5)[SNew(SImage).Image(&SkillBrushes[Index])]
            + SOverlay::Slot()
            [SNew(SBorder).BorderImage(&PGMainHUD::Cooldown)
                .Visibility_Lambda([this,Index](){return Cooldowns[Index] > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",16)).ColorAndOpacity(PGMainHUD::Text)
                .ShadowOffset(FVector2D(0,1)).Text_Lambda([this,Index]()
                {
                    if(Cooldowns[Index]>0.f) return FText::FromString(FString::Printf(TEXT("%.1f"),Cooldowns[Index]));
                    if(SkillIds[Index]<=0) return FText::FromString(TEXT("·"));
                    if(SkillTextures[Index]) return FText::GetEmpty();
                    return FText::FromString(Index==0 ? TEXT("공격") : Index==7 ? TEXT("회피") : FString::FromInt(Index));
                })]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
            [SNew(SBox).WidthOverride(18).HeightOverride(2)
                [SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .ColorAndOpacity_Lambda([this,Index,Accent](){return SkillIds[Index]>0 ? Accent : FLinearColor(.14f,.18f,.24f);})]]
        ]
    ];
}

TSharedRef<SWidget> UPGUIMainHUD::RebuildWidget()
{
    auto Skills = SNew(SHorizontalBox);
    for (int32 Index=0; Index<8; ++Index)
        Skills->AddSlot().AutoWidth().Padding(Index == 7 ? 14.f : Index == 0 ? 0.f : 6.f,0,0,0)[MakeSkill(Index)];
    return SNew(SSafeZone).Visibility(EVisibility::SelfHitTestInvisible)
    [SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(32,28)
        [SNew(SBorder).Visibility(EVisibility::HitTestInvisible).BorderImage(&PGMainHUD::Panel).Padding(FMargin(20,16))
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Text(FText::FromString(TEXT("T R I A L  /  시련")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold",10)).ColorAndOpacity(PGMainHUD::Mint)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,8)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",25)).ColorAndOpacity(PGMainHUD::Text)
                    .Text_Lambda([this](){return StageTitle;})]
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(PGMainHUD::Muted)
                    .WrapTextAt(340).Text_Lambda([this](){return Objective;})]
            ]]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(32,28)
        [SNew(SButton).ButtonStyle(&PGMainHUD::Button).IsFocusable(false).ContentPadding(FMargin(20,13))
            .OnClicked_Lambda([this](){if(auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->ToggleInventory(); return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("장비 · 빌드"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",12))
                .ColorAndOpacity(PGMainHUD::Text)]]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(32,92)
        [SNew(SButton).ButtonStyle(&PGMainHUD::Button).IsFocusable(false).ContentPadding(FMargin(20,13))
            .Visibility_Lambda([this](){ return Stage.IsValid() && Stage->IsManualReady() && Stage->CanReady() ? EVisibility::Visible : EVisibility::Collapsed; })
            .OnClicked_Lambda([this](){ if (Stage.IsValid()) Stage->ReadyForNextStage(); return FReply::Handled(); })
            [SNew(STextBlock).Text(FText::FromString(TEXT("준비 완료 →"))).ColorAndOpacity(PGMainHUD::Mint)]]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(24,0,24,30)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
                [SNew(SBorder).Visibility(EVisibility::HitTestInvisible).BorderImage(&PGMainHUD::Panel).Padding(FMargin(18,12))
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()[MakeResource(true)]
                        + SHorizontalBox::Slot().AutoWidth().Padding(30,0,0,0)[MakeResource(false)]]]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Skills]
            ]]
    ];
}

void UPGUIMainHUD::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Refresh();
    GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&ThisClass::Refresh,FMath::Max(.05f,RefreshInterval),true);
}
void UPGUIMainHUD::NativeDestruct()
{
    if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
    Stage.Reset();
    Super::NativeDestruct();
}
void UPGUIMainHUD::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
}

FReply UPGUIMainHUD::ActivateSlot(int32 Index)
{
    auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
    if(Player && Player->IsGameplayInputAllowed() && Index >= 0 && Index < 8)
    {
        auto* ASC=Player->GetPGAbilitySystemComponent();
        if(ASC) { ASC->OnAbilityInputPressed(PGMainHUD::Tag(Index)); ASC->OnAbilityInputReleased(PGMainHUD::Tag(Index)); }
    }
    return FReply::Handled();
}

void UPGUIMainHUD::Refresh()
{
    auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
    bCanAct = Player && Player->IsGameplayInputAllowed();
    const auto* ASC = Player ? Player->GetPGAbilitySystemComponent() : nullptr;
    const auto* Stats = ASC ? ASC->GetSet<UPGAtrributeSet>() : nullptr;
    const float Health=Stats ? Stats->GetCurrentHealth() : 0.f;
    const float MaxHealth=Stats ? Stats->GetMaxHealth() : 0.f;
    const float Rage=Stats ? Stats->GetCurrentRage() : 0.f;
    const float MaxRage=Stats ? Stats->GetMaxRage() : 0.f;
    HealthRatio=MaxHealth>0 ? FMath::Clamp(Health/MaxHealth,0.f,1.f) : 0.f;
    RageRatio=MaxRage>0 ? FMath::Clamp(Rage/MaxRage,0.f,1.f) : 0.f;
    HealthText=FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),Health,MaxHealth));
    RageText=FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),Rage,MaxRage));
    const auto* Profile = UPGProfileSubsystem::Get(this);
    bRogueHUD = Profile && Profile->GetCatalog() && Profile->GetCatalog()->bRoguelikeRuns;
    if (bRogueHUD && ASC)
    {
        const float Bonus = ASC->GetFrenzyRate()-1.f;
        RageRatio=FMath::Clamp(Bonus/.5f,0.f,1.f);
        RageText=FText::FromString(FString::Printf(TEXT("공격 속도 +%.0f%%"),Bonus*100.f));
    }
    for(int32 Index=0;Index<8;++Index)
    {
        const auto* Data=Player && Player->GetSkillHandler() ? Player->GetSkillHandler()->GetSkillData(PGMainHUD::Slot(Index)) : nullptr;
        const int32 Id=Data ? Data->SkillId : 0;
        Cooldowns[Index]=Data ? FMath::Max(0.f,Data->GetRemainingCooldown()) : 0.f;
        if(SkillIds[Index]!=Id)
        {
            SkillIds[Index]=Id; SkillTextures[Index]=nullptr;
            SkillNames[Index]=FText::FromString(TEXT("장착한 스킬이 없습니다"));
            if(auto* Tables=UPGDataTableManager::Get(this); Tables && Id>0)
                if(const auto* Row=Tables->GetRowData<FPGSkillDataRow>(Id))
                {
                    SkillNames[Index]=FText::FromString(Row->Desc);
                    SkillTextures[Index]=Cast<UTexture2D>(Row->SkillIconPath.TryLoad());
                }
            SkillBrushes[Index].SetResourceObject(SkillTextures[Index]);
            SkillBrushes[Index].ImageSize=FVector2D(48,48);
            SkillBrushes[Index].DrawAs=SkillTextures[Index] ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
        }
    }
    if(!Stage.IsValid() && GetWorld()) for(TActorIterator<APGStageManager> It(GetWorld());It;++It) {Stage=*It;break;}
    StageTitle=FText::FromString(Stage.IsValid() ? FString::Printf(TEXT("시련  %02d"),Stage->GetCurrentStageId()) : TEXT("탐험"));
    FString Goal=TEXT("전투를 준비하세요");
    if(Stage.IsValid()) switch(Stage->GetCurrentStageState())
    {
        case EPGStageState::InProgress: Goal=FString::Printf(TEXT("웨이브 %d / %d   ·   남은 적 %d"),Stage->GetCurrentWaveNumber(),Stage->GetWaveCount(),Stage->GetRemainingMonsters());break;
        case EPGStageState::WaveIntermission: Goal=FString::Printf(TEXT("웨이브 %d / %d   ·   %.0f초 후 시작"),Stage->GetCurrentWaveNumber(),Stage->GetWaveCount(),FMath::CeilToFloat(Stage->GetWaveTimeRemaining()));break;
        case EPGStageState::BuildPhase: Goal=Stage->IsManualReady() ? TEXT("구간 완료 · 강화와 장비를 정비하세요") : FString::Printf(TEXT("스테이지 완료 · 빌드 시간 %d초"),FMath::CeilToInt(Stage->GetBuildTimeRemaining()));break;
        case EPGStageState::RewardPhase: Goal=TEXT("보상을 선택해 빌드를 강화하세요");break;
        case EPGStageState::Failed: Goal=TEXT("시련 실패 · 재도전을 기다립니다");break;
        case EPGStageState::RunPreparation: Goal=TEXT("장비 · 빌드에서 검술 선택 후 준비 완료\nWASD 이동 · 마우스 조준 · I 장비 · E 획득");break;
        case EPGStageState::Finished: Goal=TEXT("모든 시련을 완료했습니다");break;
        case EPGStageState::Completed: Goal=TEXT("시련 완료");break;
        default: break;
    }
    Objective=FText::FromString(Goal);
}
