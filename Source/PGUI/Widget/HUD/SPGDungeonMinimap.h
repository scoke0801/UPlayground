#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "PGActor/Dungeon/PGDungeonDiscoverySubsystem.h"

class SPGDungeonMinimap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPGDungeonMinimap) {} SLATE_ARGUMENT(UWorld*, World) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SPGDungeonMinimap() override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(280, 230); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&,
        int32, const FWidgetStyle&, bool) const override;
private:
    void UpdateMap(const FPGDungeonMapSnapshot& Snapshot);
    TWeakObjectPtr<UPGDungeonDiscoverySubsystem> Discovery;
    FDelegateHandle ChangedHandle;
    FPGDungeonMapSnapshot Map;
    FVector2D Center = FVector2D::ZeroVector;
    float Scale = 24.f;
};
