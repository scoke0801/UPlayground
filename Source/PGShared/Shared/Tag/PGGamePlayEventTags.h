

#pragma once

#include "NativeGameplayTags.h"

namespace PGGamePlayTags
{
	// Event Tags
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Event_Equip_Weapon);
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Event_UnEquip_Weapon);
	
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Event_HitPause);

	// Shared Tags
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Event_MeleeHit);
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Event_HitReact);
} 