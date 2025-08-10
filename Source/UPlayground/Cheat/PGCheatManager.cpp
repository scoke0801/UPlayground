// Fill out your copyright notice in the Description page of Project Settings.


#include "Cheat/PGCheatManager.h"

#include "PGCheatComponent.h"
#include "Component/PGItemCheatComponent.h"
#include "Component/PGObjectCheatComponent.h"
#include "Component/PGStatCheatComponent.h"
#include "Component/PGUICheatComponent.h"

void UPGCheatManager::BeginDestroy()
{
	_components.Empty();
	
	Super::BeginDestroy();
}

void UPGCheatManager::InitCheatManager()
{
	Super::InitCheatManager();

	RegisterComponents(NewObject<UPGItemCheatComponent>(this));
	RegisterComponents(NewObject<UPGObjectCheatComponent>(this));
	RegisterComponents(NewObject<UPGStatCheatComponent>(this));
	RegisterComponents(NewObject<UPGUICheatComponent>(this));
}

bool UPGCheatManager::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	bool Handled = Super::ProcessConsoleExec(Cmd, Ar, Executor);

	if (false == Handled)
	{
		for (TSoftObjectPtr<UPGCheatComponent> Component : _components)
		{
			if (nullptr == Component)
			{
				continue;
			}
			Component->ProcessConsoleExec(Cmd, Ar, Executor);
		}
	}

	return Handled;
}

void UPGCheatManager::RegisterComponents(UPGCheatComponent* NewComponent)
{
	if(nullptr == NewComponent)
	{
		return;
	}

	_components.Emplace(NewComponent);
}
