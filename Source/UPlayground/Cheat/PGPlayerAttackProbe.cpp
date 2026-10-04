#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGPlayerCombatComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGActor/Manager/PGStagePresentation.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"

// Uses the real loadout, GAS activation, animation instance and collision notifies.
// Requires a disposable test profile; never runs in a normal player's session.
static FAutoConsoleCommandWithWorld PGPlayerAttackProbe(
    TEXT("PGPlayerAttackProbe"), TEXT("Validate held combo and player attack montage windows in a test profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString Profile;
    if (!World || !FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile) ||
        !Profile.StartsWith(TEXT("PlayerAttacks_"))) return;
    struct FState
    {
        double Start = FPlatformTime::Seconds(), PhaseAt = 0;
        int32 Phase = 0, Skill = 110, Windows = 0;
        bool bCollision = false;
        TWeakObjectPtr<UAnimMontage> Previous;
        TArray<int32> Combo;
        TWeakObjectPtr<APGCharacterEnemy> Target;
        float HealthBefore = 0.f;
    };
    auto State = MakeShared<FState>();
    TWeakObjectPtr<UWorld> WeakWorld = World;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State, WeakWorld](float)
    {
        auto* W = WeakWorld.Get();
        if (!W) return false;
        const double Now = FPlatformTime::Seconds();
        auto Finish = [](bool bOK, const TCHAR* Reason)
        {
            UE_LOG(LogTemp, Display, TEXT("PGPlayerAttackProbe %s %s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Reason);
            FPlatformMisc::RequestExit(false);
            return false;
        };
        if (Now - State->Start > 50) return Finish(false, TEXT("timeout"));
        auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W, 0));
        if (!Player || !Player->GetMesh()->GetAnimInstance() || !Player->GetSkillHandler()) return true;
        auto* ASC = Player->GetPGAbilitySystemComponent();
        auto* Handler = Player->GetSkillHandler();
        auto* Anim = Player->GetMesh()->GetAnimInstance();
        auto* Montage = Anim->GetCurrentActiveMontage();
        auto* Weapon = Player->GetCombatComponent()->GetCharacterCurrentEquippedWeapon();
        const bool bCollision = Weapon && Weapon->GetWeaponCollisionBox()->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
        if (State->Phase > 0 && !State->Target.IsValid()) return Finish(false, TEXT("test target disappeared"));
        if (State->Phase == 0)
        {
            if (Now - State->Start < 4) return true;
            for (TActorIterator<APGStageManager> It(W); It; ++It)
            {
                FPGStagePresentation View; View.Owner = *It; View.bClose = true;
                if (auto* Messages = UPGMessageManager::Get(Player)) Messages->SendMessage(EPGUIMessageType::StagePresentation, &View);
            }
            if (auto* UI = UPGUIManager::Get(Player)) UI->CloseAllUI();
            if (!Player->IsGameplayInputAllowed()) return true;
            if (!Weapon) return Finish(false, TEXT("missing weapon"));
            // This probe verifies the preserved legacy contact/notify path. Profile attacks
            // deliberately suppress those notifies and are covered by PGHackSlashProbe.
            // Only this disposable process's cached rows change; no assets are saved.
            for (int32 Id : {100,101,102,111,112})
                if (auto* Row = UPGDataTableManager::Get(Player)->GetRowData<FPGSkillDataRow>(Id)) Row->PlayerProfile.Reset();
            UE_LOG(LogTemp, Display, TEXT("PGPlayerAttackProbe legacy_collision_path=1 profile_rows_disabled_in_memory=1"));
            // Keep the real weapon/abilities; isolate damage arithmetic from starter equipment bonuses.
            ASC->SetEquipmentBonuses({});
            ASC->SetProfileBonuses({});
            ASC->SetCombatPerks({});
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetAttackPowerAttribute(), 100.f);
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCriticalRateAttribute(), 0.f);
            auto* Target = W->SpawnActor<APGCharacterEnemy>(APGCharacterEnemy::StaticClass(), FVector(10000,10000,100), FRotator::ZeroRotator);
            if (!Target) return Finish(false, TEXT("missing test target"));
            Target->GetCharacterMovement()->DisableMovement();
            Target->SetActorEnableCollision(false);
            Target->bCanDropLoot = false;
            auto* TargetASC = Target->GetPGAbilitySystemComponent();
            TargetASC->InitAbilityActorInfo(Target, Target);
            TargetASC->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(), 10000.f);
            TargetASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 10000.f);
            TargetASC->SetNumericAttributeBase(UPGAtrributeSet::GetDefensePowerAttribute(), 0.f);
            State->Target = Target;
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Normal);
            State->Phase = 1; State->PhaseAt = Now;
        }
        else if (State->Phase == 1)
        {
            if (Montage && State->Previous != Montage)
            {
                const FString Name = Montage->GetName();
                int32 Strike = Name.EndsWith(TEXT("Attack_1")) ? 1 : Name.EndsWith(TEXT("Attack_2")) ? 2 : Name.EndsWith(TEXT("Attack_3")) ? 3 : 0;
                State->Combo.Add(Strike);
                UE_LOG(LogTemp, Display, TEXT("PGPlayerAttackProbe combo=%d time=%.3f montage=%s"), Strike, Now - State->PhaseAt, *Name);
                // Reproduce the saved-asset self-block independently of eventual natural completion.
                for (const auto& Spec : ASC->GetActivatableAbilities())
                    if (Spec.GetDynamicSpecSourceTags().HasTagExact(PGGamePlayTags::InputTag_Skill_Normal) &&
                        ASC->AreAbilityTagsBlocked(Spec.Ability->GetAssetTags()))
                        return Finish(false, TEXT("active attack blocks the next combo ability"));
                if (State->Combo.Num() == 4)
                {
                    if (State->Combo != TArray<int32>{1, 2, 3, 1}) return Finish(false, TEXT("wrong combo order"));
                    ASC->OnAbilityInputReleased(PGGamePlayTags::InputTag_Skill_Normal);
                    State->Phase = 2; State->PhaseAt = Now;
                }
            }
            if (State->Phase == 1) ASC->OnAbilityInputHeld(PGGamePlayTags::InputTag_Skill_Normal);
        }
        else if (State->Phase == 2)
        {
            ASC->OnAbilityInputHeld(PGGamePlayTags::InputTag_Skill_Normal);
            if (Montage && State->Previous.IsValid() && Montage != State->Previous)
                return Finish(false, TEXT("release started an extra attack"));
            if (Now - State->PhaseAt > 3)
            {
                if (Montage || bCollision) return Finish(false, TEXT("release left montage/collision active"));
                if (Handler->GetSkillID(EPGSkillSlot::NormalAttack) != 100) return Finish(false, TEXT("idle did not reset combo"));
                State->Phase = 3;
            }
        }
        else if (State->Phase == 3)
        {
            Handler->RemoveSkill(EPGSkillSlot::SkillSlot_1);
            Handler->AddSkill(EPGSkillSlot::SkillSlot_1, State->Skill);
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
            State->Phase = 4; State->PhaseAt = Now; State->Windows = 0; State->bCollision = false;
            State->HealthBefore = State->Target->GetPGAbilitySystemComponent()->GetHealth();
            if (!Anim->GetCurrentActiveMontage()) return Finish(false, TEXT("skill failed to start"));
        }
        else if (State->Phase == 4)
        {
            if (bCollision && !State->bCollision) ++State->Windows;
            // Inject repeated contact into the production dedup/event path. This does not test spatial reach.
            if (bCollision) Player->GetPlayerCombatComponent()->OnHitTargetActor(State->Target.Get());
            if (!Montage && Now - State->PhaseAt > .5)
            {
                const int32 Expected = State->Skill == 111 || State->Skill == 112 ? 2 : State->Skill == 113 ? 3 : 1;
                UE_LOG(LogTemp, Display, TEXT("PGPlayerAttackProbe skill=%d windows=%d expected=%d"), State->Skill, State->Windows, Expected);
                if (bCollision || State->Windows != Expected) return Finish(false, TEXT("wrong melee notify windows"));
                const auto* Row = UPGDataTableManager::Get(Player)->GetRowData<FPGSkillDataRow>(State->Skill);
                const float Damage = State->HealthBefore - State->Target->GetPGAbilitySystemComponent()->GetHealth();
                UE_LOG(LogTemp, Display, TEXT("PGPlayerAttackProbe skill=%d damage=%.1f expected=%.1f"), State->Skill, Damage, 100.f * Row->PlayerMeleeDamageMultiplier * Expected);
                if (!FMath::IsNearlyEqual(Damage, 100.f * Row->PlayerMeleeDamageMultiplier * Expected, .1f))
                    return Finish(false, TEXT("contact dedup or melee coefficient failed"));
                State->Phase = ++State->Skill > 114 ? 5 : 3;
            }
            State->bCollision = bCollision;
        }
        else if (State->Phase == 5)
        {
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Normal);
            ASC->OnAbilityInputReleased(PGGamePlayTags::InputTag_Skill_Normal);
            State->Phase = 6;
        }
        else if (State->Phase == 6 && bCollision)
        {
            TArray<FGameplayAbilitySpecHandle> Handles;
            for (const auto& Spec : ASC->GetActivatableAbilities())
                if (Spec.GetDynamicSpecSourceTags().HasTagExact(PGGamePlayTags::InputTag_Skill_Normal)) Handles.Add(Spec.Handle);
            for (auto Handle : Handles) ASC->CancelAbilityHandle(Handle);
            if (Weapon->GetWeaponCollisionBox()->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
                return Finish(false, TEXT("cancel leaves damage collision enabled"));
            State->Target->Destroy();
            return Finish(true, TEXT("combo=1,2,3,1 release=1 idle_reset=1 skills=5 melee_dedup=1 cancel_cleanup=1"));
        }
        State->Previous = Montage;
        return true;
    }));
}));
#endif
