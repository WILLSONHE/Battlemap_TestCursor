#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleVoiceCommandTypes.h"
#include "BattleVoiceCommandExecutor.generated.h"

class ABattlemap_TestCursorPlayerController;
class ABattlemap_TestCursorGameMode;
class ABattleUnit;
class ATacticalMapGrid;

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleVoiceCommandExecutor : public UObject
{
	GENERATED_BODY()

public:
	FBattleCommandIssueResult Execute(ABattlemap_TestCursorPlayerController* PC, const FBattleVoiceParsedCommand& Command);

private:
	void ResolveFriendlyUnits(
		ABattlemap_TestCursorGameMode* GM,
		const FString& UnitPhrase,
		TArray<ABattleUnit*>& OutUnits) const;

	ABattleUnit* FindNearestEnemyAtGrid(ATacticalMapGrid* Grid, const FIntPoint& Cell) const;

	FVector GridCellToWorldLocation(ATacticalMapGrid* Grid, const FIntPoint& Cell) const;
};
