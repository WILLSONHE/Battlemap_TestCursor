#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleTypes.h"
#include "ScaleManagerComponent.generated.h"

UCLASS(ClassGroup=(Battle), meta=(BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UScaleManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Scale")
	FScaleConfig ScaleConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Scale")
	float CurrentScale = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Battle|Scale")
	float ApplyZoomDelta(float Delta);
};
