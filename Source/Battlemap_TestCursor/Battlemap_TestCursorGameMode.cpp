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
		SpawnedMapGrid->InitializeTerrainFromPipelineOutputs();
	}

	FVector FriendlyA(-1200.0f, -600.0f, 80.0f);
	FVector FriendlyB(-1200.0f, 600.0f, 80.0f);
	FVector Enemy(1200.0f, 0.0f, 80.0f);
	if (SpawnedMapGrid)
	{
		const float UnitBaseZ = SpawnedMapGrid->GetActorLocation().Z + 20.0f;
		FriendlyA.Z = UnitBaseZ;
		FriendlyB.Z = UnitBaseZ;
		Enemy.Z = UnitBaseZ;
	}

	SpawnTestUnit(FriendlyA, TEXT("Alpha-1"), true);
	SpawnTestUnit(FriendlyB, TEXT("Bravo-2"), true);
	SpawnTestUnit(Enemy, TEXT("Enemy-Tank"), false);

	if (ABattlemap_TestCursorPlayerController* BattleController = Cast<ABattlemap_TestCursorPlayerController>(World->GetFirstPlayerController()))
	{
		BattleController->SetTacticalMapGrid(SpawnedMapGrid);
		ABattleUnit* InitialSelectedUnit = SpawnedUnits.Num() > 0 ? SpawnedUnits[0] : nullptr;
		BattleController->SetSelectedUnit(InitialSelectedUnit);

		if (APawn* PlayerPawn = BattleController->GetPawn())
		{
			FVector CameraLocation = InitialSelectedUnit ? InitialSelectedUnit->GetActorLocation() : FVector::ZeroVector;
			if (SpawnedMapGrid)
			{
				CameraLocation.Z = SpawnedMapGrid->GetHeightAtWorldXY(CameraLocation.X, CameraLocation.Y) + 8000.0f;
			}
			PlayerPawn->SetActorLocation(CameraLocation, false, nullptr, ETeleportType::TeleportPhysics);
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
	Unit->UnitData.MaxHealth = 10000.0f;
	Unit->CurrentHealth = 10000.0f;
	Unit->AttackDamage = 1.0f;
	Unit->AttackRange = bFriendly ? 1800.0f : 2200.0f;
	Unit->DetectionRange = Unit->AttackRange * 2.0f;
	if (!bFriendly && UnitName == TEXT("Enemy-Tank"))
	{
		Unit->DetectionRange = 150.0f;
	}

	Unit->MobilityType = bFriendly ? EUnitMobilityType::Land : EUnitMobilityType::Land;

	if (Unit->CommsComponent)
	{
		Unit->CommsComponent->CommsChannel.Frequency = bFriendly ? 100 : 200;
		Unit->CommsComponent->CommsChannel.State = ECommsState::Online;
	}

	SpawnedUnits.Add(Unit);
}
