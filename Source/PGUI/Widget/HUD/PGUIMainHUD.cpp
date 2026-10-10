#include "PGUIMainHUD.h"
#include "PGCombatHUDStyle.h"
#include "SPGResourceOrb.h"
#include "PGUI/Style/PGUIStyle.h"
#include "PGUI/Style/PGUIStyleSettings.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Combat/PGBossPresentation.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGData/DataTable/Skill/PGPlayerSkillText.h"
#include "PGData/DataAsset/Input/DataAsset_InputConfig.h"
#include "EnhancedInputSubsystems.h"
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
    ResourceStyle.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(.025f,.016f,.012f), 1.f))
        .SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 3.f));
}

TSharedRef<SWidget> UPGUIMainHUD::MakeResource(bool bHealth)
{
    const auto& Art=FPGCombatHUDStyle::Get();
    return SNew(SBox).WidthOverride(144).HeightOverride(172)
    .VAlign(VAlign_Top)
    [SNew(SBox).HeightOverride(144)
        [SNew(SOverlay)
            + SOverlay::Slot()
            [SNew(SPGResourceOrb).Frame(&OrbFrameBrush).Health(bHealth)
                .Ratio_Lambda([this,bHealth](){return bHealth ? HealthRatio : RageRatio;})]
            // Reserve a chord inside the glass; long values shrink without crossing the frame.
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SBox).WidthOverride(82).HeightOverride(19)
                [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                    [SNew(STextBlock).Text_Lambda([this,bHealth](){return bHealth ? HealthText : RageText;})
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold",11)).ColorAndOpacity(Art.Ivory)
                        .ShadowColorAndOpacity(FLinearColor(0,0,0,.95f)).ShadowOffset(FVector2D(0,1.5f))]]]
        ]
    ];
}

TSharedRef<SWidget> UPGUIMainHUD::MakeSkill(int32 Index)
{
    const auto& Art=FPGCombatHUDStyle::Get();
    return SNew(SBox).WidthOverride(56).HeightOverride(70)
    .Visibility_Lambda([this,Index](){return SkillIds[Index]>0 ? EVisibility::Visible : EVisibility::Collapsed;})
    [SNew(SButton).ButtonStyle(&Art.Button).IsFocusable(false).ContentPadding(3)
        // Keep the icon readable during preparation; gameplay activation still uses the input guard.
        .ToolTipText_Lambda([this,Index](){return FText::FromString(SkillNames[Index].ToString()+TEXT("\n")+SkillReasons[Index].ToString());})
        .OnClicked_Lambda([this,Index](){return bCanAct && SkillIds[Index]>0 && Cooldowns[Index]<=0.f ? ActivateSlot(Index) : FReply::Handled();})
        [SNew(SOverlay)
            + SOverlay::Slot().Padding(0,0,0,14)[SNew(SImage).Image(&SkillBrushes[Index])]
            + SOverlay::Slot().Padding(0,0,0,14)
            [SNew(SBorder).BorderImage(&Art.Shade)
                .Visibility_Lambda([this,Index](){return Cooldowns[Index]>0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})]
            + SOverlay::Slot().VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(0,0,0,12)
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",17)).ColorAndOpacity(Art.Ivory).ShadowOffset(FVector2D(1,1))
                .Text_Lambda([this,Index](){return Cooldowns[Index]>0 ? FText::FromString(FString::Printf(TEXT("%.1f"),Cooldowns[Index])) : SkillTextures[Index] ? FText::GetEmpty() : FText::FromString(Index==0 ? TEXT("공격") : Index==7 ? TEXT("대시") : FString::FromInt(Index));})]
            + SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Center)
            [SNew(STextBlock).Text_Lambda([this,Index](){return SkillKeys[Index];})
                .Font(FCoreStyle::GetDefaultFontStyle("Bold",10)).ColorAndOpacity(Art.Ivory).ShadowOffset(FVector2D(1,1))]
        ]
    ];
}

TSharedRef<SWidget> UPGUIMainHUD::RebuildWidget()
{
    const auto& Art=FPGCombatHUDStyle::Get();
    OrbFrameTexture=GetDefault<UPGUIStyleSettings>()->CombatOrbFrame.LoadSynchronous();
    OrbFrameBrush=FSlateBrush();
    OrbFrameBrush.SetResourceObject(OrbFrameTexture);
    OrbFrameBrush.ImageSize=FVector2D(144,144);
    CombatPlateTexture=GetDefault<UPGUIStyleSettings>()->CombatPlate.LoadSynchronous();
    CombatPlateBrush=Art.Panel;
    if(CombatPlateTexture)
    {
        CombatPlateBrush=FSlateBrush();
        CombatPlateBrush.SetResourceObject(CombatPlateTexture);
        CombatPlateBrush.SetUVRegion(FBox2f(FVector2f(0,.10f),FVector2f(1,.90f)));
        CombatPlateBrush.ImageSize=FVector2D(320,86);
        // The authored plate already matches these shallow HUD proportions. Image drawing
        // preserves its thin bevels; Box slicing uses source-pixel corners on high-res art.
        CombatPlateBrush.DrawAs=ESlateBrushDrawType::Image;
    }
    PotionIconTexture=GetDefault<UPGUIStyleSettings>()->CombatPotionIcon.LoadSynchronous();
    PotionIconBrush=FSlateBrush();
    PotionIconBrush.SetResourceObject(PotionIconTexture);
    PotionIconBrush.SetUVRegion(FBox2f(FVector2f(.13f,0),FVector2f(.87f,.96f)));
    PotionIconBrush.ImageSize=FVector2D(42,46);
    ActionStyle=Art.Button;
    FSlateBrush Hover=CombatPlateBrush,Pressed=CombatPlateBrush;
    Hover.TintColor=FLinearColor(1.18f,1.12f,1.f);
    Pressed.TintColor=FLinearColor(.65f,.65f,.70f);
    ActionStyle.SetNormal(CombatPlateBrush).SetHovered(Hover).SetPressed(Pressed);
    auto Skills=SNew(SHorizontalBox);
    Skills->AddSlot().AutoWidth().Padding(0,0,12,0)[MakeHealingPotion()];
    for(int32 Index=0;Index<8;++Index)
        Skills->AddSlot().AutoWidth().Padding(Index==7 ? 10.f : 2.f,0,2,0)[MakeSkill(Index)];
    return SNew(SSafeZone).Visibility(EVisibility::SelfHitTestInvisible)
    [SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(4).HeightOverride(4)
            .Visibility_Lambda([this]()
            {
                const auto* PC = GetOwningPlayer();
                const auto* Player = Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
                return PC && !PC->bShowMouseCursor && Player && Player->GetCameraMode() == EPGCameraMode::Action3D
                    ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
            })
            [SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(Art.Ivory)]]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,28)
        [SNew(SBox).WidthOverride(430)
            .Visibility_Lambda([this](){return bShowBoss ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})
            [SNew(SBorder).BorderImage(&CombatPlateBrush).Padding(FMargin(16,12))
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [SNew(STextBlock).Text_Lambda([this](){return BossTitle;}).Font(FCoreStyle::GetDefaultFontStyle("Bold",16)).ColorAndOpacity(Art.Ivory)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0,7)
                    [SNew(SBox).HeightOverride(8)[SNew(SProgressBar).Style(&ResourceStyle).Percent_Lambda([this](){return BossHealth;}).FillColorAndOpacity(FLinearColor(.55f,.016f,.025f))]]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [SNew(STextBlock).Text_Lambda([this](){return BossStatus;}).ColorAndOpacity_Lambda([this](){return BossStatusColor;}).Font(FCoreStyle::GetDefaultFontStyle("Bold",11))]]]]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(28,32)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MakeStagePanel()]
            + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[MakeActions()]]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(16,0,16,12)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
            [SNew(SBox).Tag(TEXT("PGCombatHUDLayout")).WidthOverride(900).HeightOverride(172)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()[MakeResource(true)]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0,0,0,20)
                        [SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                            [SNew(SBox).HeightOverride(26)
                                [SNew(STextBlock).Text_Lambda([this](){return PotionNotice;}).Font(FCoreStyle::GetDefaultFontStyle("Bold",12))
                                    .ColorAndOpacity(Art.Ivory).ShadowOffset(FVector2D(1,1))]]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                            [SNew(SBorder).BorderImage(&CombatPlateBrush).Padding(FMargin(14,10))[Skills]]]
                        + SHorizontalBox::Slot().AutoWidth()[MakeResource(false)]]
                ]]]
    ];
}

void UPGUIMainHUD::NativeConstruct()
{
    Super::NativeConstruct();
    if (auto* Messages = UPGMessageManager::Get(this))
    {
        BossPresentationHandle = Messages->RegisterDelegate(EPGUIMessageType::BossPresentation, this, &ThisClass::OnBossPresentation);
        ConsumableHandle = Messages->RegisterDelegate(EPGUIMessageType::ConsumableChanged, this, &ThisClass::OnConsumableChanged);
    }
    for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It) It->PublishBossPresentation();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Refresh();
    GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&ThisClass::Refresh,FMath::Max(.05f,RefreshInterval),true);
}
void UPGUIMainHUD::NativeDestruct()
{
    if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
    if (auto* Messages = UPGMessageManager::Get(this)) Messages->UnregisterDelegate(EPGUIMessageType::BossPresentation, BossPresentationHandle);
    if (auto* Messages = UPGMessageManager::Get(this)) Messages->UnregisterDelegate(EPGUIMessageType::ConsumableChanged, ConsumableHandle);
    Boss.Reset();
    bShowBoss = bBossDefeated = false;
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
    RefreshHealingPotion();
    RageText=FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),Rage,MaxRage));
    const auto* Profile = UPGProfileSubsystem::Get(this);
    bRogueHUD = Profile && Profile->GetCatalog() && Profile->GetCatalog()->bRoguelikeRuns;
    if (bRogueHUD && ASC)
    {
        const auto State = ASC->GetBuildCombatState();
        RageRatio = State.FrenzyMaxStacks > 0 ? float(State.FrenzyStacks)/State.FrenzyMaxStacks : 0;
        RageText = FText::FromString(FString::Printf(TEXT("%d / %d"),State.FrenzyStacks,State.FrenzyMaxStacks));
    }
    for(int32 Index=0;Index<8;++Index)
    {
        const auto* Data=Player && Player->GetSkillHandler() ? Player->GetSkillHandler()->GetSkillData(PGMainHUD::Slot(Index)) : nullptr;
        const int32 Id=Data ? Data->SkillId : 0;
        Cooldowns[Index]=Data ? FMath::Max(0.f,Data->GetRemainingCooldown()) : 0.f;
        SkillReasons[Index]=FText::FromString(Health<=0 ? TEXT("사망") : !bCanAct ? TEXT("준비 중") : Cooldowns[Index]>0 ? TEXT("재사용 대기") : Player && !Player->CanStartSkill(Index==7) ? TEXT("동작 중") : TEXT(""));
        SkillKeys[Index]=FText::FromString(TEXT("미지정"));
        if (Player && Player->GetInputConfig() && GetOwningLocalPlayer())
            if (auto* Input=GetOwningLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                for (const auto& Mapping : Player->GetInputConfig()->AbilityInputActions)
                    if (Mapping.InputTag==PGMainHUD::Tag(Index))
                    {
                        const auto Keys=Input->QueryKeysMappedToAction(Mapping.InputAction);
                        if (!Keys.IsEmpty()) SkillKeys[Index]=Keys[0].GetDisplayName(false);
                    }
        if(SkillIds[Index]!=Id)
        {
            SkillIds[Index]=Id; SkillTextures[Index]=nullptr;
            SkillNames[Index]=FText::FromString(TEXT("장착한 스킬이 없습니다"));
            if(auto* Tables=UPGDataTableManager::Get(this); Tables && Id>0)
                if(const auto* Row=Tables->GetRowData<FPGSkillDataRow>(Id))
                {
                    SkillNames[Index]=FText::FromString(PGPlayerSkillText::Describe(*Row));
                    SkillTextures[Index]=Cast<UTexture2D>(Row->SkillIconPath.TryLoad());
                }
            SkillBrushes[Index].SetResourceObject(SkillTextures[Index]);
            SkillBrushes[Index].ImageSize=FVector2D(48,48);
            SkillBrushes[Index].DrawAs=SkillTextures[Index] ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
        }
    }
    if(!Stage.IsValid() && GetWorld()) for(TActorIterator<APGStageManager> It(GetWorld());It;++It) {Stage=*It;break;}
    StageTitle=FText::FromString(Stage.IsValid() ? FString::Printf(TEXT("제 %d 시련"),Stage->GetCurrentStageId()) : TEXT("탐험"));
    StagePhase=FText::FromString(TEXT("준비"));
    FString Goal=TEXT("검술과 장비를 정비하세요.");
    if(Stage.IsValid()) switch(Stage->GetCurrentStageState())
    {
        case EPGStageState::InProgress: StagePhase=FText::FromString(TEXT("전투 중")); Goal=FString::Printf(TEXT("공세 %d / %d   ·   남은 적 %d"),Stage->GetCurrentWaveNumber(),Stage->GetWaveCount(),Stage->GetRemainingMonsters());break;
        case EPGStageState::WaveIntermission: Goal=FString::Printf(TEXT("다음 공세까지 %.0f초"),FMath::CeilToFloat(Stage->GetWaveTimeRemaining()));break;
        case EPGStageState::BuildPhase: StagePhase=FText::FromString(TEXT("정비")); Goal=Stage->IsManualReady() ? TEXT("획득한 힘으로 다음 시련을 준비하세요.") : FString::Printf(TEXT("다음 시련까지 %d초"),FMath::CeilToInt(Stage->GetBuildTimeRemaining()));break;
        case EPGStageState::RewardPhase: Goal=TEXT("보상을 선택해 빌드를 강화하세요");break;
        case EPGStageState::Failed: StagePhase=FText::FromString(TEXT("종료")); Goal=TEXT("잠시 숨을 고르고 다시 도전하세요.");break;
        case EPGStageState::RunPreparation: Goal=TEXT("준비를 마치면 시련을 시작하세요.");break;
        case EPGStageState::Finished: StagePhase=FText::FromString(TEXT("돌파")); Goal=TEXT("모든 시련을 완료했습니다");break;
        case EPGStageState::Completed: Goal=TEXT("시련 완료");break;
        default: break;
    }
    Objective=FText::FromString(Goal);
    if (bBossDefeated && GetWorld()->GetTimeSeconds() >= BossDefeatUntil) bShowBoss = false;
    if (!Boss.IsValid() && !bBossDefeated) bShowBoss = false;
    if (Stage.IsValid() && (Stage->GetCurrentStageState() == EPGStageState::RunPreparation || Stage->GetCurrentStageState() == EPGStageState::Failed))
    {
        bShowBoss = false;
        bBossDefeated = false;
        Boss.Reset();
    }
}

void UPGUIMainHUD::OnBossPresentation(const IPGEventData* Event)
{
    if (!Event || !GetWorld()) return;
    const auto& View = *static_cast<const FPGSharedBossPresentation*>(Event);
    if (View.State == EPGBossCombatState::Hidden)
    {
        if (Boss == View.Owner && !bBossDefeated) { Boss.Reset(); bShowBoss = false; }
        return;
    }
    if (!View.Owner.IsValid() || View.Owner->GetWorld() != GetWorld()) return;
    if (Boss != View.Owner) bBossDefeated = false;
    Boss = View.Owner;
    BossTitle = View.Name;
    BossHealth = View.HealthRatio;
    bShowBoss = true;
    FString Status;
    BossStatusColor = FPGUIStyle::Get().Lavender;
    switch (View.State)
    {
    case EPGBossCombatState::Windup: Status = TEXT("위험 예고 · ") + View.Attack.ToString(); BossStatusColor = FLinearColor(1.f,.5f,.2f); break;
    case EPGBossCombatState::Attacking: Status = TEXT("공격 중 · ") + View.Attack.ToString(); BossStatusColor = FLinearColor(1.f,.3f,.3f); break;
    case EPGBossCombatState::Recovery: Status = TEXT("빈틈 · 반격 기회"); BossStatusColor = FPGUIStyle::Get().Mint; break;
    case EPGBossCombatState::Transition: Status = View.TransitionText.ToString(); break;
    case EPGBossCombatState::Guard: Status = TEXT("검막 · 측후방 공격 또는 대기"); BossStatusColor = FLinearColor(.3f,.5f,1.f); break;
    case EPGBossCombatState::Defeated:
        Status = View.Name.ToString() + TEXT(" 격파"); BossStatusColor = FPGUIStyle::Get().Mint;
        if (!bBossDefeated) BossDefeatUntil = GetWorld()->GetTimeSeconds() + View.DefeatDisplaySeconds;
        bBossDefeated = true;
        break;
    default: Status = TEXT("다음 공격 준비"); break;
    }
    BossStatus = FText::FromString(FString::Printf(TEXT("%d페이즈  ·  %s"), View.Phase, *Status));
}
