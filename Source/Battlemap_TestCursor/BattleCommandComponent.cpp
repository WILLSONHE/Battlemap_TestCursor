#include "BattleCommandComponent.h"

void UBattleCommandComponent::EnqueueCommand(const FActiveCommand& Command)
{
	CommandQueue.Add(Command);
	CommandQueue.Sort([](const FActiveCommand& A, const FActiveCommand& B)
	{
		if (A.bDirectCommand != B.bDirectCommand)
		{
			return A.bDirectCommand;
		}

		return static_cast<uint8>(A.Priority) < static_cast<uint8>(B.Priority);
	});
}

bool UBattleCommandComponent::TryPopNextCommand(FActiveCommand& OutCommand)
{
	if (CommandQueue.Num() == 0)
	{
		return false;
	}

	OutCommand = CommandQueue[0];
	CommandQueue.RemoveAt(0);
	return true;
}

void UBattleCommandComponent::RemoveCommandsByType(ECommandType CommandType)
{
	CommandQueue.RemoveAll([CommandType](const FActiveCommand& Command)
	{
		return Command.CommandType == CommandType;
	});
}
