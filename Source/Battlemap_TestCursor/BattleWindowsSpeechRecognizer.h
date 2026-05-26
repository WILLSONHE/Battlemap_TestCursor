#pragma once

#include "CoreMinimal.h"

DECLARE_DELEGATE_TwoParams(FBattleSpeechPhraseDelegate, const FString& /*Text*/, bool /*bIsFinal*/);

/** Windows SAPI dictation wrapper (Win64 only). */
class BATTLEMAP_TESTCURSOR_API FBattleWindowsSpeechRecognizer
{
public:
	FBattleWindowsSpeechRecognizer();
	~FBattleWindowsSpeechRecognizer();

	void SetPreferredCaptureDevice(const FString& DeviceId, const FString& DeviceDisplayName = FString());
	bool Initialize();
	void Shutdown();
	bool IsAvailable() const { return bAvailable; }
	const FString& GetLastError() const { return LastError; }

	bool Start();
	void Stop();
	void PollEvents();

	FBattleSpeechPhraseDelegate OnPhrase;

private:
#if PLATFORM_WINDOWS
	bool ApplyCaptureInputToRecognizer();
	bool ApplyChineseRecognizer();
	void HandleSpeechEvent(uintptr_t EventId, uintptr_t Param);
	void* Recognizer = nullptr;
	void* RecoContext = nullptr;
	void* RecoGrammar = nullptr;
#endif

	bool bAvailable = false;
	bool bRunning = false;
	FString LastError;
	FString PreferredCaptureDeviceId;
	FString PreferredCaptureDeviceDisplayName;
};
