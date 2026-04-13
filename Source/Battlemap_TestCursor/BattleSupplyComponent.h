#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleTypes.h"
#include "BattleSupplyComponent.generated.h"

UCLASS(ClassGroup=(Battle), meta=(BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UBattleSupplyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Supply")
	float Food = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Supply")
	float Fuel = 100.0f;

	UFUNCTION(BlueprintCallable, Category = "Battle|Supply")
	void ConsumeForUnitType(EUnitCategory Category, bool bRoadConnected, float Delta);
};
