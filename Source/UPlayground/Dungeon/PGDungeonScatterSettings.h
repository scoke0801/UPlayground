#pragma once
#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PGShared/Shared/Structure/PGDungeonTypes.h"
#include "PGDungeonScatterSettings.generated.h"

// A real PCG node: deterministic, bounded scatter constrained by the generated room kit.
UCLASS(BlueprintType, ClassGroup=(Procedural))
class UPLAYGROUND_API UPGDungeonScatterSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) FPGDungeonLayout Layout;
    UPROPERTY(Transient) FVector Origin;
    UPROPERTY(Transient) TObjectPtr<class UPGDungeonDefinition> Definition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(ClampMin="0", ClampMax="2")) float Density = 1.f;
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("PGDungeonScatter"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PGDungeon", "Scatter", "던전 안전 영역 장식"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
    virtual FPCGElementPtr CreateElement() const override;
};
