#include "BattleCommsComponent.h"

bool UBattleCommsComponent::CanCommunicateWith(const UBattleCommsComponent* Other) const
{
	if (!Other)
	{
		return false;
	}

	return IsCommandEnabled()
		&& Other->IsCommandEnabled()
		&& CommsChannel.Frequency == Other->CommsChannel.Frequency;
}

bool UBattleCommsComponent::IsCommandEnabled() const
{
	return CommsChannel.State == ECommsState::Online;
}
