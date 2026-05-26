#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleVoiceCommandTypes.h"
#include "BattleWindowsSpeechRecognizer.h"
#include "BattleVoiceCommandSession.generated.h"

class ABattlemap_TestCursorPlayerController;
class UBattleVoiceCommandOverlayWidget;
class UBattleVoiceCommandExecutor;

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleVoiceCommandSession : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABattlemap_TestCursorPlayerController* InPC, UBattleVoiceCommandOverlayWidget* InOverlay);
	void Tick(float DeltaSeconds);

	void OnVoiceKeyPressed();
	void OnVoiceKeyReleased();
	void InjectDebugPhrase(const FString& Text);

	EVoiceCommandSessionState GetState() const { return State; }

private:
	void TryInitializeRecognizer();
	void BindRecognizer();
	void OnSpeechPhrase(const FString& Text, bool bIsFinal);
	void ProcessPhrase(const FString& Text, bool bIsFinal);
	void FinalizeFromText(const FString& TextForParse, const FString& HeardRawForErrors);
	void FinalizeCapture();
	void ResetToIdle();
	void UpdateOverlayFromCapture();

	static FString StripWakePrefix(const FString& Text);

	UPROPERTY()
	TObjectPtr<ABattlemap_TestCursorPlayerController> PC;

	UPROPERTY()
	TObjectPtr<UBattleVoiceCommandOverlayWidget> Overlay;

	UPROPERTY()
	TObjectPtr<UBattleVoiceCommandExecutor> Executor;

	EVoiceCommandSessionState State = EVoiceCommandSessionState::Idle;
	FBattleWindowsSpeechRecognizer Recognizer;
	FString CapturedText;
	FString LastRawPhrase;
	float SilenceTimer = 0.f;
	float ResultDisplayTimer = 0.f;
	static constexpr float SilenceFinalizeSeconds = 1.0f;
	static constexpr float ResultDisplaySeconds = 2.5f;
};
