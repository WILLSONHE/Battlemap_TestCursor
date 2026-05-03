// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/GameModeBase.h"
#include "BattleTypes.h"
#include "BattleBalanceTableTypes.h"
#include "Battlemap_TestCursorGameMode.generated.h"

class ATacticalMapGrid;
class ABattleUnit;
class UDataTable;

UCLASS(minimalapi)
class ABattlemap_TestCursorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABattlemap_TestCursorGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Battle|Mission")
	EMissionType GetMissionType() const { return MissionType; }

	UFUNCTION(BlueprintPure, Category = "Battle|Mission")
	EMissionOutcomeState GetMissionOutcome() const { return MissionOutcome; }

	UFUNCTION(BlueprintPure, Category = "Battle|Mission")
	FVector GetSupplyRoadAnchorWorld() const { return SupplyRoadAnchorWorld; }

	UFUNCTION(BlueprintPure, Category = "Battle|Mission")
	float GetMissionElapsedSeconds() const { return MissionElapsedSeconds; }

	UFUNCTION(BlueprintPure, Category = "Battle|Mission")
	float GetDefenseHoldDurationSeconds() const { return DefenseHoldDurationSeconds; }

	/** Merged `UBattleBalanceDeveloperSettings` + optional DataTable row (same rules as `ApplyBalanceToUnit`). */
	UFUNCTION(BlueprintPure, Category = "Battle|Balance")
	FBattleBalanceTableRow GetEffectiveBattleBalanceRow() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Battle|Test")
	TSubclassOf<ATacticalMapGrid> TacticalMapClass;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Test")
	TSubclassOf<ABattleUnit> FriendlyUnitClass;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Test")
	TSubclassOf<ABattleUnit> EnemyUnitClass;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Test")
	int32 TestGridHalfExtent = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Mission")
	EMissionType MissionType = EMissionType::Assault;

	/** When MissionType is Defense and this is > 0, surviving friendlies until this many seconds elapses counts as victory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Mission", meta = (ClampMin = "0.0"))
	float DefenseHoldDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance")
	UDataTable* BalanceTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Balance")
	FName BalanceRowName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Mission")
	EMissionOutcomeState MissionOutcome = EMissionOutcomeState::InProgress;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Mission")
	float MissionElapsedSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Mission")
	FVector SupplyRoadAnchorWorld = FVector::ZeroVector;

	UPROPERTY()
	ATacticalMapGrid* SpawnedMapGrid;

	UPROPERTY()
	TArray<ABattleUnit*> SpawnedUnits;

	void SpawnTestEnvironment();

	void SpawnTestUnit(const FVector& Location, const FString& UnitName, bool bFriendly);

	void ApplyBalanceToUnit(ABattleUnit* Unit, bool bFriendly) const;

	void BuildEffectiveBattleBalanceRow(FBattleBalanceTableRow& OutEffective) const;

	void EvaluateMissionState();

	bool AreAllFriendliesDead() const;
	bool AreAllEnemiesDead() const;

	void HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);

	FDelegateHandle WorldPostActorTickHandle;
};



