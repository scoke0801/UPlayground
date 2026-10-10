#include "PGToonPresentationComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

ADirectionalLight* UPGToonPresentationComponent::ResolveKeyLight(UWorld* World, FName Tag)
{
    if (!World) return nullptr;
    ADirectionalLight* Best = nullptr;
    bool bBestTagged = false;
    float BestIntensity = -1.f;
    for (TActorIterator<ADirectionalLight> It(World); It; ++It)
    {
        auto* Light = *It;
        const auto* Component = Light->GetLightComponent();
        if (!IsValid(Light) || Light->IsHidden() || !Component || !Component->IsVisible()) continue;
        const bool bTagged = !Tag.IsNone() && Light->ActorHasTag(Tag);
        const float Intensity = Component->Intensity;
        if (!FMath::IsFinite(Intensity) || Intensity <= 0.f) continue;
        if (!Best || (bTagged && !bBestTagged) || (bTagged == bBestTagged &&
            (Intensity > BestIntensity || (Intensity == BestIntensity && Light->GetPathName() < Best->GetPathName()))))
        {
            Best = Light;
            bBestTagged = bTagged;
            BestIntensity = Intensity;
        }
    }
    return Best;
}

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
    if (HairProxy) { HairProxy->DestroyComponent(); HairProxy = nullptr; }
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
    LightRetrySeconds = 0.f;
    QualitySeconds = 0.f;
    LastDetailWeight = -1.f;
    bDistant = false;
    HeadUpdateSeconds = 0.f;
    if (!IsValid(Mesh))
    {
        SetComponentTickEnabled(false);
        return;
    }
    SetComponentTickEnabled(true);
    if (!IsValid(KeyLight)) KeyLight = ResolveKeyLight(GetWorld(), KeyLightTag);
    AddTickPrerequisiteComponent(Mesh);
    if (HairShadowMesh && Mesh->DoesSocketExist(HeadBone))
    {
        HairProxy = NewObject<UStaticMeshComponent>(GetOwner());
        HairProxy->SetupAttachment(Mesh, HeadBone);
        HairProxy->SetStaticMesh(HairShadowMesh);
        HairProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HairProxy->SetGenerateOverlapEvents(false);
        HairProxy->SetCanEverAffectNavigation(false);
        HairProxy->SetVisibility(false);
        HairProxy->SetCastHiddenShadow(true);
        HairProxy->SetCastShadow(bHairShadowEnabled);
        HairProxy->SetRenderInMainPass(false);
        HairProxy->SetRenderInDepthPass(false);
        HairProxy->SetRenderCustomDepth(false);
        HairProxy->bAffectDistanceFieldLighting = false;
        HairProxy->bAffectDynamicIndirectLighting = false;
        HairProxy->bReceivesDecals = false;
        HairProxy->RegisterComponent();
    }
    for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
    {
        UMaterialInterface* Source = Mesh->GetMaterial(Index);
        OriginalMaterials.Add(Source);
        UMaterialInstanceDynamic* MID = Source ? Mesh->CreateDynamicMaterialInstance(Index, Source) : nullptr;
        Materials.Add(MID);
        if (MID)
        {
            float Face = 0, FaceSDF = 0, HairNormal = 0;
            MID->GetScalarParameterValue(FMaterialParameterInfo(TEXT("FaceShading")), Face);
            MID->GetScalarParameterValue(FMaterialParameterInfo(TEXT("FaceSDFEnabled")), FaceSDF);
            MID->GetScalarParameterValue(FMaterialParameterInfo(TEXT("HairNormalBlend")), HairNormal);
            if (Face > .01f || FaceSDF > .01f || HairNormal > .01f)
                HeadMaterials.Add(MID);
        }
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
    if (HeadMaterials.IsEmpty() || (bDistant && HeadUpdateSeconds > 0.f))
        return;
    HeadUpdateSeconds = bDistant ? (1.f / 15.f) : 0.f;
    const FTransform Head = Mesh->DoesSocketExist(HeadBone) ? Mesh->GetSocketTransform(HeadBone) : Mesh->GetComponentTransform();
    const FVector Forward = Head.TransformVectorNoScale(HeadForwardAxis).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    const FVector Right = Head.TransformVectorNoScale(HeadRightAxis).GetSafeNormal(SMALL_NUMBER, FVector::RightVector);
    // Position changes even without rotation. Keep the hair lighting volume
    // attached to the animated head during locomotion and world translation.
    for (UMaterialInstanceDynamic* MID : HeadMaterials)
    {
        MID->SetVectorParameterValue(TEXT("HairHeadCenterWS"), FLinearColor(Head.GetLocation()));
        MID->SetScalarParameterValue(TEXT("HairHeadFrameValid"), Mesh->DoesSocketExist(HeadBone) ? 1.f : 0.f);
    }
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

float UPGToonPresentationComponent::CalculateDetailWeight(float Distance, float Near, float Far, float Minimum)
{
    if (!FMath::IsFinite(Distance) || !FMath::IsFinite(Near) || !FMath::IsFinite(Far) || !FMath::IsFinite(Minimum)) return 1.f;
    const float Start = FMath::Max(0.f, Near);
    const float Alpha = FMath::Clamp((Distance - Start) / FMath::Max(1.f, Far - Start), 0.f, 1.f);
    return FMath::Lerp(1.f, FMath::Clamp(Minimum, 0.f, 1.f), Alpha * Alpha * (3.f - 2.f * Alpha));
}

void UPGToonPresentationComponent::RefreshQuality(float DeltaTime)
{
    HeadUpdateSeconds -= DeltaTime;
    if ((QualitySeconds -= DeltaTime) > 0.f || !IsValid(Mesh)) return;
    QualitySeconds = .2f;
    const auto* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager) return;
    const float Distance = FVector::Distance(PC->PlayerCameraManager->GetCameraLocation(), Mesh->Bounds.Origin);
    const float Weight = CalculateDetailWeight(Distance, DetailDistance, SimpleDistance, FarDetailWeight);
    bDistant = Distance > FMath::Max(DetailDistance, SimpleDistance);
    if (!FMath::IsNearlyEqual(LastDetailWeight, Weight, .01f))
    {
        for (UMaterialInstanceDynamic* MID : Materials) if (MID) MID->SetScalarParameterValue(TEXT("ToonDetailWeight"), Weight);
        LastDetailWeight = Weight;
    }
    if (HairProxy)
    {
        // Hysteresis avoids a shadow popping repeatedly at the quality boundary.
        const float Threshold = HairShadowDistance + (HairProxy->CastShadow ? 50.f : -50.f);
        HairProxy->SetCastShadow(bHairShadowEnabled && HairShadowDistance > 0.f && Distance < FMath::Max(0.f, Threshold));
    }
}

void UPGToonPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    RefreshQuality(DeltaTime);
    // Streaming or destruction may remove the chosen sun. Retry only while missing,
    // at most once a second; never scan the world every frame or override explicit links.
    if (!IsValid(KeyLight) && (LightRetrySeconds -= DeltaTime) <= 0.f)
    {
        KeyLight = ResolveKeyLight(GetWorld(), KeyLightTag);
        LightRetrySeconds = 1.f;
    }
    RefreshPresentation();
}

void UPGToonPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    RestoreMaterials();
    Mesh = nullptr;
    Super::EndPlay(Reason);
}
