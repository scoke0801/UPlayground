#include "PGCombatCameraShake.h"
UPGCombatCameraShake::UPGCombatCameraShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    bSingleInstance = true;
    SetRootShakePattern(CreateDefaultSubobject<UPGCombatShakePattern>(TEXT("ImpactPattern")));
}
void UPGCombatShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& Result)
{
    Elapsed += Params.DeltaTime;
    const float Envelope = FMath::Square(1.f - FMath::Clamp(Elapsed / FMath::Max(.01f, Duration), 0.f, 1.f));
    Result.Location = FVector(0, FMath::Sin(Elapsed * 150), FMath::Sin(Elapsed * 110)) * Envelope * Amplitude;
}
