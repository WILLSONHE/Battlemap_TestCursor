#pragma once

#include "CoreMinimal.h"

/** Shared column widths: header row and data rows must use identical values. */
namespace BattleLoadoutTableLayout
{
	/** Auto battle-sequence label (e.g. 1\u84251\u8fde3\u73ed); left edge aligns with \u7f16\u7ec4\u6761\u76ee row. */
	inline constexpr float WBattleSeq = 410.f;
	inline constexpr float WScale = 200.f;
	inline constexpr float WName = 280.f;
	inline constexpr float WBranch = 130.f;
	inline constexpr float WClass = 220.f;
	inline constexpr float WUnit = 300.f;
	inline constexpr float WDel = 100.f;
	inline constexpr float WInsert = 100.f;
	/** Direct-subordinate count: x/11 */
	inline constexpr float WSubCount = 138.f;
	/** HorizontalBoxSlot horizontal padding per slot (left+right). */
	inline constexpr float SlotHPadding = 2.f;

	inline constexpr float TotalMinWidth()
	{
		return WBattleSeq + WScale + WName + WBranch + WClass + WUnit + WDel + WInsert + WSubCount
			+ 9.f * (2.f * SlotHPadding);
	}
}
