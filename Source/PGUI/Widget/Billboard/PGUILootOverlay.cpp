#include "PGUILootOverlay.h"
#include "PGLootLabelLayout.h"
#include "PGUI/Style/PGUIStyle.h"
#include "PGUI/Style/PGUIStyleSettings.h"
#include "PGUI/Widget/Window/PGInventoryPresentation.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"

struct FPGLootLabelEntry
{
    FGuid Guid;
    FVector2D Position = FVector2D::ZeroVector, Anchor = FVector2D::ZeroVector;
    FText Name, Hint;
    FLinearColor Color;
    FSlateBrush Brush;
    bool bVisible = false;
    bool bTarget = false;
};

TSharedRef<SWidget> UPGUILootOverlay::RebuildWidget()
{
    const auto& Style = FPGUIStyle::Get();
    const auto* Settings = GetDefault<UPGUIStyleSettings>();
    Entries.Empty();
    SAssignNew(Canvas,SCanvas);
    for (int32 I = 0; I < FMath::Clamp(Settings->MaxLootLabels,3,24); ++I)
    {
        auto Entry = MakeShared<FPGLootLabelEntry>(); Entries.Add(Entry);
        Canvas->AddSlot().Position_Lambda([Entry](){return Entry->Position;}).Size(FVector2D(Settings->LootLabelWidth,48))
            [SNew(SBorder).Visibility_Lambda([Entry](){return Entry->bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})
                .BorderImage_Lambda([Entry](){return Entry->bTarget ? &FPGUIStyle::Get().Selected : &FPGUIStyle::Get().Panel;}).Padding(FMargin(8,4))
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [SNew(SBox).WidthOverride(30).HeightOverride(30)[SNew(SImage).Image(&Entry->Brush)]]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(8,0,0,0).VAlign(VAlign_Center)
                    [SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([Entry](){return Entry->Name;})
                            .Font(Style.Font(16,true)).ColorAndOpacity_Lambda([Entry](){return Entry->Color;}).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
                        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([Entry](){return Entry->Hint;})
                            .Font(Style.Font(12)).ColorAndOpacity(Style.Muted)]]]];
    }
    return SNew(SOverlay).Visibility(EVisibility::HitTestInvisible)
        + SOverlay::Slot()[Canvas.ToSharedRef()]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(24,0,24,200)
        [SNew(SBorder).BorderImage(&Style.Panel).Padding(FMargin(12,8))
            .Visibility_Lambda([this](){return HiddenCount>0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})
            [SNew(STextBlock).Text_Lambda([this](){return Overflow;}).Font(Style.Font(14)).ColorAndOpacity(Style.Muted)]];
}
void UPGUILootOverlay::NativeConstruct()
{
    Super::NativeConstruct(); SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UPGUILootOverlay::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren); Canvas.Reset(); Entries.Empty(); Icons.Empty(); CachedDrops.Empty(); RefreshIn=0;
}
void UPGUILootOverlay::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    RefreshIn -= DeltaTime;
    // Discovery is throttled; camera projection and layout must follow every rendered frame.
    if (RefreshIn<=0)
    {
        RefreshIn=.1f;
        CachedDrops.Reset();
        for (TActorIterator<APGLootDrop> It(GetWorld()); It; ++It) CachedDrops.Add(*It);
    }
    Refresh(Geometry);
}
void UPGUILootOverlay::Refresh(const FGeometry& Geometry)
{
    for (const auto& Entry : Entries) Entry->bVisible = false;
    const int32 PreviousHiddenCount=HiddenCount;
    HiddenCount=0;
    auto* PC=GetOwningPlayer();
    auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
    const auto* Profile=UPGProfileSubsystem::Get(this);
    const auto* Catalog=Profile ? Profile->GetCatalog() : nullptr;
    if (!PC || !Player || !Catalog || PC->IsMoveInputIgnored()) return;
    int32 Width=0,Height=0; PC->GetViewportSize(Width,Height);
    if (Width<=0 || Height<=0) return;
    const auto* Settings=GetDefault<UPGUIStyleSettings>();
    const FVector2D ViewSize=Geometry.GetLocalSize();
    const FSlateRect Bounds(310,170,ViewSize.X-24,ViewSize.Y-190);
    APGLootDrop* Target=nullptr;
    double BestPickupDistance=FMath::Square(Catalog->PickupRadius);
    struct FCandidate { APGLootDrop* Drop; const FPGItemDataRow* Def; double Distance; FVector2D Anchor; };
    TArray<FCandidate> Candidates;
    for (const auto& WeakDrop : CachedDrops)
    {
        auto* Drop=WeakDrop.Get();
        if (!IsValid(Drop) || !Drop->GetItem().Guid.IsValid()) continue;
        const auto* Def=Catalog->FindItem(Drop->GetItem().DefinitionId);
        const double Distance=FVector::DistSquared(Player->GetActorLocation(),Drop->GetActorLocation());
        if (!Def || Distance>FMath::Square(FMath::Max(Settings->LootLabelDistance,Catalog->PickupRadius))) continue;
        if (Player->IsGameplayInputAllowed() && (Distance<BestPickupDistance ||
            (Distance==BestPickupDistance && (!Target || Drop->GetItem().Guid<Target->GetItem().Guid))))
        { BestPickupDistance=Distance; Target=Drop; }
        FVector2D Screen;
        if (!PC->ProjectWorldLocationToScreen(Drop->GetLabelLocation(),Screen,true)) continue;
        Screen *= FVector2D(ViewSize.X/Width,ViewSize.Y/Height);
        if (Screen.X<0 || Screen.Y<0 || Screen.X>ViewSize.X || Screen.Y>ViewSize.Y) continue;
        Candidates.Add({Drop,Def,Distance,Screen});
    }
    Candidates.Sort([Target](const auto& A,const auto& B)
    {
        if ((A.Drop==Target)!=(B.Drop==Target)) return A.Drop==Target;
        if (A.Def->Rarity!=B.Def->Rarity) return A.Def->Rarity>B.Def->Rarity;
        if (A.Distance!=B.Distance) return A.Distance<B.Distance;
        return A.Drop->GetItem().Guid<B.Drop->GetItem().Guid;
    });
    TArray<FSlateRect> Occupied;
    for (const auto& Candidate : Candidates)
    {
        FVector2D Position;
        const FVector2D Size(Settings->LootLabelWidth,48);
        if (Occupied.Num()>=Entries.Num() || !PGLootLabelLayout::Place(Candidate.Anchor,Size,Bounds,Occupied,Position)) { ++HiddenCount; continue; }
        const auto& Entry=Entries[Occupied.Num()];
        const bool bPresentationChanged=Entry->Guid!=Candidate.Drop->GetItem().Guid || Entry->bTarget!=(Candidate.Drop==Target);
        Entry->Position=Position; Entry->Anchor=Candidate.Anchor; Entry->bTarget=Candidate.Drop==Target; Entry->bVisible=true;
        Occupied.Add(FSlateRect(Position.X,Position.Y,Position.X+Size.X,Position.Y+Size.Y));
        if (!bPresentationChanged) continue;
        Entry->Guid=Candidate.Drop->GetItem().Guid; Entry->Name=Candidate.Def->DisplayName;
        Entry->Color=FPGUIStyle::Get().RarityColor(Candidate.Def->Rarity);
        Entry->Hint=FText::FromString(PGInventoryPresentation::RarityName(Candidate.Def->Rarity).ToString()+TEXT(" · ")+
            PGInventoryPresentation::SlotName(Candidate.Def->Slot).ToString()+(Entry->bTarget ? TEXT("   [E] 획득") : TEXT("")));
        auto& Icon=Icons.FindOrAdd(Candidate.Def->Id);
        if (!Icon) Icon=Candidate.Def->Icon.LoadSynchronous();
        Entry->Brush=FSlateBrush(); Entry->Brush.SetResourceObject(Icon); Entry->Brush.ImageSize=FVector2D(30);
        if (Candidate.Def->IconPanel>=0 && Candidate.Def->IconPanel<3)
            Entry->Brush.SetUVRegion(FBox2f(FVector2f(Candidate.Def->IconPanel/3.f,0),FVector2f((Candidate.Def->IconPanel+1)/3.f,1)));
    }
    if (HiddenCount>0 && HiddenCount!=PreviousHiddenCount)
        Overflow=FText::FromString(FString::Printf(TEXT("전리품 +%d개 · I → 근처 전리품"),HiddenCount));
}
int32 UPGUILootOverlay::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
    FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const
{
    for (const auto& Entry : Entries)
    {
        if (!Entry->bVisible) continue;
        const FVector2D End(Entry->Position.X+GetDefault<UPGUIStyleSettings>()->LootLabelWidth*.5,Entry->Position.Y+48);
        TArray<FVector2D> Points{Entry->Anchor,End};
        FSlateDrawElement::MakeLines(Elements,Layer,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,
            Entry->Color.CopyWithNewOpacity(Entry->bTarget ? .65f : .22f),true,1.f);
    }
    return Super::NativePaint(Args,Geometry,Culling,Elements,Layer+1,Style,bEnabled);
}

bool UPGUILootOverlay::ValidateLayoutForQA() const
{
    int32 Visible=0,Targets=0;
    bool Valid=true;
    const auto* Target=APGLootDrop::FindNearestPickup(GetOwningPlayerPawn());
    TArray<FSlateRect> Rects;
    for (const auto& Entry : Entries)
    {
        if (!Entry->bVisible) continue;
        ++Visible;
        if (Entry->bTarget) { ++Targets; Valid &= Target && Target->GetItem().Guid==Entry->Guid; }
        const FSlateRect Rect(Entry->Position.X,Entry->Position.Y,Entry->Position.X+GetDefault<UPGUIStyleSettings>()->LootLabelWidth,Entry->Position.Y+48);
        for (const auto& Other : Rects)
            Valid &= !(Rect.Left<Other.Right && Rect.Right>Other.Left && Rect.Top<Other.Bottom && Rect.Bottom>Other.Top);
        Rects.Add(Rect);
    }
    Valid &= Visible<=Entries.Num() && Targets==(Target ? 1 : 0);
    UE_LOG(LogTemp,Log,TEXT("PGRewardProbe LABELS visible=%d targets=%d hidden=%d valid=%d"),Visible,Targets,HiddenCount,Valid);
    return Valid;
}
