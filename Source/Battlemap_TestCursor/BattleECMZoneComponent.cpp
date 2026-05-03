#include "BattleECMZoneComponent.h"
#include "BattleUnit.h"
#include "BattleCommsComponent.h"
#include "BattleTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UBattleECMZoneComponent::UBattleECMZoneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

namespace
{
	struct FEcmDisk
	{
		FVector2D CenterXY;
		float RadiusUU = 0.0f;
		bool bAffectsFriendliesOnly = true;
	};
}

void UBattleECMZoneComponent::UpdateFriendlyJamForWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	TArray<FEcmDisk> Disks;
	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* Emitter = *It;
		if (!Emitter || !Emitter->IsAlive() || !Emitter->EcmZone)
		{
			continue;
		}

		const float R = Emitter->EcmZone->JamRadiusUU;
		if (R <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector O = Emitter->GetActorLocation();
		Disks.Add({ FVector2D(O.X, O.Y), R, Emitter->EcmZone->bAffectsFriendliesOnly });
	}

	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* Victim = *It;
		if (!Victim || !Victim->IsAlive() || !Victim->CommsComponent)
		{
			continue;
		}

		ECommsState& State = Victim->CommsComponent->CommsChannel.State;
		if (State == ECommsState::Lost)
		{
			continue;
		}

		const FVector2D P(Victim->GetActorLocation().X, Victim->GetActorLocation().Y);
		bool bInsideAny = false;
		for (const FEcmDisk& D : Disks)
		{
			if (D.bAffectsFriendliesOnly && !Victim->bFriendly)
			{
				continue;
			}

			if (FVector2D::DistSquared(P, D.CenterXY) <= FMath::Square(D.RadiusUU))
			{
				bInsideAny = true;
				break;
			}
		}

		if (bInsideAny)
		{
			State = ECommsState::Jammed;
		}
		else if (State == ECommsState::Jammed)
		{
			State = ECommsState::Online;
		}
	}
}
