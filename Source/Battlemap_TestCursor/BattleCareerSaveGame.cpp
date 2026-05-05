#include "BattleCareerSaveGame.h"
#include "BattleCareerTypes.h"

FString UBattleCareerSaveGame::SlotName()
{
	return TEXT("BattleCareerSlot0");
}

void UBattleCareerSaveGame::ResetToDefaultRoster()
{
	MilitaryRankIndex = 8;
	Experience = 0;
	FriendlyLoadoutSlots.Reset();

	FPlayerLoadoutSlot Company;
	Company.bEnabled = true;
	Company.bSlotLabelUserOverride = false;
	Company.ParentSlotIndex = INDEX_NONE;
	Company.UnitScale = EFormationUnitScale::Company;
	Company.LoadoutBranch = TEXT("\u9646\u519b");
	Company.LoadoutClass = TEXT("\u6b65\u5175");
	Company.LoadoutUnit = TEXT("\u57fa\u7840\u6b65\u5175\u73ed");
	Company.Category = EUnitCategory::Infantry;
	Company.UnitType = EUnitType::Infantry;
	FriendlyLoadoutSlots.Add(Company);

	for (int32 i = 0; i < 2; ++i)
	{
		FPlayerLoadoutSlot Sq;
		Sq.bEnabled = true;
		Sq.bSlotLabelUserOverride = false;
		Sq.ParentSlotIndex = 0;
		Sq.UnitScale = EFormationUnitScale::Squad;
		Sq.LoadoutBranch = TEXT("\u9646\u519b");
		Sq.LoadoutClass = TEXT("\u6b65\u5175");
		Sq.LoadoutUnit = TEXT("\u57fa\u7840\u6b65\u5175\u73ed");
		Sq.Category = EUnitCategory::Infantry;
		Sq.UnitType = EUnitType::Infantry;
		FriendlyLoadoutSlots.Add(Sq);
	}
}
