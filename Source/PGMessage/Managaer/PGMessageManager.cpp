// Fill out your copyright notice in the Description page of Project Settings.


#include "PGMessageManager.h"

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
}

void UPGMessageManager::Deinitialize()
{
    WeakThis = nullptr;
    
	Super::Deinitialize();
}
