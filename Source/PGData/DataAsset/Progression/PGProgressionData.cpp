#include "PGProgressionData.h"
#include "PGLootRules.h"
#include "Misc/DataValidation.h"

bool UPGProgressionData::ValidateEquipmentGeneration(FString& Error) const
{
    if (!PGCraftingRules::Validate(*this, Error)) return false;
    if (!PGLegendaryRules::Validate(*this, Error)) return false;
    if (!SalvageRules.IsValid()) { Error = TEXT("분해 재료 설정 오류"); return false; }
    if (!LevelRules.IsValid()) { Error = TEXT("레벨 성장 설정 오류"); return false; }
    return PGEquipmentRules::ValidateRarities(RarityRules, Error) && PGLootRules::ValidatePools(*this, Error);
}

#if WITH_EDITOR
EDataValidationResult UPGProgressionData::IsDataValid(FDataValidationContext& Context) const
{
    const auto ParentResult = Super::IsDataValid(Context);
    FString Error;
    if (!ValidateEquipmentGeneration(Error))
    {
        Context.AddError(FText::FromString(Error));
        return EDataValidationResult::Invalid;
    }
    return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
