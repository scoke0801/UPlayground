#pragma once

#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

/** Height-clipped liquid, readable even when empty; authored metal remains static. */
class SPGResourceOrb : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPGResourceOrb) : _Frame(nullptr), _Health(true) {}
        SLATE_ATTRIBUTE(float, Ratio)
        SLATE_ARGUMENT(const FSlateBrush*, Frame)
        SLATE_ARGUMENT(bool, Health)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Ratio=Args._Ratio; Frame=Args._Frame; bHealth=Args._Health; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(144,144); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const FVector2D Size=G.GetLocalSize(), C=Size*.5;
        const float R=FMath::Min(Size.X,Size.Y)*.337f;
        const float Fill=FMath::Clamp(Ratio.Get(),0.f,1.f), Surface=R*(1-2*Fill);
        const FLinearColor Base=bHealth ? FLinearColor(.58f,.012f,.023f) : FLinearColor(.72f,.25f,.016f);
        // Horizontal scanlines describe a true circular reservoir, rather than a round progress bar.
        constexpr int32 Rows=128;
        const float Step=2*R/Rows;
        for(int32 I=0;I<Rows;++I)
        {
            const float Y=-R+(I+.5f)*Step;
            const float X=FMath::Sqrt(FMath::Max(0.f,R*R-Y*Y));
            const bool bLiquid=Fill>0 && Y>=Surface;
            const float Light=.28f+.65f*FMath::Sqrt(FMath::Max(0.f,1-FMath::Square(Y/R)));
            FLinearColor Color=bLiquid ? Base*Light : FLinearColor(.016f,.012f,.011f);
            if(bLiquid && Y-Surface<Step*2) Color=Base*1.5f;
            Color.A=1;
            const float Glint=FMath::Exp(-FMath::Square((Y/R+.48f)*8.f));
            TArray<FSlateGradientStop> Stops;
            auto Stop=[&](float At,FLinearColor Value)
            { Stops.Emplace(FVector2f(2*X*At,0),Value.CopyWithNewOpacity(1)*Style.GetColorAndOpacityTint()); };
            Stop(0,Color*.08f);
            Stop(.25f,Color*.50f);
            Stop(.38f,Color+FLinearColor(.40f,.22f,.15f)*Glint);
            Stop(.62f,Color*.72f);
            Stop(1,Color*.06f);
            // Slate's vertical gradient denotes vertical color bands (stops use X).
            FSlateDrawElement::MakeGradient(Out,Layer,G.ToPaintGeometry(FVector2D(2*X,Step+.25f),FSlateLayoutTransform(C+FVector2D(-X,Y-Step*.5f))),MoveTemp(Stops),Orient_Vertical);
        }
        // Glass catches a narrow crescent; no animation or per-frame material allocation.
        TArray<FVector2D> Arc;
        for(int32 I=0;I<=28;++I)
        {
            const float A=PI*(1.10f+.52f*I/28.f);
            Arc.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*R*.86f);
        }
        FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Arc,ESlateDrawEffect::None,FLinearColor(.9f,.76f,.60f,.25f)*Style.GetColorAndOpacityTint(),true,1.5f);
        if(Frame && Frame->GetResourceObject())
            FSlateDrawElement::MakeBox(Out,Layer+2,G.ToPaintGeometry(),Frame,ESlateDrawEffect::None,Style.GetColorAndOpacityTint());
        else
        {
            Arc.Reset();
            for(int32 I=0;I<=64;++I) { const float A=2*PI*I/64.f; Arc.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*(R+3)); }
            FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Arc,ESlateDrawEffect::None,FLinearColor(.40f,.27f,.12f),true,5.f);
        }
        return Layer+2;
    }
private:
    TAttribute<float> Ratio;
    const FSlateBrush* Frame=nullptr;
    bool bHealth=true;
};
