#include "CommanderProgressionComponent.h"

int32 UCommanderProgressionComponent::GetCommandCapacity() const
{
	switch (Profile.Rank)
	{
	case ERank::PlatoonLeader:
		return 3;
	case ERank::CompanyCommander:
		return 12;
	case ERank::BattalionCommander:
		return 40;
	case ERank::RegimentCommander:
		return 80;
	case ERank::BrigadeCommander:
		return 160;
	case ERank::DivisionCommander:
		return 320;
	case ERank::ArmyCommander:
		return 640;
	default:
		return 1;
	}
}
