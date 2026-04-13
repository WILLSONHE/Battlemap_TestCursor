#include "BattleCombatLibrary.h"

float UBattleCombatLibrary::CalculateDamage(float RawDamage, float CoverPercent, float TerrainDefensePercent, float ArmorValue)
{
	const float CoverFactor = 1.0f - FMath::Clamp(CoverPercent, 0.0f, 1.0f);
	const float TerrainFactor = 1.0f - FMath::Clamp(TerrainDefensePercent, 0.0f, 1.0f);
	const float Mitigated = RawDamage * CoverFactor * TerrainFactor;
	return FMath::Max(0.0f, Mitigated - ArmorValue);
}
