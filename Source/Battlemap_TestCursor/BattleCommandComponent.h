#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleTypes.h"
#include "BattleCommandComponent.generated.h"

UCLASS(ClassGroup=(Battle), meta=(BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UBattleCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle|Command")
	void EnqueueCommand(const FActiveCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "Battle|Command")
	bool TryPopNextCommand(FActiveCommand& OutCommand);

	UFUNCTION(BlueprintCallable, Category = "Battle|Command")
	void RemoveCommandsByType(ECommandType CommandType);

private:
	UPROPERTY()
	TArray<FActiveCommand> CommandQueue;
};
