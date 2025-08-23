// Fill out your copyright notice in the Description page of Project Settings.


#include "PGDamageFloaterManager.h"


TWeakObjectPtr<UPGDamageFloaterManager> UPGDamageFloaterManager::WeakThis = nullptr;

UPGDamageFloaterManager* UPGDamageFloaterManager::Get()
{
	if (WeakThis.IsValid())
	{
		return WeakThis.Get();
	}
	return nullptr;
}

UPGUIDamageFloater* UPGDamageFloaterManager::GetPooledFloater()
{
	return nullptr;
}

void UPGDamageFloaterManager::ReturnFloaterToPool(UPGUIDamageFloater* Floater)
{
}

void UPGDamageFloaterManager::AddFloater(float DamageAmount, EPGDamageType DamageType, bool IsPlayer)
{
	
}
