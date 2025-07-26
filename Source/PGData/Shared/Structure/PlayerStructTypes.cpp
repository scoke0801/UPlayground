


#include "Types/PlayerStructTypes.h"

#include "AbilitySystem/Abilities/PGGameplayAbility.h"

bool FPGPlayerAbilitySet::IsValid() const
{
	return InputTag.IsValid() && AbilityToGrant; 
}
