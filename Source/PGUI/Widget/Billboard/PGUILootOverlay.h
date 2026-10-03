#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGUILootOverlay.generated.h"

struct FPGLootLabelEntry;
UCLASS()
class PGUI_API UPGUILootOverlay : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
    friend class APGPlayerController;
    TSharedPtr<class SCanvas> Canvas;
    TArray<TSharedPtr<FPGLootLabelEntry>> Entries;
    UPROPERTY(Transient) TMap<int32,TObjectPtr<UTexture2D>> Icons;
    FText Overflow;
    int32 HiddenCount = 0;
    float RefreshIn = 0;
    void Refresh(const FGeometry& Geometry);
    bool ValidateLayoutForQA() const;
};
