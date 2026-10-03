#include "PGCheatManager.h"

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGAI/PGRoleAIController.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#endif

void UPGCheatManager::PGGuardianScenario(int32 PackCount)
{
#if !UE_BUILD_SHIPPING
    FString TestProfile;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), TestProfile)
        || !TestProfile.StartsWith(TEXT("GuardianSoak_"))) return;
    APawn* Player = GetOuterAPlayerController()->GetPawn();
    auto* Profile = UPGProfileSubsystem::Get(this);
    auto* Tables = UPGDataTableManager::Get(this);
    if (!Player || !Profile || !Tables || !Profile->MarkRunAssisted()) return;

    TArray<APGCharacterEnemy*> Previous;
    for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("PGGuardianSoak"))) Previous.Add(*It);
    for (auto* Enemy : Previous)
    {
        auto* Controller = Enemy->GetController();
        if (auto* AI = Cast<APGRoleAIController>(Controller)) AI->SetCombatThinkingEnabled(false);
        Enemy->Destroy();
        if (Controller) Controller->Destroy();
    }

    PackCount = FMath::Clamp(PackCount, 0, 10);
    int32 Spawned = 0;
    for (int32 Pack = 0; Pack < PackCount; ++Pack)
        for (const int32 TID : {15103, 15103, 15102, 15102, 15102})
        {
            const auto* Row = Tables->GetRowData<FPGEnemyDataRow>(TID);
            UClass* Class = Row ? Row->ActorClass.LoadSynchronous() : nullptr;
            if (!Class) continue;
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Enemy = GetWorld()->SpawnActor<APGCharacterEnemy>(Class,
                Player->GetActorLocation() + FVector(800.f + Spawned * 90.f, 800.f, 0.f), FRotator::ZeroRotator, Params);
            if (!Enemy) continue;
            Enemy->Tags.Add(TEXT("PGGuardianSoak"));
            Enemy->Tags.Add(FName(*FString::Printf(TEXT("PGGuardianRole_%d"), TID)));
            if (Spawned == 0) Enemy->Tags.Add(TEXT("PGGuardianPrimary"));
            // This fixture has no stage spawn identity and must never award loot.
            Enemy->bCanDropLoot = false;
            ++Spawned;
        }
    UE_LOG(LogTemp, Display, TEXT("PGGuardianScenario packs=%d spawned=%d assisted=1"), PackCount, Spawned);
#endif
}
