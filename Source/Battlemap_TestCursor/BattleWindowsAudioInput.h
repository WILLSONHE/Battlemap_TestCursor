#pragma once

#include "CoreMinimal.h"
#include "BattleAudioInputTypes.h"

/** Win64 capture device enumeration and input peak metering (WASAPI). */
class BATTLEMAP_TESTCURSOR_API FBattleWindowsAudioInput
{
public:
	static bool EnumerateCaptureDevices(TArray<FBattleAudioInputDeviceInfo>& OutDevices, FString* OutError = nullptr);

	/** One-shot peak (often 0 on capture endpoints); prefer meter session below. */
	static float QueryCapturePeakLevel(const FString& DeviceId, FString* OutError = nullptr);

	/** Open a shared-mode capture stream for live input level metering. */
	static bool StartCaptureMeter(const FString& DeviceId, FString* OutError = nullptr);
	static void StopCaptureMeter();
	static bool IsCaptureMeterActive();
	static float PollCaptureMeterPeak(FString* OutError = nullptr);
};
