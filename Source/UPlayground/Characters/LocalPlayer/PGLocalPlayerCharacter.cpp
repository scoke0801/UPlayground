// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/LocalPlayer/PGLocalPlayerCharacter.h"

#include "Managers/PGDataTableManager.h"
#include "Skill/PGSkillDataRow.h"

void APGLocalPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UPGDataTableManager* dataTableManager = GetGameInstance()->GetSubsystem<UPGDataTableManager>())
	{
		FPGSkillDataRow* skillData = dataTableManager->GetRowData<FPGSkillDataRow>(1);

		if (skillData)
		{
			UE_LOG(LogTemp, Log, TEXT("%d"), skillData->SkillID);
		}
		//	dataTableManager->GetRowData<FPGSkillDataRow>(1);		
	}
	
}
