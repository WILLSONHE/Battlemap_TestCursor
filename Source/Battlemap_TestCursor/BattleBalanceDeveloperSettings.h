#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BattleBalanceDeveloperSettings.generated.h"

/**
 * Project-wide battle tuning (config/BattleBalance.ini). Mirrors FBattleBalanceTableRow for editor defaults.
 * Optional DataTable rows can override per-scenario values when wired on game mode.
 */
UCLASS(Config = BattleBalance, DefaultConfig, meta = (DisplayName = "Battle Balance"))
class BATTLEMAP_TESTCURSOR_API UBattleBalanceDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBattleBalanceDeveloperSettings();

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance")
	FString BalanceVersion = TEXT("1.0.0");

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Combat")
	float AttackDamage = 12.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Combat")
	float AttackRange = 1800.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Mobility")
	float MoveSpeed = 1200.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Detection")
	float DetectionRange = 2200.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Supply")
	float SupplyFoodConsumePerSecond = 0.02f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Supply")
	float SupplyFuelConsumePerSecond = 0.03f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Supply", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SupplyRoadFoodMultiplier = 0.65f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Supply", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SupplyRoadFuelMultiplier = 0.45f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|ECM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EcmJammedDetectionRangeScale = 0.55f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Career", meta = (ClampMin = "0"))
	int32 MissionVictoryExperience = 50;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance|Career", meta = (ClampMin = "0"))
	int32 MissionDefeatExperience = 15;
};
