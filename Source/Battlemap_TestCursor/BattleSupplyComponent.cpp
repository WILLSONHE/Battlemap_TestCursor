#include "BattleSupplyComponent.h"
#include "BattleBalanceDeveloperSettings.h"

UBattleSupplyComponent::UBattleSupplyComponent()
{
	const UBattleBalanceDeveloperSettings* Balance = GetDefault<UBattleBalanceDeveloperSettings>();
	FoodConsumePerSecond = Balance->SupplyFoodConsumePerSecond;
	FuelConsumePerSecond = Balance->SupplyFuelConsumePerSecond;
	RoadFoodMultiplier = Balance->SupplyRoadFoodMultiplier;
	RoadFuelMultiplier = Balance->SupplyRoadFuelMultiplier;
}

void UBattleSupplyComponent::ApplySupplyRatesFromBalanceRow(const FBattleBalanceTableRow& Row)
{
	FoodConsumePerSecond = Row.SupplyFoodConsumePerSecond;
	FuelConsumePerSecond = Row.SupplyFuelConsumePerSecond;
	RoadFoodMultiplier = Row.SupplyRoadFoodMultiplier;
	RoadFuelMultiplier = Row.SupplyRoadFuelMultiplier;
}

void UBattleSupplyComponent::ConsumeForUnitType(EUnitCategory Category, bool bRoadConnected, float Delta)
{
	if (Category == EUnitCategory::Facility && bRoadConnected)
	{
		return;
	}

	float FoodRate = FoodConsumePerSecond;
	float FuelRate = FuelConsumePerSecond;
	if (bRoadConnected)
	{
		FoodRate *= RoadFoodMultiplier;
		FuelRate *= RoadFuelMultiplier;
	}

	Food = FMath::Max(0.0f, Food - FoodRate * Delta);

	if (Category == EUnitCategory::Vehicle
		|| Category == EUnitCategory::Aircraft
		|| Category == EUnitCategory::NavalSurface
		|| Category == EUnitCategory::NavalSub)
	{
		Fuel = FMath::Max(0.0f, Fuel - FuelRate * Delta);
	}
}
