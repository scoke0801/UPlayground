#include "PGPlayerSlashFX.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UNiagaraComponent* PGPlayerSlashFX::Spawn(const UObject* WorldContext, UNiagaraSystem* System,
    const UPGPlayerSkillProfile* Profile, float Radius, const FVector& Location,
    const FRotator& Rotation, bool bReverse)
{
    if (!System || !Profile) return nullptr;
    const float Scale = Radius / FMath::Max(1.f, Profile->NiagaraReferenceRadius);
    const FRotator Orientation = (Rotation.Quaternion() * Profile->NiagaraRotation.Quaternion()).Rotator();
    auto* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, System, Location,
        Orientation, FVector(Scale, bReverse ? -Scale : Scale, Scale), false, false,
        ENCPoolMethod::ManualRelease, false);
    if (!FX) return nullptr;
    const FLinearColor Color = Profile->SlashTint * Profile->SlashIntensity;
    FX->SetVariableVec3(TEXT("User.SlashTint"), FVector(Color.R, Color.G, Color.B));
    FX->SetForceSolo(true);
    FX->SetAgeUpdateMode(ENiagaraAgeUpdateMode::DesiredAge);
    FX->SetSeekDelta(1.f / 120.f);
    FX->SetLockDesiredAgeDeltaTimeToSeekDelta(false);
    FX->SetCanRenderWhileSeeking(true);
    FX->Activate(true);
    SetProgress(FX, Profile, 0.f);
    return FX;
}

void PGPlayerSlashFX::SetProgress(UNiagaraComponent* FX, const UPGPlayerSkillProfile* Profile, float Progress)
{
    if (IsValid(FX) && Profile)
    {
        FX->SetDesiredAge(1.f / 120.f + FMath::Clamp(Progress, 0.f, 1.f) * Profile->NiagaraReferenceDuration);
        FX->SetVariableFloat(TEXT("User.SlashAlpha"), 1.f - FMath::SmoothStep(.6f, 1.f, Progress));
    }
}

void PGPlayerSlashFX::Release(UNiagaraComponent* FX)
{
    if (!IsValid(FX)) return;
    FX->PrimaryComponentTick.GetPrerequisites().Empty();
    FX->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    FX->DeactivateImmediate();
    if (FX->PoolingMethod == ENCPoolMethod::ManualRelease) FX->ReleaseToPool();
    else FX->DestroyComponent();
}
