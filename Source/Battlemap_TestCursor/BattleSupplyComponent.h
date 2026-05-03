#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleBalanceTableTypes.h"
#include "BattleTypes.h"
#include "BattleSupplyComponent.generated.h"

UCLASS(ClassGroup=(Battle), meta=(BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UBattleSupplyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleSupplyComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Supply")
	float Food = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Supply")
	float Fuel = 100.0f;

	/** Filled from `FBattleBalanceTableRow` / defaults when `ABattlemap_TestCursorGameMode::ApplyBalanceToUnit` runs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Supply|Rates")
	float FoodConsumePerSecond = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Supply|Rates")
	float FuelConsumePerSecond = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Supply|Rates", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RoadFoodMultiplier = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Supply|Rates", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RoadFuelMultiplier = 0.45f;

	UFUNCTION(BlueprintCallable, Category = "Battle|Supply")
	void ApplySupplyRatesFromBalanceRow(const FBattleBalanceTableRow& Row);

	UFUNCTION(BlueprintCallable, Category = "Battle|Supply")
	void ConsumeForUnitType(EUnitCategory Category, bool bRoadConnected, float Delta);
};
