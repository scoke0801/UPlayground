#pragma once
#include "CoreMinimal.h"
#include "PGEquipmentRules.h"
#include "PGData/DataTable/Item/PGItemDataRow.h"

namespace PGItemGeneration
{
    PGDATA_API void Range(const FPGItemDataRow& Definition, const FPGItemOptionDefinition& Option,
        int32 Power, int32& Min, int32& Max);
    PGDATA_API bool Validate(const FPGItemDataRow& Definition, const FPGEquipmentRarityRule& Rule, FString& Error);
    // GUID identity is supplied separately by the drop owner for checkpoint replay.
    PGDATA_API bool Generate(const FPGItemDataRow& Definition, const FPGEquipmentRarityRule& Rule,
        FRandomStream& Random, FPGItemInstance& Out, FString& Error);
}
