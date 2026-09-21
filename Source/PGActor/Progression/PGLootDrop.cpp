#include "PGLootDrop.h"
#include "PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Components/WidgetComponent.h"
#include "PGUI/Widget/Billboard/PGUILootLabel.h"
#include "NiagaraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
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
    Beam = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RarityBeam"));
    Beam->SetupAttachment(GetRootComponent()); Beam->SetAutoActivate(false);
    BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LootBeam"));
    BeamMesh->SetupAttachment(GetRootComponent()); BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); BeamMesh->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    BeamMesh->SetStaticMesh(Mesh.Object); BeamMesh->SetVisibility(false);
}
void APGLootDrop::InitializeItem(const FPGItemInstance& InItem)
{
    Item = InItem;
    auto* Profile = UPGProfileSubsystem::Get(this);
    const auto* Catalog = Profile ? Profile->GetCatalog() : nullptr;
    const auto* Def = Catalog ? Catalog->FindItem(Item.DefinitionId) : nullptr;
    if (!Def) return;
    auto* Widget = CreateWidget<UPGUILootLabel>(GetWorld(), UPGUILootLabel::StaticClass());
    const auto Color = Def->Rarity == EPGItemRarity::Rare ? FLinearColor(1,.65f,.15f) : Def->Rarity == EPGItemRarity::Magic ? FLinearColor(.2f,.65f,1) : FLinearColor(.75f,.8f,.85f);
    if (Widget) { Widget->Configure(Def->DisplayName, Color, Def->Icon.LoadSynchronous()); Label->SetWidget(Widget); }
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
}bool APGLootDrop::TryPickup(APawn* Player)
{
    auto* Profile = UPGProfileSubsystem::Get(this);
    auto* Character = Cast<APGCharacterPlayer>(Player);
    if (bClaimed || !Character || !Profile || !Profile->GetCatalog() || Character->GetWorld() != GetWorld() ||
        Character->GetStatComponent()->GetCurrentHealth() <= 0 || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(Profile->GetCatalog()->PickupRadius)) return false;
    bClaimed = true;
    if (!Profile->TryPickup(Item)) { bClaimed = false; return false; }
    Destroy(); return true;
}
void APGLootDrop::SpawnForEnemy(AActor* Enemy)
{
    auto* Profile = UPGProfileSubsystem::Get(Enemy);
    if (!Profile) return;
    FRandomStream Random(FMath::Rand());
    FPGItemInstance Item;
    if (!Profile->RollDrop(Random, Item)) return;
    FVector Location = Enemy->GetActorLocation();
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGLootGround), false, Enemy);
    if (Enemy->GetWorld()->LineTraceSingleByChannel(Hit, Location, Location - FVector(0,0,500), ECC_WorldStatic, Params)) Location = Hit.ImpactPoint + FVector(0,0,30);
    auto* Drop = Enemy->GetWorld()->SpawnActor<APGLootDrop>(Location, FRotator::ZeroRotator);
    if (Drop) Drop->InitializeItem(Item);
}

