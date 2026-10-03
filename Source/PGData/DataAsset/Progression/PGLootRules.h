#pragma once
#include "PGProgressionData.h"

namespace PGLootRules
{
    PGDATA_API bool ValidatePools(const UPGProgressionData& Catalog, FString& Error);
    PGDATA_API bool Roll(const UPGProgressionData& Catalog, FName PoolId, FRandomStream& Random, FPGItemInstance& Out);
}
