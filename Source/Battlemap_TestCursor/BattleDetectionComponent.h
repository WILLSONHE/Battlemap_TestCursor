#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleTypes.h"
#include "BattleDetectionComponent.generated.h"

UCLASS(ClassGroup=(Battle), meta=(BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UBattleDetectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle|Detection")
	FDetectionResult EvaluateDetection(float ObserverDetectionLevel, int32 TargetStealthLevel) const;

	UFUNCTION(BlueprintCallable, Category = "Battle|Detection")
	float GetBaseDetectionRangeKm(EUnitCategory Category) const;
};
