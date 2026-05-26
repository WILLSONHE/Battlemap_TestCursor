#pragma once

#include "CoreMinimal.h"
#include "BattleVoiceCommandTypes.h"

class BATTLEMAP_TESTCURSOR_API FBattleVoiceCommandParser
{
public:
	static bool Parse(const FString& CapturedText, FBattleVoiceParsedCommand& OutCommand);

private:
	static FString NormalizeCommandBody(const FString& Raw);
	static bool TryParseMove(const FString& Body, FString& OutUnitPhrase, FIntPoint& OutCell, FString& OutDisplay);
	static bool TryParseStop(const FString& Body, FString& OutUnitPhrase, FString& OutDisplay);
	static bool TryParseAttack(const FString& Body, FString& OutUnitPhrase, FIntPoint& OutCell, FString& OutDisplay);
	static bool ExtractGridCoords(const FString& Tail, FIntPoint& OutCell);
};
