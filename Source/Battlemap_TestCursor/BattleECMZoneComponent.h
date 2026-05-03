#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BattleECMZoneComponent.generated.h"

/**
 * Area ECM emitter: jam is resolved globally once per frame from unit world positions
 * (see UBattleECMZoneComponent::UpdateFriendlyJamForWorld, bound from ABattlemap_TestCursorGameMode).
 */
UCLASS(ClassGroup = (Battle), meta = (BlueprintSpawnableComponent))
class BATTLEMAP_TESTCURSOR_API UBattleECMZoneComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBattleECMZoneComponent();

	/** After all actors have ticked: set friendly comms Jammed only when their world position is inside any active ECM disk. */
	static void UpdateFriendlyJamForWorld(UWorld* World);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|ECM")
	float JamRadiusUU = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|ECM")
	bool bAffectsFriendliesOnly = true;
};
