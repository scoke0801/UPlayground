#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGUIShadingComparison.generated.h"

/** Persistent controls for the isolated shading fixture. */
UCLASS()
class PGUI_API UPGUIShadingComparison : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetViewLabel(const FText& Label) { ViewLabel = Label; }
    void SetShadowEnabled(bool bEnabled) { bShadowEnabled = bEnabled; }
    void SetHairShadowEnabled(bool bEnabled) { bHairShadowEnabled = bEnabled; }
    void SetLightState(bool bAvailable, float Azimuth, float Elevation, bool bOrbiting);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FText ViewLabel;
    bool bShadowEnabled = false;
    bool bHairShadowEnabled = true;
    FText LightLabel;
};
