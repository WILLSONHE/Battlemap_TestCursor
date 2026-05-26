#include "BattleVoiceCommandExecutor.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "Battlemap_TestCursorGameMode.h"
#include "BattleLoadoutScreenWidget.h"
#include "BattleUnit.h"
#include "TacticalMapGrid.h"
#include "EngineUtils.h"

void UBattleVoiceCommandExecutor::ResolveFriendlyUnits(
	ABattlemap_TestCursorGameMode* GM,
	const FString& UnitPhrase,
	TArray<ABattleUnit*>& OutUnits) const
{
	OutUnits.Reset();
	if (!GM || UnitPhrase.IsEmpty())
	{
		return;
	}

	FString TrimmedPhrase = UnitPhrase.TrimStartAndEnd();
	TrimmedPhrase.ReplaceInline(TEXT("\u4e00"), TEXT("1"));
	TrimmedPhrase.ReplaceInline(TEXT("\u4e8c"), TEXT("2"));
	TrimmedPhrase.ReplaceInline(TEXT("\u4e09"), TEXT("3"));
	TrimmedPhrase.ReplaceInline(TEXT("\u56db"), TEXT("4"));
	TrimmedPhrase.ReplaceInline(TEXT("\u4e94"), TEXT("5"));
	TrimmedPhrase.ReplaceInline(TEXT("\u516d"), TEXT("6"));
	TrimmedPhrase.ReplaceInline(TEXT("\u4e03"), TEXT("7"));
	TrimmedPhrase.ReplaceInline(TEXT("\u516b"), TEXT("8"));
	TrimmedPhrase.ReplaceInline(TEXT("\u4e5d"), TEXT("9"));
	TrimmedPhrase.ReplaceInline(TEXT("\u7396"), TEXT("1"));
	const TArray<FPlayerLoadoutSlot>& Slots = GM->GetCachedBattleLoadoutSlots();
	TSet<ABattleUnit*> UniqueUnits;

	auto AddUnit = [&](ABattleUnit* Unit)
	{
		if (Unit && Unit->bFriendly && Unit->IsAlive() && !UniqueUnits.Contains(Unit))
		{
			UniqueUnits.Add(Unit);
			OutUnits.Add(Unit);
		}
	};

	int32 BestSlot = INDEX_NONE;
	bool bBestExact = false;
	int32 BestLabelLen = 0;
	for (int32 Si = 0; Si < Slots.Num(); ++Si)
	{
		const FString Lab = UBattleLoadoutScreenWidget::ComputeBattleSequenceLabel(Si, Slots);
		const bool bExact = Lab.Equals(TrimmedPhrase, ESearchCase::IgnoreCase);
		const bool bPhrasePrefixesLabel = TrimmedPhrase.StartsWith(Lab, ESearchCase::IgnoreCase);
		const bool bLabelPrefixesPhrase = Lab.StartsWith(TrimmedPhrase, ESearchCase::IgnoreCase);
		if (!bExact && !bPhrasePrefixesLabel && !bLabelPrefixesPhrase)
		{
			continue;
		}
		if (bExact)
		{
			if (!bBestExact || Lab.Len() < BestLabelLen)
			{
				bBestExact = true;
				BestLabelLen = Lab.Len();
				BestSlot = Si;
			}
		}
		else if (!bBestExact)
		{
			if (bLabelPrefixesPhrase)
			{
				// e.g. phrase "1\u8425" should bind to slot "1\u8425", not "1\u84251\u8fde".
				if (BestSlot == INDEX_NONE || Lab.Len() < BestLabelLen)
				{
					BestLabelLen = Lab.Len();
					BestSlot = Si;
				}
			}
			else if (bPhrasePrefixesLabel)
			{
				if (Lab.Len() > BestLabelLen)
				{
					BestLabelLen = Lab.Len();
					BestSlot = Si;
				}
			}
		}
	}

	UWorld* World = GM->GetWorld();
	if (!World)
	{
		return;
	}

	if (BestSlot != INDEX_NONE)
	{
		TArray<int32> SquadSlots;
		UBattleLoadoutScreenWidget::CollectEligibleBattleSquadsUnder(BestSlot, Slots, SquadSlots);
		for (TActorIterator<ABattleUnit> It(World); It; ++It)
		{
			ABattleUnit* Unit = *It;
			if (!Unit)
			{
				continue;
			}
			const int32 UnitSlot = Unit->SourceLoadoutSlotIndex;
			if (UnitSlot != INDEX_NONE
				&& (UnitSlot == BestSlot
					|| SquadSlots.Contains(UnitSlot)
					|| UBattleLoadoutScreenWidget::IsOrbatSlotStrictlyUnderParent(BestSlot, UnitSlot, Slots)))
			{
				AddUnit(Unit);
			}
		}
	}

	if (OutUnits.Num() > 0)
	{
		return;
	}

	int32 AnchorLabelLen = 0;
	bool bAnchorExact = false;
	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* Unit = *It;
		if (!Unit || !Unit->bFriendly || !Unit->IsAlive())
		{
			continue;
		}
		const FString& Lab = Unit->UnitLabel;
		const bool bExact = Lab.Equals(TrimmedPhrase, ESearchCase::IgnoreCase);
		const bool bPhrasePrefixesLabel = TrimmedPhrase.StartsWith(Lab, ESearchCase::IgnoreCase);
		const bool bLabelPrefixesPhrase = Lab.StartsWith(TrimmedPhrase, ESearchCase::IgnoreCase);
		if (!bExact && !bPhrasePrefixesLabel && !bLabelPrefixesPhrase)
		{
			continue;
		}
		if (bExact)
		{
			if (!bAnchorExact || Lab.Len() < AnchorLabelLen)
			{
				bAnchorExact = true;
				AnchorLabelLen = Lab.Len();
			}
		}
		else if (!bAnchorExact)
		{
			if (bLabelPrefixesPhrase && (AnchorLabelLen == 0 || Lab.Len() < AnchorLabelLen))
			{
				AnchorLabelLen = Lab.Len();
			}
			else if (bPhrasePrefixesLabel && Lab.Len() > AnchorLabelLen)
			{
				AnchorLabelLen = Lab.Len();
			}
		}
	}

	if (AnchorLabelLen == 0)
	{
		return;
	}

	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* Unit = *It;
		if (!Unit || !Unit->bFriendly || !Unit->IsAlive())
		{
			continue;
		}
		const FString& Lab = Unit->UnitLabel;
		const bool bExact = Lab.Equals(TrimmedPhrase, ESearchCase::IgnoreCase);
		const bool bPhrasePrefixesLabel = TrimmedPhrase.StartsWith(Lab, ESearchCase::IgnoreCase);
		const bool bLabelPrefixesPhrase = Lab.StartsWith(TrimmedPhrase, ESearchCase::IgnoreCase);
		if (bAnchorExact)
		{
			if (!bExact)
			{
				continue;
			}
		}
		else if (bLabelPrefixesPhrase)
		{
			if (Lab.Len() != AnchorLabelLen)
			{
				continue;
			}
		}
		else if (bPhrasePrefixesLabel)
		{
			if (Lab.Len() != AnchorLabelLen)
			{
				continue;
			}
		}
		else
		{
			continue;
		}
		AddUnit(Unit);
	}
}

ABattleUnit* UBattleVoiceCommandExecutor::FindNearestEnemyAtGrid(ATacticalMapGrid* Grid, const FIntPoint& Cell) const
{
	if (!Grid)
	{
		return nullptr;
	}
	const FVector Goal = GridCellToWorldLocation(Grid, Cell);
	ABattleUnit* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	UWorld* World = Grid->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* Unit = *It;
		if (!Unit || Unit->bFriendly || !Unit->IsAlive())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Unit->GetActorLocation(), Goal);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Unit;
		}
	}
	return Best;
}

FVector UBattleVoiceCommandExecutor::GridCellToWorldLocation(ATacticalMapGrid* Grid, const FIntPoint& Cell) const
{
	if (!Grid)
	{
		return FVector::ZeroVector;
	}
	FVector Loc = Grid->CellToWorldCenter(Cell);
	Loc.Z = Grid->GetHeightAtWorldXY(Loc.X, Loc.Y) + 80.f;
	return Loc;
}

FBattleCommandIssueResult UBattleVoiceCommandExecutor::Execute(
	ABattlemap_TestCursorPlayerController* PC,
	const FBattleVoiceParsedCommand& Command)
{
	FBattleCommandIssueResult Result;
	if (!PC)
	{
		Result.Message = TEXT("\u8bed\u97f3\u547d\u4ee4\u5931\u8d25\uff1a\u65e0 PlayerController\u3002");
		return Result;
	}

	ABattlemap_TestCursorGameMode* GM = PC->GetWorld()
		? Cast<ABattlemap_TestCursorGameMode>(PC->GetWorld()->GetAuthGameMode())
		: nullptr;
	if (!GM)
	{
		Result.Message = TEXT("\u8bed\u97f3\u547d\u4ee4\u5931\u8d25\uff1a\u975e\u6218\u6597\u5173\u5361\u3002");
		return Result;
	}

	TArray<ABattleUnit*> Units;
	ResolveFriendlyUnits(GM, Command.UnitPhrase, Units);
	if (Units.Num() == 0)
	{
		Result.Message = FString::Printf(TEXT("\u8bed\u97f3\u547d\u4ee4\u5931\u8d25\uff1a\u672a\u627e\u5230\u5355\u4f4d \"%s\"\u3002"), *Command.UnitPhrase);
		return Result;
	}

	ATacticalMapGrid* Grid = PC->GetTacticalMapGrid();
	switch (Command.Kind)
	{
	case EVoiceCommandKind::Move:
	{
		if (!Grid)
		{
			Result.Message = TEXT("\u8bed\u97f3\u547d\u4ee4\u5931\u8d25\uff1a\u65e0\u5730\u5f62\u7f51\u683c\u3002");
			return Result;
		}
		const FVector Dest = GridCellToWorldLocation(Grid, Command.GridCell);
		Result = PC->IssueMoveCommandToUnits(Units, Dest);
		break;
	}
	case EVoiceCommandKind::Stop:
		Result = PC->IssueStopCommandToUnits(Units);
		break;
	case EVoiceCommandKind::Attack:
	{
		if (!Grid)
		{
			Result.Message = TEXT("\u8bed\u97f3\u547d\u4ee4\u5931\u8d25\uff1a\u65e0\u5730\u5f62\u7f51\u683c\u3002");
			return Result;
		}
		ABattleUnit* Enemy = FindNearestEnemyAtGrid(Grid, Command.GridCell);
		if (!Enemy)
		{
			Result.Message = FString::Printf(
				TEXT("\u8bed\u97f3\u547d\u4ee4\u5931\u8d25\uff1a\u683c(%d,%d)\u9644\u8fd1\u65e0\u654c\u65b9\u5355\u4f4d\u3002"),
				Command.GridCell.X, Command.GridCell.Y);
			return Result;
		}
		Result = PC->IssueAttackCommandToUnits(Units, Enemy);
		break;
	}
	default:
		Result.Message = Command.DisplayText.IsEmpty()
			? TEXT("\u8bed\u97f3\u547d\u4ee4\u65e0\u6cd5\u8bc6\u522b\u3002")
			: Command.DisplayText;
		return Result;
	}

	Result.bSuccess = Result.IssuedCount > 0;
	if (Result.bSuccess && !Command.DisplayText.IsEmpty())
	{
		Result.Message = Command.DisplayText + TEXT(" ") + Result.Message;
	}
	return Result;
}
