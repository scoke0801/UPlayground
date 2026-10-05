#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "PGCharacterAppearanceComponent.generated.h"

class UPGCharacterAppearance;
class USkeletalMeshComponent;
class UPGToonPresentationComponent;

USTRUCT()
struct FPGAppearanceEquipmentAnchor
{
    GENERATED_BODY()
    UPROPERTY() FName SourceSocket;
    UPROPERTY() FGameplayTag WeaponTag;
    UPROPERTY() TObjectPtr<class USceneComponent> Component;
};

UCLASS(ClassGroup=(PG), meta=(BlueprintSpawnableComponent))
class PGACTOR_API UPGCharacterAppearanceComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPGCharacterAppearanceComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PG|Appearance") TSoftObjectPtr<UPGCharacterAppearance> DefaultAppearance;
    UFUNCTION(BlueprintCallable, Category="PG|Appearance") bool ApplyAppearance(UPGCharacterAppearance* Appearance);
    bool CanApply(UPGCharacterAppearance* Appearance) const;
    class USceneComponent* ResolveEquipmentAttachment(FName& Socket, const FGameplayTag& WeaponTag = FGameplayTag());
    int32 GetEquippedGripIndex() const;
    UFUNCTION(BlueprintPure, Category="PG|Appearance") USkeletalMeshComponent* GetPresentationMesh() const { return VisibleMesh; }
    UFUNCTION(BlueprintPure, Category="PG|Appearance") UPGCharacterAppearance* GetAppearance() const { return CurrentAppearance; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UPGCharacterAppearance> CurrentAppearance;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> VisibleMesh;
    UPROPERTY(Transient) TArray<TObjectPtr<USkeletalMeshComponent>> Parts;
    UPROPERTY(Transient) TArray<TObjectPtr<class UStaticMeshComponent>> Attachments;
    UPROPERTY(Transient) TArray<TObjectPtr<UPGToonPresentationComponent>> Presentations;
    UPROPERTY(Transient) TArray<FPGAppearanceEquipmentAnchor> EquipmentAnchors;
    void ClearPresentation();
    void AddToon(USkeletalMeshComponent* Mesh, UPGCharacterAppearance* Appearance);
};
