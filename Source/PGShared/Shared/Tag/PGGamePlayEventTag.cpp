
#include "PGGamePlayEventTags.h"

namespace PGGamePlayTags
{
	// Event Tags
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Equip_Weapon, "Player.Event.Equip.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_UnEquip_Weapon, "Player.Event.UnEquip.Weapon");

	UE_DEFINE_GAMEPLAY_TAG(Player_Event_JumpLanded, "Player.Event.JumpLanded");
		
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_HitPause, "Player.Event.HitPause");
	
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Event_SummonEnemy, "Enemy.Event.SummonEnemy");


	// Shared Tags
	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_Hit, "Shared.Event.MeleeHit");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_HitReact, "Shared.Event.HitReact");
}
