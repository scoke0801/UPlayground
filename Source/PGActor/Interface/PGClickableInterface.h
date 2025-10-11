// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PGClickableInterface.generated.h"

/**
 * 클릭 가능한 액터 인터페이스
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UPGClickableInterface : public UInterface
{
	GENERATED_BODY()
};

class PGACTOR_API IPGClickableInterface
{
	GENERATED_BODY()

public:
	/**
	 * 액터가 클릭되었을 때 호출
	 * @param ClickedActor 클릭된 액터
	 * @param ClickLocation 클릭된 위치
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PG|Click")
	void OnClicked(AActor* ClickedActor, const FVector& ClickLocation);

	/**
	 * 다른 액터가 클릭되어 이전 클릭이 취소되었을 때 호출
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PG|Click")
	void OnClickCancelled();

	/**
	 * 마우스가 액터 위에 올라갔을 때 호출
	 * TODO 마우스 커서 변경 용으로 사용예정..
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PG|Click")
	void OnMouseOver();

	/**
	 * 마우스가 액터에서 벗어났을 때 호출
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PG|Click")
	void OnMouseLeave();

	/**
	 * 클릭 가능 여부 반환
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PG|Click")
	bool IsClickable() const;
};
