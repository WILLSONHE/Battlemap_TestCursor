#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleTypes.h"
#include "BattleEnemyTacticalBrainComponent.generated.h"

class ABattleUnit;

/**
 * Minimal scripted enemy behaviour: patrol when idle (Recon-style missions) and defer strike logic to TryAutoEngage.
 */
UCLASS(ClassGroup = (Battle), meta = (BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UBattleEnemyTacticalBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleEnemyTacticalBrainComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|AI")
	bool bEnablePatrolWhenIdle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|AI")
	float PatrolRadiusUU = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|AI")
	float PatrolCommandCooldown = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|AI")
	EMissionType PatrolMissionFilter = EMissionType::Recon;

protected:
	float PatrolCooldownRemaining = 0.0f;
	bool bPatrolTowardB = true;
	FVector PatrolOrigin = FVector::ZeroVector;

	bool OwnerHasCommandActivity() const;
	EMissionType ResolveCurrentMissionType() const;
};
