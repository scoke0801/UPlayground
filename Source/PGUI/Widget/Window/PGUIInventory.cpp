#include "PGUIInventory.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "EngineUtils.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UPGUIInventory::RebuildWidget()
{
    auto Root = SNew(SBorder).Padding(24).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.018f,.025f,.04f,.98f))
    [ SNew(SBox).WidthOverride(980).HeightOverride(840)
      [ SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",28)).Text(FText::FromString(TEXT("장비와 빌드  |  I 닫기"))) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0,12)[SAssignNew(StatusText, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",20))]
        + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(Rows, SVerticalBox)]]
      ] ];
    Refresh();
    return Root;
}
void UPGUIInventory::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren); Rows.Reset(); StatusText.Reset();
}
void UPGUIInventory::Refresh()
{
    if (!Rows) return;
    Rows->ClearChildren();
    auto* Profile = UPGProfileSubsystem::Get(this);
    if (!Profile || !Profile->GetCatalog()) return;
    const auto* Save = Profile->GetProfile(); const auto* Catalog = Profile->GetCatalog();
    StatusText->SetText(FText::FromString(FString::Printf(TEXT("%s | 가방 %d/%d | 구간 %d"), *Profile->Status, Save->Items.Num(), Catalog->BagCapacity, Save->Checkpoint)));
    auto Button = [this](FString Label, TFunction<void()> Action, FLinearColor Color = FLinearColor::White)
    {
        Rows->AddSlot().AutoHeight().Padding(0,5)[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(12,8)).OnClicked_Lambda([this, Action](){ Action(); Refresh(); return FReply::Handled(); })
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",20)).Text(FText::FromString(Label)).ColorAndOpacity(Color).AutoWrapText(true)]];
    };
    if (Profile->IsSaveBlocked())
        Button(bConfirmRecovery ? TEXT("복구 확인: 유효한 백업이 없으면 빈 프로필로 시작합니다") : TEXT("원본을 보관하고 저장 복구"), [this, Profile](){
            if (bConfirmRecovery) { Profile->RecoverSave(); bConfirmRecovery = false; } else bConfirmRecovery = true;
        });
    if (Catalog->bRoguelikeRuns)
    {
        Rows->AddSlot().AutoHeight().Padding(0,8)[SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("런 기록 · 최고 구간 %d · 승리 %d회\n장비와 강화는 이번 도전에서만 유지됩니다"), Save->BestStage, Save->CompletedRuns)))
            .ColorAndOpacity(FLinearColor(.6f,.85f,1.f))];
        for (auto Chosen : Save->SelectedRewards)
            if (const auto* Reward = PGData()->GetRowData<FPGRewardStatDataRow>(Chosen.Key))
                Rows->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).AutoWrapText(true)
                    .Text(FText::FromString(FString::Printf(TEXT("%s ×%d — %s"), *Reward->DisplayName.ToString(), Chosen.Value, *Reward->PlaystyleDescription.ToString())))
                    .ColorAndOpacity(FLinearColor(.75f,.7f,1.f))];
    }
    for (auto EquipmentSlot : {EPGEquipmentSlot::Weapon, EPGEquipmentSlot::Accessory})
        Button(EquipmentSlot == EPGEquipmentSlot::Weapon ? TEXT("무기 해제") : TEXT("장신구 해제"), [Profile, EquipmentSlot](){ Profile->Unequip(EquipmentSlot); });
    for (const auto& Item : Save->Items)
    {
        const auto* Def = Catalog->FindItem(Item.DefinitionId);
        if (!Def) { Button(FString::Printf(TEXT("알 수 없는 ID %d (보관)"), Item.DefinitionId), [](){}); continue; }
        const FGuid* Current = Save->Equipment.Find(Def->Slot);
        const auto* Equipped = Current ? Save->Items.FindByPredicate([&](const auto& I){ return I.Guid == *Current; }) : nullptr;
        FString Label = FString::Printf(TEXT("[%s] %s %s"), Def->Rarity == EPGItemRarity::Rare ? TEXT("희귀") : Def->Rarity == EPGItemRarity::Magic ? TEXT("마법") : TEXT("일반"), *Def->DisplayName.ToString(), Current && *Current == Item.Guid ? TEXT("[장착 중]") : TEXT("[장착]"));
        TSet<EPGStatType> Keys; for (auto P : Item.Options) Keys.Add(P.Key); if (Equipped) for (auto P : Equipped->Options) Keys.Add(P.Key);
        if (!Def->EffectDescription.IsEmpty()) Label += TEXT("\n") + Def->EffectDescription.ToString();
        for (auto Key : Keys)
        {
            const int32 Value = Item.Options.FindRef(Key); const int32 Delta = Value - (Equipped ? Equipped->Options.FindRef(Key) : 0);
            Label += FString::Printf(TEXT("\n%s %d (%+d)"), *StaticEnum<EPGStatType>()->GetDisplayNameTextByValue(static_cast<int64>(Key)).ToString(), Value, Delta);
        }
        Button(Label, [Profile, Id=Item.Guid](){ Profile->Equip(Id); }, Def->Rarity == EPGItemRarity::Rare ? FLinearColor::Yellow : Def->Rarity == EPGItemRarity::Magic ? FLinearColor(.3f,.8f,1) : FLinearColor::White);
        if (!Current || *Current != Item.Guid)
            Button(PendingDiscard == Item.Guid ? TEXT("정말 버리기 — 다시 클릭하면 영구 삭제") : TEXT("이 아이템 버리기"), [this, Profile, Id=Item.Guid](){
                if (PendingDiscard == Id) { Profile->Discard(Id); PendingDiscard.Invalidate(); } else PendingDiscard = Id;
            });
    }
    for (const auto& Build : Catalog->Builds)
    {
        FString Text = FString::Printf(TEXT("빌드: %s | 필요 클리어 %d %s"), *Build.DisplayName.ToString(), Build.RequiredClears, Build.Id == Save->BuildId ? TEXT("[선택됨]") : TEXT(""));
        if (Build.Skills.Num() > 1) Text += FString::Printf(TEXT("\n액티브 스킬 대기시간 %.0f%% 감소"), (1.f - Build.Skills[1].CooldownScale) * 100.f);
        if (Build.Skills.Num() > 2 && Build.Skills[1].CooldownSeconds >= 0.f)
            Text += FString::Printf(TEXT(" · 주력 스킬 %.1f초 / %.1f초"), Build.Skills[1].CooldownSeconds * Build.Skills[1].CooldownScale, Build.Skills[2].CooldownSeconds * Build.Skills[2].CooldownScale);
        Button(Text, [Profile, Id=Build.Id](){ Profile->SelectBuild(Id); });
    }
    Button(FString::Printf(TEXT("근처 드랍 필터: %s (클릭 변경)"), MinRarity == 0 ? TEXT("전체") : MinRarity == 1 ? TEXT("마법 이상") : TEXT("희귀")), [this](){ MinRarity = (MinRarity+1)%3; });
    // A single vertical list provides non-overlapping labels for crowded ground drops.
    for (TActorIterator<APGLootDrop> It(GetWorld()); It; ++It)
    {
        if (!GetOwningPlayerPawn() || FVector::DistSquared(It->GetActorLocation(), GetOwningPlayerPawn()->GetActorLocation()) > FMath::Square(Catalog->PickupRadius)) continue;
        const auto* Def = Catalog->FindItem(It->GetItem().DefinitionId);
        if (!Def || static_cast<int32>(Def->Rarity) < MinRarity) continue;
        TWeakObjectPtr<APGLootDrop> Drop(*It);
        Button(TEXT("획득: ") + Def->DisplayName.ToString(), [this, Drop](){ if (Drop.IsValid()) Drop->TryPickup(GetOwningPlayerPawn()); });
    }
}
