#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PGNiagaraFXTools.generated.h"

/** Editor inspection for reproducible Niagara asset configuration from Python. */
UCLASS()
class PGBLUEPRINTUTIL_API UPGNiagaraFXTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="PG|Niagara")
    static FString DescribeSystem(class UNiagaraSystem* System);
    UFUNCTION(BlueprintCallable, Category="PG|Niagara")
    static bool ConfigureSlash(class UNiagaraSystem* System);
    UFUNCTION(BlueprintCallable, Category="PG|Niagara")
    static bool ConfigureCombatBurst(class UNiagaraSystem* System, bool bSparks, class UMaterialInterface* DebrisMaterial);
};
