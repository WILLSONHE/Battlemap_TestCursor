#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleTypes.h"
#include "TacticalMapGrid.generated.h"

class UScaleManagerComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

UCLASS()
class BATTLEMAP_TESTCURSOR_API ATacticalMapGrid : public AActor
{
	GENERATED_BODY()

public:
	ATacticalMapGrid();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Map")
	float TileSizeMeters = 10.0f;

	// Large runtime cache intentionally not exposed to Details.
	TMap<FIntPoint, FTileData> TileDataMap;

	// Large runtime cache intentionally not exposed to Details.
	TMap<FIntPoint, FTerrainCellState> TerrainCellStateMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain")
	int32 GridHalfExtentTiles = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain")
	float ElevationScaleMeters = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain")
	float TerrainNoiseAmplitudeMeters = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain")
	float TerrainNoiseFrequency = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain")
	FString HeightmapPath = TEXT("Saved/TerrainPipeline/heightmap.png");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain")
	FString TerrainTypePath = TEXT("Saved/TerrainPipeline/terrain_types.png");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain")
	FString FacilitiesPath = TEXT("Saved/TerrainPipeline/facilities.json");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain")
	FString TerrainOverlayPath = TEXT("Saved/TerrainPipeline/terrain_overlay.png");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain")
	FString WaterLayersPath = TEXT("Saved/TerrainPipeline/water_layers.png");

	// Visual-only upscale for discrete cell textures (terrain types / overlays) to reduce half-cell bleeding
	// without editing material graphs. 1 = no upscale.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Terrain|Visual")
	int32 DiscreteTextureUpscaleFactor = 8;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Status")
	bool bHeightmapLoaded = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Status")
	bool bUsedProceduralFallback = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Status")
	bool bVisualGeometryDeformed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Status")
	bool bUseFlatContourMode = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Status")
	int32 LoadedHeightmapWidth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Status")
	int32 LoadedHeightmapHeight = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Profile")
	int32 LandscapeSectionSizeQuads = 63;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Profile")
	int32 LandscapeSectionsPerComponent = 2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Profile")
	int32 LandscapeComponentCountX = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Profile")
	int32 LandscapeComponentCountY = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Profile")
	int32 LandscapeOverallResolutionX = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Terrain|Profile")
	int32 LandscapeOverallResolutionY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour")
	UMaterialInterface* TerrainSurfaceMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour")
	float ContourIntervalMeters = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour")
	float ContourLineWidth = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour")
	float ContourDepthOffset = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour")
	bool bShowCoordinateAxes = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Contour|Status")
	bool bRuntimeTextureInputReady = false;

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

	UFUNCTION(BlueprintCallable, Category = "Battle|Terrain")
	bool InitializeTerrainFromPipelineOutputs();

	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	bool GetCellStateAtWorldXY(float WorldX, float WorldY, FTerrainCellState& OutCellState) const;

	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	float GetHeightAtWorldXY(float WorldX, float WorldY) const;

	UFUNCTION(BlueprintCallable, Category = "Battle|Terrain")
	bool ApplyFacilityDamage(float WorldX, float WorldY, float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	FIntPoint WorldToCell(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	FVector CellToWorldCenter(const FIntPoint& CellId) const;

	UFUNCTION(BlueprintPure, Category = "Battle|Contour")
	bool IsContourMaterialConfigured() const { return TerrainSurfaceMaterial != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	bool TryGetCellStateById(const FIntPoint& CellId, FTerrainCellState& OutCellState) const;

	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	float GetTileSizeUU() const { return FMath::Max(1.0f, TileSizeMeters * 100.0f); }

protected:
	virtual void BeginPlay() override;

private:
	void TryAutoAssignContourMaterial();
	UTexture2D* BuildHeightTextureFrom16Bit(const TArray<uint16>& HeightPixels, int32 Width, int32 Height);
	UTexture2D* BuildColorTextureFromRGBA(const TArray<FColor>& ColorPixels, int32 Width, int32 Height, int32 UpscaleFactor);
	void ApplyRuntimeTextureInputsToMaterial();
	bool TryLoadGrayscalePng(const FString& InPath, TArray<uint16>& OutPixels, int32& OutWidth, int32& OutHeight) const;
	bool TryLoadColorPng(const FString& InPath, TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight) const;
	bool TryLoadFacilitiesJson(const FString& InPath);
	FTerrainCellState BuildDefaultCellState(const FIntPoint& CellId, float ElevationMeters) const;
	void ApplyTerrainTypeRules(FTerrainCellState& InOutCell) const;
	ETerrainType TerrainTypeFromPalette(const FColor& Color) const;
	void RecomputeCellSlopes();
	void ApplyContourMaterialParameters();
	void RefreshMapMeshFromTerrain();

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* TerrainMaterialMID;

	UPROPERTY(Transient)
	UTexture2D* RuntimeHeightTexture;

	UPROPERTY(Transient)
	UTexture2D* RuntimeTerrainTypeTexture;

	UPROPERTY(Transient)
	UTexture2D* RuntimeOverlayTexture;

	UPROPERTY(Transient)
	UTexture2D* RuntimeWaterLayersTexture;
};
