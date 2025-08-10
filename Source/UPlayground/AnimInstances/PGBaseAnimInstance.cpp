


#include "AnimInstances/PGBaseAnimInstance.h"

#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "Utils/PGFunctionLibrary.h"

bool UPGBaseAnimInstance::DoesOwnerHaveTag(FGameplayTag TagToCheck) const
{
	if (APawn* OwningPawn = TryGetPawnOwner())
	{
		return UPGAbilityBPLibrary::NativeDoesActorHaveTag(OwningPawn, TagToCheck);
	}
	
	return false;
}
