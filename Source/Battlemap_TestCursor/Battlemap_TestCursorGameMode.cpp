// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battlemap_TestCursorGameMode.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "Battlemap_TestCursorCharacter.h"
#include "Battlemap_TestCursorHUD.h"
#include "BattleGameInstance.h"
#include "BattleCareerSaveGame.h"
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
	FriendlyVehicleUnitClass = ABattleVehicleUnit::StaticClass();
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

	TArray<FPlayerLoadoutSlot> LoadoutSlots;
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		if (UBattleCareerSaveGame* Career = GI->GetCareerSave())
		{
			LoadoutSlots = Career->FriendlyLoadoutSlots;
		}
	}

	const FVector FriendlyLane = FriendlyB - FriendlyA;
	int32 FriendlyPlaceIndex = 0;
	bool bAnchorFromSpawn = false;
	for (const FPlayerLoadoutSlot& Slot : LoadoutSlots)
	{
		if (!Slot.bEnabled)
		{
			continue;
		}
		TSubclassOf<ABattleUnit> ClassToSpawn = (Slot.Category == EUnitCategory::Vehicle) ? FriendlyVehicleUnitClass : FriendlyUnitClass;
		if (!ClassToSpawn)
		{
			ClassToSpawn = FriendlyUnitClass;
		}
		const FVector SpawnLoc = FriendlyA + FriendlyLane * static_cast<float>(FriendlyPlaceIndex);
		if (!bAnchorFromSpawn)
		{
			SupplyRoadAnchorWorld = SpawnLoc;
			bAnchorFromSpawn = true;
		}
		SpawnBattleUnit(SpawnLoc, Slot.SlotLabel, true, ClassToSpawn, Slot.Category, Slot.UnitType, false);
		++FriendlyPlaceIndex;
	}

	SpawnBattleUnit(Enemy, TEXT("Enemy-Tank"), false, EnemyUnitClass, EUnitCategory::Vehicle, EUnitType::Tank, true);

	CaptureMissionStartSnapshots();

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
	if (MissionOutcome != EMissionOutcomeState::InProgress)
	{
		return;
	}

	if (AreAllFriendliesDead())
	{
		MissionOutcome = EMissionOutcomeState::Defeat;
		UE_LOG(LogTemp, Log, TEXT("Mission outcome: Defeat (%s)"), *UEnum::GetValueAsString(MissionType));
		TryPresentMissionDebrief();
		return;
	}

	if (AreAllEnemiesDead())
	{
		MissionOutcome = EMissionOutcomeState::Victory;
		UE_LOG(LogTemp, Log, TEXT("Mission outcome: Victory (%s) — all hostiles eliminated."), *UEnum::GetValueAsString(MissionType));
		TryPresentMissionDebrief();
		return;
	}

	if (MissionType == EMissionType::Defense && DefenseHoldDurationSeconds > 0.0f && MissionElapsedSeconds >= DefenseHoldDurationSeconds)
	{
		MissionOutcome = EMissionOutcomeState::Victory;
		UE_LOG(LogTemp, Log, TEXT("Mission outcome: Victory (Defense hold target %.1fs, elapsed %.1fs)."), DefenseHoldDurationSeconds, MissionElapsedSeconds);
		TryPresentMissionDebrief();
	}
}

void ABattlemap_TestCursorGameMode::CaptureMissionStartSnapshots()
{
	MissionStartRecords.Reset();
	for (ABattleUnit* Unit : SpawnedUnits)
	{
		if (!Unit)
		{
			continue;
		}
		FMissionUnitStartRecord Rec;
		Rec.UnitLabel = Unit->UnitLabel;
		Rec.bFriendly = Unit->bFriendly;
		Rec.MaxHealth = Unit->UnitData.MaxHealth;
		Rec.HealthStart = Unit->CurrentHealth;
		MissionStartRecords.Add(Rec);
	}
}

void ABattlemap_TestCursorGameMode::TryPresentMissionDebrief()
{
	if (bMissionDebriefPresented || MissionOutcome == EMissionOutcomeState::InProgress)
	{
		return;
	}
	bMissionDebriefPresented = true;

	FMissionDebriefPayload Payload;
	Payload.Outcome = MissionOutcome;
	Payload.MissionType = MissionType;

	for (ABattleUnit* Unit : SpawnedUnits)
	{
		if (!Unit)
		{
			continue;
		}
		FDebriefUnitLine Line;
		Line.UnitLabel = Unit->UnitLabel;
		Line.bFriendly = Unit->bFriendly;
		Line.HealthEnd = Unit->CurrentHealth;
		Line.bDestroyed = !Unit->IsAlive();
		Line.MaxHealthStart = Unit->UnitData.MaxHealth;
		Line.HealthStart = Unit->CurrentHealth;
		for (const FMissionUnitStartRecord& Rec : MissionStartRecords)
		{
			if (Rec.UnitLabel == Unit->UnitLabel && Rec.bFriendly == Unit->bFriendly)
			{
				Line.MaxHealthStart = Rec.MaxHealth;
				Line.HealthStart = Rec.HealthStart;
				break;
			}
		}
		Payload.Lines.Add(Line);
	}

	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		GI->ApplyPostMissionExperience(Payload.Outcome);
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			GI->ShowMissionDebrief(PC, Payload);
		}
	}
}

void ABattlemap_TestCursorGameMode::SpawnBattleUnit(const FVector& Location, const FString& UnitName, bool bFriendly, TSubclassOf<ABattleUnit> UnitClass, EUnitCategory Category, EUnitType Type, const bool bApplyEnemyTankPreset)
{
	UWorld* World = GetWorld();
	if (!World || !UnitClass)
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
	Unit->UnitData.Category = Category;
	Unit->UnitData.UnitType = Type;

	if (bApplyEnemyTankPreset)
	{
		Unit->UnitData.MaxHealth = 500.0f;
		Unit->CurrentHealth = 500.0f;
	}
	else if (bFriendly && Category == EUnitCategory::Infantry)
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

	if (bApplyEnemyTankPreset)
	{
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
