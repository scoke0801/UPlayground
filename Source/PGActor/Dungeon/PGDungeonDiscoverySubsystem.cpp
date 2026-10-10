#include "PGDungeonDiscoverySubsystem.h"

void UPGDungeonDiscoverySubsystem::EnableRewards(bool bEnabled)
{
    Snapshot.bHasRewards = bEnabled;
    OnMapChanged.Broadcast(Snapshot);
}

void UPGDungeonDiscoverySubsystem::SetTreasureClaimed(int32 RoomId)
{
    if (!Snapshot.bActive || !Snapshot.bHasRewards || Snapshot.ClaimedTreasures.Contains(RoomId)) return;
    if (!Layout.Rooms.ContainsByPredicate([RoomId](const auto& R){ return R.Id == RoomId && R.Role == EPGDungeonRoomRole::Treasure; })) return;
    Snapshot.ClaimedTreasures.Add(RoomId);
    OnMapChanged.Broadcast(Snapshot);
}

void UPGDungeonDiscoverySubsystem::InitializeMap(const FPGDungeonLayout& InLayout, FVector InOrigin, float InRoomSize, float InSpacing)
{
    ResetMap();
    if (!FMath::IsFinite(InRoomSize) || !FMath::IsFinite(InSpacing) || InRoomSize <= 0 || InSpacing < InRoomSize || InOrigin.ContainsNaN()) return;
    Layout = InLayout;
    Origin = InOrigin;
    RoomSize = InRoomSize;
    Spacing = InSpacing;
    Snapshot.bActive = !Layout.Rooms.IsEmpty();
    OnMapChanged.Broadcast(Snapshot);
}

void UPGDungeonDiscoverySubsystem::ResetMap()
{
    Layout = {};
    Snapshot = {};
    Discovered.Reset();
    OnMapChanged.Broadcast(Snapshot);
}

FVector2D UPGDungeonDiscoverySubsystem::ToMapPosition(FVector Position) const
{
    const FVector Local = (Position - Origin) / Spacing;
    return FVector2D(Local.X, Local.Y);
}

void UPGDungeonDiscoverySubsystem::Observe(FVector Position, int32 Objective)
{
    if (!Snapshot.bActive || Position.ContainsNaN()) return;
    int32 RoomId = INDEX_NONE;
    const FVector Local = Position - Origin;
    for (const auto& Room : Layout.Rooms)
    {
        if (FMath::Abs(Local.Z) <= 300.f &&
            FMath::Abs(Local.X - Room.Cell.X * Spacing) < RoomSize * .5f &&
            FMath::Abs(Local.Y - Room.Cell.Y * Spacing) < RoomSize * .5f)
        { RoomId = Room.Id; break; }
    }
    bool Changed = Snapshot.CurrentRoom != RoomId || Snapshot.Objective != Objective;
    Snapshot.CurrentRoom = RoomId;
    Snapshot.Objective = Objective;
    if (RoomId != INDEX_NONE && !Discovered.Contains(RoomId))
    {
        Discovered.Add(RoomId);
        Snapshot.Rooms.Reset();
        Snapshot.Connections.Reset();
        Snapshot.Doorways.Reset();
        for (const auto& Room : Layout.Rooms) if (Discovered.Contains(Room.Id)) Snapshot.Rooms.Add(Room);
        for (const auto& Link : Layout.Connections)
        {
            if (Discovered.Contains(Link.A) && Discovered.Contains(Link.B)) Snapshot.Connections.Add(Link);
            else if (Discovered.Contains(Link.A) || Discovered.Contains(Link.B))
            {
                const int32 Known = Discovered.Contains(Link.A) ? Link.A : Link.B;
                const int32 Unknown = Known == Link.A ? Link.B : Link.A;
                const auto* A = Layout.Rooms.FindByPredicate([&](const auto& R) { return R.Id == Known; });
                const auto* B = Layout.Rooms.FindByPredicate([&](const auto& R) { return R.Id == Unknown; });
                if (A && B)
                {
                    const FVector2D P(A->Cell.X, A->Cell.Y);
                    const FVector2D Direction = FVector2D(B->Cell.X-A->Cell.X, B->Cell.Y-A->Cell.Y).GetSafeNormal();
                    Snapshot.Doorways.Emplace(P + Direction * (GetRoomRatio()*.5f), P + Direction*.5f);
                }
            }
        }
        Changed = true;
    }
    if (Changed) OnMapChanged.Broadcast(Snapshot);
}
