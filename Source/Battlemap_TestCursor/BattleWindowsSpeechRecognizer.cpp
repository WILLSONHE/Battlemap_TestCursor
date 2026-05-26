#include "BattleWindowsSpeechRecognizer.h"
#include "Async/Async.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <sapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

FBattleWindowsSpeechRecognizer::FBattleWindowsSpeechRecognizer() = default;

FBattleWindowsSpeechRecognizer::~FBattleWindowsSpeechRecognizer()
{
	Shutdown();
}

void FBattleWindowsSpeechRecognizer::SetPreferredCaptureDevice(const FString& DeviceId, const FString& DeviceDisplayName)
{
	PreferredCaptureDeviceId = DeviceId;
	PreferredCaptureDeviceDisplayName = DeviceDisplayName;
}

#if PLATFORM_WINDOWS
namespace
{
	FString HrMessage(const TCHAR* Context, const HRESULT Hr)
	{
		return FString::Printf(TEXT("%s (0x%08X)"), Context, static_cast<uint32>(Hr));
	}

	bool CreateSpeechRecognizer(ISpRecognizer** OutRecognizer, FString& OutError)
	{
		*OutRecognizer = nullptr;
		const CLSID RecognizerClsids[] = { CLSID_SpInprocRecognizer, CLSID_SpSharedRecognizer };
		for (const CLSID& Clsid : RecognizerClsids)
		{
			ISpRecognizer* Candidate = nullptr;
			const DWORD ClsCtx = (Clsid == CLSID_SpSharedRecognizer) ? CLSCTX_ALL : CLSCTX_INPROC_SERVER;
			const HRESULT Hr = CoCreateInstance(Clsid, nullptr, ClsCtx, IID_ISpRecognizer, reinterpret_cast<void**>(&Candidate));
			if (SUCCEEDED(Hr) && Candidate)
			{
				*OutRecognizer = Candidate;
				return true;
			}
		}
		OutError = TEXT("Speech recognizer unavailable. Install Chinese (Simplified) speech recognition in Windows Settings.");
		return false;
	}
}
#endif

bool FBattleWindowsSpeechRecognizer::Initialize()
{
#if PLATFORM_WINDOWS
	if (bAvailable)
	{
		return true;
	}

	Shutdown();

	const HRESULT CoHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(CoHr) && CoHr != RPC_E_CHANGED_MODE && CoHr != S_FALSE)
	{
		LastError = HrMessage(TEXT("COM init failed for speech"), CoHr);
		return false;
	}

	ISpRecognizer* SpRecognizer = nullptr;
	if (!CreateSpeechRecognizer(&SpRecognizer, LastError))
	{
		return false;
	}

	Recognizer = SpRecognizer;
	ApplyChineseRecognizer();
	if (!ApplyCaptureInputToRecognizer())
	{
		// Continue with default input if explicit device binding fails.
		SpRecognizer->SetInput(nullptr, static_cast<BOOL>(true));
	}

	ISpRecoContext* SpContext = nullptr;
	HRESULT Hr = SpRecognizer->CreateRecoContext(&SpContext);
	if (FAILED(Hr) || !SpContext)
	{
		SpRecognizer->Release();
		Recognizer = nullptr;
		LastError = HrMessage(TEXT("CreateRecoContext failed"), Hr);
		return false;
	}

	const ULONGLONG Interest = SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS);
	SpContext->SetInterest(Interest, Interest);

	RecoContext = SpContext;
	RecoGrammar = nullptr;
	bAvailable = true;
	LastError.Reset();
	return true;
#else
	LastError = TEXT("Speech recognition only supported on Windows.");
	return false;
#endif
}

#if PLATFORM_WINDOWS
namespace
{
	FString GetSpObjectTokenLabel(ISpObjectToken* Token)
	{
		if (!Token)
		{
			return FString();
		}
		LPWSTR Label = nullptr;
		if (SUCCEEDED(Token->GetStringValue(nullptr, &Label)) && Label)
		{
			const FString Out(Label);
			CoTaskMemFree(Label);
			return Out;
		}
		return FString();
	}

	bool TokenMatchesPreferredMic(ISpObjectToken* Token, const FString& WantedId, const FString& WantedDisplayName)
	{
		if (!Token)
		{
			return false;
		}
		LPWSTR TokenId = nullptr;
		if (SUCCEEDED(Token->GetId(&TokenId)) && TokenId)
		{
			const FString IdStr(TokenId);
			CoTaskMemFree(TokenId);
			if (!WantedId.IsEmpty() && IdStr.Equals(WantedId, ESearchCase::IgnoreCase))
			{
				return true;
			}
			if (!WantedId.IsEmpty() && IdStr.Contains(WantedId, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		if (!WantedDisplayName.IsEmpty())
		{
			const FString Label = GetSpObjectTokenLabel(Token);
			if (Label.Equals(WantedDisplayName, ESearchCase::IgnoreCase)
				|| Label.Contains(WantedDisplayName, ESearchCase::IgnoreCase)
				|| WantedDisplayName.Contains(Label, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}

bool FBattleWindowsSpeechRecognizer::ApplyChineseRecognizer()
{
	ISpRecognizer* SpRecognizer = reinterpret_cast<ISpRecognizer*>(Recognizer);
	if (!SpRecognizer)
	{
		return false;
	}

	ISpObjectTokenCategory* Category = nullptr;
	HRESULT Hr = CoCreateInstance(
		CLSID_SpObjectTokenCategory, nullptr, CLSCTX_INPROC_SERVER, IID_ISpObjectTokenCategory, reinterpret_cast<void**>(&Category));
	if (FAILED(Hr) || !Category)
	{
		return false;
	}

	Hr = Category->SetId(SPCAT_RECOGNIZERS, static_cast<BOOL>(true));
	if (FAILED(Hr))
	{
		Category->Release();
		return false;
	}

	IEnumSpObjectTokens* EnumTokens = nullptr;
	Hr = Category->EnumTokens(L"Language=804", nullptr, &EnumTokens);
	if (FAILED(Hr) || !EnumTokens)
	{
		Hr = Category->EnumTokens(nullptr, nullptr, &EnumTokens);
	}
	Category->Release();
	if (FAILED(Hr) || !EnumTokens)
	{
		return false;
	}

	ISpObjectToken* ChineseToken = nullptr;
	if (EnumTokens->Next(1, &ChineseToken, nullptr) != S_OK || !ChineseToken)
	{
		EnumTokens->Release();
		return false;
	}
	EnumTokens->Release();

	Hr = SpRecognizer->SetRecognizer(ChineseToken);
	ChineseToken->Release();
	return SUCCEEDED(Hr);
}

bool FBattleWindowsSpeechRecognizer::ApplyCaptureInputToRecognizer()
{
	ISpRecognizer* SpRecognizer = reinterpret_cast<ISpRecognizer*>(Recognizer);
	if (!SpRecognizer)
	{
		return false;
	}

	if (PreferredCaptureDeviceId.IsEmpty() && PreferredCaptureDeviceDisplayName.IsEmpty())
	{
		const HRESULT Hr = SpRecognizer->SetInput(nullptr, static_cast<BOOL>(true));
		return SUCCEEDED(Hr);
	}

	ISpObjectTokenCategory* Category = nullptr;
	HRESULT Hr = CoCreateInstance(
		CLSID_SpObjectTokenCategory, nullptr, CLSCTX_INPROC_SERVER, IID_ISpObjectTokenCategory, reinterpret_cast<void**>(&Category));
	if (FAILED(Hr) || !Category)
	{
		return false;
	}

	Hr = Category->SetId(SPCAT_AUDIOIN, static_cast<BOOL>(true));
	if (FAILED(Hr))
	{
		Category->Release();
		return false;
	}

	IEnumSpObjectTokens* EnumTokens = nullptr;
	Hr = Category->EnumTokens(nullptr, nullptr, &EnumTokens);
	Category->Release();
	if (FAILED(Hr) || !EnumTokens)
	{
		return false;
	}

	const FString WantedId = PreferredCaptureDeviceId;
	const FString WantedName = PreferredCaptureDeviceDisplayName;
	ISpObjectToken* MatchedToken = nullptr;
	while (true)
	{
		ISpObjectToken* Token = nullptr;
		if (EnumTokens->Next(1, &Token, nullptr) != S_OK || !Token)
		{
			break;
		}

		if (TokenMatchesPreferredMic(Token, WantedId, WantedName))
		{
			MatchedToken = Token;
			break;
		}
		Token->Release();
	}
	EnumTokens->Release();

	if (!MatchedToken)
	{
		return false;
	}

	Hr = SpRecognizer->SetInput(MatchedToken, static_cast<BOOL>(true));
	MatchedToken->Release();
	return SUCCEEDED(Hr);
}
#endif

void FBattleWindowsSpeechRecognizer::Shutdown()
{
	Stop();
#if PLATFORM_WINDOWS
	if (RecoGrammar)
	{
		reinterpret_cast<ISpRecoGrammar*>(RecoGrammar)->Release();
		RecoGrammar = nullptr;
	}
	if (RecoContext)
	{
		reinterpret_cast<ISpRecoContext*>(RecoContext)->Release();
		RecoContext = nullptr;
	}
	if (Recognizer)
	{
		reinterpret_cast<ISpRecognizer*>(Recognizer)->Release();
		Recognizer = nullptr;
	}
#endif
	bAvailable = false;
}

bool FBattleWindowsSpeechRecognizer::Start()
{
#if PLATFORM_WINDOWS
	if (!bAvailable || bRunning)
	{
		return bAvailable;
	}
	ISpRecognizer* SpRecognizer = reinterpret_cast<ISpRecognizer*>(Recognizer);
	ISpRecoContext* SpContext = reinterpret_cast<ISpRecoContext*>(RecoContext);
	if (!SpRecognizer || !SpContext)
	{
		return false;
	}

	ISpRecoGrammar* Grammar = reinterpret_cast<ISpRecoGrammar*>(RecoGrammar);
	if (!Grammar)
	{
		if (FAILED(SpContext->CreateGrammar(0, &Grammar)) || !Grammar)
		{
			LastError = TEXT("CreateGrammar failed.");
			return false;
		}
		RecoGrammar = Grammar;
		if (FAILED(Grammar->LoadDictation(nullptr, SPLO_STATIC)))
		{
			LastError = TEXT("LoadDictation failed.");
			return false;
		}
	}

	Grammar->SetDictationState(SPRS_ACTIVE);
	SpRecognizer->SetRecoState(SPRST_ACTIVE);
	bRunning = true;
	return true;
#else
	return false;
#endif
}

void FBattleWindowsSpeechRecognizer::Stop()
{
#if PLATFORM_WINDOWS
	if (!bRunning)
	{
		return;
	}
	if (ISpRecoGrammar* Grammar = reinterpret_cast<ISpRecoGrammar*>(RecoGrammar))
	{
		Grammar->SetDictationState(SPRS_INACTIVE);
	}
	if (ISpRecognizer* SpRecognizer = reinterpret_cast<ISpRecognizer*>(Recognizer))
	{
		SpRecognizer->SetRecoState(SPRST_INACTIVE);
	}
	bRunning = false;
#endif
}

void FBattleWindowsSpeechRecognizer::HandleSpeechEvent(uintptr_t EventId, uintptr_t Param)
{
#if PLATFORM_WINDOWS
	const SPEVENTENUM Eid = static_cast<SPEVENTENUM>(EventId);
	if (Eid != SPEI_RECOGNITION && Eid != SPEI_HYPOTHESIS)
	{
		return;
	}

	WCHAR* Text = nullptr;
	HRESULT TextHr = E_FAIL;
	if (Eid == SPEI_RECOGNITION)
	{
		ISpRecoResult* Result = reinterpret_cast<ISpRecoResult*>(Param);
		if (!Result)
		{
			return;
		}
		TextHr = Result->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, static_cast<BOOL>(true), &Text, nullptr);
	}
	else
	{
		ISpPhrase* Phrase = reinterpret_cast<ISpPhrase*>(Param);
		if (!Phrase)
		{
			return;
		}
		TextHr = Phrase->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, static_cast<BOOL>(true), &Text, nullptr);
	}

	if (FAILED(TextHr) || !Text)
	{
		return;
	}

	const FString PhraseText(Text);
	CoTaskMemFree(Text);
	const bool bIsFinal = (Eid == SPEI_RECOGNITION);

	if (OnPhrase.IsBound())
	{
		FBattleSpeechPhraseDelegate Delegate = OnPhrase;
		AsyncTask(ENamedThreads::GameThread, [Delegate, PhraseText, bIsFinal]()
		{
			Delegate.ExecuteIfBound(PhraseText, bIsFinal);
		});
	}
#endif
}

void FBattleWindowsSpeechRecognizer::PollEvents()
{
#if PLATFORM_WINDOWS
	if (!bRunning || !RecoContext)
	{
		return;
	}
	ISpRecoContext* SpContext = reinterpret_cast<ISpRecoContext*>(RecoContext);
	SPEVENT Events[8];
	ULONG Fetched = 0;
	while (SUCCEEDED(SpContext->GetEvents(8, Events, &Fetched)) && Fetched > 0)
	{
		for (ULONG i = 0; i < Fetched; ++i)
		{
			HandleSpeechEvent(static_cast<uintptr_t>(Events[i].eEventId), static_cast<uintptr_t>(Events[i].lParam));
		}
		Fetched = 0;
	}
#endif
}
