#include "PGUIMainHUD.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGData/DataAsset/Input/DataAsset_InputConfig.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
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

namespace PGMainHUD
{
    const FLinearColor Gold(.67f,.51f,.29f);
    const FLinearColor Ink(.012f,.016f,.021f,.94f);
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
    static ConstructorHelpers::FObjectFinder<UTexture2D> Plate(TEXT("/Game/UI/Main/T_MainHUDPlate.T_MainHUDPlate"));
    PlateTexture = Plate.Object;
    SkillTextures.SetNum(8);
    ResourceStyle.SetBackgroundImage(FSlateColorBrush(FLinearColor(.035f,.025f,.02f)))
        .SetFillImage(FSlateColorBrush(FLinearColor::White));
}

TSharedRef<SWidget> UPGUIMainHUD::MakeResource(bool bHealth)
{
    const FLinearColor Color = bHealth ? FLinearColor(.62f,.035f,.025f) : FLinearColor(.88f,.38f,.06f);
    return SNew(SBox).WidthOverride(178).Padding(FMargin(12,0))
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [SNew(STextBlock).Text(FText::FromString(bHealth ? TEXT("생명력") : TEXT("분노"))).ColorAndOpacity(PGMainHUD::Gold).Font(FCoreStyle::GetDefaultFontStyle("Bold",12))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,9)
        [SNew(SBox).HeightOverride(12)[SNew(SProgressBar).Style(&ResourceStyle).FillColorAndOpacity(Color).Percent_Lambda([this,bHealth](){return bHealth ? HealthRatio : RageRatio;})]]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",16)).Text_Lambda([this,bHealth](){return bHealth ? HealthText : RageText;})]
    ];
}

TSharedRef<SWidget> UPGUIMainHUD::MakeSkill(int32 Index)
{
    return SNew(SBox).WidthOverride(66).HeightOverride(86).Padding(3)
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SBox).HeightOverride(58)
            [SNew(SButton).IsFocusable(false).ContentPadding(3)
                .ButtonColorAndOpacity(PGMainHUD::Gold)
                .IsEnabled_Lambda([this,Index](){return bCanAct && SkillIds[Index] > 0 && Cooldowns[Index] <= 0.f;})
                .ToolTipText_Lambda([this,Index](){return SkillNames[Index];})
                .OnClicked_Lambda([this,Index](){return ActivateSlot(Index);})
                [SNew(SOverlay)
                    + SOverlay::Slot()[SNew(SImage).Image(&SkillBrushes[Index])]
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",14)).ShadowOffset(FVector2D(1,1))
                        .Text_Lambda([this,Index]()
                        {
                            if(Cooldowns[Index]>0.f) return FText::FromString(FString::Printf(TEXT("%.1f"),Cooldowns[Index]));
                            if(SkillIds[Index]<=0) return FText::FromString(TEXT("—"));
                            if(SkillTextures[Index]) return FText::GetEmpty();
                            return FText::FromString(Index==0 ? TEXT("공격") : Index==7 ? TEXT("회피") : FString::FromInt(Index));
                        })]
                ]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0,5).HAlign(HAlign_Center)
        [SNew(SBox).WidthOverride(58).HeightOverride(15)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",10)).ColorAndOpacity(PGMainHUD::Gold).Text_Lambda([this,Index](){return KeyLabels[Index];})]]]
    ];
}

TSharedRef<SWidget> UPGUIMainHUD::RebuildWidget()
{
    PlateBrush.SetResourceObject(PlateTexture);
    PlateBrush.ImageSize = FVector2D(1080,240);
    PlateBrush.DrawAs = PlateTexture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
    auto Skills = SNew(SHorizontalBox);
    for (int32 Index=0; Index<8; ++Index) Skills->AddSlot().AutoWidth()[MakeSkill(Index)];
    return SNew(SSafeZone).Visibility(EVisibility::SelfHitTestInvisible)
    [SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(32,26)
        [SNew(SBorder).Visibility(EVisibility::HitTestInvisible).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PGMainHUD::Ink).Padding(FMargin(20,14))
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("U P L A Y G R O U N D"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",10)).ColorAndOpacity(PGMainHUD::Gold)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",23)).Text_Lambda([this](){return StageTitle;})]
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).Text_Lambda([this](){return Objective;})]
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(32,26)
        [SNew(SButton).IsFocusable(false).ContentPadding(FMargin(18,12)).ButtonColorAndOpacity(PGMainHUD::Ink)
            .OnClicked_Lambda([this](){if(auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->ToggleInventory(); return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("장비 · 빌드   [I]"))).ColorAndOpacity(PGMainHUD::Gold)]]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(12,0,12,8)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
            [SNew(SBox).WidthOverride(1080).HeightOverride(210)
                [SNew(SOverlay)
                    + SOverlay::Slot()[SNew(SImage).Visibility(EVisibility::HitTestInvisible).Image(&PlateBrush)]
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0,0,0,8)
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeResource(true)]
                        + SHorizontalBox::Slot().AutoWidth()[Skills]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeResource(false)]]
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,14)
                    [SNew(STextBlock).Visibility(EVisibility::HitTestInvisible).Font(FCoreStyle::GetDefaultFontStyle("Regular",11)).ColorAndOpacity(PGMainHUD::Gold).Text(FText::FromString(TEXT("W A S D  이동     ·     E  아이템 획득     ·     I  장비와 빌드")))]
                ]
            ]
        ]
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
    for(int32 Index=0;Index<8;++Index)
    {
        KeyLabels[Index]=FText::FromString(TEXT("—"));
        const auto* Config=Player ? Player->GetInputConfig() : nullptr;
        const auto* PC=GetOwningPlayer();
        const auto* Local=PC ? PC->GetLocalPlayer() : nullptr;
        const auto* Inputs=Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
        if(Config && Inputs)
            for(const auto& Binding:Config->AbilityInputActions)
                if(Binding.InputTag==PGMainHUD::Tag(Index) && Binding.InputAction)
                {
                    const auto Keys=Inputs->QueryKeysMappedToAction(Binding.InputAction);
                    if(!Keys.IsEmpty()) KeyLabels[Index]=Keys[0].GetDisplayName(false);
                    break;
                }
        const auto* Data=Player && Player->GetSkillHandler() ? Player->GetSkillHandler()->GetSkillData(PGMainHUD::Slot(Index)) : nullptr;
        const int32 Id=Data ? Data->SkillId : 0;
        if(Id>0 && KeyLabels[Index].ToString()==TEXT("—")) KeyLabels[Index]=FText::FromString(TEXT("클릭"));
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
        case EPGStageState::InProgress: Goal=FString::Printf(TEXT("적을 처치하세요   ·   남은 적 %d"),Stage->GetRemainingMonsters());break;
        case EPGStageState::RewardPhase: Goal=TEXT("보상을 선택해 빌드를 강화하세요");break;
        case EPGStageState::Failed: Goal=TEXT("시련 실패 · 재도전을 기다립니다");break;
        case EPGStageState::Finished: Goal=TEXT("모든 시련을 완료했습니다");break;
        case EPGStageState::Completed: Goal=TEXT("시련 완료");break;
        default: break;
    }
    Objective=FText::FromString(Goal);
}
