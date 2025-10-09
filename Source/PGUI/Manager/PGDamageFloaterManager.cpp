// Fill out your copyright notice in the Description page of Project Settings.


#include "PGDamageFloaterManager.h"

#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/AssetPath/PGUIWidgetPathRow.h"
#include "PGUI/Widget/DamageFloater/PGUIDamageFloater.h"


TWeakObjectPtr<UPGDamageFloaterManager> UPGDamageFloaterManager::WeakThis = nullptr;

UPGDamageFloaterManager* UPGDamageFloaterManager::Get()
{
	if (WeakThis.IsValid())
	{
		return WeakThis.Get();
	}
	return nullptr;
}

UPGUIDamageFloater* UPGDamageFloaterManager::GetPooledFloater(EPGDamageType DamageType)
{
	if (Pools[DamageType].Widgets.Num() > 0)
	{
		UPGUIDamageFloater* Floater = Pools[DamageType].Widgets.Pop();
		Floater->SetVisibility(ESlateVisibility::Visible);
		return Floater;
	}

	return CreateFloaterWidget(DamageType);
}

void UPGDamageFloaterManager::ReturnFloaterToPool(EPGDamageType DamageType, UPGUIDamageFloater* Floater)
{
	if (Floater)
	{
		Floater->SetVisibility(ESlateVisibility::Collapsed);
		Floater->RemoveFromParent();
		
		if (Pools[DamageType].Widgets.Num() < MaxPoolSize)
		{
			Pools[DamageType].Widgets.Add(Floater);
		}
	}
}

void UPGDamageFloaterManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	WeakThis = MakeWeakObjectPtr(this);
	
	Pools.Emplace(EPGDamageType::Normal);
	Pools.Emplace(EPGDamageType::Critical);
	Pools.Emplace(EPGDamageType::Miss);
	Pools.Emplace(EPGDamageType::Heal);
	
	// 정리 타이머 시작
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			CleanupTimerHandle,
			this,
			&UPGDamageFloaterManager::CleanupExpiredFloaters,
			CleanupInterval,
			true
		);
	}
}

void UPGDamageFloaterManager::Deinitialize()
{
	// 풀 정리
	ClearPool();

	if (CleanupTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(CleanupTimerHandle);
	}
	
	Super::Deinitialize();
}

void UPGDamageFloaterManager::AddFloater(float DamageAmount,
                                         EPGDamageType DamageType,
                                         AActor* TargetActor,
                                         bool IsPlayer)
{
	if (!TargetActor)
	{
		return;
	}
	
	UPGUIDamageFloater* Floater = GetPooledFloater(DamageType);
	if (!Floater)
	{
		return;
	}
	
	// 액터의 콜리전 높이 계산
	FVector LocalOffset = FVector::ZeroVector;
	if (UCapsuleComponent* CapsuleComp = TargetActor->FindComponentByClass<UCapsuleComponent>())
	{
		LocalOffset.Z = CapsuleComp->GetScaledCapsuleHalfHeight();
	}
	else
	{
		// 기본값: 70cm
		LocalOffset.Z = 70.0f;
	}
	
	// 뷰포트에 추가
	Floater->AddToViewport();
	
	// 액터와 오프셋 설정
	FVector WorldLocation = TargetActor->GetActorLocation() + LocalOffset;
	Floater->SetTargetActor(TargetActor, LocalOffset);
	
	// 초기 위치 계산
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			FVector2D ScreenPosition;
			if (PC->ProjectWorldLocationToScreen(WorldLocation, ScreenPosition))
			{
				Floater->SetDamage(DamageAmount, DamageType, ScreenPosition);
				Floater->SetPositionInViewport(ScreenPosition);
			}
		}
	}
}

void UPGDamageFloaterManager::CleanupExpiredFloaters()
{
}

UPGUIDamageFloater* UPGDamageFloaterManager::CreateFloaterWidget(EPGDamageType DamageType)
{
	if (FPGUIWidgetPathRow* Data = PGData()->GetRowData<FPGUIWidgetPathRow>(static_cast<int64>(DamageType)))
	{
		TSoftClassPtr<UPGUIDamageFloater> SoftClassPtr(Data->ClassPath);

		if (UClass* LoadedClass = SoftClassPtr.LoadSynchronous())
		{
			// 현재 월드 가져오기
			if (UWorld* World = GetWorld())
			{
				return CreateWidget<UPGUIDamageFloater>(World, LoadedClass);
			}
		}
	}

	return nullptr;
}


void UPGDamageFloaterManager::ClearPool()
{
	// 활성 플로터들 정리
	for (UPGUIDamageFloater* Floater : ActiveFloaters)
	{
		if (IsValid(Floater))
		{
			Floater->RemoveFromParent();
		}
	}
	ActiveFloaters.Empty();

	// 풀링된 플로터들 정리
	for (auto& Pair : Pools)
	{
		for (UPGUIDamageFloater* Floater : Pair.Value.Widgets)
		{
			if (IsValid(Floater))
			{
				Floater->RemoveFromParent();
			}
		}
		Pair.Value.Widgets.Empty();
	}
}
