#pragma once
#include "PGProjectileBase.h"
#include "PGPatternProjectile.generated.h"

UCLASS()
class PGACTOR_API APGPatternProjectile : public APGProjectileBase
{
    GENERATED_BODY()
    friend class FPGAttackPatternLifecycleTest;
public:
    APGPatternProjectile();
    virtual void Fire(AActor* Source, const FVector& Start, const FVector& Direction, float Speed = 1000.f, float Power = 10.f) override;
    void SetCollisionHalfWidth(float HalfWidth);
    // A fan is one attack: overlapping bolts cannot shotgun the same target.
    void SetVolleyHits(const TSharedPtr<TSet<TWeakObjectPtr<AActor>>>& Hits) { VolleyHits = Hits; }
protected:
    virtual void Tick(float Delta) override;
    virtual void OnProjectileHit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, FVector Impulse, const FHitResult& Hit) override;
    virtual void OnProjectileOverlapped(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int Index, bool bSweep, const FHitResult& Hit) override;
private:
    bool bConsumed = false;
    TSharedPtr<TSet<TWeakObjectPtr<AActor>>> VolleyHits;
};
