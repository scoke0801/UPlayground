#include "Dungeon/PGDungeonGenerator.h"
#include "PGData/DataAsset/Dungeon/PGDungeonDefinition.h"
#include "PGData/Dungeon/PGDungeonLayoutBuilder.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#if !UE_BUILD_SHIPPING
namespace
{
APGDungeonGenerator* FindDungeon(UWorld* World)
{
    if (World) for (TActorIterator<APGDungeonGenerator> It(World); It; ++It) return *It;
    return nullptr;
}
FAutoConsoleCommandWithWorldAndArgs Generate(TEXT("pg.Dungeon.Generate"),TEXT("Generate dungeon with an integer seed"),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
{
    int32 Seed=0;
    if (Args.Num()!=1 || !LexTryParseString(Seed,*Args[0])) { UE_LOG(LogTemp,Warning,TEXT("Usage: pg.Dungeon.Generate <int32 seed>")); return; }
    if (auto* G=FindDungeon(World)) G->Generate(Seed);
}));
FAutoConsoleCommandWithWorldAndArgs Debug(TEXT("pg.Dungeon.Debug"),TEXT("Draw dungeon rooms and objectives"),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* World)
{ if (auto* G=FindDungeon(World)) G->DrawDebugLayout(); }));
FAutoConsoleCommandWithWorldAndArgs Validate(TEXT("pg.Dungeon.Validate"),TEXT("Check current player navigation"),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* World)
{ if (auto* G=FindDungeon(World)) { const bool OK=G->ValidateNavigation(); UE_LOG(LogTemp,Display,TEXT("PGDungeon VALIDATE %s: %s"),OK ? TEXT("PASS"):TEXT("FAIL"),*G->LastError); } }));
FAutoConsoleCommandWithWorldAndArgs Batch(TEXT("pg.Dungeon.Batch"),TEXT("Validate 1..10000 layout seeds (no world navigation)"),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
{
    auto* G=FindDungeon(World); int32 Count=1000;
    if (!G || !G->Definition || (Args.Num()>0 && !LexTryParseString(Count,*Args[0])) || Count<1 || Count>10000) return;
    int32 Failed=0,Fallbacks=0;
    for (int32 Seed=0; Seed<Count; ++Seed)
    {
        FPGDungeonLayout L; FString Error; bool OK=false;
        for (int32 Attempt=0; Attempt<FMath::Clamp(G->Definition->MaxAttempts,1,3); ++Attempt)
            if (PGDungeon::Build(*G->Definition,Seed,Attempt,L,Error)) { OK=true; break; }
        if (!OK) { ++Fallbacks; OK=PGDungeon::Build(*G->Definition,Seed,3,L,Error,true); }
        if (!OK) { ++Failed; UE_LOG(LogTemp,Warning,TEXT("PGDungeon FAIL seed=%d version=1 reason=%s replay: pg.Dungeon.Generate %d"),Seed,*Error,Seed); }
    }
    UE_LOG(LogTemp,Display,TEXT("PGDungeon BATCH count=%d failures=%d fallbacks=%d (structure only)"),Count,Failed,Fallbacks);
}));
}
#endif
