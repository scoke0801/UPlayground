#include "PGData/Dungeon/PGDungeonLayoutBuilder.h"
#include "PGData/DataAsset/Dungeon/PGDungeonDefinition.h"

namespace PGDungeon
{
int32 StreamSeed(int32 Seed, int32 Salt, int32 RoomId)
{
    uint32 X = uint32(Seed) ^ (uint32(Salt) * 0x9e3779b9u) ^ (uint32(RoomId) * 0x85ebca6bu);
    X ^= X >> 16; X *= 0x7feb352du; X ^= X >> 15; X *= 0x846ca68bu; X ^= X >> 16;
    return int32(X);
}
static const FIntPoint Directions[] = { {1,0}, {0,1}, {-1,0}, {0,-1} };
static int32 Distance(FIntPoint A, FIntPoint B) { return FMath::Abs(A.X-B.X)+FMath::Abs(A.Y-B.Y); }
static void Connect(FPGDungeonLayout& L, int32 A, int32 B)
{
    FPGDungeonConnection E; E.A=A; E.B=B; L.Connections.Add(E);
}
static int32 Add(FPGDungeonLayout& L, FIntPoint Cell, EPGDungeonRoomRole Role, int32 Objective=0)
{
    FPGDungeonRoom R; R.Id=L.Rooms.Num(); R.Cell=Cell; R.Role=Role; R.Objective=Objective;
    return L.Rooms.Add(R);
}
bool Validate(const UPGDungeonDefinition& D, const FPGDungeonLayout& L, FString& Error)
{
    auto Fail=[&Error](const TCHAR* S){ Error=S; return false; };
    if (D.MinRooms<8 || D.MaxRooms>12 || D.MaxRooms<D.MinRooms ||
        !FMath::IsFinite(D.RoomSize) || !FMath::IsFinite(D.CorridorLength) ||
        !FMath::IsFinite(D.CorridorWidth) || !FMath::IsFinite(D.WallHeight) ||
        D.RoomSize<2000 || D.CorridorLength<600 || D.CorridorWidth<600 ||
        D.CorridorWidth>D.RoomSize-400 || D.WallHeight<100)
        return Fail(TEXT("Invalid dungeon size/socket contract"));
    if (L.Version!=1 || L.Rooms.Num()<D.MinRooms || L.Rooms.Num()>D.MaxRooms)
        return Fail(TEXT("Invalid version/room count"));
    TSet<FIntPoint> Cells; TSet<uint64> Edges;
    int32 Entrance=0, Boss=0, Elite=0; int32 Objectives[6]={};
    for (int32 I=0; I<L.Rooms.Num(); ++I)
    {
        const auto& R=L.Rooms[I];
        if (R.Id!=I || Cells.Contains(R.Cell) || FMath::Abs(int64(R.Cell.X))>12 || FMath::Abs(int64(R.Cell.Y))>12)
            return Fail(TEXT("Invalid identity, overlap or extent"));
        Cells.Add(R.Cell);
        Entrance+=R.Role==EPGDungeonRoomRole::Entrance; Boss+=R.Role==EPGDungeonRoomRole::Boss;
        Elite+=R.Role==EPGDungeonRoomRole::Elite;
        if (R.Objective<0 || R.Objective>5) return Fail(TEXT("Invalid objective"));
        ++Objectives[R.Objective];
    }
    if (Entrance!=1 || Boss!=1 || Elite<1 || Elite>2 || L.Rooms[0].Role!=EPGDungeonRoomRole::Entrance || L.Rooms[6].Role!=EPGDungeonRoomRole::Boss)
        return Fail(TEXT("Invalid mandatory roles"));
    for (int32 I=1; I<=5; ++I) if (Objectives[I]!=1 || L.Rooms[I].Objective!=I)
        return Fail(TEXT("Invalid mandatory objectives"));
    for (const auto& E:L.Connections)
    {
        if (!L.Rooms.IsValidIndex(E.A) || !L.Rooms.IsValidIndex(E.B) || E.A==E.B || Distance(L.Rooms[E.A].Cell,L.Rooms[E.B].Cell)!=1)
            return Fail(TEXT("Invalid socket connection"));
        const uint64 Key=(uint64(FMath::Min(E.A,E.B))<<32)|uint32(FMath::Max(E.A,E.B));
        if (Edges.Contains(Key)) return Fail(TEXT("Duplicate connection"));
        Edges.Add(Key);
    }
    for (int32 I=0; I<6; ++I) if (!Edges.Contains((uint64(I)<<32)|uint32(I+1))) return Fail(TEXT("Missing main path"));
    TSet<int32> Reached; Reached.Add(0);
    for (int32 Pass=0; Pass<L.Rooms.Num(); ++Pass) for (const auto& E:L.Connections)
    {
        if (Reached.Contains(E.A)) Reached.Add(E.B);
        if (Reached.Contains(E.B)) Reached.Add(E.A);
    }
    if (Reached.Num()!=L.Rooms.Num()) return Fail(TEXT("Disconnected layout"));
    // P0 is a tree: no branch can bypass an objective or the boss entrance.
    if (L.Connections.Num()!=L.Rooms.Num()-1) return Fail(TEXT("Unexpected cycle"));
    Error.Reset(); return true;
}
bool Build(const UPGDungeonDefinition& D, int32 Seed, int32 Attempt, FPGDungeonLayout& Out, FString& Error, bool bFallback)
{
    Out=FPGDungeonLayout(); Out.Seed=Seed; Out.Attempt=Attempt; Out.bFallback=bFallback;
    if (D.MinRooms<8 || D.MaxRooms>12 || D.MinRooms>D.MaxRooms) { Error=TEXT("Invalid room limits"); return false; }
    FRandomStream R(StreamSeed(Seed,1,Attempt));
    const int32 Count=bFallback ? D.MinRooms : R.RandRange(D.MinRooms,D.MaxRooms);
    TSet<FIntPoint> Used;
    FIntPoint Cell(0,0);
    for (int32 I=0; I<7; ++I)
    {
        if (I) Cell+=bFallback ? Directions[0] : Directions[R.RandRange(0,1)];
        Add(Out,Cell,I==0 ? EPGDungeonRoomRole::Entrance : I==6 ? EPGDungeonRoomRole::Boss :
            I==4 ? EPGDungeonRoomRole::Elite : EPGDungeonRoomRole::Combat, I>0 && I<6 ? I : 0);
        Used.Add(Cell); if (I) Connect(Out,I-1,I);
    }
    // At most three branches, each one or two rooms. Enumerate candidates in stable order.
    TArray<int32> Roots={1,2,3,4,5};
    if (!bFallback) for (int32 I=Roots.Num()-1; I>0; --I) Roots.Swap(I,R.RandRange(0,I));
    int32 Branches=0;
    for (int32 Root:Roots)
    {
        if (Out.Rooms.Num()>=Count || Branches==3) break;
        TArray<int32> Choices={0,1,2,3};
        if (!bFallback) for (int32 I=3; I>0; --I) Choices.Swap(I,R.RandRange(0,I));
        const int32 Depth=FMath::Min(2,Count-Out.Rooms.Num());
        for (int32 Dir:Choices)
        {
            const FIntPoint Start=Out.Rooms[Root].Cell;
            if (Used.Contains(Start+Directions[Dir]) || (Depth==2 && Used.Contains(Start+Directions[Dir]*2))) continue;
            int32 Parent=Root;
            for (int32 Step=1; Step<=Depth; ++Step)
            {
                Cell=Start+Directions[Dir]*Step;
                int32 Id=Add(Out,Cell,Step==Depth ? EPGDungeonRoomRole::Treasure : EPGDungeonRoomRole::Combat);
                Connect(Out,Parent,Id); Parent=Id; Used.Add(Cell);
            }
            ++Branches; break;
        }
    }
    if (Out.Rooms.Num()!=Count) { Error=TEXT("Branch search exhausted"); return false; }
    return Validate(D,Out,Error);
}
}
