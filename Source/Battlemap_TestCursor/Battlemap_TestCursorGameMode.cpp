// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battlemap_TestCursorGameMode.h"
#include "BattleLoadoutScreenWidget.h"
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

namespace
{
	void AddDestroyedByClass(TArray<FDebriefClassCasualty>& Arr, const FString& ClassName)
	{
		const FString Key = ClassName.IsEmpty() ? FString(TEXT("\u672a\u5206\u7c7b")) : ClassName;
		for (FDebriefClassCasualty& Entry : Arr)
		{
			if (Entry.ClassName == Key)
			{
				Entry.DestroyedCount++;
				return;
			}
		}
		FDebriefClassCasualty NewEntry;
		NewEntry.ClassName = Key;
		NewEntry.DestroyedCount = 1;
		Arr.Add(NewEntry);
	}

	bool LoadoutSlotHasChild(int32 SlotIdx, const TArray<FPlayerLoadoutSlot>& Slots)
	{
		for (int32 j = 0; j < Slots.Num(); ++j)
		{
			if (Slots[j].ParentSlotIndex == SlotIdx)
			{
				return true;
			}
		}
		return false;
	}

	/** Only spawn \u73ed that belong under a non-\u73ed ORBAT row (excludes mis-tagged parent rows and root-only squads). */
	bool SquadSlotEligibleForBattleSpawn(const FPlayerLoadoutSlot& Slot, const TArray<FPlayerLoadoutSlot>& Slots)
	{
		const int32 P = Slot.ParentSlotIndex;
		if (P == INDEX_NONE || !Slots.IsValidIndex(P))
		{
			return false;
		}
		return Slots[P].UnitScale != EFormationUnitScale::Squad;
	}
}

TSubclassOf<ABattleUnit> ABattlemap_TestCursorGameMode::PickClassForLoadoutSlot(const FPlayerLoadoutSlot& Slot) const
{
	switch (Slot.Category)
	{
	case EUnitCategory::Infantry:
		return FriendlyUnitClass ? FriendlyUnitClass : TSubclassOf<ABattleUnit>(ABattleInfantryUnit::StaticClass());
	case EUnitCategory::Vehicle:
		return FriendlyVehicleUnitClass ? FriendlyVehicleUnitClass : TSubclassOf<ABattleUnit>(ABattleVehicleUnit::StaticClass());
	case EUnitCategory::Aircraft:
		if (FriendlyAircraftClass)
		{
			return FriendlyAircraftClass;
		}
		return FriendlyVehicleUnitClass ? FriendlyVehicleUnitClass : TSubclassOf<ABattleUnit>(ABattleAircraftUnit::StaticClass());
	case EUnitCategory::NavalSurface:
		if (FriendlyNavalSurfaceClass)
		{
			return FriendlyNavalSurfaceClass;
		}
		return FriendlyVehicleUnitClass ? FriendlyVehicleUnitClass : TSubclassOf<ABattleUnit>(ABattleNavalSurfaceUnit::StaticClass());
	case EUnitCategory::NavalSub:
		if (FriendlySubmarineClass)
		{
			return FriendlySubmarineClass;
		}
		return FriendlyVehicleUnitClass ? FriendlyVehicleUnitClass : TSubclassOf<ABattleUnit>(ABattleSubmarineUnit::StaticClass());
	case EUnitCategory::Facility:
		return FriendlyVehicleUnitClass ? FriendlyVehicleUnitClass : TSubclassOf<ABattleUnit>(ABattleFacilityUnit::StaticClass());
	default:
		return FriendlyVehicleUnitClass ? FriendlyVehicleUnitClass : TSubclassOf<ABattleUnit>(ABattleVehicleUnit::StaticClass());
	}
}

ABattlemap_TestCursorGameMode::ABattlemap_TestCursorGameMode()
{
	PlayerControllerClass = ABattlemap_TestCursorPlayerController::StaticClass();
	DefaultPawnClass = ABattlemap_TestCursorCharacter::StaticClass();
	HUDClass = ABattlemap_TestCursorHUD::StaticClass();
	TacticalMapClass = ATacticalMapGrid::StaticClass();
	FriendlyUnitClass = ABattleInfantryUnit::StaticClass();
	FriendlyVehicleUnitClass = ABattleVehicleUnit::StaticClass();
	FriendlyNavalSurfaceClass = ABattleNavalSurfaceUnit::StaticClass();
	FriendlySubmarineClass = ABattleSubmarineUnit::StaticClass();
	FriendlyAircraftClass = ABattleAircraftUnit::StaticClass();
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

	if (ABattlemap_TestCursorPlayerController* PC = Cast<ABattlemap_TestCursorPlayerController>(World->GetFirstPlayerController()))
	{
		PC->SetSelectedUnits(TArray<ABattleUnit*>());
	}
	for (ABattleUnit* Old : SpawnedUnits)
	{
		if (IsValid(Old))
		{
			Old->Destroy();
		}
	}
	SpawnedUnits.Reset();
	MissionStartRecords.Reset();
	bMissionDebriefPresented = false;
	MissionOutcome = EMissionOutcomeState::InProgress;
	MissionElapsedSeconds = 0.f;

	if (SpawnedMapGrid)
	{
		SpawnedMapGrid->Destroy();
		SpawnedMapGrid = nullptr;
	}

	SpawnedMapGrid = World->SpawnActor<ATacticalMapGrid>(TacticalMapClass, FVector::ZeroVector, FRotator::ZeroRotator);
	if (SpawnedMapGrid)
	{
		SpawnedMapGrid->BuildTestGrid(TestGridHalfExtent);
		SpawnedMapGrid->InitializeTerrainFromPipelineOutputs();
	}

	FVector FriendlyA(-1200.0f, -600.0f, 80.0f);
	FVector FriendlyB(-1200.0f, 600.0f, 80.0f);
	FVector EnemyA(1200.0f, -600.0f, 80.0f);
	FVector EnemyB(1200.0f, 600.0f, 80.0f);
	if (SpawnedMapGrid)
	{
		const float UnitBaseZ = SpawnedMapGrid->GetActorLocation().Z + 20.0f;
		FriendlyA.Z = UnitBaseZ;
		FriendlyB.Z = UnitBaseZ;
		EnemyA.Z = UnitBaseZ;
		EnemyB.Z = UnitBaseZ;
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
	const FVector EnemyLane = EnemyB - EnemyA;
	int32 FriendlyPlaceIndex = 0;
	bool bAnchorFromSpawn = false;
	for (int32 SlotIdx = 0; SlotIdx < LoadoutSlots.Num(); ++SlotIdx)
	{
		const FPlayerLoadoutSlot& Slot = LoadoutSlots[SlotIdx];
		if (!Slot.bEnabled || Slot.UnitScale != EFormationUnitScale::Squad)
		{
			continue;
		}
		if (LoadoutSlotHasChild(SlotIdx, LoadoutSlots))
		{
			continue;
		}
		if (!SquadSlotEligibleForBattleSpawn(Slot, LoadoutSlots))
		{
			continue;
		}
		TSubclassOf<ABattleUnit> ClassToSpawn = PickClassForLoadoutSlot(Slot);
		if (!ClassToSpawn)
		{
			continue;
		}
		const FVector SpawnLoc = FriendlyA + FriendlyLane * static_cast<float>(FriendlyPlaceIndex);
		const FVector EnemySpawnLoc = EnemyA + EnemyLane * static_cast<float>(FriendlyPlaceIndex);
		if (!bAnchorFromSpawn)
		{
			SupplyRoadAnchorWorld = SpawnLoc;
			bAnchorFromSpawn = true;
		}
		const FString SpawnLabel = Slot.bSlotLabelUserOverride && !Slot.SlotLabel.IsEmpty()
			? Slot.SlotLabel
			: UBattleLoadoutScreenWidget::ComputeBattleSequenceLabel(SlotIdx, LoadoutSlots);
		const FString LoadoutClassName = Slot.LoadoutClass;
		SpawnBattleUnit(SpawnLoc, SpawnLabel, true, ClassToSpawn, Slot.Category, Slot.UnitType, false, LoadoutClassName);
		const FString EnemyLabel = FString::Printf(TEXT("\u654c-%s"), *SpawnLabel);
		SpawnBattleUnit(EnemySpawnLoc, EnemyLabel, false, ClassToSpawn, Slot.Category, Slot.UnitType, false, LoadoutClassName);
		++FriendlyPlaceIndex;
	}

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
		Rec.SpawnOrdinal = Unit->MissionSpawnOrdinal;
		Rec.LoadoutClass = Unit->UnitData.LoadoutClass;
		if (Unit->SupplyComponent)
		{
			Rec.FoodStart = Unit->SupplyComponent->Food;
			Rec.FuelStart = Unit->SupplyComponent->Fuel;
		}
		Rec.AmmoStart = Unit->CurrentAmmo;
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
		Line.LoadoutClass = Unit->UnitData.LoadoutClass;
		const FMissionUnitStartRecord* StartMatch = nullptr;
		for (const FMissionUnitStartRecord& Rec : MissionStartRecords)
		{
			if (Rec.SpawnOrdinal != INDEX_NONE && Rec.SpawnOrdinal == Unit->MissionSpawnOrdinal)
			{
				StartMatch = &Rec;
				break;
			}
		}
		if (StartMatch)
		{
			Line.MaxHealthStart = StartMatch->MaxHealth;
			Line.HealthStart = StartMatch->HealthStart;
			Payload.TotalRoundsExpended += FMath::Max(0, StartMatch->AmmoStart - Unit->CurrentAmmo);
		}
		if (Unit->SupplyComponent && StartMatch)
		{
			Payload.TotalFoodConsumed += FMath::Max(0.f, StartMatch->FoodStart - Unit->SupplyComponent->Food);
			Payload.TotalFuelConsumed += FMath::Max(0.f, StartMatch->FuelStart - Unit->SupplyComponent->Fuel);
		}
		if (Line.bDestroyed)
		{
			if (Line.bFriendly)
			{
				AddDestroyedByClass(Payload.FriendlyDestroyedByClass, Line.LoadoutClass);
			}
			else
			{
				AddDestroyedByClass(Payload.EnemyDestroyedByClass, Line.LoadoutClass);
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

void ABattlemap_TestCursorGameMode::SpawnBattleUnit(const FVector& Location, const FString& UnitName, bool bFriendly, TSubclassOf<ABattleUnit> UnitClass, EUnitCategory Category, EUnitType Type, const bool bApplyEnemyTankPreset, const FString& LoadoutClass)
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

	const int32 SpawnOrdinal = SpawnedUnits.Num();
	Unit->MissionSpawnOrdinal = SpawnOrdinal;
	Unit->UnitLabel = UnitName;
	Unit->bFriendly = bFriendly;
	Unit->UnitData.UnitId = FName(*UnitName);
	Unit->UnitData.Category = Category;
	Unit->UnitData.UnitType = Type;
	Unit->UnitData.LoadoutClass = LoadoutClass;

	if (!bFriendly)
	{
		Unit->UnitData.MaxHealth = 400.0f;
		Unit->CurrentHealth = 400.0f;
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
