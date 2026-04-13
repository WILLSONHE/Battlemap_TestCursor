#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleTypes.h"
#include "TacticalMapGrid.generated.h"

class UScaleManagerComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class BATTLEMAP_TESTCURSOR_API ATacticalMapGrid : public AActor
{
	GENERATED_BODY()

public:
	ATacticalMapGrid();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Map")
	float TileSizeMeters = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Map")
	TMap<FIntPoint, FTileData> TileDataMap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Map")
	UScaleManagerComponent* ScaleManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Map")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Map")
	UStaticMeshComponent* MapMesh;

	UFUNCTION(BlueprintCallable, Category = "Battle|Map")
	void ApplyZoom(float DeltaScale);

	UFUNCTION(BlueprintCallable, Category = "Battle|Map")
	void RotateMap(float DeltaYawDeg);

	UFUNCTION(BlueprintCallable, Category = "Battle|Map")
	void BuildTestGrid(int32 HalfExtentTiles);
};
