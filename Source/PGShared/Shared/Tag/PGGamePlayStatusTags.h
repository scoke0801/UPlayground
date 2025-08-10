

#pragma once

#include "NativeGameplayTags.h"

namespace PGGamePlayTags
{
	// Event Tags
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Status_Strafing);
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Status_UnderAttack);

	// Shared Tags
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Status_Dead);

	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Status_HitReact_Front);
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Status_HitReact_Left);
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Status_HitReact_Right);
	PGSHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Status_HitReact_Back);
} 