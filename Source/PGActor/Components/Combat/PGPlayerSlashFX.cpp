#include "PGPlayerSlashFX.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void PGPlayerSlashFX::Prepare(UNiagaraSystem* System)
{
    if (!System) return;
#if WITH_EDITOR
    // Loading an uncooked system can defer compilation until first activation.
    // PSO precaching alone does not flush that request; the first swing can expire
    // before the component ever simulates. Cooked builds already have the VM data.
    System->WaitForCompilationComplete(false, false);
#endif
    System->PrecacheAssetPSOs();
}

UNiagaraComponent* PGPlayerSlashFX::Spawn(const UObject* WorldContext, UNiagaraSystem* System,
    const UPGPlayerSkillProfile* Profile, float Radius, const FVector& Location,
    const FRotator& Rotation, bool bReverse)
{
#if !UE_BUILD_SHIPPING
    if (Profile && FParse::Param(FCommandLine::Get(),TEXT("PGGripTrace")))
        UE_LOG(LogTemp,Display,TEXT("PGGrip VFX Skill=%d Available=%d X=%.6f Y=%.6f Z=%.6f Pitch=%.6f Yaw=%.6f Roll=%.6f Radius=%.6f Reverse=%d"),
            Profile->SkillID,System!=nullptr,Location.X,Location.Y,Location.Z,Rotation.Pitch,Rotation.Yaw,Rotation.Roll,Radius,bReverse);
#endif
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
