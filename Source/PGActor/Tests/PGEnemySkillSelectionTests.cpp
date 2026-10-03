#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Handler/Skill/PGEnemySkillHandler.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGEnemySkillSelectionTest, "PG.AI.ExactSkillSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGEnemySkillSelectionTest::RunTest(const FString&)
{
    FPGEnemySkillHandler Handler;
    EPGSkillSlot EmptySlot = EPGSkillSlot::NormalAttack;
    TestFalse(TEXT("Ability activation before skill initialization is safe"), Handler.ResolveSkillSlot(0, {}, EmptySlot));
    Handler.AddSkill(EPGSkillSlot::NormalAttack, 900001);
    Handler.AddSkill(EPGSkillSlot::SkillSlot_1, 900002);
    for (auto Slot : {EPGSkillSlot::NormalAttack, EPGSkillSlot::SkillSlot_1})
    {
        auto* Data = Handler.GetSkillData(Slot);
        Data->SkillId = Slot == EPGSkillSlot::NormalAttack ? 900001 : 900002;
        Data->SkillType = EPGSkillType::Melee; Data->CoolTime = 10; Data->LastSkillUsedTime = -100;
    }
    EPGSkillSlot Selected = EPGSkillSlot::NormalAttack;
    const auto Tags = PGGamePlayTags::Enemy_Ability_MeleeSkill.GetTag().GetSingleTagContainer();
    for (int32 Index = 0; Index < 20; ++Index)
    {
        TestTrue(TEXT("Explicit request resolves"), Handler.ResolveSkillSlot(900002, Tags, Selected));
        TestEqual(TEXT("Same-type alternatives do not reroll the selected ID"), Handler.GetSkillID(Selected), 900002);
    }
    Handler.UseSkill(Selected);
    TestFalse(TEXT("Cooling request cannot fall back to another ready skill"), Handler.ResolveSkillSlot(900002, Tags, Selected));
    TestEqual(TEXT("Repeated usage does not escalate authored priority"), Handler.GetPriorityByID(900002), 1);
    TestFalse(TEXT("Unknown request cannot fall back to normal attack"), Handler.ResolveSkillSlot(999999, Tags, Selected));
    TestFalse(TEXT("Unknown ID is not a ready skill"), Handler.IsSkillReadyByID(999999));
    TestTrue(TEXT("Legacy tag selection still finds a ready matching skill"), Handler.ResolveSkillSlot(0, Tags, Selected));
    TestEqual(TEXT("Cooling candidate excluded from tag selection"), Handler.GetSkillID(Selected), 900001);
    const auto ProjectileTags = PGGamePlayTags::Enemy_Ability_ProjectileSkill.GetTag().GetSingleTagContainer();
    TestFalse(TEXT("An unavailable type cannot silently become a melee attack"), Handler.ResolveSkillSlot(0, ProjectileTags, Selected));
    return true;
}
#endif
