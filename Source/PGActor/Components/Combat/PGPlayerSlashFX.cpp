#include "PGPlayerSlashFX.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#if WITH_EDITOR
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRendererProperties.h"
#include "Misc/App.h"
#endif
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPGExternalVFX(TEXT("pg.Skill.ExternalVFX"), 1,
    TEXT("Toggle the external combat particle layer for visual comparison; gameplay is unchanged."));

void PGPlayerSlashFX::SnapshotBuild(UPGPlayerSkillProfile* Profile, const UPGAbilitySystemComponent* ASC)
{
    if (!Profile) return;
    Profile->BuildVFXWeights = FVector::ZeroVector;
    if (!Profile->bBuildReactiveVFX || !ASC) return;
    // Ownership gives a readable baseline; upgrades and active frenzy enrich it.
    const auto Weight = [ASC](EPGCombatPerk Base, EPGCombatPerk Upgrade)
    {
        return ASC->GetPerkPercent(Base) > 0 ? FMath::Clamp(.55f +
            ASC->GetPerkPercent(Upgrade) * .01f, .55f, 1.f) : 0.f;
    };
    Profile->BuildVFXWeights = FVector(Weight(EPGCombatPerk::Bleed, EPGCombatPerk::BleedPotency),
        Weight(EPGCombatPerk::Shockwave, EPGCombatPerk::ShockRadius),
        Weight(EPGCombatPerk::Frenzy, EPGCombatPerk::FrenzyAfterimage));
    if (Profile->BuildVFXWeights.Z > 0.f)
    {
        const auto State = ASC->GetBuildCombatState();
        Profile->BuildVFXWeights.Z = FMath::Clamp(Profile->BuildVFXWeights.Z +
            .45f * State.FrenzyStacks / FMath::Max(1, State.FrenzyMaxStacks), 0.f, 1.f);
    }
}

void PGPlayerSlashFX::SetMaterialBuild(UMaterialInstanceDynamic* Material, const UPGPlayerSkillProfile* Profile)
{
    if (!Material || !Profile) return;
    Material->SetVectorParameterValue(TEXT("BuildWeights"), FLinearColor(
        Profile->BuildVFXWeights.X, Profile->BuildVFXWeights.Y, Profile->BuildVFXWeights.Z, 0));
    Material->SetVectorParameterValue(TEXT("BleedTint"), Profile->BleedVFXTint);
    Material->SetVectorParameterValue(TEXT("ShockTint"), Profile->ShockVFXTint);
    Material->SetVectorParameterValue(TEXT("FrenzyTint"), Profile->FrenzyVFXTint);
}

void PGPlayerSlashFX::Prepare(UNiagaraSystem* System)
{
    if (!System) return;
#if WITH_EDITOR
    // Loading an uncooked system can defer compilation until first activation.
    // PSO precaching alone does not flush that request; the first swing can expire
    // before the component ever simulates. Cooked builds already have the VM data.
    System->WaitForCompilationComplete(false, false);
    if (FApp::CanEverRender())
    {
        TArray<UMaterialInterface*> Materials;
        for (const auto& Handle : System->GetEmitterHandles())
            if (Handle.GetIsEnabled())
                if (const auto* Data = Handle.GetEmitterData())
                    for (const auto* Renderer : Data->GetRenderers())
                        Renderer->GetUsedMaterials(nullptr, Materials);
        for (auto* Material : Materials)
            if (Material) Material->EnsureIsComplete();
    }
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

UNiagaraComponent* PGPlayerSlashFX::SpawnExternal(const UObject* WorldContext, const FPGExternalCombatVFX& Definition,
    const UPGPlayerSkillProfile* Profile, float Radius, const FVector& Location, const FRotator& Rotation, bool bReverse)
{
    auto* System = Definition.System.Get(); // Preloaded with the loadout/cast, never load during a contact.
    if (!System || !Profile || CVarPGExternalVFX.GetValueOnGameThread() == 0) return nullptr;
    FVector Scale = Definition.Scale * (Radius / Definition.ReferenceRadius);
    if (bReverse && Definition.bMirror) Scale.Y *= -1.f;
    const FRotator Orientation = (Rotation.Quaternion() * Definition.Rotation.Quaternion()).Rotator();
    auto* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, System,
        Location + Rotation.RotateVector(Definition.Offset * Radius), Orientation, Scale,
        false, false, ENCPoolMethod::ManualRelease, false);
    if (!FX) return nullptr;
    const FLinearColor Color = Profile->SlashTint * Definition.Intensity;
    FX->SetVariableVec3(TEXT("User.SlashTint"), FVector(Color.R, Color.G, Color.B));
    FX->SetVariableVec3(TEXT("User.Color"), FVector(Color.R, Color.G, Color.B));
    FX->SetVariableFloat(TEXT("User.SlashAlpha"), 1.f);
    FX->SetForceSolo(true);
    FX->SetAgeUpdateMode(ENiagaraAgeUpdateMode::DesiredAge);
    FX->SetSeekDelta(1.f / 120.f);
    FX->SetLockDesiredAgeDeltaTimeToSeekDelta(false);
    FX->SetCanRenderWhileSeeking(true);
    FX->Activate(true);
    SetExternalProgress(FX, Definition.ReferenceDuration, 0.f);
    return FX;
}

void PGPlayerSlashFX::SetExternalProgress(UNiagaraComponent* FX, float ReferenceDuration, float Progress)
{
    if (!IsValid(FX)) return;
    FX->SetDesiredAge(1.f / 120.f + FMath::Clamp(Progress, 0.f, 1.f) * ReferenceDuration);
    FX->SetVariableFloat(TEXT("User.SlashAlpha"), 1.f - FMath::SmoothStep(.6f, 1.f, Progress));
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
