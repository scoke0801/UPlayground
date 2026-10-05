#include "PGToonPresentationComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInstanceDynamic.h"

UPGToonPresentationComponent::UPGToonPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    bTickInEditor = true;
}

void UPGToonPresentationComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner() && !IsValid(Mesh))
        Initialize(GetOwner()->FindComponentByClass<USkeletalMeshComponent>());
}

void UPGToonPresentationComponent::RestoreMaterials()
{
    if (IsValid(Mesh))
    {
        for (int32 Index = 0; Index < Materials.Num(); ++Index)
        {
            // Do not undo material changes made by another presentation system.
            if (Mesh->GetMaterial(Index) == Materials[Index] && OriginalMaterials.IsValidIndex(Index))
                Mesh->SetMaterial(Index, OriginalMaterials[Index]);
        }
        RemoveTickPrerequisiteComponent(Mesh);
    }
    Materials.Reset();
    OriginalMaterials.Reset();
    HeadMaterials.Reset();
}

void UPGToonPresentationComponent::Initialize(USkeletalMeshComponent* InMesh)
{
    RestoreMaterials();
    Mesh = InMesh;
    LastLight = LastForward = LastRight = FVector::ZeroVector;
    if (!IsValid(Mesh))
        return;
    AddTickPrerequisiteComponent(Mesh);
    for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
    {
        UMaterialInterface* Source = Mesh->GetMaterial(Index);
        OriginalMaterials.Add(Source);
        UMaterialInstanceDynamic* MID = Source ? Mesh->CreateDynamicMaterialInstance(Index, Source) : nullptr;
        Materials.Add(MID);
        float Face = 0;
        if (MID && MID->GetScalarParameterValue(FMaterialParameterInfo(TEXT("FaceShading")), Face) && Face > .01f)
            HeadMaterials.Add(MID);
    }
    RefreshPresentation();
}

void UPGToonPresentationComponent::RefreshPresentation()
{
    if (!IsValid(Mesh))
        return;
    FVector Direction = IsValid(KeyLight) ? KeyLight->GetActorForwardVector() : FallbackLightDirection;
    Direction = Direction.GetSafeNormal(SMALL_NUMBER, FVector(0, 0, -1));
    if (!Direction.Equals(LastLight, .0001))
    {
        for (UMaterialInstanceDynamic* MID : Materials)
            if (MID) MID->SetVectorParameterValue(TEXT("LightDirection"), FLinearColor(Direction));
        LastLight = Direction;
    }
    if (HeadMaterials.IsEmpty())
        return;
    const FTransform Head = Mesh->DoesSocketExist(HeadBone) ? Mesh->GetSocketTransform(HeadBone) : Mesh->GetComponentTransform();
    const FVector Forward = Head.TransformVectorNoScale(HeadForwardAxis).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    const FVector Right = Head.TransformVectorNoScale(HeadRightAxis).GetSafeNormal(SMALL_NUMBER, FVector::RightVector);
    if (!Forward.Equals(LastForward, .0001) || !Right.Equals(LastRight, .0001))
    {
        for (UMaterialInstanceDynamic* MID : HeadMaterials)
        {
            MID->SetVectorParameterValue(TEXT("HeadForwardWS"), FLinearColor(Forward));
            MID->SetVectorParameterValue(TEXT("HeadRightWS"), FLinearColor(Right));
        }
        LastForward = Forward;
        LastRight = Right;
    }
}

void UPGToonPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    // No world light scans, per-frame MID allocations, or skeletal pose duplication.
    RefreshPresentation();
}

void UPGToonPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    RestoreMaterials();
    Mesh = nullptr;
    Super::EndPlay(Reason);
}
