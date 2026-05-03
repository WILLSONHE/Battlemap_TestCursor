#include "BattleEnemyTacticalBrainComponent.h"
#include "BattleUnit.h"
#include "BattleCommandComponent.h"
#include "Battlemap_TestCursorGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"

UBattleEnemyTacticalBrainComponent::UBattleEnemyTacticalBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBattleEnemyTacticalBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ABattleUnit* Unit = Cast<ABattleUnit>(GetOwner());
	if (!Unit || Unit->bFriendly || !Unit->IsAlive())
	{
		return;
	}

	PatrolCooldownRemaining = FMath::Max(0.0f, PatrolCooldownRemaining - DeltaTime);

	if (!bEnablePatrolWhenIdle)
	{
		return;
	}

	const EMissionType Mission = ResolveCurrentMissionType();
	if (Mission != PatrolMissionFilter && Mission != EMissionType::Escort)
	{
		return;
	}

	if (OwnerHasCommandActivity() || PatrolCooldownRemaining > 0.0f)
	{
		return;
	}

	if (PatrolOrigin.IsNearlyZero())
	{
		PatrolOrigin = Unit->GetActorLocation();
	}

	const FVector Target = bPatrolTowardB
		? PatrolOrigin + FVector(PatrolRadiusUU, 0.0f, 0.0f)
		: PatrolOrigin - FVector(PatrolRadiusUU, 0.0f, 0.0f);

	if (Unit->IssueMoveCommandInterrupt(Target, ECommandPriority::Low))
	{
		bPatrolTowardB = !bPatrolTowardB;
		PatrolCooldownRemaining = PatrolCommandCooldown;
	}
}

bool UBattleEnemyTacticalBrainComponent::OwnerHasCommandActivity() const
{
	const ABattleUnit* Unit = Cast<ABattleUnit>(GetOwner());
	if (!Unit)
	{
		return true;
	}

	if (Unit->GetRuntimeState() == EUnitRuntimeState::Move || Unit->GetRuntimeState() == EUnitRuntimeState::Attack || Unit->GetRuntimeState() == EUnitRuntimeState::Reload)
	{
		return true;
	}

	if (const UBattleCommandComponent* Cmd = Unit->CommandComponent)
	{
		return Cmd->HasPendingCommands();
	}

	return false;
}

EMissionType UBattleEnemyTacticalBrainComponent::ResolveCurrentMissionType() const
{
	if (UWorld* World = GetWorld())
	{
		if (ABattlemap_TestCursorGameMode* GM = Cast<ABattlemap_TestCursorGameMode>(World->GetAuthGameMode()))
		{
			return GM->GetMissionType();
		}
	}
	return EMissionType::Assault;
}
