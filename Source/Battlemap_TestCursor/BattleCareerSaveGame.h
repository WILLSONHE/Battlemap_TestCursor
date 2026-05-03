#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BattleCareerTypes.h"
#include "BattleCareerSaveGame.generated.h"

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleCareerSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 MilitaryRankIndex = 0;

	UPROPERTY()
	int32 Experience = 0;

	UPROPERTY()
	TArray<FPlayerLoadoutSlot> FriendlyLoadoutSlots;

	/** Default two-infantry roster matching legacy Alpha/Bravo test. */
	void ResetToDefaultRoster();

	static FString SlotName();
};
