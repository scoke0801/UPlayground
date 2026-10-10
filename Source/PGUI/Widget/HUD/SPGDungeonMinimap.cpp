#include "SPGDungeonMinimap.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"

void SPGDungeonMinimap::Construct(const FArguments& Args)
{
    SetVisibility(EVisibility::HitTestInvisible);
    // Only the player marker moves per paint; discovered topology is cached by event.
    ForceVolatile(true);
    if (Args._World) Discovery = Args._World->GetSubsystem<UPGDungeonDiscoverySubsystem>();
    if (Discovery.IsValid())
    {
        ChangedHandle = Discovery->OnMapChanged.AddSP(this, &SPGDungeonMinimap::UpdateMap);
        UpdateMap(Discovery->GetSnapshot());
    }
}

SPGDungeonMinimap::~SPGDungeonMinimap()
{
    if (Discovery.IsValid()) Discovery->OnMapChanged.Remove(ChangedHandle);
}

void SPGDungeonMinimap::UpdateMap(const FPGDungeonMapSnapshot& Snapshot)
{
    Map = Snapshot;
    FBox2D Bounds(ForceInit);
    for (const auto& Room : Map.Rooms) Bounds += FVector2D(Room.Cell.X, Room.Cell.Y);
    if (Bounds.bIsValid)
    {
        Center = Bounds.GetCenter();
        const FVector2D Extent = Bounds.GetSize() + FVector2D(2, 2);
        Scale = FMath::Min(252.f / Extent.X, 154.f / Extent.Y);
    }
    SetVisibility(Map.bActive ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
    Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SPGDungeonMinimap::OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&,
    FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
    if (!Map.bActive || !Discovery.IsValid()) return Layer;
    const auto* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
    const FVector2D Size = Geo.GetLocalSize();
    auto Point = [&](FVector2D P) { return FVector2D(Size.X*.5,Size.Y*.5-22) + FVector2D(P.X - Center.X, Center.Y - P.Y) * Scale; };
    auto Box = [&](FVector2D P, FVector2D S, FLinearColor Color, int32 Z)
    {
        FSlateDrawElement::MakeBox(Out, Z, Geo.ToPaintGeometry(S, FSlateLayoutTransform(P)), Brush,
            ESlateDrawEffect::None, Color);
    };
    Box(FVector2D::ZeroVector, Size, FLinearColor(.018f,.025f,.03f,.85f), Layer);
    for (const auto& Door : Map.Doorways)
    {
        TArray<FVector2D> Points = {Point(Door.Key), Point(Door.Value)};
        FSlateDrawElement::MakeLines(Out, Layer+1, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None,
            FLinearColor(.7f,.7f,.65f), true, 3.f);
    }
    for (const auto& Link : Map.Connections)
    {
        const auto* A = Map.Rooms.FindByPredicate([&](const auto& R) { return R.Id == Link.A; });
        const auto* B = Map.Rooms.FindByPredicate([&](const auto& R) { return R.Id == Link.B; });
        if (!A || !B) continue;
        TArray<FVector2D> Points = {Point(FVector2D(A->Cell.X,A->Cell.Y)), Point(FVector2D(B->Cell.X,B->Cell.Y))};
        FSlateDrawElement::MakeLines(Out, Layer+1, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None,
            FLinearColor(.48f,.49f,.44f), true, 3.f);
    }
    const float Width = Scale * Discovery->GetRoomRatio();
    for (const auto& Room : Map.Rooms)
    {
        const FVector2D P = Point(FVector2D(Room.Cell.X,Room.Cell.Y));
        const bool Objective = Map.Objective > 0 && (Room.Objective == Map.Objective ||
            (Room.Role == EPGDungeonRoomRole::Boss && Map.Objective == 6));
        FLinearColor Color = Objective ? FLinearColor(1.f,.72f,.24f) : FLinearColor(.48f,.49f,.44f);
        Box(P-FVector2D(Width,Width)*.5, FVector2D(Width,Width), Color, Layer+2);
        Box(P-FVector2D(Width-2,Width-2)*.5, FVector2D(Width-2,Width-2), FLinearColor(.07f,.09f,.10f), Layer+3);
        const bool Treasure=Room.Role==EPGDungeonRoomRole::Treasure && Map.bHasRewards;
        if (Treasure) Color=Map.ClaimedTreasures.Contains(Room.Id) ? FLinearColor(.45f,.45f,.45f) : FLinearColor(.9f,.7f,.25f);
        if (Room.Role == EPGDungeonRoomRole::Entrance || Room.Role == EPGDungeonRoomRole::Boss || Objective || Treasure)
        {
            const FString Text = Room.Role == EPGDungeonRoomRole::Entrance ? TEXT("입구") :
                Room.Role == EPGDungeonRoomRole::Boss ? TEXT("보스") : Treasure ?
                (Map.ClaimedTreasures.Contains(Room.Id) ? TEXT("획득") : TEXT("보물")) : TEXT("목표");
            // Keep labels outside the room so the live player marker cannot cover them.
            FSlateDrawElement::MakeText(Out, Layer+4, Geo.ToPaintGeometry(FVector2D(32,16),
                FSlateLayoutTransform(P-FVector2D(14,Width*.5f+17))),
                Text, FCoreStyle::GetDefaultFontStyle("Bold", 13), ESlateDrawEffect::None, Color);
        }
    }
    if (auto* Player = UGameplayStatics::GetPlayerPawn(Discovery.Get(), 0))
    {
        FVector2D P = Point(Discovery->ToMapPosition(Player->GetActorLocation()));
        P.X = FMath::Clamp(P.X, 4., Size.X-4.); P.Y = FMath::Clamp(P.Y, 4., Size.Y-4.);
        Box(P-FVector2D(3,3), FVector2D(6,6), FLinearColor(.3f,.9f,1.f), Layer+5);
        if (const auto* PC=Cast<APlayerController>(Player->GetController()))
        {
            FVector Camera; FRotator Rotation; PC->GetPlayerViewPoint(Camera,Rotation);
            const FVector Forward=Rotation.Vector().GetSafeNormal2D();
            const FVector2D Direction(Forward.X,-Forward.Y), Side(-Direction.Y,Direction.X);
            TArray<FVector2D> Arrow={P+Direction*5-Side*4,P+Direction*12,P+Direction*5+Side*4};
            FSlateDrawElement::MakeLines(Out,Layer+5,Geo.ToPaintGeometry(),Arrow,ESlateDrawEffect::None,FLinearColor(.3f,.9f,1.f),true,1.5f);
        }
    }
    FSlateDrawElement::MakeText(Out,Layer+5,Geo.ToPaintGeometry(FVector2D(260,38),FSlateLayoutTransform(FVector2D(10,Size.Y-43))),
        FText::FromString(TEXT("청록: 내 위치·시야\n금색: 목표·보물")),FCoreStyle::GetDefaultFontStyle("Regular",12),ESlateDrawEffect::None,FLinearColor(.8f,.8f,.75f));
    return Layer+5;
}
