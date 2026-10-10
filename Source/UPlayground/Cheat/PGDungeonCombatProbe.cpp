#include "PGCheatManager.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGActor/Dungeon/PGDungeonTreasure.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"

void UPGCheatManager::PGDungeonStep(FString Action)
{
#if !UE_BUILD_SHIPPING
    FString TestName;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), TestName)) return;
    auto* Profile = UPGProfileSubsystem::Get(this);
    if (!Profile || !Profile->MarkRunAssisted()) return;
    if (Action==TEXT("actioncamera") || Action==TEXT("quartercamera"))
    {
        if (auto* Player=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this,0)))
            Player->SetCameraMode(Action==TEXT("actioncamera") ? EPGCameraMode::Action3D : EPGCameraMode::QuarterView);
        return;
    }
    if (Action==TEXT("treasure"))
    {
        APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
        for (TActorIterator<APGDungeonTreasure> It(GetWorld()); It; ++It)
        {
            auto* Treasure=*It;
            if (!Player || FVector::DistSquared(Player->GetActorLocation(),Treasure->GetActorLocation())>FMath::Square(250.f)) continue;
            const auto Reward=Treasure->GetItem();
            const int32 Before=Profile->GetProfile()->Items.Num();
            Profile->bInjectSaveFailure=true;
            bool Valid=!Treasure->TryPickup(Player) && !Profile->HasClaimedLoot(Reward.Guid) && Profile->GetProfile()->Items.Num()==Before;
            Profile->bInjectSaveFailure=false;
            auto* Catalog=const_cast<UPGProgressionData*>(Profile->GetCatalog());
            const int32 Capacity=Catalog->BagCapacity;
            Catalog->BagCapacity=Before;
            Valid &= !Treasure->TryPickup(Player) && !Profile->HasClaimedLoot(Reward.Guid);
            Catalog->BagCapacity=Capacity;
            Valid &= Treasure->TryPickup(Player) && Profile->HasClaimedLoot(Reward.Guid) && Profile->GetProfile()->Items.Num()==Before+1;
            Valid &= !Profile->TryPickup(Reward);
            UE_LOG(LogTemp,Display,TEXT("PGDungeonP2 treasure valid=%d before=%d after=%d"),Valid,Before,Profile->GetProfile()->Items.Num());
            if (!Valid) UE_LOG(LogTemp,Error,TEXT("PGDungeonP2 treasure transaction failed"));
            return;
        }
        UE_LOG(LogTemp,Error,TEXT("PGDungeonP2 treasure probe found no nearby reward"));
        return;
    }
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It)
    {
        auto* Stage = *It;
        if (!Stage->IsDungeonCombat()) continue;
        if (Action == TEXT("spawnfail")) Stage->DungeonSpawnInset = Stage->DungeonHalfSize + 1;
        else if (Action == TEXT("summons"))
        {
            const auto Enemies = Stage->SpawnedEnemies;
            for (auto* Enemy : Enemies) if (IsValid(Enemy) && Enemy->GetCharacterTID() == 15502)
            {
                const auto* Skill = PGData()->GetRowData<FPGSkillDataRow>(15523);
                const int32 Before = Stage->RemainingMonsters;
                const int32 Count = Skill ? Enemy->SpawnPatternSummons(*Skill) : 0;
                const bool Tracked = Stage->RemainingMonsters == Before + Count;
                const auto Children = Enemy->PatternSummons;
                Enemy->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),0);
                UE_LOG(LogTemp, Display, TEXT("PGDungeonProbe summoner death tracked=%d remaining=%d before=%d children=%d"),
                    Stage->SpawnedEnemies.Contains(Enemy), Stage->RemainingMonsters, Before, Count);
                for (const auto& Child : Children) if (Child.IsValid())
                    UE_LOG(LogTemp, Display, TEXT("PGDungeonProbe child=%s owner=%s instigator=%s tracked=%d marked=%d"),
                        *Child->GetName(), *GetNameSafe(Child->GetOwner()), *GetNameSafe(Child->GetInstigator()),
                        Stage->DungeonSummons.Contains(Child), Stage->DungeonSummonsWithDefeatedOwner.Contains(Child));
                Enemy->Destroy();
                bool Valid = Count > 0 && Tracked && Stage->CurrentStageState != EPGStageState::Failed && Stage->RemainingMonsters == Before - 1;
                for (const auto& Child : Children) if (Child.IsValid()) Valid = false;
                UE_LOG(LogTemp, Display, TEXT("PGDungeonProbe summons count=%d valid=%d"),Count,Valid);
                if (!Valid) UE_LOG(LogTemp, Error, TEXT("PGDungeonProbe summon cleanup failed"));
                break;
            }
        }
        else if (Action == TEXT("kill"))
        {
            const auto Enemies = Stage->SpawnedEnemies;
            for (auto* Enemy : Enemies) if (IsValid(Enemy))
                Enemy->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),0);
        }
        else if (Action == TEXT("reward") && Stage->CurrentStageState == EPGStageState::BuildPhase && !Stage->bRewardCommitted)
        {
            const FGuid Token = Stage->RewardToken;
            if (!Token.IsValid()) return;
            const int32 Before = Profile->GetProfile()->StageRewardCounts.FindRef(Stage->CurrentStageId);
            const bool Applied = Stage->CommitReward(Token,0);
            const bool Duplicate = Stage->CommitReward(Token,0);
            const int32 After = Profile->GetProfile()->StageRewardCounts.FindRef(Stage->CurrentStageId);
            const bool Valid = !Duplicate && After == Before + (Applied ? 1 : 0);
            UE_LOG(LogTemp, Display, TEXT("PGDungeonProbe reward stage=%d applied=%d duplicate=%d before=%d after=%d valid=%d"),
                Stage->CurrentStageId, Applied, Duplicate, Before, After, Valid);
            if (!Valid) UE_LOG(LogTemp, Error, TEXT("PGDungeonProbe reward invariant failed"));
        }
        else if (Action == TEXT("die"))
        {
            if (auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this,0)))
                Player->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),0);
        }
        else if (Action == TEXT("restart")) { Stage->RestartRun(); return; }
        else if (Action == TEXT("status"))
        {
            int32 Rewards = 0;
            for (auto Pair : Profile->GetProfile()->StageRewardCounts) Rewards += Pair.Value;
            UE_LOG(LogTemp, Display, TEXT("PGDungeonProbe status stage=%d state=%d rewards=%d counts=%d,%d,%d,%d,%d victory=%d"),
                Stage->CurrentStageId, int32(Stage->CurrentStageState), Rewards,
                Profile->GetProfile()->StageRewardCounts.FindRef(1), Profile->GetProfile()->StageRewardCounts.FindRef(2),
                Profile->GetProfile()->StageRewardCounts.FindRef(3), Profile->GetProfile()->StageRewardCounts.FindRef(4),
                Profile->GetProfile()->StageRewardCounts.FindRef(5), Profile->GetProfile()->BossReward.Guid.IsValid());
        }
        return;
    }
#endif
}
