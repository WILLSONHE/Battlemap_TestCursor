#include "BattleVoiceCommandSession.h"
#include "BattleVoiceCommandOverlayWidget.h"
#include "BattleVoiceCommandParser.h"
#include "BattleVoiceCommandExecutor.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "BattleGameInstance.h"

FString UBattleVoiceCommandSession::StripWakePrefix(const FString& Text)
{
	FString Body = Text.TrimStartAndEnd();
	static const FString WakeWord = TEXT("\u547d\u4ee4");
	const int32 WakeIdx = Body.Find(WakeWord, ESearchCase::IgnoreCase);
	if (WakeIdx != INDEX_NONE)
	{
		Body = Body.Mid(WakeIdx + WakeWord.Len()).TrimStart();
	}
	while (Body.Len() > 0)
	{
		const TCHAR C = Body[0];
		if (C == TEXT(':') || C == 0xFF1A || C == TEXT('，') || C == TEXT(',') || C == TEXT(' ') || C == TEXT('、'))
		{
			Body = Body.Mid(1).TrimStart();
			continue;
		}
		break;
	}
	return Body;
}

void UBattleVoiceCommandSession::Initialize(ABattlemap_TestCursorPlayerController* InPC, UBattleVoiceCommandOverlayWidget* InOverlay)
{
	PC = InPC;
	Overlay = InOverlay;
	Executor = NewObject<UBattleVoiceCommandExecutor>(this);
	ResetToIdle();
	TryInitializeRecognizer();
}

void UBattleVoiceCommandSession::TryInitializeRecognizer()
{
	if (UBattleGameInstance* GI = PC ? Cast<UBattleGameInstance>(PC->GetGameInstance()) : nullptr)
	{
		Recognizer.SetPreferredCaptureDevice(
			GI->GetSelectedMicrophoneDeviceId(),
			GI->GetSelectedMicrophoneDisplayName());
	}
	Recognizer.Shutdown();
	if (Recognizer.Initialize())
	{
		BindRecognizer();
	}
}

void UBattleVoiceCommandSession::BindRecognizer()
{
	Recognizer.OnPhrase.Unbind();
	Recognizer.OnPhrase.BindUObject(this, &UBattleVoiceCommandSession::OnSpeechPhrase);
}

void UBattleVoiceCommandSession::ResetToIdle()
{
	State = EVoiceCommandSessionState::Idle;
	CapturedText.Reset();
	LastRawPhrase.Reset();
	SilenceTimer = 0.f;
	ResultDisplayTimer = 0.f;
	Recognizer.Stop();
	if (Overlay)
	{
		Overlay->SetListening(false);
		Overlay->SetOverlayText(FString(), false);
	}
}

void UBattleVoiceCommandSession::UpdateOverlayFromCapture()
{
	if (!Overlay)
	{
		return;
	}
	if (!CapturedText.IsEmpty())
	{
		Overlay->SetOverlayText(FString::Printf(TEXT("\u547d\u4ee4\uff1a%s"), *CapturedText), false);
	}
	else if (!LastRawPhrase.IsEmpty())
	{
		Overlay->SetOverlayText(FString::Printf(TEXT("\u8bc6\u522b\uff1a%s"), *LastRawPhrase), false);
	}
}

void UBattleVoiceCommandSession::OnVoiceKeyPressed()
{
	if (!PC)
	{
		return;
	}
	if (!Recognizer.IsAvailable())
	{
		TryInitializeRecognizer();
	}
	if (!Recognizer.IsAvailable())
	{
		if (Overlay)
		{
			Overlay->SetOverlayText(
				Recognizer.GetLastError().IsEmpty()
					? TEXT("\u8bed\u97f3\u8bc6\u522b\u4e0d\u53ef\u7528\uff08\u8bf7\u5b89\u88c5\u4e2d\u6587\u8bed\u97f3\u8bc6\u522b\u8bed\u8a00\u5305\uff09")
					: Recognizer.GetLastError(),
				true);
		}
		return;
	}

	State = EVoiceCommandSessionState::Capturing;
	CapturedText.Reset();
	LastRawPhrase.Reset();
	SilenceTimer = 0.f;
	Recognizer.Start();
	if (Overlay)
	{
		Overlay->SetListening(true);
		Overlay->SetOverlayText(TEXT("\u6309\u4f4f M \u8bf4\u8bdd\uff0c\u677e\u5f00\u6267\u884c"), false);
	}
}

void UBattleVoiceCommandSession::OnVoiceKeyReleased()
{
	if (State != EVoiceCommandSessionState::Capturing)
	{
		ResetToIdle();
		return;
	}

	for (int32 Flush = 0; Flush < 16; ++Flush)
	{
		Recognizer.PollEvents();
	}

	const FString Body = CapturedText.IsEmpty() ? StripWakePrefix(LastRawPhrase) : CapturedText;
	if (Body.IsEmpty())
	{
		Recognizer.Stop();
		State = EVoiceCommandSessionState::Idle;
		if (Overlay)
		{
			Overlay->SetListening(false);
			Overlay->SetOverlayText(
				TEXT("\u672a\u542c\u5230\u8bed\u97f3\uff08\u8bf7\u68c0\u67e5\u9ea6\u514b\u98ce\u4e0e\u4e2d\u6587\u8bed\u97f3\u8bc6\u522b\u8bed\u8a00\u5305\uff09"),
				true);
		}
		if (PC)
		{
			PC->SetStatusHint(TEXT("\u672a\u542c\u5230\u8bed\u97f3\u3002"));
		}
		ResultDisplayTimer = ResultDisplaySeconds;
		return;
	}

	FinalizeCapture();
}

void UBattleVoiceCommandSession::InjectDebugPhrase(const FString& Text)
{
	// Bypass state machine (tilde/console can leave Capturing in a bad state).
	FinalizeFromText(StripWakePrefix(Text), Text);
}

void UBattleVoiceCommandSession::OnSpeechPhrase(const FString& Text, bool bIsFinal)
{
	ProcessPhrase(Text, bIsFinal);
}

void UBattleVoiceCommandSession::ProcessPhrase(const FString& Text, bool bIsFinal)
{
	if (Text.IsEmpty() || State != EVoiceCommandSessionState::Capturing)
	{
		return;
	}

	LastRawPhrase = Text.TrimStartAndEnd();
	const FString Stripped = StripWakePrefix(LastRawPhrase);
	if (!Stripped.IsEmpty())
	{
		CapturedText = Stripped;
	}

	UpdateOverlayFromCapture();

	if (!CapturedText.IsEmpty())
	{
		SilenceTimer = 0.f;
	}
}

void UBattleVoiceCommandSession::FinalizeFromText(const FString& TextForParse, const FString& HeardRawForErrors)
{
	Recognizer.Stop();

	FBattleVoiceParsedCommand Parsed;
	const bool bParsed = FBattleVoiceCommandParser::Parse(TextForParse, Parsed);
	if (!bParsed)
	{
		if (!HeardRawForErrors.IsEmpty())
		{
			Parsed.DisplayText = FString::Printf(TEXT("\u547d\u4ee4\uff1a\u65e0\u6cd5\u8bc6\u522b\uff08\u542c\u5230\uff1a%s)"), *HeardRawForErrors);
		}
		else if (!TextForParse.IsEmpty())
		{
			Parsed.DisplayText = FString::Printf(TEXT("\u547d\u4ee4\uff1a\u65e0\u6cd5\u8bc6\u522b\uff08\u539f\u6587\uff1a%s)"), *TextForParse);
		}
		else
		{
			Parsed.DisplayText = TEXT("\u547d\u4ee4\uff1a\u65e0\u6cd5\u8bc6\u522b\uff08\u7a7a\u6587\u672c\uff09");
		}
	}

	if (Overlay)
	{
		Overlay->SetListening(false);
		Overlay->SetOverlayText(Parsed.DisplayText, !bParsed);
	}

	if (PC && Executor)
	{
		if (bParsed)
		{
			const FBattleCommandIssueResult Result = Executor->Execute(PC, Parsed);
			PC->SetStatusHint(Result.Message);
			if (Overlay && !Result.Message.IsEmpty())
			{
				Overlay->SetOverlayText(Result.Message, !Result.bSuccess);
			}
		}
		else
		{
			PC->SetStatusHint(Parsed.DisplayText);
		}
	}

	ResultDisplayTimer = ResultDisplaySeconds;
	State = EVoiceCommandSessionState::Idle;
	CapturedText.Reset();
	LastRawPhrase.Reset();
}

void UBattleVoiceCommandSession::FinalizeCapture()
{
	const FString Body = CapturedText.IsEmpty() ? StripWakePrefix(LastRawPhrase) : CapturedText;
	FinalizeFromText(Body, LastRawPhrase.IsEmpty() ? Body : LastRawPhrase);
}

void UBattleVoiceCommandSession::Tick(float DeltaSeconds)
{
	if (State == EVoiceCommandSessionState::Capturing)
	{
		Recognizer.PollEvents();
	}

	// Only auto-finish after speech was captured (avoid empty finalize while holding M in silence).
	if (State == EVoiceCommandSessionState::Capturing && PC && PC->IsInputKeyDown(EKeys::M) && !CapturedText.IsEmpty())
	{
		SilenceTimer += DeltaSeconds;
		if (SilenceTimer >= SilenceFinalizeSeconds)
		{
			FinalizeCapture();
			return;
		}
	}

	if (ResultDisplayTimer > 0.f)
	{
		ResultDisplayTimer = FMath::Max(0.f, ResultDisplayTimer - DeltaSeconds);
		if (ResultDisplayTimer <= 0.f && Overlay)
		{
			Overlay->SetOverlayText(FString(), false);
		}
	}
}
