#pragma once
#include "CoreMinimal.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UPGPlayerSkillProfile;

// Manually owned pooled systems: their simulation follows the attack's logical clock.
struct FPGPlayerSlashFXInstance
{
    TWeakObjectPtr<UNiagaraComponent> Component;
    float StartedAt = 0.f;
};

namespace PGPlayerSlashFX
{
    // Finish editor on-demand compilation before the short attack clock starts.
    void Prepare(UNiagaraSystem* System);
    UNiagaraComponent* Spawn(const UObject* WorldContext, UNiagaraSystem* System,
        const UPGPlayerSkillProfile* Profile, float Radius, const FVector& Location,
        const FRotator& Rotation, bool bReverse);
    void SetProgress(UNiagaraComponent* FX, const UPGPlayerSkillProfile* Profile, float Progress);
    void Release(UNiagaraComponent* FX);
}
