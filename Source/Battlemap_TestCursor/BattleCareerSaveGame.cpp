#include "BattleCareerSaveGame.h"

FString UBattleCareerSaveGame::SlotName()
{
	return TEXT("BattleCareerSlot0");
}

void UBattleCareerSaveGame::ResetToDefaultRoster()
{
	MilitaryRankIndex = 0;
	Experience = 0;
	FriendlyLoadoutSlots.Reset();

	FPlayerLoadoutSlot A;
	A.bEnabled = true;
	A.SlotLabel = TEXT("Alpha-1");
	A.Category = EUnitCategory::Infantry;
	A.UnitType = EUnitType::Infantry;
	FriendlyLoadoutSlots.Add(A);

	FPlayerLoadoutSlot B;
	B.bEnabled = true;
	B.SlotLabel = TEXT("Bravo-2");
	B.Category = EUnitCategory::Infantry;
	B.UnitType = EUnitType::Infantry;
	FriendlyLoadoutSlots.Add(B);
}
