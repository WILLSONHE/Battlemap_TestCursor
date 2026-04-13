#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BattleCombatLibrary.generated.h"

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Battle|Combat")
	static float CalculateDamage(float RawDamage, float CoverPercent, float TerrainDefensePercent, float ArmorValue);
};
