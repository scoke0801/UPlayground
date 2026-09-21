#include "PGMessageManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
// Fill out your copyright notice in the Description page of Project Settings.



TWeakObjectPtr<UPGMessageManager> UPGMessageManager::WeakThis = nullptr;

UPGMessageManager* UPGMessageManager::Get()
{	
	if (WeakThis.IsValid())
	{
		return WeakThis.Get();
	}
	return nullptr;
}

void UPGMessageManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	WeakThis = MakeWeakObjectPtr(this);
	
	UE_LOG(LogTemp, Log, TEXT("PGMessageManager Initialized"));
}

void UPGMessageManager::Deinitialize()
{
	// 모든 델리게이터 정리
	ClearAllDelegates();
	
    if (WeakThis.Get() == this) WeakThis = nullptr;
    
	Super::Deinitialize();
	
	UE_LOG(LogTemp, Log, TEXT("PGMessageManager Deinitialized"));
}

void UPGMessageManager::ClearAllDelegates()
{
	for (auto& Pair : MessageDelegates)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Clear();
		}
	}
	MessageDelegates.Empty();
}

UPGMessageManager* UPGMessageManager::Get(const UObject* Context)
{
    const UWorld* World = Context ? Context->GetWorld() : nullptr;
    return World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPGMessageManager>() : nullptr;
}
