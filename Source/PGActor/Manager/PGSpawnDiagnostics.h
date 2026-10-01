#pragma once

#include "CoreMinimal.h"

enum class EPGSpawnFailure : uint8
{
    None, Asset, NavigationSystem, NavigationData, NavigationProjection,
    Player, Path, Ground, Slope, Capsule, PlayerDistance, ActorSpawn
};

inline const TCHAR* PGSpawnFailureName(EPGSpawnFailure Reason)
{
    switch (Reason)
    {
    case EPGSpawnFailure::None: return TEXT("None");
    case EPGSpawnFailure::Asset: return TEXT("Asset");
    case EPGSpawnFailure::NavigationSystem: return TEXT("NavigationSystem");
    case EPGSpawnFailure::NavigationData: return TEXT("NavigationData");
    case EPGSpawnFailure::NavigationProjection: return TEXT("NavigationProjection");
    case EPGSpawnFailure::Player: return TEXT("Player");
    case EPGSpawnFailure::Path: return TEXT("Path");
    case EPGSpawnFailure::Ground: return TEXT("Ground");
    case EPGSpawnFailure::Slope: return TEXT("Slope");
    case EPGSpawnFailure::Capsule: return TEXT("Capsule");
    case EPGSpawnFailure::PlayerDistance: return TEXT("PlayerDistance");
    case EPGSpawnFailure::ActorSpawn: return TEXT("ActorSpawn");
    default: return TEXT("Unknown");
    }
}
