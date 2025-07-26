


#include "AbilitySystem/Abilities/PGPlayerTargetLockAbility.h"

#include "EnhancedInputSubsystems.h"
#include "PGGamePlayTags.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Characters/Player/PGPlayerCharacter.h"
#include "Components/SizeBox.h"
#include "Controllers/PGPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Helper/Debug/PGDebugHelper.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Utils/PGFunctionLibrary.h"
#include "Widgets/PGActorWidgetBase.h"

void UPGPlayerTargetLockAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                 const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                 const FGameplayEventData* TriggerEventData)
{
	TryLockOnTarget();

	InitTargetLockMovement();

	InitTArgetLockMappingContext();
	
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UPGPlayerTargetLockAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	ResetTargetLockMovement();
	ResetTargetLockMappingContext();
	CleanUp();
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPGPlayerTargetLockAbility::OnTargetLockTick(float DeltaTime)
{
	if (nullptr == CurrentLockedActor
		|| UPGFunctionLibrary::NativeDoesActorHaveTag(CurrentLockedActor, PGGamePlayTags::Shared_Status_Dead)
		|| UPGFunctionLibrary::NativeDoesActorHaveTag(GetPlayerCharacterFromActorInfo(), PGGamePlayTags::Shared_Status_Dead))
	{
		CancelTargetLockAbility();
		return;
	}

	SetTargetLockWidgetPosition();

	const bool bShouldOverrideRotation =
		false == UPGFunctionLibrary::NativeDoesActorHaveTag(GetPlayerCharacterFromActorInfo(), PGGamePlayTags::Player_Status_Rolling)
		&& false == UPGFunctionLibrary::NativeDoesActorHaveTag(GetPlayerCharacterFromActorInfo(), PGGamePlayTags::Player_Status_Blocking);

	if (bShouldOverrideRotation)
	{
		FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
			GetPlayerCharacterFromActorInfo()->GetActorLocation(),
			CurrentLockedActor->GetActorLocation());


		LookAtRot -= FRotator(TargetLockCameraOffsetDistance, 0.f, 0.f);

		const FRotator CurrentControlRot = GetPlayerControllerFromActorInfo()->GetControlRotation();
		const FRotator TargetRot = FMath::RInterpTo(CurrentControlRot, LookAtRot, DeltaTime, TargetLockRotationInterpSpeed);

		GetPlayerControllerFromActorInfo()->SetControlRotation(FRotator(TargetRot.Pitch, TargetRot.Yaw, 0.f));
		GetPlayerCharacterFromActorInfo()->SetActorRotation(FRotator(0.f, TargetRot.Yaw, 0.f));
	}
}

void UPGPlayerTargetLockAbility::SwitchTarget(const FGameplayTag& InSwitchDirectionTag)
{
	GetAvailableActorsToLock();

	TArray<AActor*> ActorsOnLeft;
	TArray<AActor*> ActorsOnRight;
	AActor* NewTargetToLock = nullptr;
	
	GetAvailableActorAroundTarget(ActorsOnLeft, ActorsOnRight);

	PG_Debug::Print(InSwitchDirectionTag.ToString());
	
	if (InSwitchDirectionTag.MatchesTagExact(PGGamePlayTags::Player_Event_SwitchLockOnTarget_Left))
	{
		NewTargetToLock = GetNearestTargetFromAvailableActors(ActorsOnLeft);
	}
	else if (InSwitchDirectionTag.MatchesTagExact(PGGamePlayTags::Player_Event_SwitchLockOnTarget_Right))
	{
		NewTargetToLock = GetNearestTargetFromAvailableActors(ActorsOnRight);
	}

	if (nullptr != NewTargetToLock)
	{
		CurrentLockedActor = NewTargetToLock;
	}
}

void UPGPlayerTargetLockAbility::TryLockOnTarget()
{
	GetAvailableActorsToLock();

	if (AvailableActorToLock.IsEmpty())
	{
		CancelTargetLockAbility();
		return;
	}

	CurrentLockedActor = GetNearestTargetFromAvailableActors(AvailableActorToLock);

	if (CurrentLockedActor)
	{
		DrawTargetLockWidget();
		SetTargetLockWidgetPosition();
	}
	else
	{
		CancelTargetLockAbility();
	}
}

void UPGPlayerTargetLockAbility::GetAvailableActorsToLock()
{
	TArray<FHitResult> BoxTraceHits;

	AvailableActorToLock.Empty();
	UKismetSystemLibrary::BoxTraceMultiForObjects(
		GetPlayerCharacterFromActorInfo(),
		GetPlayerCharacterFromActorInfo()->GetActorLocation(),
		GetPlayerCharacterFromActorInfo()->GetActorLocation() + GetPlayerCharacterFromActorInfo()->GetActorForwardVector() * BoxTraceDistance,
			TraceBoxSize / 2.0f,
		GetPlayerCharacterFromActorInfo()->GetActorForwardVector().ToOrientationRotator(),
		BoxTraceChannel,
		false,
		TArray<AActor*>(),
		bShowPersistentDebugShape ? EDrawDebugTrace::Persistent :EDrawDebugTrace::None,
		BoxTraceHits,
		true
		);

	for (const FHitResult& HitResult : BoxTraceHits)
	{
		if (AActor* HitActor = HitResult.GetActor())
		{
			if (HitActor != GetPlayerCharacterFromActorInfo())
			{
				AvailableActorToLock.AddUnique(HitActor);
			}
		}
	}
}

AActor* UPGPlayerTargetLockAbility::GetNearestTargetFromAvailableActors(const TArray<AActor*>& InAvailableActors)
{
	float ClosestDistance = 0.f;
	return UGameplayStatics::FindNearestActor(
		GetPlayerCharacterFromActorInfo()->GetActorLocation(),
		InAvailableActors,
		ClosestDistance
		);
}

void UPGPlayerTargetLockAbility::GetAvailableActorAroundTarget(TArray<AActor*>& OutActorsOnLeft,
	TArray<AActor*>& OutActorOnRight)
{
	if (nullptr == CurrentLockedActor || AvailableActorToLock.IsEmpty())
	{
		CancelTargetLockAbility();
		return;
	}

	const FVector PlayerLocation = GetPlayerCharacterFromActorInfo()->GetActorLocation();
	const FVector PlayerToCurrentNormalize = (CurrentLockedActor->GetActorLocation() - PlayerLocation).GetSafeNormal();

	for (AActor* AvailableActor : AvailableActorToLock)
	{
		if (nullptr == AvailableActor || AvailableActor == CurrentLockedActor)
		{
			continue;
		}

		const FVector PlayerToAvailableNormalized = (AvailableActor->GetActorLocation() - PlayerLocation).GetSafeNormal();

		const FVector CrossResult = FVector::CrossProduct(PlayerToCurrentNormalize, PlayerToAvailableNormalized);

		// 오른쪽
		if (CrossResult.Z > 0.f)
		{
			OutActorOnRight.AddUnique(AvailableActor);
		}
		else
		{
			OutActorsOnLeft.AddUnique(AvailableActor);
		}
	}
}

void UPGPlayerTargetLockAbility::DrawTargetLockWidget()
{
	if (nullptr == DrawnTargetLockWidget)
	{
		check(TargetLockWidgetClass);

		DrawnTargetLockWidget = CreateWidget<UPGActorWidgetBase>(GetPlayerControllerFromActorInfo(), TargetLockWidgetClass);

		check(DrawnTargetLockWidget);

		DrawnTargetLockWidget->AddToViewport();
	}
}

void UPGPlayerTargetLockAbility::SetTargetLockWidgetPosition()
{
	if (nullptr == DrawnTargetLockWidget || nullptr== CurrentLockedActor)
	{
		CancelTargetLockAbility();
		return;
	}

	FVector2D ScreenPosition;
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		GetPlayerControllerFromActorInfo(),
		CurrentLockedActor->GetActorLocation(),
		ScreenPosition,
		true
		);

	if (TargetLockWidgetSize == FVector2D::ZeroVector)
	{
		DrawnTargetLockWidget->WidgetTree->ForEachWidget(
			[this](UWidget* FoundWidget)
			{
				if (USizeBox* FoundSizeBox = Cast<USizeBox>(FoundWidget))
				{
					TargetLockWidgetSize.X = FoundSizeBox->GetWidthOverride();
					TargetLockWidgetSize.Y = FoundSizeBox->GetHeightOverride();
				}
			}
		);
	}

	ScreenPosition -= (TargetLockWidgetSize * 0.5f);
	DrawnTargetLockWidget->SetPositionInViewport(ScreenPosition, false);
}

void UPGPlayerTargetLockAbility::InitTargetLockMovement()
{
	CachedMaxWalkSpeed = GetPlayerCharacterFromActorInfo()->GetCharacterMovement()->MaxWalkSpeed;

	GetPlayerCharacterFromActorInfo()->GetCharacterMovement()->MaxWalkSpeed = TargetLockMaxWalkSpeed;
}

void UPGPlayerTargetLockAbility::InitTArgetLockMappingContext()
{
	const ULocalPlayer* LocalPlayer = GetPlayerControllerFromActorInfo()->GetLocalPlayer();
	
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	check(Subsystem);

	Subsystem->AddMappingContext(TargetLockMappingContext, 3);
}

void UPGPlayerTargetLockAbility::CancelTargetLockAbility()
{
	CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),true);
}

void UPGPlayerTargetLockAbility::CleanUp()
{
	AvailableActorToLock.Empty();

	CurrentLockedActor = nullptr;

	if (DrawnTargetLockWidget)
	{
		DrawnTargetLockWidget->RemoveFromParent();
	}

	DrawnTargetLockWidget = nullptr;
	TargetLockWidgetSize = FVector2D::ZeroVector;

	CachedMaxWalkSpeed = 0.f;
}

void UPGPlayerTargetLockAbility::ResetTargetLockMovement()
{
	if (CachedMaxWalkSpeed >0.0f)
	{
		GetPlayerCharacterFromActorInfo()->GetCharacterMovement()->MaxWalkSpeed = CachedMaxWalkSpeed;
	}
}

void UPGPlayerTargetLockAbility::ResetTargetLockMappingContext()
{
	if (nullptr == GetPlayerControllerFromActorInfo())
	{
		return;
	}
	
	const ULocalPlayer* LocalPlayer = GetPlayerControllerFromActorInfo()->GetLocalPlayer();
	
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	check(Subsystem);

	Subsystem->RemoveMappingContext(TargetLockMappingContext);
}
