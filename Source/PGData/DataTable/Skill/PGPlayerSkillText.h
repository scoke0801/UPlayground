#pragma once
#include "CoreMinimal.h"
struct FPGSkillDataRow;
namespace PGPlayerSkillText
{
    struct PGDATA_API FView { FString Name, Description, Damage, Cooldown; };
    PGDATA_API FView MakeView(const FPGSkillDataRow& Row);
    PGDATA_API FString Describe(const FPGSkillDataRow& Row);
}
