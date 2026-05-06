// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battlemap_TestCursorGameMode.h"
#include "BattleDeploymentEntryActor.h"
#include "BattleLoadoutScreenWidget.h"
#include "BattleOrbatBattleWidget.h"
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
		PC->SetDeploymentSquadSlotSelection(TArray<int32>());
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
	bMissionStartCaptured = false;
	bMissionDebriefPresented = false;
	MissionOutcome = EMissionOutcomeState::InProgress;
	MissionElapsedSeconds = 0.f;

	if (SpawnedMapGrid)
	{
		SpawnedMapGrid->Destroy();
		SpawnedMapGrid = nullptr;
	}

	CachedBattleLoadoutSlots.Reset();
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		if (UBattleCareerSaveGame* Career = GI->GetCareerSave())
		{
			CachedBattleLoadoutSlots = Career->FriendlyLoadoutSlots;
		}
	}

	SpawnedMapGrid = World->SpawnActor<ATacticalMapGrid>(TacticalMapClass, FVector::ZeroVector, FRotator::ZeroRotator);
	if (SpawnedMapGrid)
	{
		SpawnedMapGrid->BuildTestGrid(TestGridHalfExtent);
		SpawnedMapGrid->InitializeTerrainFromPipelineOutputs();
	}

	SupplyRoadAnchorWorld = SpawnedMapGrid ? SpawnedMapGrid->GetActorLocation() : FVector::ZeroVector;

	DeploymentActorsByEdge.SetNum(4);
	for (int32 i = 0; i < 4; ++i)
	{
		DeploymentActorsByEdge[i] = nullptr;
	}
	SpawnDeploymentEdgeActors();

	if (ABattlemap_TestCursorPlayerController* BattleController = Cast<ABattlemap_TestCursorPlayerController>(World->GetFirstPlayerController()))
	{
		BattleController->SetTacticalMapGrid(SpawnedMapGrid);

		BattleOrbatBattleWidget = nullptr;
		if (UBattleOrbatBattleWidget* OrbatUi = CreateWidget<UBattleOrbatBattleWidget>(BattleController, UBattleOrbatBattleWidget::StaticClass()))
		{
			BattleOrbatBattleWidget = OrbatUi;
			OrbatUi->SetupWithLoadout(CachedBattleLoadoutSlots);
			OrbatUi->AddToViewport(25);
		}

		if (APawn* PlayerPawn = BattleController->GetPawn())
		{
			FVector CameraLocation = SpawnedMapGrid
				? SpawnedMapGrid->CellToWorldCenter(FIntPoint::ZeroValue)
				: FVector::ZeroVector;
			if (SpawnedMapGrid)
			{
				CameraLocation.Z = SpawnedMapGrid->GetHeightAtWorldXY(CameraLocation.X, CameraLocation.Y) + 8000.0f;
			}
			else
			{
				CameraLocation.Z = 8000.f;
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
	int32 FriendlyCount = 0;
	for (ABattleUnit* Unit : SpawnedUnits)
	{
		if (!Unit || !Unit->bFriendly)
		{
			continue;
		}
		++FriendlyCount;
		if (Unit->IsAlive())
		{
			return false;
		}
	}
	return FriendlyCount > 0;
}

bool ABattlemap_TestCursorGameMode::AreAllEnemiesDead() const
{
	int32 EnemyCount = 0;
	for (ABattleUnit* Unit : SpawnedUnits)
	{
		if (!Unit || Unit->bFriendly)
		{
			continue;
		}
		++EnemyCount;
		if (Unit->IsAlive())
		{
			return false;
		}
	}
	return EnemyCount > 0;
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

void ABattlemap_TestCursorGameMode::SpawnBattleUnit(const FVector& Location, const FString& UnitName, bool bFriendly, TSubclassOf<ABattleUnit> UnitClass, EUnitCategory Category, EUnitType Type, const bool bApplyEnemyTankPreset, const FString& LoadoutClass, int32 SourceLoadoutSlotIndex)
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
	Unit->SourceLoadoutSlotIndex = SourceLoadoutSlotIndex;
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

FVector ABattlemap_TestCursorGameMode::GetMapEdgeWorldLocation(int32 EdgeIndex, float OutwardPaddingUU) const
{
	if (!SpawnedMapGrid)
	{
		return FVector::ZeroVector;
	}
	const float TileUU = SpawnedMapGrid->GetTileSizeUU();
	const int32 H = SpawnedMapGrid->GridHalfExtentTiles;
	const float Dim = static_cast<float>(H * 2 + 1);
	const float HalfWorld = 0.5f * Dim * TileUU;
	FVector Local(0.f, 0.f, 20.f);
	switch (EdgeIndex)
	{
	case 0:
		Local = FVector(0.f, HalfWorld + OutwardPaddingUU, 20.f);
		break;
	case 1:
		Local = FVector(HalfWorld + OutwardPaddingUU, 0.f, 20.f);
		break;
	case 2:
		Local = FVector(0.f, -(HalfWorld + OutwardPaddingUU), 20.f);
		break;
	default:
		Local = FVector(-(HalfWorld + OutwardPaddingUU), 0.f, 20.f);
		break;
	}
	return SpawnedMapGrid->GetActorTransform().TransformPosition(Local);
}

void ABattlemap_TestCursorGameMode::SpawnDeploymentEdgeActors()
{
	UWorld* World = GetWorld();
	if (!World || !SpawnedMapGrid)
	{
		return;
	}

	TArray<int32> Perm = { 0, 1, 2, 3 };
	for (int32 i = 3; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Perm.Swap(i, j);
	}
	PlayerEdgeIndex0 = Perm[0];
	PlayerEdgeIndex1 = Perm[1];
	EnemyEdgeIndex0 = Perm[2];
	EnemyEdgeIndex1 = Perm[3];

	const FVector Center = SpawnedMapGrid->GetActorLocation();

	for (int32 e = 0; e < 4; ++e)
	{
		const FVector Loc = GetMapEdgeWorldLocation(e, 350.f);
		FVector Flat = Loc;
		Flat.Z = Center.Z;
		const FVector ToCenter = (Center - Flat).GetSafeNormal2D();
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToCenter.Y, ToCenter.X));
		const FRotator Rot(0.f, Yaw + 90.f, 0.f);

		ABattleDeploymentEntryActor* A = World->SpawnActor<ABattleDeploymentEntryActor>(ABattleDeploymentEntryActor::StaticClass(), Loc, Rot);
		if (!A)
		{
			continue;
		}
		A->EdgeIndex = e;
		A->bPlayerOwned = (e == PlayerEdgeIndex0 || e == PlayerEdgeIndex1);
		A->ApplyBattlePresentation(SpawnedMapGrid->GetTileSizeUU());
		if (DeploymentActorsByEdge.IsValidIndex(e))
		{
			DeploymentActorsByEdge[e] = A;
		}
	}
}

bool ABattlemap_TestCursorGameMode::IsSideAlreadySpawnedForSlot(int32 SlotIdx, bool bFriendlyUnit) const
{
	for (ABattleUnit* U : SpawnedUnits)
	{
		if (U && U->bFriendly == bFriendlyUnit && U->SourceLoadoutSlotIndex == SlotIdx)
		{
			return true;
		}
	}
	return false;
}

void ABattlemap_TestCursorGameMode::AppendMissionStartRecordsForNewSpawnedUnits()
{
	for (ABattleUnit* Unit : SpawnedUnits)
	{
		if (!Unit)
		{
			continue;
		}
		bool bFound = false;
		for (const FMissionUnitStartRecord& R : MissionStartRecords)
		{
			if (R.SpawnOrdinal == Unit->MissionSpawnOrdinal)
			{
				bFound = true;
				break;
			}
		}
		if (bFound)
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

void ABattlemap_TestCursorGameMode::TryCaptureMissionStartWhenReady()
{
	if (SpawnedUnits.Num() == 0)
	{
		return;
	}
	if (!bMissionStartCaptured)
	{
		CaptureMissionStartSnapshots();
		bMissionStartCaptured = true;
		return;
	}
	AppendMissionStartRecordsForNewSpawnedUnits();
}

void ABattlemap_TestCursorGameMode::HandleDeploymentEntryClicked(ABattleDeploymentEntryActor* Entry, ABattlemap_TestCursorPlayerController* PC)
{
	if (!Entry || !PC || !SpawnedMapGrid)
	{
		return;
	}
	if (!Entry->bPlayerOwned)
	{
		PC->NotifyDeploymentHint(TEXT("\u654c\u65b9\u5165\u573a\u70b9\uff08\u4ec5\u793a\u610f\uff09\u3002"));
		return;
	}

	const int32 Eidx = Entry->EdgeIndex;
	if (Eidx != PlayerEdgeIndex0 && Eidx != PlayerEdgeIndex1)
	{
		return;
	}

	const TArray<int32>& Want = PC->GetDeploymentSquadSlotSelection();
	if (Want.Num() == 0)
	{
		PC->NotifyDeploymentHint(TEXT("\u8bf7\u5148\u5728\u6218\u6597\u5e8f\u5217\u4e2d\u9009\u62e9\u7f16\u5236\u3002"));
		return;
	}

	const FVector Tangent = Entry->GetActorRightVector().GetSafeNormal();
	FVector BaseLoc = Entry->GetActorLocation();
	BaseLoc.Z = SpawnedMapGrid->GetHeightAtWorldXY(BaseLoc.X, BaseLoc.Y) + 80.f;

	int32 SpawnCounter = 0;
	TArray<int32> SpawnedFriendlySlots;

	for (int32 SlotIdx : Want)
	{
		if (!CachedBattleLoadoutSlots.IsValidIndex(SlotIdx))
		{
			continue;
		}
		if (IsSideAlreadySpawnedForSlot(SlotIdx, true))
		{
			continue;
		}

		const FPlayerLoadoutSlot& Slot = CachedBattleLoadoutSlots[SlotIdx];
		if (!Slot.bEnabled || Slot.UnitScale != EFormationUnitScale::Squad)
		{
			continue;
		}
		if (LoadoutSlotHasChild(SlotIdx, CachedBattleLoadoutSlots))
		{
			continue;
		}
		if (!SquadSlotEligibleForBattleSpawn(Slot, CachedBattleLoadoutSlots))
		{
			continue;
		}

		TSubclassOf<ABattleUnit> ClassToSpawn = PickClassForLoadoutSlot(Slot);
		if (!ClassToSpawn)
		{
			continue;
		}

		const FVector Offset = Tangent * (450.f * static_cast<float>(SpawnCounter));
		FVector SpawnLoc = BaseLoc + Offset;
		SpawnLoc.Z = SpawnedMapGrid->GetHeightAtWorldXY(SpawnLoc.X, SpawnLoc.Y) + 80.f;

		const FString SpawnLabel = Slot.bSlotLabelUserOverride && !Slot.SlotLabel.IsEmpty()
			? Slot.SlotLabel
			: UBattleLoadoutScreenWidget::ComputeBattleSequenceLabel(SlotIdx, CachedBattleLoadoutSlots);

		SpawnBattleUnit(SpawnLoc, SpawnLabel, true, ClassToSpawn, Slot.Category, Slot.UnitType, false, Slot.LoadoutClass, SlotIdx);
		SpawnedFriendlySlots.Add(SlotIdx);

		if (!IsSideAlreadySpawnedForSlot(SlotIdx, false))
		{
			const int32 EnemyEdgePick = (SpawnCounter % 2 == 0) ? EnemyEdgeIndex0 : EnemyEdgeIndex1;
			if (DeploymentActorsByEdge.IsValidIndex(EnemyEdgePick) && DeploymentActorsByEdge[EnemyEdgePick])
			{
				ABattleDeploymentEntryActor* EnemyEntry = DeploymentActorsByEdge[EnemyEdgePick];
				FVector ELoc = EnemyEntry->GetActorLocation();
				ELoc.Z = SpawnedMapGrid->GetHeightAtWorldXY(ELoc.X, ELoc.Y) + 80.f;
				const FVector ETangent = EnemyEntry->GetActorRightVector().GetSafeNormal();
				ELoc += ETangent * (450.f * static_cast<float>(SpawnCounter));
				ELoc.Z = SpawnedMapGrid->GetHeightAtWorldXY(ELoc.X, ELoc.Y) + 80.f;
				const FString EnemyLabel = FString::Printf(TEXT("\u654c-%s"), *SpawnLabel);
				SpawnBattleUnit(ELoc, EnemyLabel, false, ClassToSpawn, Slot.Category, Slot.UnitType, false, Slot.LoadoutClass, SlotIdx);
			}
		}

		++SpawnCounter;
	}

	if (SpawnedFriendlySlots.Num() > 0)
	{
		TryCaptureMissionStartWhenReady();
		for (ABattleUnit* U : SpawnedUnits)
		{
			if (U && U->bFriendly && SpawnedFriendlySlots.Contains(U->SourceLoadoutSlotIndex))
			{
				SupplyRoadAnchorWorld = U->GetActorLocation();
				break;
			}
		}
	}

	PC->RefreshWorldSelectionForDeploymentSlots();
	PC->NotifyDeploymentHint(FString::Printf(TEXT("\u672c\u6b21\u6295\u5165 %d \u4e2a\u73ed\u3002"), SpawnedFriendlySlots.Num()));
}
