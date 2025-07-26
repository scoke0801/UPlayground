

#pragma once

#include "CoreMinimal.h"
#include "PGDataAsset_StartUpDataBase.h"
#include "PGData/Shared/Structure/PlayerStructTypes.h"
#include "PGDataAsset_PlayerStartUpData.generated.h"

/**
 * 
 */
UCLASS()
class PGDATA_API UPGDataAsset_PlayerStartUpData : public UPGDataAsset_StartUpDataBase
{
	GENERATED_BODY()

public:
	virtual void GiveToAbilitySystemComponent(UPGAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "PG|StartUpData", meta = (TitleProperty = "InputTag"))
	TArray<FPGPlayerAbilitySet> PlayerStartUpAbilitySets;
};
