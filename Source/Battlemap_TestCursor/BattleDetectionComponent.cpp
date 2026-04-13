#include "BattleDetectionComponent.h"

FDetectionResult UBattleDetectionComponent::EvaluateDetection(float ObserverDetectionLevel, int32 TargetStealthLevel) const
{
	FDetectionResult Result;
	Result.EffectiveDetectionLevel = ObserverDetectionLevel;
	Result.bDetected = ObserverDetectionLevel > static_cast<float>(TargetStealthLevel);
	return Result;
}

float UBattleDetectionComponent::GetBaseDetectionRangeKm(EUnitCategory Category) const
{
	switch (Category)
	{
	case EUnitCategory::Aircraft:
		return 20.0f;
	case EUnitCategory::NavalSurface:
		return 15.0f;
	case EUnitCategory::NavalSub:
		return 10.0f;
	default:
		return 5.0f;
	}
}
