#pragma once
#include "PGProjectileBase.h"
#include "PGPatternProjectile.generated.h"

UCLASS()
class PGACTOR_API APGPatternProjectile : public APGProjectileBase
{
    GENERATED_BODY()
public:
    APGPatternProjectile();
    virtual void Fire(AActor* Source, const FVector& Start, const FVector& Direction, float Speed = 1000.f, float Power = 10.f) override;
    void SetCollisionHalfWidth(float HalfWidth);
protected:
    virtual void Tick(float Delta) override;
    virtual void OnProjectileHit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, FVector Impulse, const FHitResult& Hit) override;
    virtual void OnProjectileOverlapped(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int Index, bool bSweep, const FHitResult& Hit) override;
private:
    bool bConsumed = false;
};
