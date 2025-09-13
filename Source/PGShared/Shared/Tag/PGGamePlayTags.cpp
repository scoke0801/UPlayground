
#include "PGGamePlayTags.h"

namespace PGGamePlayTags
{
	// Player Ability
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Equip_Weapon, "Player.Ability.Equip.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_UnEquip_Weapon, "Player.Ability.UnEquip.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack, "Player.Ability.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_HitPause, "Player.Ability.HitPause");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Roll, "Player.Ability.Roll");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Jump, "Player.Ability.Jump");

	// Enemy Ability
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Ability_Melee, "Enemy.Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Ability_Range, "Enemy.Ability.Range");
	
	// Shared Ability
	UE_DEFINE_GAMEPLAY_TAG(Shared_Ability_HitReact, "Shared.Ability.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Ability_Death, "Shared.Ability.Death");
	
	// Weapon Tags
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Sword, "Weapon.Sword");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Bow, "Weapon.Bow");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Staff, "Weapon.Staff");
	
}
