#include "BattleForceBuilderComponent.h"

bool UBattleForceBuilderComponent::IsSquadSizeValid(int32 SquadSize) const
{
	return SquadSize >= 3 && SquadSize <= 15;
}

bool UBattleForceBuilderComponent::IsHierarchySizeValid(int32 ChildCount) const
{
	return ChildCount >= 1 && ChildCount <= 10;
}

bool UBattleForceBuilderComponent::BuildPlatoonFromSquads(const TArray<FName>& SquadIds, FPlatoonData& OutPlatoon) const
{
	if (!IsHierarchySizeValid(SquadIds.Num()))
	{
		return false;
	}

	OutPlatoon.SquadUnitIds = SquadIds;
	return true;
}
