// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battlemap_TestCursorGameMode.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "Battlemap_TestCursorCharacter.h"
#include "Battlemap_TestCursorHUD.h"
#include "TacticalMapGrid.h"
#include "BattleUnit.h"
#include "BattleSupplyComponent.h"
#include "BattleCommsComponent.h"
#include "BattleBalanceDeveloperSettings.h"
#include "BattleBalanceTableTypes.h"
#include "BattleECMZoneComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ABattlemap_TestCursorGameMode::ABattlemap_TestCursorGameMode()
{
	PlayerControllerClass = ABattlemap_TestCursorPlayerController::StaticClass();
	DefaultPawnClass = ABattlemap_TestCursorCharacter::StaticClass();
	HUDClass = ABattlemap_TestCursorHUD::StaticClass();
	TacticalMapClass = ATacticalMapGrid::StaticClass();
	FriendlyUnitClass = ABattleInfantryUnit::StaticClass();
	EnemyUnitClass = ABattleVehicleUnit::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ABattlemap_TestCursorGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &ABattlemap_TestCursorGameMode::HandleWorldPostActorTick);
	}

	SpawnTestEnvironment();
}

void ABattlemap_TestCursorGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WorldPostActorTickHandle.IsValid())
	{
		FWorldDelegates::OnWorldPostActorTick.Remove(WorldPostActorTickHandle);
		WorldPostActorTickHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ABattlemap_TestCursorGameMode::HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (!World || World != GetWorld())
	{
		return;
	}
	UBattleECMZoneComponent::UpdateFriendlyJamForWorld(World);
}

void ABattlemap_TestCursorGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MissionOutcome == EMissionOutcomeState::InProgress)
	{
		MissionElapsedSeconds += DeltaSeconds;
		EvaluateMissionState();
	}
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

	SupplyRoadAnchorWorld = FriendlyA;

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

void ABattlemap_TestCursorGameMode::BuildEffectiveBattleBalanceRow(FBattleBalanceTableRow& OutEffective) const
{
	const UBattleBalanceDeveloperSettings* Defaults = GetDefault<UBattleBalanceDeveloperSettings>();
	OutEffective.BalanceVersion = Defaults->BalanceVersion;
	OutEffective.AttackDamage = Defaults->AttackDamage;
	OutEffective.AttackRange = Defaults->AttackRange;
	OutEffective.MoveSpeed = Defaults->MoveSpeed;
	OutEffective.DetectionRange = Defaults->DetectionRange;
	OutEffective.SupplyFoodConsumePerSecond = Defaults->SupplyFoodConsumePerSecond;
	OutEffective.SupplyFuelConsumePerSecond = Defaults->SupplyFuelConsumePerSecond;
	OutEffective.SupplyRoadFoodMultiplier = Defaults->SupplyRoadFoodMultiplier;
	OutEffective.SupplyRoadFuelMultiplier = Defaults->SupplyRoadFuelMultiplier;
	OutEffective.EcmJammedDetectionRangeScale = Defaults->EcmJammedDetectionRangeScale;

	if (BalanceTable && BalanceRowName != NAME_None)
	{
		if (const FBattleBalanceTableRow* Row = BalanceTable->FindRow<FBattleBalanceTableRow>(BalanceRowName, TEXT("BattleBalance")))
		{
			OutEffective = *Row;
		}
	}
}

FBattleBalanceTableRow ABattlemap_TestCursorGameMode::GetEffectiveBattleBalanceRow() const
{
	FBattleBalanceTableRow Row;
	BuildEffectiveBattleBalanceRow(Row);
	return Row;
}

void ABattlemap_TestCursorGameMode::ApplyBalanceToUnit(ABattleUnit* Unit, bool bFriendly) const
{
	if (!Unit)
	{
		return;
	}

	FBattleBalanceTableRow Effective;
	BuildEffectiveBattleBalanceRow(Effective);

	Unit->AttackDamage = Effective.AttackDamage;
	Unit->AttackRange = Effective.AttackRange;
	Unit->MoveSpeed = Effective.MoveSpeed;
	Unit->DetectionRange = Effective.DetectionRange;

	if (Unit->SupplyComponent)
	{
		Unit->SupplyComponent->ApplySupplyRatesFromBalanceRow(Effective);
	}
}

bool ABattlemap_TestCursorGameMode::AreAllFriendliesDead() const
{
	for (ABattleUnit* Unit : SpawnedUnits)
	{
		if (Unit && Unit->bFriendly && Unit->IsAlive())
		{
			return false;
		}
	}
	return true;
}

bool ABattlemap_TestCursorGameMode::AreAllEnemiesDead() const
{
	for (ABattleUnit* Unit : SpawnedUnits)
	{
		if (Unit && !Unit->bFriendly && Unit->IsAlive())
		{
			return false;
		}
	}
	return true;
}

void ABattlemap_TestCursorGameMode::EvaluateMissionState()
{
	if (AreAllFriendliesDead())
	{
		MissionOutcome = EMissionOutcomeState::Defeat;
		UE_LOG(LogTemp, Log, TEXT("Mission outcome: Defeat (%s)"), *UEnum::GetValueAsString(MissionType));
		return;
	}

	if (AreAllEnemiesDead())
	{
		MissionOutcome = EMissionOutcomeState::Victory;
		UE_LOG(LogTemp, Log, TEXT("Mission outcome: Victory (%s) — all hostiles eliminated."), *UEnum::GetValueAsString(MissionType));
		return;
	}

	if (MissionType == EMissionType::Defense && DefenseHoldDurationSeconds > 0.0f && MissionElapsedSeconds >= DefenseHoldDurationSeconds)
	{
		MissionOutcome = EMissionOutcomeState::Victory;
		UE_LOG(LogTemp, Log, TEXT("Mission outcome: Victory (Defense hold target %.1fs, elapsed %.1fs)."), DefenseHoldDurationSeconds, MissionElapsedSeconds);
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
	if (UnitName == TEXT("Alpha-1"))
	{
		Unit->UnitData.MaxHealth = 500.0f;
		Unit->CurrentHealth = 500.0f;
	}
	else
	{
		Unit->UnitData.MaxHealth = 10000.0f;
		Unit->CurrentHealth = 10000.0f;
	}
	ApplyBalanceToUnit(Unit, bFriendly);
	if (!bFriendly && UnitName == TEXT("Enemy-Tank"))
	{
		Unit->UnitData.MaxHealth = 500.0f;
		Unit->CurrentHealth = 500.0f;
		Unit->DetectionRange = 800.0f;
		Unit->AttackRange = 700.0f;
	}

	if (Unit->CommsComponent)
	{
		Unit->CommsComponent->CommsChannel.Frequency = bFriendly ? 100 : 200;
		Unit->CommsComponent->CommsChannel.State = ECommsState::Online;
	}

	SpawnedUnits.Add(Unit);
}
