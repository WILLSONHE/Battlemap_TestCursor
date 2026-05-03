#pragma once

#include "CoreMinimal.h"
#include "BattleTypes.h"
#include "BattleCareerTypes.generated.h"

/** One slot in the player's pre-mission friendly roster. */
USTRUCT(BlueprintType)
struct FPlayerLoadoutSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SlotLabel = TEXT("Unit");

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
};
