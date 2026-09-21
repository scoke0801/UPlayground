#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGUILootLabel.generated.h"
UCLASS()
class PGUI_API UPGUILootLabel : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(const FText& Name, FLinearColor Color, UTexture2D* Texture);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FText ItemName;
    FLinearColor GradeColor = FLinearColor::White;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Icon;
};
