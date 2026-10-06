#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGData/DataTable/Skill/PGAttackPattern.h"
#include "PGEnemyAttackProfile.generated.h"

/** One timer-authoritative contact. Montage poses never generate damage. */
USTRUCT(BlueprintType)
struct PGDATA_API FPGEnemyAttackContact
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MotionStart = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Time = .85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EPGAttackPattern Shape = EPGAttackPattern::Sweep;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Radius = 300.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HalfAngle = 70.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Length = 520.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HalfWidth = 55.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float InnerRadius = 260.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageMultiplier = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHeavyImpact = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UAnimMontage> Montage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float StartFraction = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ContactFraction = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float EndFraction = 1.f;

    bool IsValid(float PreviousTime) const
    {
        return FMath::IsFinite(MotionStart) && MotionStart >= 0.f && MotionStart >= PreviousTime &&
            FMath::IsFinite(Time) && Time > MotionStart && Time > PreviousTime &&
            (Shape == EPGAttackPattern::Sweep || Shape == EPGAttackPattern::Thrust || Shape == EPGAttackPattern::RingBurst || Shape == EPGAttackPattern::LegacySlam) &&
            FMath::IsFinite(Radius) && Radius > 0.f && FMath::IsFinite(HalfAngle) && HalfAngle > 0.f && HalfAngle <= 180.f &&
            FMath::IsFinite(Length) && Length > 0.f && FMath::IsFinite(HalfWidth) && HalfWidth > 0.f &&
            FMath::IsFinite(InnerRadius) && InnerRadius >= 0.f && (Shape != EPGAttackPattern::RingBurst || (InnerRadius > 0.f && InnerRadius < Radius)) &&
            FMath::IsFinite(DamageMultiplier) && DamageMultiplier > 0.f &&
            FMath::IsFinite(StartFraction) && FMath::IsFinite(ContactFraction) && FMath::IsFinite(EndFraction) &&
            StartFraction >= 0.f && StartFraction < ContactFraction && ContactFraction < EndFraction && EndFraction <= 1.f;
    }
};

/** Optional enemy-only sequence/guard data; existing single patterns keep their path. */
UCLASS(BlueprintType)
class PGDATA_API UPGEnemyAttackProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPGEnemyAttackContact> Contacts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPGEnemyAttackContact> PhaseTwoContacts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PhaseTwoRecovery = 1.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGuardCounter = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AttacksBeforeGuard = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GuardStartSeconds = .37f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GuardHoldSeconds = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GuardAcceptSeconds = .27f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GuardEndSeconds = .57f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CounterTelegraphSeconds = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FailedRecoverySeconds = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FailedRecoveryBonus = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UAnimMontage> GuardStartMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UAnimMontage> GuardHoldMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UAnimMontage> GuardAcceptMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UAnimMontage> GuardEndMontage;

    const TArray<FPGEnemyAttackContact>& GetContacts(int32 Phase) const { return Phase >= 2 && !PhaseTwoContacts.IsEmpty() ? PhaseTwoContacts : Contacts; }
    bool IsValid() const
    {
        for (const auto* List : {&Contacts, &PhaseTwoContacts})
        {
            float Previous = -1.f;
            if (List->Num() > 8) return false;
            for (const auto& Contact : *List) { if (!Contact.IsValid(Previous)) return false; Previous = Contact.Time; }
        }
        if (Contacts.IsEmpty() || !FMath::IsFinite(PhaseTwoRecovery) || PhaseTwoRecovery < 0.f) return false;
        if (!bGuardCounter) return true;
        return Contacts.Num() == 1 && PhaseTwoContacts.IsEmpty() && AttacksBeforeGuard >= 1 &&
            FMath::IsFinite(GuardStartSeconds) && GuardStartSeconds > 0.f && FMath::IsFinite(GuardHoldSeconds) && GuardHoldSeconds > 0.f &&
            FMath::IsFinite(GuardAcceptSeconds) && GuardAcceptSeconds >= 0.f && FMath::IsFinite(GuardEndSeconds) && GuardEndSeconds >= 0.f &&
            FMath::IsFinite(CounterTelegraphSeconds) && CounterTelegraphSeconds >= .65f &&
            FMath::IsFinite(FailedRecoverySeconds) && FailedRecoverySeconds > 0.f &&
            FMath::IsFinite(FailedRecoveryBonus) && FailedRecoveryBonus >= 0.f;
    }
};
