


#include "PlayerStructTypes.h"

#include "Abilities/GameplayAbility.h"

bool FPGPlayerAbilitySet::IsValid() const
{
	return InputTag.IsValid() && AbilityToGrant; 
}
