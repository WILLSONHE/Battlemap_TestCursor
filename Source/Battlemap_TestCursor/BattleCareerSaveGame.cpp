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

	FPlayerLoadoutSlot A;
	A.bEnabled = true;
	A.bSlotLabelUserOverride = false;
	A.ParentSlotIndex = INDEX_NONE;
	A.UnitScale = EFormationUnitScale::Squad;
	A.LoadoutBranch = TEXT("\u9646\u519b");
	A.LoadoutClass = TEXT("\u6b65\u5175");
	A.LoadoutUnit = TEXT("\u57fa\u7840\u6b65\u5175\u73ed");
	A.Category = EUnitCategory::Infantry;
	A.UnitType = EUnitType::Infantry;
	FriendlyLoadoutSlots.Add(A);

	FPlayerLoadoutSlot B;
	B.bEnabled = true;
	B.bSlotLabelUserOverride = false;
	B.ParentSlotIndex = INDEX_NONE;
	B.UnitScale = EFormationUnitScale::Squad;
	B.LoadoutBranch = TEXT("\u9646\u519b");
	B.LoadoutClass = TEXT("\u6b65\u5175");
	B.LoadoutUnit = TEXT("\u57fa\u7840\u6b65\u5175\u73ed");
	B.Category = EUnitCategory::Infantry;
	B.UnitType = EUnitType::Infantry;
	FriendlyLoadoutSlots.Add(B);
}
