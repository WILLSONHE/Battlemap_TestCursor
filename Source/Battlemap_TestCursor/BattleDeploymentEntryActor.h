#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleDeploymentEntryActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** World-space \"进入战场\" click target at a map edge (N/E/S/W). */
UCLASS()
class BATTLEMAP_TESTCURSOR_API ABattleDeploymentEntryActor : public AActor
{
	GENERATED_BODY()

public:
	ABattleDeploymentEntryActor();

	/** Call after spawn: large flat pad + label, colors by bPlayerOwned. */
	void ApplyBattlePresentation(float TileWorldUU);

	/** 0=N(+Y), 1=E(+X), 2=S(-Y), 3=W(-X) in grid local frame before grid actor rotation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	int32 EdgeIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	bool bPlayerOwned = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	UStaticMeshComponent* PlaneMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	UTextRenderComponent* LabelText = nullptr;
};
