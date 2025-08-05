
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

	UE_DEFINE_GAMEPLAY_TAG(InputTag_Skill_Normal, "InputTag.Skill_Normal");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Skill_Slot1, "InputTag.Skill_Slot1");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Skill_Slot2, "InputTag.Skill_Slot2");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Skill_Slot3, "InputTag.Skill_Slot3");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Skill_Slot4, "InputTag.Skill_Slot4");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Skill_Slot5, "InputTag.Skill_Slot5");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Skill_Slot6, "InputTag.Skill_Slot6");
	
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Attack, "InputTag.Attack");
	
	UE_DEFINE_GAMEPLAY_TAG(InputTag_MustBeHeld, "InputTag.MustBeHeld");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_MustBeHeld_Block, "InputTag.MustBeHeld.Block");
	
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Toggleable, "InputTag.Toggleable");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Toggleable_TargetLock, "InputTag.Toggleable.TargetLock");
	
	// Event Tags
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Equip_Weapon, "Player.Event.Equip.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_UnEquip_Weapon, "Player.Event.UnEquip.Weapon");
	
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_HitPause, "Player.Event.HitPause");

	
	// Weapon Tags
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Sword, "Weapon.Sword");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Bow, "Weapon.Bow");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Staff, "Weapon.Staff");


	// Shared Tags
	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_MeleeHit, "Shared.Event.MeleeHit");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_HitReact, "Shared.Event.HitReact");
	
}
