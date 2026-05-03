#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BattleBalanceTableTypes.generated.h"

/** Row for optional `UDataTable` balance packs (import CSV in editor). Version string is duplicated in project settings for runtime defaults. */
USTRUCT(BlueprintType)
struct FBattleBalanceTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance")
	FString BalanceVersion = TEXT("1.0.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance|Combat")
	float AttackDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance|Combat")
	float AttackRange = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance|Mobility")
	float MoveSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance|Detection")
	float DetectionRange = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance|Supply", meta = (ClampMin = "0.0"))
	float SupplyFoodConsumePerSecond = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance|Supply", meta = (ClampMin = "0.0"))
	float SupplyFuelConsumePerSecond = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance|Supply", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SupplyRoadFoodMultiplier = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance|Supply", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SupplyRoadFuelMultiplier = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance|ECM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EcmJammedDetectionRangeScale = 0.55f;
};
