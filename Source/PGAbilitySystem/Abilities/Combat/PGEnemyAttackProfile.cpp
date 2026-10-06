#include "PGEnemyAbilityAttack.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGEnemyPresentationComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGAI/PGCombatDirectorSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UPGEnemyAbilityAttack::PlayProfileMontage(UAnimMontage* Montage, float Duration)
{
    auto* Anim = GetEnemyCharacterFromActorInfo()->GetMesh()->GetAnimInstance();
    if (!Anim) return;
    if (ProfileMontage && ProfileMontage != Montage) Anim->Montage_Stop(.1f, ProfileMontage);
    ProfileMontage = Montage;
    if (!Montage) return;
    Anim->Montage_Play(Montage, Duration > 0.f ? Montage->GetPlayLength() / Duration : 1.f);
    if (Duration <= 0.f) Anim->Montage_Pause(Montage);
}

void UPGEnemyAbilityAttack::SelectContact(int32 Index)
{
    const auto& Hit = Contacts[Index];
    EliteData.Pattern = Hit.Shape; EliteData.TelegraphRadius = Hit.Radius;
    EliteData.HalfAngleDegrees = Hit.HalfAngle; EliteData.TravelDistance = Hit.Length;
    EliteData.LineHalfWidth = Hit.HalfWidth; EliteData.InnerSafeRadius = Hit.InnerRadius;
    EliteData.EnemyDamageMultiplier = Hit.DamageMultiplier;
    EliteData.bHeavyImpactFeedback = Hit.bHeavyImpact;
    GetEnemyCharacterFromActorInfo()->bUseHeavyImpactFeedback = Hit.bHeavyImpact;
    StrikeCenter = PatternOrigin;
    ShowTelegraph(StrikeCenter, Hit.Shape == EPGAttackPattern::Thrust);
}

void UPGEnemyAbilityAttack::BeginProfile()
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    Contacts = AttackProfile->GetContacts(Enemy->BossPhase);
    ContactIndex = 0; MotionIndex = INDEX_NONE; bGuardSucceeded = false;
    if (!AttackProfile->PhaseTwoContacts.IsEmpty() && Enemy->BossPhase >= 2)
        EliteData.RecoveryDuration = AttackProfile->PhaseTwoRecovery;
    GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdatePattern, .02f, true);
    UE_LOG(LogTemp, Log, TEXT("PGPattern Windup skill=%d pattern=%d contacts=%d"), EliteData.SkillID, int32(EliteData.Pattern), Contacts.Num());
    if (AttackProfile->bGuardCounter)
    {
        Enemy->CompletedAttackPatterns = 0;
        Enemy->SetGuarding(false);
        if (auto* Director = GetWorld()->GetSubsystem<UPGCombatDirectorSubsystem>()) Director->Release(Enemy);
        GuardStage = EGuardStage::Start;
        PlayProfileMontage(AttackProfile->GuardStartMontage.LoadSynchronous(), AttackProfile->GuardStartSeconds);
        GuardHitDelegate = Enemy->GetPGAbilitySystemComponent()->OnConfirmedGuardHit.AddUObject(this, &ThisClass::OnGuardHit);
        GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::BeginGuardHold, AttackProfile->GuardStartSeconds, false);
    }
    else
    {
        GuardStage = EGuardStage::None;
        SelectContact(0); UpdateProfileMotion();
        GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::StrikeProfileContact, Contacts[0].Time, false);
    }
    Enemy->PublishBossPresentation();
}

void UPGEnemyAbilityAttack::UpdateProfileMotion()
{
    if (AttackProfile->bGuardCounter && (ContactIndex == 0 && GuardStage == EGuardStage::Recovery)) return;
    if (AttackProfile->bGuardCounter && GuardStage != EGuardStage::Counter && GuardStage != EGuardStage::Recovery) return;
    float Time = GetWorld()->GetTimeSeconds() - (AttackProfile->bGuardCounter ? CounterStartedAt : PatternStartedAt);
    // A delayed timer must still show its pending contact before the following clip.
    if (Contacts.IsValidIndex(ContactIndex)) Time = FMath::Min(Time, Contacts[ContactIndex].Time);
    int32 Index = 0;
    for (int32 I = 1; I < Contacts.Num(); ++I) if (Time >= Contacts[I].MotionStart) Index = I;
    const auto& Hit = Contacts[Index];
    if (MotionIndex != Index)
    {
        MotionIndex = Index; PlayProfileMontage(Hit.Montage.LoadSynchronous());
    }
    if (!ProfileMontage) return;
    const float EndTime = Contacts.IsValidIndex(Index + 1) ? Contacts[Index + 1].MotionStart : Hit.Time + EliteData.RecoveryDuration;
    const float Fraction = Time <= Hit.Time ?
        FMath::Lerp(Hit.StartFraction, Hit.ContactFraction, FMath::Clamp((Time - Hit.MotionStart) / (Hit.Time - Hit.MotionStart), 0.f, 1.f)) :
        FMath::Lerp(Hit.ContactFraction, Hit.EndFraction, FMath::Clamp((Time - Hit.Time) / FMath::Max(.01f, EndTime - Hit.Time), 0.f, 1.f));
    if (auto* Anim = GetEnemyCharacterFromActorInfo()->GetMesh()->GetAnimInstance())
        Anim->Montage_SetPosition(ProfileMontage, ProfileMontage->GetPlayLength() * Fraction);
}

void UPGEnemyAbilityAttack::StrikeProfileContact()
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    auto* Target = Cast<APGCharacterBase>(PatternTarget.Get());
    if (!IsActive() || !Enemy || Enemy->IsBossTransitioning() || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0.f ||
        !Target || !Target->GetPGAbilitySystemComponent() || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0.f) { EndAbilitySelf(); return; }
    if (!Contacts.IsValidIndex(ContactIndex) || Enemy->bPatternRecovering ||
        (AttackProfile->bGuardCounter && GuardStage != EGuardStage::Counter)) return;
    double& Clock = AttackProfile->bGuardCounter ? CounterStartedAt : PatternStartedAt;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - Clock + .002 < Contacts[ContactIndex].Time) return;
    // Preserve the designed escape interval after a hitch instead of catching up in a burst.
    Clock = FMath::Max(Clock, Now - Contacts[ContactIndex].Time);
    const double StartedAt = Clock;
    const int32 ClaimedIndex = ContactIndex++; // Claim before synchronous damage/phase/death callbacks.
    SelectContact(ClaimedIndex);
    UpdateProfileMotion();
    if (auto* Sound = EliteData.AttackSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(this, Sound, Enemy->GetActorLocation());
    {
        TGuardValue<bool> Dispatch(bDispatchingContact, true);
        StrikeElitePattern();
    }
    if (!IsActive()) return;
    UE_LOG(LogTemp, Log, TEXT("PGProfile Contact skill=%d index=%d multiplier=%.2f"), EliteData.SkillID, ClaimedIndex, EliteData.EnemyDamageMultiplier);
    if (Contacts.IsValidIndex(ContactIndex))
    {
        SelectContact(ContactIndex);
        const float Delay = FMath::Max(.01f, float(StartedAt + Contacts[ContactIndex].Time - GetWorld()->GetTimeSeconds()));
        Enemy->GetEnemyPresentation()->BeginWindup(Delay, 0.f);
        GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::StrikeProfileContact, Delay, false);
    }
    else BeginRecovery();
}

void UPGEnemyAbilityAttack::BeginGuardHold()
{
    if (!IsActive() || GuardStage != EGuardStage::Start) return;
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    GuardStage = EGuardStage::Hold;
    Enemy->bBossPatternGuard = true; Enemy->SetGuarding(true);
    // Freeze facing after the visible start; never track during the defensive hold.
    bStriking = true;
    PlayProfileMontage(AttackProfile->GuardHoldMontage.LoadSynchronous(), AttackProfile->GuardHoldSeconds);
    GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::FailGuard, AttackProfile->GuardHoldSeconds, false);
    UE_LOG(LogTemp, Log, TEXT("PGProfile GuardHold skill=%d"), EliteData.SkillID);
    Enemy->PublishBossPresentation();
}

void UPGEnemyAbilityAttack::OnGuardHit(AActor* Attacker)
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    if (!IsActive() || GuardStage != EGuardStage::Hold || bGuardSucceeded || !Enemy || !IsValid(Attacker) ||
        !UPGAbilityBPLibrary::IsTargetActorHostile(Enemy, Attacker) ||
        Enemy->IsBossTransitioning() || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0.f) return;
    bGuardSucceeded = true; GuardStage = EGuardStage::Accept;
    Enemy->SetGuarding(false); Enemy->bBossPatternGuard = false;
    Enemy->GetEnemyPresentation()->PlayGuardImpact();
    PlayProfileMontage(AttackProfile->GuardAcceptMontage.LoadSynchronous(), FMath::Max(.01f, AttackProfile->GuardAcceptSeconds));
    GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::BeginCounter, FMath::Max(.01f, AttackProfile->GuardAcceptSeconds), false);
    UE_LOG(LogTemp, Log, TEXT("PGProfile GuardSuccess skill=%d"), EliteData.SkillID);
    Enemy->PublishBossPresentation();
}

void UPGEnemyAbilityAttack::BeginCounter()
{
    if (!IsActive() || GuardStage != EGuardStage::Accept) return;
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    auto* Target = Cast<APGCharacterBase>(PatternTarget.Get());
    auto* Director = GetWorld()->GetSubsystem<UPGCombatDirectorSubsystem>();
    if (!Target || !Target->GetPGAbilitySystemComponent() || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0.f ||
        (Director && !Director->TryReserve(Enemy, Target, EliteData.AttackPressureCost))) { FailGuard(); return; }
    GuardStage = EGuardStage::Counter;
    Contacts[0].Time = FMath::Max(.65f, AttackProfile->CounterTelegraphSeconds);
    Contacts[0].MotionStart = 0.f;
    CounterStartedAt = GetWorld()->GetTimeSeconds();
    bStriking = false; Enemy->bPatternStriking = false;
    MotionIndex = INDEX_NONE;
    SelectContact(0); UpdateProfileMotion();
    Enemy->GetEnemyPresentation()->BeginWindup(Contacts[0].Time, 0.f);
    GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::StrikeProfileContact, Contacts[0].Time, false);
    Enemy->PublishBossPresentation();
}

void UPGEnemyAbilityAttack::FailGuard()
{
    if (!IsActive() || GuardStage == EGuardStage::End || GuardStage == EGuardStage::Recovery) return;
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    GuardStage = EGuardStage::End;
    Enemy->SetGuarding(false); Enemy->bBossPatternGuard = false;
    EliteData.RecoveryDuration = AttackProfile->FailedRecoverySeconds;
    EliteData.RecoveryDamageBonus = AttackProfile->FailedRecoveryBonus;
    PlayProfileMontage(AttackProfile->GuardEndMontage.LoadSynchronous(), FMath::Max(.01f, AttackProfile->GuardEndSeconds));
    GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::BeginRecovery, FMath::Max(.01f, AttackProfile->GuardEndSeconds), false);
    UE_LOG(LogTemp, Log, TEXT("PGProfile GuardEnd skill=%d success=%d"), EliteData.SkillID, bGuardSucceeded);
}
