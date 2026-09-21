// Fill out your copyright notice in the Description page of Project Settings.


#include "PGEnemyAbilityAttack.h"

#include "AbilitySystemComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Handler/Skill/PGEnemySkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGActor/Components/Stat/PGStatComponent.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPGEliteDebug(TEXT("pg.Combat.EliteDebug"), 0, TEXT("Draw elite telegraph and strike bounds."));

void UPGEnemyAbilityAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 스킬 결정해서 대상 스킬 Ability를 활성화할 수 있어야한다.
	APGCharacterEnemy* Character = GetEnemyCharacterFromActorInfo();
	if (nullptr == Character)
	{
		EndAbilitySelf();
		return;
	}
	if (false == CheckMontageIsPlaying(Character, 0.2f))
	{
		EndAbilitySelf();
		return;
	}
	
	
	FPGEnemySkillHandler* SkillHandler = static_cast<FPGEnemySkillHandler*>(Character->GetSkillHandler());
	if (nullptr == SkillHandler)
	{
		EndAbilitySelf();
		return;
	}

	// Spec에서 태그 확인
	// 태그 기반 스킬 선택
	EPGSkillSlot SelectedSkillSlot = EPGSkillSlot::NormalAttack;
	if (FGameplayAbilitySpec* Spec = GetAbilitySystemComponentFromActorInfo()->FindAbilitySpecFromHandle(Handle))
	{
		SelectedSkillSlot = SkillHandler->GetSkillSlotByTag( Spec->DynamicAbilityTags);
	}
	// 스킬을 사용할 수 없으면 랜덤 선택
	if (!SkillHandler->IsCanUseSkill(SelectedSkillSlot))
	{
		SelectedSkillSlot = SkillHandler->GetRandomSkillSlot();
		if (!SkillHandler->IsCanUseSkill(SelectedSkillSlot))
		{
			EndAbilitySelf();
			return;
		}
	}

	FPGSkillDataRow* Row = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(SkillHandler->GetSkillID(SelectedSkillSlot));
	if(nullptr == Row)
	{
		EndAbilitySelf();
		return;
	}

    if (Row->TelegraphDuration > 0.f)
    {
        const FPGSkillDataRow Pattern = *Row;
        if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbilitySelf(); return; }
        SkillHandler->UseSkill(SelectedSkillSlot);
        BeginElitePattern(Pattern);
        return;
    }
	UAnimMontage* MontageToPlay = nullptr;
	if (UObject* LoadedObject = Row->MontagePath.TryLoad())
	{
		MontageToPlay = Cast<UAnimMontage>(LoadedObject);
	}
	if (nullptr == MontageToPlay)
	{
		EndAbilitySelf();
        return;
	}

	if (UAbilityTask_PlayMontageAndWait* MontageTask = PlayMontageWait(MontageToPlay))
	{
		MontageTask->ReadyForActivation();
	}
	
	SkillHandler->UseSkill(SelectedSkillSlot);
}

void UPGEnemyAbilityAttack::OnGameplayEventReceived(FGameplayEventData Payload)
{
	if (const APGCharacterBase* TargetActor = Cast<APGCharacterBase>(Payload.Target.Get()))
	{
		if (UAbilitySystemComponent* ASC = TargetActor->GetAbilitySystemComponent())
		{
			FGameplayEventData Data;
			Data.Instigator = Payload.Instigator;
			Data.Target = Payload.Target;
			
			ASC->HandleGameplayEvent(PGGamePlayTags::Shared_Event_HitReact, &Data);
		}
	}
}

void UPGEnemyAbilityAttack::BeginElitePattern(const FPGSkillDataRow& Row)
{
    auto* Character = GetEnemyCharacterFromActorInfo();
    if (!Character || !FMath::IsFinite(Row.TelegraphRadius) || Row.TelegraphRadius <= 0) { EndAbilitySelf(); return; }
    EliteData = Row; bElitePattern = true; Character->bPerformingHeavyAttack = true;
    if (auto* Montage = EliteData.ElitePresentationMontage.LoadSynchronous())
        if (auto* Anim = Character->GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Play(Montage);
            Anim->Montage_SetPosition(Montage, Montage->GetPlayLength() * FMath::Clamp(Row.WindupMontageFraction, 0.f, 1.f));
            Anim->Montage_Pause(Montage);
        }
    auto* Movement = Character->GetCharacterMovement();
    SavedMovementMode = Movement->MovementMode; Movement->StopMovementImmediately(); Movement->DisableMovement();
    if (auto* AI = Cast<AAIController>(Character->GetController())) AI->StopMovement();
    StrikeCenter = Character->GetActorLocation() + Character->GetActorForwardVector() * Row.TelegraphRadius;
    FHitResult Ground;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGTelegraphGround), false, Character);
    if (GetWorld()->LineTraceSingleByChannel(Ground, StrikeCenter + FVector(0,0,100), StrikeCenter - FVector(0,0,500), ECC_WorldStatic, Params)) StrikeCenter.Z = Ground.ImpactPoint.Z;
    if (auto* Material = Row.TelegraphMaterial.LoadSynchronous())
        Telegraph = UGameplayStatics::SpawnDecalAtLocation(this, Material, FVector(120, Row.TelegraphRadius, Row.TelegraphRadius), StrikeCenter, FRotator(-90,0,0), 0);
    if (CVarPGEliteDebug.GetValueOnGameThread()) DrawDebugCircle(GetWorld(), StrikeCenter + FVector(0,0,3), Row.TelegraphRadius, 64, FColor::Orange, false, Row.TelegraphDuration, 0, 3, FVector::ForwardVector, FVector::RightVector, false);
    UE_LOG(LogTemp, Log, TEXT("PGElite Telegraph skill=%d radius=%.1f duration=%.2f"), Row.SkillID, Row.TelegraphRadius, Row.TelegraphDuration);
    GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::StrikeElitePattern, FMath::Max(.01f, Row.TelegraphDuration), false);
}
void UPGEnemyAbilityAttack::StrikeElitePattern()
{
    auto* Character = GetEnemyCharacterFromActorInfo();
    if (!IsActive() || !IsValid(Character) || !Character->GetStatComponent() || Character->GetStatComponent()->GetCurrentHealth() <= 0) { EndAbilitySelf(); return; }
    if (Telegraph) { Telegraph->DestroyComponent(); Telegraph = nullptr; }
    if (auto* Anim = Character->GetMesh()->GetAnimInstance())
        if (auto* Montage = EliteData.ElitePresentationMontage.Get()) Anim->Montage_Resume(Montage);
    // Query broadly, then use ground-projected actor centers for exactly the displayed disk.
    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1); Objects.AddObjectTypesToQuery(ECC_GameTraceChannel3);
    GetWorld()->OverlapMultiByObjectType(Hits, StrikeCenter, FQuat::Identity, Objects, FCollisionShape::MakeSphere(EliteData.TelegraphRadius + 200));
    TSet<AActor*> HitActors;
    for (const auto& Hit : Hits)
    {
        auto* Target = Cast<APawn>(Hit.GetActor());
        if (!Target || HitActors.Contains(Target) || !UPGAbilityBPLibrary::IsTargetActorHostile(Character, Target)) continue;
        if (FVector::DistSquared2D(StrikeCenter, Target->GetActorLocation()) > FMath::Square(EliteData.TelegraphRadius) || FMath::Abs(Target->GetActorLocation().Z - StrikeCenter.Z) > 200.f) continue;
        HitActors.Add(Target);
        FGameplayEventData Event; Event.Instigator = Character; Event.Target = Target;
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target, PGGamePlayTags::Shared_Event_HitReact, Event);
    }
    if (auto* VFX = EliteData.SlamVFX.LoadSynchronous()) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX, StrikeCenter);
    if (CVarPGEliteDebug.GetValueOnGameThread()) DrawDebugCircle(GetWorld(), StrikeCenter + FVector(0,0,4), EliteData.TelegraphRadius, 64, FColor::Red, false, EliteData.RecoveryDuration, 0, 3, FVector::ForwardVector, FVector::RightVector, false);
    UE_LOG(LogTemp, Log, TEXT("PGElite Strike skill=%d hits=%d recovery=%.2f"), EliteData.SkillID, HitActors.Num(), EliteData.RecoveryDuration);
    Character->GetPGAbilitySystemComponent()->OpenRecoveryWindow(EliteData.RecoveryDuration, EliteData.RecoveryDamageBonus);
    // Cyan replaces danger orange: the stationary enemy is now vulnerable.
    if (auto* Material = EliteData.TelegraphMaterial.Get())
    {
        FVector Position = Character->GetActorLocation();
        Position.Z -= Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Telegraph = UGameplayStatics::SpawnDecalAtLocation(this, Material, FVector(120, 100, 100), Position, FRotator(-90,0,0), 0);
        if (Telegraph) if (auto* Dynamic = Telegraph->CreateDynamicMaterialInstance()) Dynamic->SetVectorParameterValue(TEXT("GradeColor"), FLinearColor(.05f, .8f, 1.5f));
    }
    GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::FinishElitePattern, FMath::Max(.01f, EliteData.RecoveryDuration), false);
}
void UPGEnemyAbilityAttack::FinishElitePattern()
{
    EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, false);
}
void UPGEnemyAbilityAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PatternTimer);
    if (Telegraph) { Telegraph->DestroyComponent(); Telegraph = nullptr; }
    if (bElitePattern)
    {
        if (auto* Character = GetEnemyCharacterFromActorInfo())
        {
            if (auto* Anim = Character->GetMesh()->GetAnimInstance())
                if (auto* Montage = EliteData.ElitePresentationMontage.Get()) Anim->Montage_Stop(.1f, Montage);
            Character->GetPGAbilitySystemComponent()->CloseRecoveryWindow();
            Character->bPerformingHeavyAttack = false;
            if (Character->GetStatComponent() && Character->GetStatComponent()->GetCurrentHealth() > 0) Character->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode));
        }
        bElitePattern = false;
        UE_LOG(LogTemp, Log, TEXT("PGElite End skill=%d cancelled=%d"), EliteData.SkillID, bWasCancelled);
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
