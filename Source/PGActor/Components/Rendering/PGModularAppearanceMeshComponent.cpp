#include "PGModularAppearanceMeshComponent.h"

FBoxSphereBounds UPGModularAppearanceMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds Result = Super::CalcBounds(LocalToWorld);
    // Reuse the already evaluated pose, once on the leader: no extra tick or skinning.
    // Imported limb-only bounds describe the reference pose, not a lowered/swinging arm.
    FBox Body(ForceInit);
    for (const FTransform& Bone : GetComponentSpaceTransforms())
        if (!Bone.ContainsNaN()) Body += Bone.GetTranslation();
    if (Body.IsValid)
        Result = Result + FBoxSphereBounds(Body.ExpandBy(12.f)).TransformBy(LocalToWorld);
    // Also retain the combat body's volume for clothing around the torso.
    if (const auto* Source = Cast<USkeletalMeshComponent>(GetAttachParent()))
        // CalcBounds can also be queried with Identity for local-space bounds.
        Result = Result + Source->Bounds.TransformBy(GetComponentTransform().Inverse() * LocalToWorld);
    return Result;
}
