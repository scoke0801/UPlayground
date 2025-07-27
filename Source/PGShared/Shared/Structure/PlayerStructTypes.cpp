


#include "PlayerStructTypes.h"

#include "PGAbilitySystem/Abilities/PGPlayerGameplayAbility.h"

bool FPGPlayerAbilitySet::IsValid() const
{
	return InputTag.IsValid() && AbilityToGrant; 
}
