// Fill out your copyright notice in the Description page of Project Settings.


#include "PGSkillIndicator.h"

#include "Components/DecalComponent.h"

APGSkillIndicator::APGSkillIndicator()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APGSkillIndicator::InidicatorScale(FVector Scale)
{
	if (GetDecal())
	{
		GetDecal()->DecalSize = Scale;
		
		// 변경사항을 즉시 반영
		GetDecal()->MarkRenderStateDirty();
	}
}

