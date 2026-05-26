#pragma once

#include "CoreMinimal.h"
#include "BattleAudioInputTypes.generated.h"

USTRUCT(BlueprintType)
struct FBattleAudioInputDeviceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString DeviceId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;
};
