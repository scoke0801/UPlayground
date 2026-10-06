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
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FText ViewLabel;
};
