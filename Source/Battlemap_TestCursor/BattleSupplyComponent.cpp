#include "BattleSupplyComponent.h"

void UBattleSupplyComponent::ConsumeForUnitType(EUnitCategory Category, bool bRoadConnected, float Delta)
{
	if (Category == EUnitCategory::Facility && bRoadConnected)
	{
		return;
	}

	Food = FMath::Max(0.0f, Food - Delta);

	if (Category == EUnitCategory::Vehicle
		|| Category == EUnitCategory::Aircraft
		|| Category == EUnitCategory::NavalSurface
		|| Category == EUnitCategory::NavalSub)
	{
		Fuel = FMath::Max(0.0f, Fuel - Delta);
	}
}
