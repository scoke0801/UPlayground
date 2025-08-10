// Fill out your copyright notice in the Description page of Project Settings.


#include "PGMessageDispatcher.h"

TWeakObjectPtr<UPGMessageDispatcher> UPGMessageDispatcher::WeakThis = nullptr;

UPGMessageDispatcher::UPGMessageDispatcher()
{
}

UPGMessageDispatcher* UPGMessageDispatcher::Get()
{
	if (WeakThis.IsValid())
	{
		return WeakThis.Get();
	}
	return nullptr;
}

void UPGMessageDispatcher::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	WeakThis = MakeWeakObjectPtr(this);
    
}

void UPGMessageDispatcher::Deinitialize()
{
    WeakThis = nullptr;
	
	Super::Deinitialize();
}
