


#include "DataAssets/Input/DataAsset_InputConfig.h"

UInputAction* UDataAsset_InputConfig::FindNativeInputActionsByTag(const FGameplayTag& InInputTag) const
{
	for (const FPGInputActionConfig& config : NativeInputActions)
	{
		if (InInputTag == config.InputTag && nullptr != config.InputAction)
		{
			return config.InputAction;
		}
	}

	return nullptr;
}
