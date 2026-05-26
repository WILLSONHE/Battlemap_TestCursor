#include "BattleVoiceCommandParser.h"

namespace
{
	FString TrimCommandWhitespace(const FString& S)
	{
		return S.TrimStartAndEnd();
	}

	void StripLeadingPunctuation(FString& S)
	{
		while (S.Len() > 0)
		{
			const TCHAR C = S[0];
			if (C == TEXT(':') || C == 0xFF1A || C == TEXT('，') || C == TEXT(',') || C == TEXT(' ') || C == TEXT('、'))
			{
				S = S.Mid(1).TrimStart();
				continue;
			}
			break;
		}
	}

	void NormalizeChineseNumeralsInPlace(FString& S)
	{
		S.ReplaceInline(TEXT("\u7396"), TEXT("1"));
		S.ReplaceInline(TEXT("\u4e00"), TEXT("1"));
		S.ReplaceInline(TEXT("\u4e8c"), TEXT("2"));
		S.ReplaceInline(TEXT("\u4e09"), TEXT("3"));
		S.ReplaceInline(TEXT("\u56db"), TEXT("4"));
		S.ReplaceInline(TEXT("\u4e94"), TEXT("5"));
		S.ReplaceInline(TEXT("\u516d"), TEXT("6"));
		S.ReplaceInline(TEXT("\u4e03"), TEXT("7"));
		S.ReplaceInline(TEXT("\u516b"), TEXT("8"));
		S.ReplaceInline(TEXT("\u4e5d"), TEXT("9"));
	}

	int32 FindVerbIndex(const FString& Body, const TArray<FString>& Verbs)
	{
		int32 BestIdx = INDEX_NONE;
		for (const FString& Verb : Verbs)
		{
			const int32 Idx = Body.Find(Verb, ESearchCase::IgnoreCase);
			if (Idx != INDEX_NONE && (BestIdx == INDEX_NONE || Idx < BestIdx))
			{
				BestIdx = Idx;
			}
		}
		return BestIdx;
	}
}

FString FBattleVoiceCommandParser::NormalizeCommandBody(const FString& Raw)
{
	FString Body = TrimCommandWhitespace(Raw);
	static const FString WakeWord = TEXT("\u547d\u4ee4");
	const int32 WakeIdx = Body.Find(WakeWord, ESearchCase::IgnoreCase);
	if (WakeIdx != INDEX_NONE)
	{
		Body = Body.Mid(WakeIdx + WakeWord.Len());
	}
	StripLeadingPunctuation(Body);
	Body.ReplaceInline(TEXT(" "), TEXT(""));
	NormalizeChineseNumeralsInPlace(Body);
	return TrimCommandWhitespace(Body);
}

bool FBattleVoiceCommandParser::ExtractGridCoords(const FString& Tail, FIntPoint& OutCell)
{
	FString Work = Tail;
	Work.ReplaceInline(TEXT("\u5750\u6807"), TEXT(""));
	Work.ReplaceInline(TEXT("\uff0c"), TEXT(","));
	Work.ReplaceInline(TEXT("，"), TEXT(","));
	Work.ReplaceInline(TEXT(" "), TEXT(","));

	TArray<int32> Numbers;
	for (int32 i = 0; i < Work.Len();)
	{
		if (FChar::IsDigit(Work[i]) || (Work[i] == TEXT('-') && i + 1 < Work.Len() && FChar::IsDigit(Work[i + 1])))
		{
			int32 j = i + 1;
			while (j < Work.Len() && FChar::IsDigit(Work[j]))
			{
				++j;
			}
			Numbers.Add(FCString::Atoi(*Work.Mid(i, j - i)));
			i = j;
		}
		else
		{
			++i;
		}
	}

	if (Numbers.Num() >= 2)
	{
		OutCell.X = Numbers[0];
		OutCell.Y = Numbers[1];
		return true;
	}

	TArray<FString> Parts;
	Work.ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() >= 2)
	{
		OutCell.X = FCString::Atoi(*Parts[0]);
		OutCell.Y = FCString::Atoi(*Parts[1]);
		return true;
	}
	return false;
}

bool FBattleVoiceCommandParser::TryParseMove(const FString& Body, FString& OutUnitPhrase, FIntPoint& OutCell, FString& OutDisplay)
{
	static const TArray<FString> Verbs = {
		TEXT("\u79fb\u52a8\u81f3\u5750\u6807"),
		TEXT("\u79fb\u52a8\u5230\u5750\u6807"),
		TEXT("\u79fb\u52a8\u5230"),
		TEXT("\u79fb\u52a8\u81f3"),
		TEXT("\u5f00\u5f80")
	};
	const int32 VerbIdx = FindVerbIndex(Body, Verbs);
	if (VerbIdx == INDEX_NONE)
	{
		return false;
	}
	FString Verb;
	for (const FString& V : Verbs)
	{
		if (Body.Mid(VerbIdx, V.Len()).Equals(V, ESearchCase::IgnoreCase))
		{
			Verb = V;
			break;
		}
	}
	if (Verb.IsEmpty())
	{
		return false;
	}
	OutUnitPhrase = TrimCommandWhitespace(Body.Left(VerbIdx));
	NormalizeChineseNumeralsInPlace(OutUnitPhrase);
	const FString Tail = Body.Mid(VerbIdx + Verb.Len());
	if (!ExtractGridCoords(Tail, OutCell))
	{
		return false;
	}
	OutDisplay = FString::Printf(TEXT("\u547d\u4ee4\uff1a%s\u79fb\u52a8\u5230%d\uff0c%d"), *OutUnitPhrase, OutCell.X, OutCell.Y);
	return !OutUnitPhrase.IsEmpty();
}

bool FBattleVoiceCommandParser::TryParseStop(const FString& Body, FString& OutUnitPhrase, FString& OutDisplay)
{
	static const TArray<FString> Verbs = { TEXT("\u505c\u6b62\u79fb\u52a8"), TEXT("\u505c\u6b62"), TEXT("\u505c\u4e0b") };
	const int32 VerbIdx = FindVerbIndex(Body, Verbs);
	if (VerbIdx == INDEX_NONE)
	{
		return false;
	}
	FString Verb;
	for (const FString& V : Verbs)
	{
		if (Body.Mid(VerbIdx, V.Len()).Equals(V, ESearchCase::IgnoreCase))
		{
			Verb = V;
			break;
		}
	}
	if (Verb.IsEmpty())
	{
		return false;
	}
	OutUnitPhrase = TrimCommandWhitespace(Body.Left(VerbIdx));
	NormalizeChineseNumeralsInPlace(OutUnitPhrase);
	if (OutUnitPhrase.IsEmpty())
	{
		return false;
	}
	OutDisplay = FString::Printf(TEXT("\u547d\u4ee4\uff1a%s\u505c\u6b62\u79fb\u52a8"), *OutUnitPhrase);
	return true;
}

bool FBattleVoiceCommandParser::TryParseAttack(const FString& Body, FString& OutUnitPhrase, FIntPoint& OutCell, FString& OutDisplay)
{
	static const TArray<FString> Verbs = { TEXT("\u653b\u51fb"), TEXT("\u6253\u51fb") };
	const int32 VerbIdx = FindVerbIndex(Body, Verbs);
	if (VerbIdx == INDEX_NONE)
	{
		return false;
	}
	FString Verb;
	for (const FString& V : Verbs)
	{
		if (Body.Mid(VerbIdx, V.Len()).Equals(V, ESearchCase::IgnoreCase))
		{
			Verb = V;
			break;
		}
	}
	if (Verb.IsEmpty())
	{
		return false;
	}
	OutUnitPhrase = TrimCommandWhitespace(Body.Left(VerbIdx));
	NormalizeChineseNumeralsInPlace(OutUnitPhrase);
	if (OutUnitPhrase.IsEmpty())
	{
		return false;
	}
	FString Tail = Body.Mid(VerbIdx + Verb.Len());
	const int32 LocIdx = Tail.Find(TEXT("\u4f4d\u4e8e"), ESearchCase::IgnoreCase);
	if (LocIdx != INDEX_NONE)
	{
		Tail = Tail.Mid(LocIdx + 2);
	}
	if (!ExtractGridCoords(Tail, OutCell))
	{
		return false;
	}
	OutDisplay = FString::Printf(TEXT("\u547d\u4ee4\uff1a%s\u653b\u51fb\u4f4d\u4e8e%d\uff0c%d\u7684\u654c\u65b9\u5355\u4f4d"), *OutUnitPhrase, OutCell.X, OutCell.Y);
	return true;
}

bool FBattleVoiceCommandParser::Parse(const FString& CapturedText, FBattleVoiceParsedCommand& OutCommand)
{
	OutCommand = FBattleVoiceParsedCommand();
	OutCommand.RawText = CapturedText;
	const FString Body = NormalizeCommandBody(CapturedText);
	if (Body.IsEmpty())
	{
		OutCommand.DisplayText = FString::Printf(TEXT("\u547d\u4ee4\uff1a\u65e0\u6cd5\u8bc6\u522b\uff08\u539f\u6587\uff1a%s)"), *CapturedText);
		return false;
	}

	FString UnitPhrase;
	FIntPoint Cell;
	FString Display;
	if (TryParseMove(Body, UnitPhrase, Cell, Display))
	{
		OutCommand.Kind = EVoiceCommandKind::Move;
		OutCommand.UnitPhrase = UnitPhrase;
		OutCommand.GridCell = Cell;
		OutCommand.DisplayText = Display;
		return true;
	}
	if (TryParseStop(Body, UnitPhrase, Display))
	{
		OutCommand.Kind = EVoiceCommandKind::Stop;
		OutCommand.UnitPhrase = UnitPhrase;
		OutCommand.DisplayText = Display;
		return true;
	}
	if (TryParseAttack(Body, UnitPhrase, Cell, Display))
	{
		OutCommand.Kind = EVoiceCommandKind::Attack;
		OutCommand.UnitPhrase = UnitPhrase;
		OutCommand.GridCell = Cell;
		OutCommand.DisplayText = Display;
		return true;
	}

	OutCommand.DisplayText = FString::Printf(TEXT("\u547d\u4ee4\uff1a\u65e0\u6cd5\u8bc6\u522b\uff08\u539f\u6587\uff1a%s)"), *CapturedText);
	return false;
}
