#pragma once

#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PGUI/Style/PGUIStyleSettings.h"
#include "UObject/StrongObjectPtr.h"

/** Height-clipped liquid, readable even when empty; authored metal remains static. */
class SPGResourceOrb : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPGResourceOrb) : _Frame(nullptr), _Health(true) {}
        SLATE_ATTRIBUTE(float, Ratio)
        SLATE_ARGUMENT(const FSlateBrush*, Frame)
        SLATE_ARGUMENT(bool, Health)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Ratio=Args._Ratio; Frame=Args._Frame; bHealth=Args._Health;
        if (UMaterialInterface* Material=GetDefault<UPGUIStyleSettings>()->CombatOrbLiquid.LoadSynchronous())
        {
            Liquid.Reset(UMaterialInstanceDynamic::Create(Material,GetTransientPackage()));
            Liquid->SetVectorParameterValue(TEXT("LiquidColor"),bHealth ? FLinearColor(.58f,.012f,.023f) : FLinearColor(.72f,.25f,.016f));
            DisplayedFill=FMath::Clamp(Ratio.Get(0.f),0.f,1.f);
            PreviousFill=DisplayedFill;
            Liquid->SetScalarParameterValue(TEXT("Fill"),DisplayedFill);
            Liquid->SetScalarParameterValue(TEXT("Phase"),bHealth ? 0.f : 2.3f);
            LiquidBrush.SetResourceObject(Liquid.Get());
            LiquidBrush.ImageSize=FVector2D(144,144);
            LiquidBrush.DrawAs=ESlateBrushDrawType::Image;
        }
        SetCanTick(true);
    }
    virtual void Tick(const FGeometry& Geometry,double CurrentTime,float DeltaTime) override
    {
        SLeafWidget::Tick(Geometry,CurrentTime,DeltaTime);
        if (!Liquid.IsValid()) return;
        const float Target=FMath::Clamp(Ratio.Get(0.f),0.f,1.f);
        const float Dt=FMath::Min(DeltaTime,.1f);
        Agitation=FMath::Clamp(Agitation+FMath::Abs(Target-PreviousFill)*2.5f,0.f,1.f)*FMath::Exp(-Dt*2.8f);
        PreviousFill=Target;
        DisplayedFill=FMath::Lerp(DisplayedFill,Target,1.f-FMath::Exp(-Dt*9.f));
        if (FMath::Abs(DisplayedFill-Target)<.001f) DisplayedFill=Target;
        LiquidTime+=Dt;
        Liquid->SetScalarParameterValue(TEXT("Fill"),DisplayedFill);
        Liquid->SetScalarParameterValue(TEXT("Agitation"),Agitation);
        Liquid->SetScalarParameterValue(TEXT("LiquidTime"),LiquidTime);
        Invalidate(EInvalidateWidgetReason::Paint);
    }
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
        if (Liquid.IsValid())
            FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),&LiquidBrush,ESlateDrawEffect::None,Style.GetColorAndOpacityTint());
        else for(int32 I=0;I<Rows;++I)
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
        // Static glass and authored metal stay above the animated liquid.
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
    TStrongObjectPtr<UMaterialInstanceDynamic> Liquid;
    FSlateBrush LiquidBrush;
    float DisplayedFill=0.f, PreviousFill=0.f, Agitation=0.f, LiquidTime=0.f;
};
