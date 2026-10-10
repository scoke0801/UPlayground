#include "PGLootDrop.h"
#include "PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Components/WidgetComponent.h"
#include "PGUI/Style/PGUIStyle.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGActor/Components/Stat/PGStatComponent.h"

APGLootDrop::APGLootDrop()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Label = CreateDefaultSubobject<UWidgetComponent>(TEXT("LootLabel"));
    Label->SetupAttachment(GetRootComponent()); Label->SetWidgetSpace(EWidgetSpace::Screen);
    Label->SetDrawAtDesiredSize(true); Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label->SetTickWhenOffscreen(false); Label->SetRelativeLocation(FVector(0,0,32));
    // Retained for old component serialization. A single screen overlay now places all labels.
    Label->SetVisibility(false); Label->SetComponentTickEnabled(false);
    Beam = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RarityBeam"));
    Beam->SetupAttachment(GetRootComponent()); Beam->SetAutoActivate(false);
    BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LootBeam"));
    BeamMesh->SetupAttachment(GetRootComponent()); BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); BeamMesh->SetCastShadow(false);
    // Blender-authored, centred 100 cm mesh. Runtime scaling preserves the
    // existing six-centimetre rarity beam width and data-driven height.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(
        TEXT("/Game/Art/UtilityModels/SM_PG_LootBeam.SM_PG_LootBeam"));
    if (Mesh.Succeeded()) BeamMesh->SetStaticMesh(Mesh.Object);
    BeamMesh->SetVisibility(false);
}
void APGLootDrop::InitializeItem(const FPGItemInstance& InItem)
{
    Item = InItem;
    auto* Profile = UPGProfileSubsystem::Get(this);
    const auto* Catalog = Profile ? Profile->GetCatalog() : nullptr;
    const auto* Def = Catalog ? Catalog->FindItem(Item.DefinitionId) : nullptr;
    if (!Def) return;
    const auto Color = FPGUIStyle::Get().RarityColor(Def->Rarity);
    if (auto* Material = Catalog->BeamMaterial.LoadSynchronous())
    {
        const float Height = FMath::Max(0.f, Catalog->BeamHeights.FindRef(Def->Rarity));
        auto* Dynamic = UMaterialInstanceDynamic::Create(Material, this); Dynamic->SetVectorParameterValue(TEXT("GradeColor"), Color);
        BeamMesh->SetMaterial(0, Dynamic); BeamMesh->SetRelativeScale3D(FVector(.06f,.06f,Height / 100.f));
        BeamMesh->SetRelativeLocation(FVector(0,0,Height / 2.f)); BeamMesh->SetVisibility(Height > 0);
    }
    if (const auto* Asset = Catalog->DropBeams.Find(Def->Rarity))
        if (auto* System = Asset->LoadSynchronous()) { Beam->SetAsset(System); Beam->SetVariableLinearColor(TEXT("User.Color"), Color); Beam->Activate(true); }
    // Sound starts at spawn; pickup remains responsive during the visual arc.
    if (const auto* Asset = Catalog->DropSounds.Find(Def->Rarity))
        if (auto* Sound = Asset->LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
    ArcElapsed = 0; ArcDuration = FMath::Max(.05f, Catalog->DropArcDuration); ArcHeight = FMath::Max(0.f, Catalog->DropArcHeight);
    SetActorTickEnabled(true);
}
void APGLootDrop::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ArcElapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp(ArcElapsed / ArcDuration, 0.f, 1.f);
    Label->SetRelativeLocation(FVector(40.f * FMath::Sin(PI * Alpha), 0, 32.f + 4.f * ArcHeight * Alpha * (1-Alpha)));
    if (Alpha >= 1.f) SetActorTickEnabled(false);
}
bool APGLootDrop::TryPickup(APawn* Player)
{
    auto* Profile = UPGProfileSubsystem::Get(this);
    auto* Character = Cast<APGCharacterPlayer>(Player);
    if (bClaimed || !Character || !Profile || !Profile->GetCatalog() || Character->GetWorld() != GetWorld() ||
        Character->GetStatComponent()->GetCurrentHealth() <= 0 || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(Profile->GetCatalog()->PickupRadius)) return false;
    bClaimed = true;
    if (!Profile->TryPickup(Item)) { bClaimed = false; return false; }
    Destroy(); return true;
}
void APGLootDrop::SpawnForEnemy(APGCharacterEnemy* Enemy)
{
    if (!IsValid(Enemy) || Enemy->bLootResolved || !Enemy->bCanDropLoot) return;
    auto* Profile = UPGProfileSubsystem::Get(Enemy);
    auto* Tables = UPGDataTableManager::Get(Enemy);
    const auto* Row = Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    if (!Profile || !Row || !Profile->GetProfile()->RunId.IsValid()) return;
    Enemy->bLootResolved = true;
    if (!Enemy->LootGuid.IsValid())
    {
        // Legacy placed/dev enemies use their actor path; staged encounters use authored ordinals.
        Enemy->LootGuid = FGuid::NewDeterministicGuid(Profile->GetProfile()->RunId.ToString() + Enemy->GetPathName());
        Enemy->LootSeed = int32(FCrc::StrCrc32(*Enemy->GetPathName()) ^ uint32(Profile->GetProfile()->RunSeed));
    }
    if (Profile->HasClaimedLoot(Enemy->LootGuid)) return;
    FRandomStream Random(Enemy->LootSeed);
    FPGItemInstance Item;
    if (!Profile->RollDrop(Random, Item, Row->DropPoolId)) return;
    Item.Guid = Enemy->LootGuid;
    const bool bResultReward = Row->Role == EPGEnemyRole::Boss && Profile->GetCatalog()->bRoguelikeRuns;
    UE_LOG(LogTemp, Log, TEXT("PGLoot rolled enemy=%d pool=%s seed=%d item=%d guid=%s result=%d"),
        Enemy->GetCharacterTID(), *Row->DropPoolId.ToString(), Enemy->LootSeed, Item.DefinitionId, *Item.Guid.ToString(), bResultReward);
    if (bResultReward)
    {
        if (!Profile->QueueBossReward(Item)) UE_LOG(LogTemp, Error, TEXT("PGLoot boss reward could not be queued"));
        return;
    }
    FVector Location = Enemy->GetActorLocation();
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGLootGround), false, Enemy);
    if (Enemy->GetWorld()->LineTraceSingleByChannel(Hit, Location, Location - FVector(0,0,500), ECC_WorldStatic, Params)) Location = Hit.ImpactPoint + FVector(0,0,30);
    FActorSpawnParameters Spawn;
    Spawn.Owner = Enemy->EncounterOwner.Get();
    auto* Drop = Enemy->GetWorld()->SpawnActor<APGLootDrop>(Location, FRotator::ZeroRotator, Spawn);
    if (Drop) Drop->InitializeItem(Item);
    else { Enemy->bLootResolved = false; UE_LOG(LogTemp, Error, TEXT("PGLoot actor spawn failed: %s"), *Item.Guid.ToString()); }
}
FVector APGLootDrop::GetLabelLocation() const
{
    return Label->GetComponentLocation();
}
APGLootDrop* APGLootDrop::FindNearestPickup(const APawn* Player)
{
    const auto* Profile = UPGProfileSubsystem::Get(Player);
    const auto* Catalog = Profile ? Profile->GetCatalog() : nullptr;
    if (!Player || !Catalog) return nullptr;
    APGLootDrop* Nearest = nullptr;
    double Best = FMath::Square(Catalog->PickupRadius);
    for (TActorIterator<APGLootDrop> It(Player->GetWorld()); It; ++It)
    {
        if (It->bClaimed || !It->Item.Guid.IsValid() || !Catalog->FindItem(It->Item.DefinitionId)) continue;
        const double Distance = FVector::DistSquared(Player->GetActorLocation(),It->GetActorLocation());
        if (Distance < Best || (Distance == Best && (!Nearest || It->Item.Guid < Nearest->Item.Guid)))
        { Best = Distance; Nearest = *It; }
    }
    return Nearest;
}

