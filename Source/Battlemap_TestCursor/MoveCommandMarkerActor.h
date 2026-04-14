#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MoveCommandMarkerActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class BATTLEMAP_TESTCURSOR_API AMoveCommandMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	AMoveCommandMarkerActor();

	UFUNCTION(BlueprintCallable, Category = "Battle|Marker")
	void SetMarkerVisible(bool bVisible);

private:
	UPROPERTY(VisibleAnywhere, Category = "Battle|Marker")
	UStaticMeshComponent* MarkerMesh;
};
