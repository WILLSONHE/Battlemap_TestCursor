#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleTypes.h"
#include "BattleForceBuilderComponent.generated.h"

UCLASS(ClassGroup=(Battle), meta=(BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UBattleForceBuilderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle|ForceBuilder")
	bool IsSquadSizeValid(int32 SquadSize) const;

	UFUNCTION(BlueprintCallable, Category = "Battle|ForceBuilder")
	bool IsHierarchySizeValid(int32 ChildCount) const;

	UFUNCTION(BlueprintCallable, Category = "Battle|ForceBuilder")
	bool BuildPlatoonFromSquads(const TArray<FName>& SquadIds, FPlatoonData& OutPlatoon) const;
};
