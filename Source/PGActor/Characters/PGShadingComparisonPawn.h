#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "PGShadingComparisonPawn.generated.h"

/** Free camera used only by the shading comparison GameMode. */
UCLASS()
class PGACTOR_API APGShadingComparisonPawn : public ASpectatorPawn
{
    GENERATED_BODY()
public:
    APGShadingComparisonPawn(const FObjectInitializer& ObjectInitializer);
    virtual void Tick(float DeltaSeconds) override;
    virtual void PawnClientRestart() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PG|Comparison")
    TObjectPtr<class UCameraComponent> Camera;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Comparison", meta=(ClampMin="10"))
    float MoveSpeed = 300.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Comparison", meta=(ClampMin="1"))
    float FastMultiplier = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Comparison", meta=(ClampMin="0.01"))
    float LookSensitivity = .25f;

    UFUNCTION(BlueprintCallable, Category="PG|Comparison")
    void ShowOverview();
    /** Zero-based model index. Keeps the selected view when changing stages. */
    UFUNCTION(BlueprintCallable, Category="PG|Comparison")
    void FocusStage(int32 Index);
    UFUNCTION(BlueprintCallable, Category="PG|Comparison")
    void ShowFace();
    UFUNCTION(BlueprintCallable, Category="PG|Comparison")
    void ShowQuarter();
    /** Input-path verification; only enabled with -PGShadingComparisonProbe in non-shipping builds. */
    UFUNCTION(BlueprintCallable, Category="PG|Comparison|Test")
    bool SendProbeInput(FKey Key, bool bPressed, float AxisValue = 0.f);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    UPROPERTY(Transient) TObjectPtr<class UPGUIShadingComparison> HelpWidget;
    TWeakObjectPtr<AActor> OverviewCamera;
    int32 SelectedStage = 4;
    enum class EComparisonView : uint8 { Front, Face, Quarter };
    EComparisonView SelectedView = EComparisonView::Front;
    void SetView(const FVector& Location, const FRotator& Rotation);
    void LookHorizontal(float Value);
    void LookVertical(float Value);
};
