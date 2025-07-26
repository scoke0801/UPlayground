


#include "AbilitySystem/AbilityTasks/PGExecuteTaskOnTickTask.h"

UPGExecuteTaskOnTickTask::UPGExecuteTaskOnTickTask()
{
	bTickingTask = true;
}

void UPGExecuteTaskOnTickTask::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnAbilityTaskTick.Broadcast(DeltaTime);
	}
	else
	{
		EndTask();
	}
}

UPGExecuteTaskOnTickTask* UPGExecuteTaskOnTickTask::ExecuteTaskOnTick(UGameplayAbility* OwningAbility)
{
	UPGExecuteTaskOnTickTask* Node = NewAbilityTask<UPGExecuteTaskOnTickTask>(OwningAbility);

	return Node;
}
