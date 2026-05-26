#pragma once

#include "CoreMinimal.h"
#include "BattleVoiceCommandTypes.generated.h"

UENUM(BlueprintType)
enum class EVoiceCommandKind : uint8
{
	Unknown,
	Move,
	Stop,
	Attack
};

USTRUCT(BlueprintType)
struct FBattleVoiceParsedCommand
{
	GENERATED_BODY()

	UPROPERTY()
	EVoiceCommandKind Kind = EVoiceCommandKind::Unknown;

	UPROPERTY()
	FString UnitPhrase;

	UPROPERTY()
	FIntPoint GridCell = FIntPoint::ZeroValue;

	UPROPERTY()
	FString RawText;

	UPROPERTY()
	FString DisplayText;
};

USTRUCT(BlueprintType)
struct FBattleCommandIssueResult
{
	GENERATED_BODY()

	UPROPERTY()
	int32 IssuedCount = 0;

	UPROPERTY()
	int32 SkippedCommsCount = 0;

	UPROPERTY()
	int32 SkippedOrbatCount = 0;

	UPROPERTY()
	int32 MovePathFailedCount = 0;

	UPROPERTY()
	bool bSuccess = false;

	UPROPERTY()
	FString Message;
};

UENUM(BlueprintType)
enum class EVoiceCommandSessionState : uint8
{
	Idle,
	Armed,
	Capturing,
	Finalizing
};
