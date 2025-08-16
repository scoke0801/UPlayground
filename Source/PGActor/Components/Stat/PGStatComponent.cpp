// Fill out your copyright notice in the Description page of Project Settings.


#include "PGStatComponent.h"

void UPGStatComponent::InitData(int32 CharacterTID)
{
	// 테이블에서 읽어오는 구조로 나중에 고치고 우선 상수
	MaxHP = 100;
	CurrentHP = MaxHP;
}
