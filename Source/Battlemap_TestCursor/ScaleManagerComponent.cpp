#include "ScaleManagerComponent.h"

float UScaleManagerComponent::ApplyZoomDelta(float Delta)
{
	CurrentScale = FMath::Clamp(CurrentScale + Delta, ScaleConfig.MinScale, ScaleConfig.MaxScale);
	return CurrentScale;
}
