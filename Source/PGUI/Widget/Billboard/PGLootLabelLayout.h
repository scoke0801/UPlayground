#pragma once
#include "CoreMinimal.h"

namespace PGLootLabelLayout
{
    /** Inputs are already ordered by pickup target, rarity, distance and stable identity. */
    PGUI_API bool Place(FVector2D Anchor, FVector2D Size, const FSlateRect& Bounds, const TArray<FSlateRect>& Occupied, FVector2D& Position);
}
