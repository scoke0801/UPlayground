#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGData/DataAsset/Dungeon/PGDungeonDefinition.h"
#include "PGData/Dungeon/PGDungeonLayoutBuilder.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGDungeonLayoutTest,"PG.Dungeon.Layout1000Seeds",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGDungeonLayoutTest::RunTest(const FString&)
{
    auto* D=NewObject<UPGDungeonDefinition>();
    int32 Fallbacks=0;
    for (int32 Seed=0; Seed<1000; ++Seed)
    {
        FPGDungeonLayout L, Repeat; FString Error; bool Success=false;
        for (int32 Attempt=0; Attempt<3; ++Attempt)
            if (PGDungeon::Build(*D,Seed,Attempt,L,Error)) { Success=true; break; }
        if (!Success) { ++Fallbacks; Success=PGDungeon::Build(*D,Seed,3,L,Error,true); }
        if (!TestTrue(FString::Printf(TEXT("Seed %d: %s"),Seed,*Error),Success)) return false;
        TestTrue(TEXT("Validated"),PGDungeon::Validate(*D,L,Error));
        D->PropsPerRoom=Seed%9;
        TestTrue(TEXT("Replay ignores decoration"),PGDungeon::Build(*D,Seed,L.Attempt,Repeat,Error,L.bFallback));
        TestEqual(TEXT("Room count stable"),L.Rooms.Num(),Repeat.Rooms.Num());
        for (int32 I=0; I<L.Rooms.Num(); ++I) TestTrue(TEXT("Stable identity and cell"),L.Rooms[I].Cell==Repeat.Rooms[I].Cell && L.Rooms[I].Id==Repeat.Rooms[I].Id);
        TestNotEqual(TEXT("Separate streams"),PGDungeon::StreamSeed(Seed,1),PGDungeon::StreamSeed(Seed,3));
    }
    TestTrue(TEXT("Fallback rate below 1 percent"),Fallbacks<10);
    FPGDungeonLayout L; FString Error;
    for (int32 Count=8; Count<=12; ++Count)
    {
        D->MinRooms=D->MaxRooms=Count;
        TestTrue(TEXT("Configured fallback room count"),PGDungeon::Build(*D,0,3,L,Error,true));
    }
    L.Rooms[1].Cell=L.Rooms[0].Cell;
    TestFalse(TEXT("Reject overlaps"),PGDungeon::Validate(*D,L,Error));
    PGDungeon::Build(*D,0,3,L,Error,true); L.Connections.Pop();
    TestFalse(TEXT("Reject disconnected room"),PGDungeon::Validate(*D,L,Error));
    D->CorridorWidth=100;
    TestFalse(TEXT("Do not relax narrow socket"),PGDungeon::Build(*D,0,3,L,Error,true));
    D->MinRooms=13;
    TestFalse(TEXT("Reject impossible bounds"),PGDungeon::Build(*D,0,0,L,Error));
    return true;
}
#endif
