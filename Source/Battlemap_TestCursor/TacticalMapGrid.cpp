#include "TacticalMapGrid.h"
#include "ScaleManagerComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

ATacticalMapGrid::ATacticalMapGrid()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MapMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MapMesh"));
	MapMesh->SetupAttachment(SceneRoot);
	MapMesh->SetCollisionProfileName(TEXT("BlockAll"));
	MapMesh->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		MapMesh->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (WhiteMaterial.Succeeded())
	{
		MapMesh->SetMaterial(0, WhiteMaterial.Object);
	}

	ScaleManager = CreateDefaultSubobject<UScaleManagerComponent>(TEXT("ScaleManager"));
}

void ATacticalMapGrid::ApplyZoom(float DeltaScale)
{
	if (!ScaleManager)
	{
		return;
	}

	const float NewScale = ScaleManager->ApplyZoomDelta(DeltaScale);
	SetActorScale3D(FVector(NewScale));
}

void ATacticalMapGrid::RotateMap(float DeltaYawDeg)
{
	AddActorWorldRotation(FRotator(0.0f, DeltaYawDeg, 0.0f));
}

void ATacticalMapGrid::BuildTestGrid(int32 HalfExtentTiles)
{
	TileDataMap.Empty();

	const int32 Dimension = FMath::Max(1, HalfExtentTiles * 2 + 1);
	const float WorldSize = static_cast<float>(Dimension) * TileSizeMeters / 100.0f;
	MapMesh->SetWorldScale3D(FVector((WorldSize / 100.0f) * 500.0f, (WorldSize / 100.0f) * 500.0f, 1.0f));

	for (int32 X = -HalfExtentTiles; X <= HalfExtentTiles; ++X)
	{
		for (int32 Y = -HalfExtentTiles; Y <= HalfExtentTiles; ++Y)
		{
			FTileData Tile;
			Tile.GridCoord = FIntPoint(X, Y);
			Tile.Terrain.TerrainType = ETerrainType::Plains;
			Tile.Terrain.MoveCost = 1.0f;
			Tile.Terrain.DefenseBonus = 0.0f;
			Tile.Terrain.StealthBonus = 0.0f;
			TileDataMap.Add(Tile.GridCoord, Tile);
		}
	}
}
