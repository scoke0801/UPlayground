#include "PGDungeonTreasure.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/Dungeon/PGDungeonLayoutBuilder.h"
#include "PGActor/Dungeon/PGDungeonDiscoverySubsystem.h"
#include "Engine/World.h"

FGuid APGDungeonTreasure::RewardGuid(FGuid RunId, int32 RoomId, int32 ContentVersion)
{
    return FGuid::NewDeterministicGuid(FString::Printf(TEXT("%s:DungeonTreasure:%d:%d"), *RunId.ToString(), ContentVersion, RoomId));
}

bool APGDungeonTreasure::InitializeTreasure(int32 Seed, int32 RoomId, int32 ContentVersion, FName Pool)
{
    auto* Profile = UPGProfileSubsystem::Get(this);
    const auto* Save = Profile ? Profile->GetProfile() : nullptr;
    const auto* Table = Profile && Profile->GetCatalog() ? Profile->GetCatalog()->FindDropPool(Pool) : nullptr;
    if (!Save || !Save->RunId.IsValid() || Save->bRunEnded || !Table || !Table->bGuaranteed || RoomId < 0) return false;
    RewardRunId = Save->RunId;
    DungeonRoomId = RoomId;
    FRandomStream Random(PGDungeon::StreamSeed(Seed, 6, RoomId));
    FPGItemInstance Reward;
    if (!Profile->RollDrop(Random, Reward, Pool)) return false;
    Reward.Guid = RewardGuid(RewardRunId, RoomId, ContentVersion);
    if (Profile->HasClaimedLoot(Reward.Guid)) return false;
    InitializeItem(Reward);
    return true;
}

bool APGDungeonTreasure::TryPickup(APawn* Player)
{
    const auto* Profile = UPGProfileSubsystem::Get(this);
    const auto* Save = Profile ? Profile->GetProfile() : nullptr;
    if (!Save || Save->RunId != RewardRunId || Save->bRunEnded) return false;
    // Existing transaction writes the item and stable claim together before destroying the drop.
    if (!Super::TryPickup(Player)) return false;
    if (auto* Map = GetWorld()->GetSubsystem<UPGDungeonDiscoverySubsystem>()) Map->SetTreasureClaimed(DungeonRoomId);
    return true;
}
