#include "PGCheatManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"

void UPGCheatManager::PGMotionExpansionHoldAttacks(int32 EnemyID)
{
    FString Profile;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile)) return;
    for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It)
        if (It->GetCharacterTID() == EnemyID && It->GetSkillHandler())
            for (const auto& Pair : It->GetSkillHandler()->GetAllSkillData())
            {
                auto* Skill = It->GetSkillHandler()->GetSkillData(Pair.Key);
                Skill->InheritedCooldownUntil = Skill->GetTime() + 5.;
            }
}

void UPGCheatManager::PGMotionExpansionCast(int32 SkillID)
{
    FString Profile;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile) || SkillID < 116 || SkillID > 118) return;
    auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Player || !Player->GetSkillHandler() || Player->GetPlayerAttackComponent()->IsRunning()) return;
    auto* Handler = Player->GetSkillHandler();
    Handler->RemoveSkill(EPGSkillSlot::SkillSlot_1); Handler->AddSkill(EPGSkillSlot::SkillSlot_1, SkillID);
    auto* Skill = Handler->GetSkillData(EPGSkillSlot::SkillSlot_1);
    Skill->LastSkillUsedTime = Skill->InheritedCooldownUntil = -1.e30;
    Player->GetPlayerAttackComponent()->PrepareLoadout();
    Player->GetPGAbilitySystemComponent()->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
    UE_LOG(LogTemp, Log, TEXT("PGExpansionCast skill=%d running=%d"), SkillID, Player->GetPlayerAttackComponent()->IsRunning());
}

void UPGCheatManager::PGMotionExpansionHit()
{
    FString Profile;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile)) return;
    auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Player) return;
    APGCharacterEnemy* Nearest = nullptr; double Distance = 1000.;
    for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It)
        if (It->GetPGAbilitySystemComponent()->GetHealth() > 0.f && FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) < Distance)
        { Nearest = *It; Distance = FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()); }
    if (Nearest)
    {
        EPGDamageType Type;
        const float Damage = Player->GetPGAbilitySystemComponent()->ReceiveCombatHit(Nearest->GetPGAbilitySystemComponent(), Type);
        UE_LOG(LogTemp, Log, TEXT("PGExpansionHit damage=%.3f"), Damage);
    }
}
