#include "PGConsumableComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Progression/PGRunTelemetrySubsystem.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGData/DataAsset/Progression/PGConsumableData.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"
#include "PGUI/Manager/PGDamageFloaterManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UPGConsumableComponent::UPGConsumableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPGConsumableComponent::Initialize(UPGConsumableData* Definition)
{
    if (bInitialized || bEnded) return;
    Data = Definition ? Definition : GetMutableDefault<UPGConsumableData>();
    if (!Data->IsValidDefinition())
    {
        UE_LOG(LogTemp, Error, TEXT("PGConsumable invalid definition: %s"), *GetNameSafe(Data));
        Data = nullptr;
        return;
    }
    bInitialized = true;
    Charges = Data->Capacity;
    PreparedVFX = Data->HealVFX.LoadSynchronous();
    PreparedSound = Data->HealSound.LoadSynchronous();
#if WITH_EDITOR
    if (PreparedVFX) PreparedVFX->WaitForCompilationComplete();
#endif
    Publish();
}

float UPGConsumableComponent::GetRemainingCooldown() const
{
    return GetWorld() ? float(FMath::Max(0., CooldownUntil - GetWorld()->GetTimeSeconds())) : 0.f;
}

bool UPGConsumableComponent::CanUse(FText& Reason) const
{
    auto Reject = [&Reason](const TCHAR* Text) { Reason = FText::FromString(Text); return false; };
    const auto* Player = Cast<APGCharacterPlayer>(GetOwner());
    const auto* ASC = Player ? Player->GetPGAbilitySystemComponent() : nullptr;
    if (!bInitialized || bEnded || !Data || !ASC || !GetWorld()) return Reject(TEXT("회복약 준비 중"));
    if (ASC->GetHealth() <= 0.f || ASC->HasMatchingGameplayTag(PGGamePlayTags::Shared_Status_Dead)) return Reject(TEXT("사망"));
    if (!ActiveStage.IsValid())
        for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It) { ActiveStage = *It; break; }
    if (ActiveStage.IsValid())
    {
        const auto Phase = ActiveStage->GetCurrentStageState();
        if (Phase != EPGStageState::InProgress && Phase != EPGStageState::WaveIntermission)
            return Reject(Phase == EPGStageState::BuildPhase || Phase == EPGStageState::RunPreparation
                ? TEXT("전투 시작 시 체력과 회복약 준비") : TEXT("지금은 사용할 수 없습니다"));
    }
    if (!Player->IsConsumableInputAllowed()) return Reject(TEXT("지금은 사용할 수 없습니다"));
    if (bUsing || ASC->IsProcessingDamage()) return Reject(TEXT("잠시 후 다시 사용하세요"));
    if (Charges <= 0) return Reject(TEXT("회복약 없음"));
    if (GetRemainingCooldown() > 0.f) return Reject(TEXT("재사용 대기"));
    if (ASC->GetHealth() >= ASC->GetCombatStat(EPGStatType::Health)) return Reject(TEXT("체력이 가득 찼습니다"));
    Reason = FText::GetEmpty();
    return true;
}

FPGConsumableState UPGConsumableComponent::GetState() const
{
    FPGConsumableState State;
    State.Count = Charges;
    State.Cooldown = GetRemainingCooldown();
    if (Data)
    {
        State.Capacity = Data->Capacity;
        State.Name = Data->DisplayName;
        State.HealFraction = Data->HealFraction;
        State.CooldownDuration = Data->CooldownSeconds;
        State.LowHealthFraction = Data->LowHealthFraction;
    }
    State.bCanUse = CanUse(State.Reason);
    if (GetWorld() && GetWorld()->GetTimeSeconds() < NoticeUntil) State.Notice = Notice;
    return State;
}

void UPGConsumableComponent::SetNotice(const FText& Text)
{
    Notice = Text;
    NoticeUntil = GetWorld()->GetTimeSeconds() + 2.5;
}

bool UPGConsumableComponent::TryUse()
{
    FText Reason;
    if (!CanUse(Reason))
    {
        // A health-change listener must not recursively publish another rejection.
        if (!bUsing && !bEnded && GetWorld()) { SetNotice(Reason); Publish(); }
        return false;
    }
    auto* Player = CastChecked<APGCharacterPlayer>(GetOwner());
    auto* ASC = Player->GetPGAbilitySystemComponent();
    float Applied = 0.f;
    const float Requested = ASC->GetCombatStat(EPGStatType::Health) * Data->HealFraction;
    {
        TGuardValue<bool> Guard(bUsing, true);
        Applied = ASC->RestoreHealth(Requested);
        if (Applied <= 0.f) return false;
        --Charges;
        CooldownUntil = GetWorld()->GetTimeSeconds() + Data->CooldownSeconds;
    }
    if (auto* Telemetry = UPGRunTelemetrySubsystem::Get(this)) Telemetry->RecordPotion(Requested, Applied);
    SetNotice(FText::FromString(FString::Printf(TEXT("생명력 +%.0f"), Applied)));
    StopEffect();
    if (PreparedVFX)
    {
        ActiveVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(PreparedVFX, Player->GetRootComponent(), NAME_None,
            FVector(0,0,-85), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false, false, ENCPoolMethod::None, true);
        if (ActiveVFX)
        {
            ActiveVFX->SetRelativeScale3D(FVector(Data->EffectScale));
            ActiveVFX->SetVariableLinearColor(TEXT("User.Color"), Data->HealColor);
            ActiveVFX->Activate(true);
            GetWorld()->GetTimerManager().SetTimer(EffectTimer, this, &ThisClass::StopEffect, Data->EffectSeconds, false);
        }
    }
    if (PreparedSound) UGameplayStatics::PlaySound2D(this, PreparedSound, .6f);
    if (auto* Floaters = UPGDamageFloaterManager::Get(this)) Floaters->AddFloater(FMath::RoundToInt(Applied), EPGDamageType::Heal, Player, true);
    UE_LOG(LogTemp, Log, TEXT("PGConsumable used requested=%.2f applied=%.2f charges=%d cooldown=%.2f"), Requested, Applied, Charges, Data->CooldownSeconds);
    Publish();
    return true;
}

void UPGConsumableComponent::BeginStage(APGStageManager* Stage)
{
    if (!bInitialized || bEnded || !IsValid(Stage) || Stage->GetWorld() != GetWorld()) return;
    ActiveStage = Stage;
    RestockedStage = INDEX_NONE;
    // A stage restart also recreates its enemies; no mid-wave refill calls this path.
    if (!bStartedStage || Data->bRefillOnStageClear) { Charges = Data->Capacity; CooldownUntil = 0.; }
    NoticeUntil = 0.;
    StopEffect();
    if (auto* Player = Cast<APGCharacterPlayer>(GetOwner()); Player && (!bStartedStage || Data->bHealOnStageClear))
        Player->GetPGAbilitySystemComponent()->RestoreHealth(Player->GetPGAbilitySystemComponent()->GetCombatStat(EPGStatType::Health));
    bStartedStage = true;
    Publish();
}

void UPGConsumableComponent::CompleteStage(APGStageManager* Stage)
{
    if (!bInitialized || bEnded || !IsValid(Stage) || Stage->GetWorld() != GetWorld() ||
        Stage->GetCurrentStageState() != EPGStageState::BuildPhase || RestockedStage == Stage->GetCurrentStageId()) return;
    auto* Player = Cast<APGCharacterPlayer>(GetOwner());
    auto* ASC = Player ? Player->GetPGAbilitySystemComponent() : nullptr;
    if (!ASC || ASC->GetHealth() <= 0.f) return;
    ActiveStage = Stage;
    RestockedStage = Stage->GetCurrentStageId();
    StopEffect();
    if (Data->bRefillOnStageClear) { Charges = Data->Capacity; CooldownUntil = 0.; }
    if (Data->bHealOnStageClear) ASC->RestoreHealth(ASC->GetCombatStat(EPGStatType::Health));
    SetNotice(FText::FromString(Data->bHealOnStageClear && Data->bRefillOnStageClear ? TEXT("정비 완료 · 체력 회복 · 회복약 보충") :
        Data->bHealOnStageClear ? TEXT("정비 완료 · 체력 회복") : Data->bRefillOnStageClear ? TEXT("정비 완료 · 회복약 보충") : TEXT("정비 완료")));
    Publish();
}

void UPGConsumableComponent::OnOwnerDied()
{
    if (auto* Telemetry = UPGRunTelemetrySubsystem::Get(this)) Telemetry->RecordPotionAtDeath(Charges, GetRemainingCooldown());
    StopEffect();
    NoticeUntil = 0.;
    Publish();
}

void UPGConsumableComponent::DebugSetCharges(int32 Count)
{
#if !UE_BUILD_SHIPPING
    if (!Data) return;
    Charges = FMath::Clamp(Count, 0, Data->Capacity);
    Publish();
#endif
}

void UPGConsumableComponent::Publish()
{
    if (bEnded) return;
    if (auto* Messages = UPGMessageManager::Get(this))
    {
        FPGConsumablePresentation View;
        View.Owner = GetOwner(); View.State = GetState();
        Messages->SendMessage(EPGUIMessageType::ConsumableChanged, &View);
    }
}

void UPGConsumableComponent::StopEffect()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(EffectTimer);
    if (IsValid(ActiveVFX)) { ActiveVFX->DeactivateImmediate(); ActiveVFX->DestroyComponent(); }
    ActiveVFX = nullptr;
}

void UPGConsumableComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnded = true;
    StopEffect();
    ActiveStage.Reset();
    Super::EndPlay(Reason);
}
