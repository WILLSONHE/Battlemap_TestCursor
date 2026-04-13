#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleTypes.h"
#include "BattleCommsComponent.generated.h"

UCLASS(ClassGroup=(Battle), meta=(BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UBattleCommsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Comms")
	FUnitCommsChannel CommsChannel;

	UFUNCTION(BlueprintCallable, Category = "Battle|Comms")
	bool CanCommunicateWith(const UBattleCommsComponent* Other) const;

	UFUNCTION(BlueprintCallable, Category = "Battle|Comms")
	bool IsCommandEnabled() const;
};
