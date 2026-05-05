#pragma once

#include "CoreMinimal.h"
#include "BattleTypes.h"
#include "BattleCareerTypes.generated.h"

/** ORBAT row unit scale (指挥链上的单位规模). */
UENUM(BlueprintType)
enum class EFormationUnitScale : uint8
{
	Squad UMETA(DisplayName = "\u73ed"),
	Platoon UMETA(DisplayName = "\u6392"),
	Company UMETA(DisplayName = "\u8fde"),
	Battalion UMETA(DisplayName = "\u8425"),
	Regiment UMETA(DisplayName = "\u56e2/\u5408\u6210\u8425"),
	Brigade UMETA(DisplayName = "\u65c5"),
	Division UMETA(DisplayName = "\u5e08"),
	Corps UMETA(DisplayName = "\u519b"),
	ArmyGroup UMETA(DisplayName = "\u96c6\u56e2\u519b")
};

/** One slot in the player's pre-mission friendly roster. */
USTRUCT(BlueprintType)
struct FPlayerLoadoutSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;

	/** INDEX_NONE = root row under player ORBAT. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ParentSlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EFormationUnitScale UnitScale = EFormationUnitScale::Squad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SlotLabel;

	/**
	 * If false, SlotLabel is filled from battle-sequence on each loadout list rebuild.
	 * Set true when the player commits a custom unit name; never auto-overwritten while true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSlotLabelUserOverride = true;

	/** 兵种 (e.g. \u9646\u519b) — design doc \u00a77 tree. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LoadoutBranch;

	/** 分类 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LoadoutClass;

	/** 单位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LoadoutUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUnitCategory Category = EUnitCategory::Infantry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUnitType UnitType = EUnitType::Infantry;
};

/** One row on the post-mission debrief screen. */
USTRUCT(BlueprintType)
struct FDebriefUnitLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString UnitLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFriendly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealthStart = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HealthStart = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HealthEnd = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDestroyed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LoadoutClass;
};

/** Destroyed units aggregated by loadout class (one side). */
USTRUCT(BlueprintType)
struct FDebriefClassCasualty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ClassName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DestroyedCount = 0;
};

/** Passed to the debrief widget after Victory / Defeat. */
USTRUCT(BlueprintType)
struct FMissionDebriefPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMissionOutcomeState Outcome = EMissionOutcomeState::InProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMissionType MissionType = EMissionType::Assault;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDebriefUnitLine> Lines;

	/** Sum over units of max(0, mission-start ammo minus current ammo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TotalRoundsExpended = 0;

	/** Sum of food consumed (start - end) per unit supply. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TotalFoodConsumed = 0.f;

	/** Sum of fuel consumed (start - end) per unit supply. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TotalFuelConsumed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDebriefClassCasualty> FriendlyDestroyedByClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDebriefClassCasualty> EnemyDestroyedByClass;
};

/** Snapshot at mission start for one unit. */
USTRUCT()
struct FMissionUnitStartRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FString UnitLabel;

	UPROPERTY()
	bool bFriendly = true;

	UPROPERTY()
	float MaxHealth = 0.0f;

	UPROPERTY()
	float HealthStart = 0.0f;

	UPROPERTY()
	int32 SpawnOrdinal = INDEX_NONE;

	UPROPERTY()
	FString LoadoutClass;

	UPROPERTY()
	float FoodStart = 0.f;

	UPROPERTY()
	float FuelStart = 0.f;

	UPROPERTY()
	int32 AmmoStart = 0;
};
