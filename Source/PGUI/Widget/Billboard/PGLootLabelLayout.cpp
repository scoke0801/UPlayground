#include "PGLootLabelLayout.h"

bool PGLootLabelLayout::Place(FVector2D Anchor, FVector2D Size, const FSlateRect& Bounds, const TArray<FSlateRect>& Occupied, FVector2D& Position)
{
    if (Size.X <= 0 || Size.Y <= 0 || Bounds.Right-Bounds.Left < Size.X || Bounds.Bottom-Bounds.Top < Size.Y) return false;
    // Bounded search: at most 65 candidates per visible label, no overlap fallback.
    for (int32 Column : {0,-1,1,-2,2})
        for (int32 Row = 0; Row < 13; ++Row)
        {
            const int32 Offset = Row == 0 ? 0 : (Row%2 ? -(Row+1)/2 : Row/2);
            const FVector2D Point(FMath::Clamp(Anchor.X-Size.X*.5+Column*(Size.X+8),double(Bounds.Left),double(Bounds.Right)-Size.X),
                FMath::Clamp(Anchor.Y-Size.Y+Offset*(Size.Y+6),double(Bounds.Top),double(Bounds.Bottom)-Size.Y));
            const FSlateRect Rect(Point.X,Point.Y,Point.X+Size.X,Point.Y+Size.Y);
            if (Occupied.ContainsByPredicate([&](const FSlateRect& Other){return Rect.Left < Other.Right+4 && Rect.Right+4 > Other.Left && Rect.Top < Other.Bottom+4 && Rect.Bottom+4 > Other.Top;})) continue;
            Position = Point; return true;
        }
    return false;
}
