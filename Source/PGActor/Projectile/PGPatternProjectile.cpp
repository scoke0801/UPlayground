#include "PGPatternProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "UObject/ConstructorHelpers.h"

APGPatternProjectile::APGPatternProjectile()
{
    ProjectileCollisionBox->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
    SetRootComponent(ProjectileCollisionBox);
    Root->SetupAttachment(ProjectileCollisionBox);
    ProjectileCollisionBox->SetBoxExtent(FVector(18,18,18));
    ProjectileCollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
    MovementComponent->SetUpdatedComponent(ProjectileCollisionBox);
    MovementComponent->bForceSubStepping = true;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Shape.Succeeded()) MeshComponent->SetStaticMesh(Shape.Object);
    MeshComponent->SetRelativeScale3D(FVector(.45f,.22f,.22f));
}
void APGPatternProjectile::SetCollisionHalfWidth(float HalfWidth)
{
    ProjectileCollisionBox->SetBoxExtent(FVector(18.f, FMath::Max(1.f, HalfWidth), 18.f));
}
void APGPatternProjectile::Fire(AActor* Source, const FVector& Start, const FVector& Direction, float Speed, float Power)
{
    bConsumed = false;
    ProjectileCollisionBox->IgnoreActorWhenMoving(Source, true);
    Super::Fire(Source, Start, Direction, Speed, Power);
}
void APGPatternProjectile::Tick(float Delta)
{
    auto* Source = Cast<APGCharacterBase>(Shooter);
    if (!IsValid(Source) || !Source->GetPGAbilitySystemComponent() || Source->GetPGAbilitySystemComponent()->GetHealth() <= 0) { Destroy(); return; }
    Super::Tick(Delta);
}
void APGPatternProjectile::OnProjectileHit(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, FVector, const FHitResult&)
{
    if (Other != Shooter) { bConsumed = true; Destroy(); }
}
void APGPatternProjectile::OnProjectileOverlapped(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int, bool, const FHitResult&)
{
    auto* Source = Cast<APGCharacterBase>(Shooter);
    if (bConsumed || !IsValid(Source) || !Source->GetPGAbilitySystemComponent() || Source->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        !IsValid(Other) || !UPGAbilityBPLibrary::IsTargetActorHostile(Source, Other)) return;
    bConsumed = true;
    FGameplayEventData Event; Event.Instigator = Shooter; Event.Target = Other;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Other, PGGamePlayTags::Shared_Event_HitReact, Event);
    Destroy();
}
