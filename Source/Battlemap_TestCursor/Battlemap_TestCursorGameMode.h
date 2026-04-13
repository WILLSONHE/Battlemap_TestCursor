// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Battlemap_TestCursorGameMode.generated.h"

class ATacticalMapGrid;
class ABattleUnit;

UCLASS(minimalapi)
class ABattlemap_TestCursorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABattlemap_TestCursorGameMode();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Battle|Test")
	TSubclassOf<ATacticalMapGrid> TacticalMapClass;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Test")
	TSubclassOf<ABattleUnit> FriendlyUnitClass;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Test")
	TSubclassOf<ABattleUnit> EnemyUnitClass;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Test")
	int32 TestGridHalfExtent = 4;

	UPROPERTY()
	ATacticalMapGrid* SpawnedMapGrid;

	UPROPERTY()
	TArray<ABattleUnit*> SpawnedUnits;

	void SpawnTestEnvironment();

	void SpawnTestUnit(const FVector& Location, const FString& UnitName, bool bFriendly);
};



