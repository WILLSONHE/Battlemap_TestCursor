// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battlemap_TestCursorGameMode.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "Battlemap_TestCursorCharacter.h"
#include "Battlemap_TestCursorHUD.h"
#include "TacticalMapGrid.h"
#include "BattleUnit.h"
#include "BattleCommsComponent.h"
#include "Engine/World.h"

ABattlemap_TestCursorGameMode::ABattlemap_TestCursorGameMode()
{
	PlayerControllerClass = ABattlemap_TestCursorPlayerController::StaticClass();
	DefaultPawnClass = ABattlemap_TestCursorCharacter::StaticClass();
	HUDClass = ABattlemap_TestCursorHUD::StaticClass();
	TacticalMapClass = ATacticalMapGrid::StaticClass();
	FriendlyUnitClass = ABattleInfantryUnit::StaticClass();
	EnemyUnitClass = ABattleVehicleUnit::StaticClass();
}

void ABattlemap_TestCursorGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnTestEnvironment();
}

void ABattlemap_TestCursorGameMode::SpawnTestEnvironment()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	SpawnedMapGrid = World->SpawnActor<ATacticalMapGrid>(TacticalMapClass, FVector::ZeroVector, FRotator::ZeroRotator);
	if (SpawnedMapGrid)
	{
		SpawnedMapGrid->BuildTestGrid(TestGridHalfExtent);
	}

	SpawnTestUnit(FVector(-1200.0f, -600.0f, 80.0f), TEXT("Alpha-1"), true);
	SpawnTestUnit(FVector(-1200.0f, 600.0f, 80.0f), TEXT("Bravo-2"), true);
	SpawnTestUnit(FVector(12000.0f, 0.0f, 80.0f), TEXT("Enemy-Tank"), false);

	if (ABattlemap_TestCursorPlayerController* BattleController = Cast<ABattlemap_TestCursorPlayerController>(World->GetFirstPlayerController()))
	{
		BattleController->SetTacticalMapGrid(SpawnedMapGrid);
		if (SpawnedUnits.Num() > 0)
		{
			BattleController->SetSelectedUnit(SpawnedUnits[0]);
		}
	}
}

void ABattlemap_TestCursorGameMode::SpawnTestUnit(const FVector& Location, const FString& UnitName, bool bFriendly)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSubclassOf<ABattleUnit> UnitClass = bFriendly ? FriendlyUnitClass : EnemyUnitClass;
	if (!UnitClass)
	{
		return;
	}

	ABattleUnit* Unit = World->SpawnActor<ABattleUnit>(UnitClass, Location, FRotator::ZeroRotator);
	if (!Unit)
	{
		return;
	}

	Unit->UnitLabel = UnitName;
	Unit->bFriendly = bFriendly;
	Unit->UnitData.UnitId = FName(*UnitName);
	Unit->UnitData.Category = bFriendly ? EUnitCategory::Infantry : EUnitCategory::Vehicle;
	Unit->UnitData.UnitType = bFriendly ? EUnitType::Infantry : EUnitType::Tank;
	Unit->CurrentHealth = 100.0f;
	Unit->AttackRange = bFriendly ? 1800.0f : 2200.0f;
	Unit->DetectionRange = Unit->AttackRange * 2.0f;

	if (Unit->CommsComponent)
	{
		Unit->CommsComponent->CommsChannel.Frequency = bFriendly ? 100 : 200;
		Unit->CommsComponent->CommsChannel.State = ECommsState::Online;
	}

	SpawnedUnits.Add(Unit);
}