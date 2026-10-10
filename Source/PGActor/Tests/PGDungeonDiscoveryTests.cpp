#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Dungeon/PGDungeonDiscoverySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGDungeonDiscoveryTest, "PG.Dungeon.Discovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGDungeonDiscoveryTest::RunTest(const FString& Parameters)
{
    auto* Map = NewObject<UPGDungeonDiscoverySubsystem>();
    FPGDungeonLayout Layout;
    FPGDungeonRoom Entrance; Entrance.Id=10; Entrance.Role=EPGDungeonRoomRole::Entrance;
    FPGDungeonRoom Combat; Combat.Id=20; Combat.Cell=FIntPoint(1,0); Combat.Objective=1;
    FPGDungeonRoom Treasure; Treasure.Id=30; Treasure.Cell=FIntPoint(1,-1); Treasure.Role=EPGDungeonRoomRole::Treasure;
    Layout.Rooms={Entrance,Combat,Treasure};
    FPGDungeonConnection A; A.A=10; A.B=20;
    FPGDungeonConnection B; B.A=20; B.B=30;
    Layout.Connections={A,B};
    int32 Events=0;
    Map->OnMapChanged.AddLambda([&](const auto&) { ++Events; });
    Map->InitializeMap(Layout,FVector(100,200,0),2800,3800);
    TestTrue(TEXT("Nothing exposed before discovery"),Map->GetSnapshot().Rooms.IsEmpty());
    Map->Observe(FVector(100,200,100),1);
    TestEqual(TEXT("Only entrance discovered"),Map->GetSnapshot().Rooms.Num(),1);
    TestTrue(TEXT("Unknown links hidden"),Map->GetSnapshot().Connections.IsEmpty());
    TestEqual(TEXT("Entrance shows only its doorway"),Map->GetSnapshot().Doorways.Num(),1);
    const int32 Before=Events;
    Map->Observe(FVector(110,210,100),1);
    TestEqual(TEXT("Movement inside room does not rebuild map"),Events,Before);
    Map->Observe(FVector(2000,200,100),1);
    TestEqual(TEXT("Corridor does not discover next room"),Map->GetSnapshot().Rooms.Num(),1);
    Map->Observe(FVector(3900,200,-500),1);
    TestEqual(TEXT("Fallen player cannot discover room"),Map->GetSnapshot().Rooms.Num(),1);
    Map->Observe(FVector(3900,200,100),1);
    TestEqual(TEXT("Combat room discovered"),Map->GetSnapshot().Rooms.Num(),2);
    TestEqual(TEXT("Known connection exposed"),Map->GetSnapshot().Connections.Num(),1);
    TestFalse(TEXT("Treasure role remains hidden"),Map->GetSnapshot().Rooms.ContainsByPredicate([](const auto& R){return R.Role==EPGDungeonRoomRole::Treasure;}));
    Map->Observe(FVector(3900,200,100),2);
    TestEqual(TEXT("Objective changes without new discovery"),Map->GetSnapshot().Objective,2);
    Map->Observe(FVector(3900,-3600,100),2);
    TestEqual(TEXT("Negative branch coordinates"),Map->GetSnapshot().Rooms.Num(),3);
    TestTrue(TEXT("All known links replace doorway stubs"),Map->GetSnapshot().Doorways.IsEmpty());
    Map->Observe(FVector(100,200,100),2);
    TestEqual(TEXT("Revisit retains discovery"),Map->GetSnapshot().Rooms.Num(),3);
    Map->ResetMap();
    TestFalse(TEXT("Cancellation hides minimap"),Map->GetSnapshot().bActive);
    TestTrue(TEXT("Cancellation removes topology"),Map->GetSnapshot().Rooms.IsEmpty());
    Map->InitializeMap(Layout,FVector::ZeroVector,2800,3800);
    TestTrue(TEXT("Same seed new run cannot inherit discovery"),Map->GetSnapshot().Rooms.IsEmpty());
    Map->InitializeMap(Layout,FVector::ZeroVector,2800,0);
    TestFalse(TEXT("Invalid spacing rejected"),Map->GetSnapshot().bActive);
    Map->OnMapChanged.Clear();
    return true;
}
#endif
