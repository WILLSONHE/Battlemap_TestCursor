#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleTypes.h"
#include "CommanderProgressionComponent.generated.h"

UCLASS(ClassGroup=(Battle), meta=(BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UCommanderProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Progression")
	FCommanderProfile Profile;

	UFUNCTION(BlueprintCallable, Category = "Battle|Progression")
	int32 GetCommandCapacity() const;
};
