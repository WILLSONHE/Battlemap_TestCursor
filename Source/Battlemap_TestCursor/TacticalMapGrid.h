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

UENUM(BlueprintType)
enum class EMapViewMode : uint8
{
	Land UMETA(DisplayName = "Land"),
	Ocean UMETA(DisplayName = "Ocean")
};

UENUM(BlueprintType)
enum class EMapDebugViewMode : uint8
{
	Final UMETA(DisplayName = "Final"),
	TerrainTypes UMETA(DisplayName = "TerrainTypes"),
	WaterLayers UMETA(DisplayName = "WaterLayers"),
	IsWaterMask UMETA(DisplayName = "IsWaterMask"),
	BaseTint UMETA(DisplayName = "BaseTint"),
	ContourMask UMETA(DisplayName = "ContourMask")
};

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
	int32 DiscreteTextureUpscaleFactor = 16;

	// Contour height label overlay (A1): generate a runtime label texture and feed to material.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour|Labels")
	bool bEnableContourHeightLabels = true;

	// Label interval in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour|Labels", meta = (ClampMin = "1.0"))
	float ContourLabelIntervalMeters = 100.0f;

	// Only label when elevation is close enough to an exact multiple of interval.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour|Labels", meta = (ClampMin = "0.0"))
	float ContourLabelToleranceMeters = 1.0f;

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
	float ContourAAWidth = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour")
	float ContourDepthOffset = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour")
	bool bShowCoordinateAxes = true;

	/** 0 = Land view (terrain palette + unified water tint); 1 = Ocean view (water layers + unified land tint). Driven by material parameter MapViewMode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour")
	EMapViewMode MapViewMode = EMapViewMode::Land;

	/** Debug switch passed to material scalar DebugViewMode (0..4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Contour|Debug")
	EMapDebugViewMode DebugViewMode = EMapDebugViewMode::Final;

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

	UFUNCTION(BlueprintCallable, Category = "Battle|Contour")
	void SetMapViewMode(EMapViewMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Battle|Contour|Debug")
	void SetDebugViewMode(EMapDebugViewMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Battle|Contour|Debug")
	void CycleDebugViewMode();

	UFUNCTION(BlueprintPure, Category = "Battle|Contour|Debug")
	EMapDebugViewMode GetDebugViewMode() const { return DebugViewMode; }

	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	bool TryGetCellStateById(const FIntPoint& CellId, FTerrainCellState& OutCellState) const;

	/** BFS over Road/Railway/Bridge cells; endpoints may be one cell off the network if adjacent to a road cell. */
	UFUNCTION(BlueprintPure, Category = "Battle|Terrain|Supply")
	bool AreWorldPositionsConnectedForRoadSupply(const FVector& WorldA, const FVector& WorldB) const;

	/**
	 * A* on the tactical grid (8-neighbor + chord simplification). Respects TraversalRules per mobility; road/rail/bridge get lower enter cost.
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle|Pathfinding")
	bool FindShortestMovePath(EUnitMobilityType MobilityType, FVector StartWorldLocation, FVector GoalWorldLocation, TArray<FVector>& OutWorldWaypoints) const;

	UFUNCTION(BlueprintPure, Category = "Battle|Terrain")
	float GetTileSizeUU() const { return FMath::Max(1.0f, TileSizeMeters * 100.0f); }

	/** Used by pathfinding / tools; same rules as unit traversability. */
	bool IsCellTraversableForMobility(EUnitMobilityType MobilityType, const FIntPoint& CellId) const;

	float GetPathfindingEnterCost(EUnitMobilityType MobilityType, const FIntPoint& CellId) const;

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
	void FixupWaterDepthPostPass();
	UTexture2D* BuildContourLabelTexture(int32 Width, int32 Height, int32 UpscaleFactor);
	void ApplyContourMaterialParameters();
	void ApplyMapViewModeToMaterial();
	void ApplyDebugViewModeToMaterial();
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

	UPROPERTY(Transient)
	UTexture2D* RuntimeContourLabelTexture;
};
