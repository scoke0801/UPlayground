

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "PGExecuteTaskOnTickTask.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityTaskTickDelegate, float, DeltaTime);
/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGExecuteTaskOnTickTask : public UAbilityTask
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable)
	FOnAbilityTaskTickDelegate OnAbilityTaskTick;
	
public:
	UPGExecuteTaskOnTickTask();
	
public:
	virtual void TickTask(float DeltaTime) override;
	
public:
	UFUNCTION(BlueprintCallable, Category = "PG|AbilityTasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UPGExecuteTaskOnTickTask* ExecuteTaskOnTick(UGameplayAbility* OwningAbility);

	
};
