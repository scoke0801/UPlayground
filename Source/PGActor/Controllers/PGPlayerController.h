// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/PlayerController.h"
#include "PGPlayerController.generated.h"

class IPGClickableInterface;

/**
 *
 */
UCLASS()
class PGACTOR_API APGPlayerController : public APlayerController, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

private:
	FGenericTeamId PlayerTeamId;

	// 마지막으로 마우스 오버된 액터
	UPROPERTY()
	TWeakObjectPtr<AActor> LastHoveredActor;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	// 클릭 감지를 위한 트레이스 채널
	UPROPERTY(EditDefaultsOnly, Category = "PG|Click")
	TEnumAsByte<ECollisionChannel> ClickTraceChannel = ECC_Visibility;

	// 클릭 감지 거리
	UPROPERTY(EditDefaultsOnly, Category = "PG|Click")
	float ClickTraceDistance = 10000.0f;

	// 마우스 오버 체크 활성화
	UPROPERTY(EditDefaultsOnly, Category = "PG|Click")
	bool bEnableMouseOverCheck = true;

	// 마우스 오버 체크 주기 (초)
	UPROPERTY(EditDefaultsOnly, Category = "PG|Click")
	float MouseOverCheckInterval = 0.1f;

public:
	APGPlayerController(const FObjectInitializer& ObjectInitializer);
	
public:
	// IGenericTeamAgentInterface
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual FGenericTeamId GetGenericTeamId() const override { return PlayerTeamId; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;

private:
	// 마우스 클릭 처리
	void HandleMouseClick();

	// 마우스 커서 아래의 액터 가져오기
	AActor* GetActorUnderCursor(FVector& OutHitLocation) const;

	// 마우스 오버 체크
	void CheckMouseOver();

	// 타이머 핸들
	FTimerHandle MouseOverTimerHandle;
};
