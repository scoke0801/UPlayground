
#include "PGGamePlayTags.h"

namespace PGGamePlayTags
{
	// INPUT Tags
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Move, "InputTag.Move");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Look, "InputTag.Look");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Roll, "InputTag.Roll");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Jump, "InputTag.Jump");
	
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Equip_Weapon, "InputTag.Equip.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_UnEquip_Weapon, "InputTag.UnEquip.Weapon");

	UE_DEFINE_GAMEPLAY_TAG(InputTag_Attack, "InputTag.Attack");
	
	UE_DEFINE_GAMEPLAY_TAG(InputTag_MustBeHeld, "InputTag.MustBeHeld");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_MustBeHeld_Block, "InputTag.MustBeHeld.Block");
	
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Toggleable, "InputTag.Toggleable");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Toggleable_TargetLock, "InputTag.Toggleable.TargetLock");
	
	// Event Tags
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Equip_Weapon, "Player.Event.Equip.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_UnEquip_Weapon, "Player.Event.UnEquip.Weapon");
}
