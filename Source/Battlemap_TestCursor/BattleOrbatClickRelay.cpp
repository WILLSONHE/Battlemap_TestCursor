#include "BattleOrbatClickRelay.h"
#include "BattleOrbatBattleWidget.h"

void UBattleOrbatClickRelay::Fire()
{
	if (Owner)
	{
		Owner->DispatchOrbatSlotClicked(RowDepth, SlotIdx);
	}
}
