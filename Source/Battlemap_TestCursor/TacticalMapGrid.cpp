#include "TacticalMapGrid.h"
#include "ScaleManagerComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Logging/LogMacros.h"
#include "Containers/Queue.h"

DEFINE_LOG_CATEGORY_STATIC(LogTacticalMapGridRuntime, Log, All);

namespace
{
	bool IsRoadNetworkTerrain(ETerrainType T)
	{
		return T == ETerrainType::Road || T == ETerrainType::Railway || T == ETerrainType::Bridge;
	}

	void CollectRoadNetworkEntryPoints(const ATacticalMapGrid* Grid, const FIntPoint& Cell, TArray<FIntPoint>& OutStarts)
	{
		OutStarts.Reset();
		FTerrainCellState Self;
		if (!Grid->TryGetCellStateById(Cell, Self))
		{
			return;
		}
		if (IsRoadNetworkTerrain(Self.TerrainType))
		{
			OutStarts.Add(Cell);
			return;
		}
		static const FIntPoint Deltas[] = {
			FIntPoint(1, 0),
			FIntPoint(-1, 0),
			FIntPoint(0, 1),
			FIntPoint(0, -1)
		};
		for (const FIntPoint& D : Deltas)
		{
			const FIntPoint N(Cell.X + D.X, Cell.Y + D.Y);
			FTerrainCellState Neighbor;
			if (Grid->TryGetCellStateById(N, Neighbor) && IsRoadNetworkTerrain(Neighbor.TerrainType))
			{
				OutStarts.Add(N);
			}
		}
	}
}

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
	TerrainMaterialMID = nullptr;
	RuntimeHeightTexture = nullptr;
	RuntimeTerrainTypeTexture = nullptr;
	RuntimeOverlayTexture = nullptr;
	RuntimeWaterLayersTexture = nullptr;
	RuntimeContourLabelTexture = nullptr;
	bRuntimeTextureInputReady = false;
}

void ATacticalMapGrid::BeginPlay()
{
	Super::BeginPlay();
	TryAutoAssignContourMaterial();
	InitializeTerrainFromPipelineOutputs();
}

void ATacticalMapGrid::TryAutoAssignContourMaterial()
{
	if (TerrainSurfaceMaterial)
	{
		return;
	}

	static const TCHAR* CandidateMaterials[] = {
		TEXT("/Game/Materials/MI_contour.MI_contour"),
		TEXT("/Game/TopDown/Materials/MI_contour.MI_contour"),
		TEXT("/Game/Materials/M_contour.M_contour"),
		TEXT("/Game/TopDown/Materials/M_contour.M_contour")
	};

	for (const TCHAR* CandidatePath : CandidateMaterials)
	{
		if (UMaterialInterface* Found = LoadObject<UMaterialInterface>(nullptr, CandidatePath))
		{
			TerrainSurfaceMaterial = Found;
			break;
		}
	}
}

UTexture2D* ATacticalMapGrid::BuildHeightTextureFrom16Bit(const TArray<uint16>& HeightPixels, int32 Width, int32 Height)
{
	if (HeightPixels.Num() != Width * Height || Width <= 0 || Height <= 0)
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}

	Texture->MipGenSettings = TMGS_NoMipmaps;
	// Preserve height precision for contour math in material.
	// Height is packed as:
	//   R = high byte, G = low byte, B = 0
	// so material can reconstruct full 16-bit height.
	Texture->CompressionSettings = TC_VectorDisplacementmap;
	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FColor* Dest = static_cast<FColor*>(Data);
	for (int32 PixelIndex = 0; PixelIndex < HeightPixels.Num(); ++PixelIndex)
	{
		const uint16 H16 = HeightPixels[PixelIndex];
		const uint8 Hi = static_cast<uint8>(H16 >> 8);
		const uint8 Lo = static_cast<uint8>(H16 & 0x00FF);
		Dest[PixelIndex] = FColor(Hi, Lo, 0, 255);
	}
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

UTexture2D* ATacticalMapGrid::BuildColorTextureFromRGBA(const TArray<FColor>& ColorPixels, int32 Width, int32 Height, int32 UpscaleFactor)
{
	const int32 Factor = FMath::Clamp(UpscaleFactor, 1, 64);
	if (ColorPixels.Num() != Width * Height || Width <= 0 || Height <= 0)
	{
		return nullptr;
	}

	const int32 OutWidth = Width * Factor;
	const int32 OutHeight = Height * Factor;

	TArray<FColor> Expanded;
	if (Factor == 1)
	{
		Expanded = ColorPixels;
	}
	else
	{
		Expanded.SetNumUninitialized(OutWidth * OutHeight);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const FColor Src = ColorPixels[Y * Width + X];
				const int32 BaseOutX = X * Factor;
				const int32 BaseOutY = Y * Factor;
				for (int32 Oy = 0; Oy < Factor; ++Oy)
				{
					FColor* DestRow = Expanded.GetData() + (BaseOutY + Oy) * OutWidth + BaseOutX;
					for (int32 Ox = 0; Ox < Factor; ++Ox)
					{
						DestRow[Ox] = Src;
					}
				}
			}
		}
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(OutWidth, OutHeight, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}

	Texture->MipGenSettings = TMGS_NoMipmaps;
	// Discrete classification maps must preserve exact RGB values for material-side comparisons
	// (e.g. IsWater palette matching). Disable sRGB and use non-lossy vector-style compression.
	Texture->CompressionSettings = TC_VectorDisplacementmap;
	Texture->SRGB = false;
	Texture->Filter = TF_Nearest;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Expanded.GetData(), Expanded.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

void ATacticalMapGrid::ApplyRuntimeTextureInputsToMaterial()
{
	bRuntimeTextureInputReady = false;
	if (!TerrainMaterialMID)
	{
		return;
	}

	if (RuntimeHeightTexture)
	{
		TerrainMaterialMID->SetTextureParameterValue(TEXT("HeightmapTex"), RuntimeHeightTexture);
	}
	if (RuntimeTerrainTypeTexture)
	{
		TerrainMaterialMID->SetTextureParameterValue(TEXT("TerrainTypesTex"), RuntimeTerrainTypeTexture);
	}
	if (RuntimeOverlayTexture)
	{
		TerrainMaterialMID->SetTextureParameterValue(TEXT("TerrainOverlayTex"), RuntimeOverlayTexture);
	}
	if (RuntimeWaterLayersTexture)
	{
		TerrainMaterialMID->SetTextureParameterValue(TEXT("WaterLayersTex"), RuntimeWaterLayersTexture);
	}
	if (RuntimeContourLabelTexture)
	{
		TerrainMaterialMID->SetTextureParameterValue(TEXT("ContourLabelTex"), RuntimeContourLabelTexture);
	}

	TerrainMaterialMID->SetScalarParameterValue(TEXT("HeightmapWidth"), static_cast<float>(LoadedHeightmapWidth));
	TerrainMaterialMID->SetScalarParameterValue(TEXT("HeightmapHeight"), static_cast<float>(LoadedHeightmapHeight));
	TerrainMaterialMID->SetScalarParameterValue(TEXT("TileSizeMeters"), TileSizeMeters);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ElevationScaleMeters"), ElevationScaleMeters);
	bRuntimeTextureInputReady = RuntimeHeightTexture != nullptr;
	ApplyMapViewModeToMaterial();
	ApplyDebugViewModeToMaterial();
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

FIntPoint ATacticalMapGrid::WorldToCell(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const float TileSizeUU = FMath::Max(1.0f, TileSizeMeters * 100.0f);
	const int32 Dimension = FMath::Max(1, GridHalfExtentTiles * 2 + 1);
	const float WorldSizeUU = static_cast<float>(Dimension) * TileSizeUU;
	const float HalfWorldUU = 0.5f * WorldSizeUU;

	// Map Local.X/Y in [-HalfWorld, +HalfWorld] to index [0..Dimension-1], then to cell id [-Half..+Half].
	const int32 IdxX = FMath::Clamp(FMath::FloorToInt((Local.X + HalfWorldUU) / TileSizeUU), 0, Dimension - 1);
	const int32 IdxY = FMath::Clamp(FMath::FloorToInt((Local.Y + HalfWorldUU) / TileSizeUU), 0, Dimension - 1);
	return FIntPoint(IdxX - GridHalfExtentTiles, IdxY - GridHalfExtentTiles);
}

FVector ATacticalMapGrid::CellToWorldCenter(const FIntPoint& CellId) const
{
	const float TileSizeUU = FMath::Max(1.0f, TileSizeMeters * 100.0f);
	const int32 Dimension = FMath::Max(1, GridHalfExtentTiles * 2 + 1);
	const float WorldSizeUU = static_cast<float>(Dimension) * TileSizeUU;
	const float HalfWorldUU = 0.5f * WorldSizeUU;

	const float IdxX = static_cast<float>(CellId.X + GridHalfExtentTiles);
	const float IdxY = static_cast<float>(CellId.Y + GridHalfExtentTiles);
	const FVector Local(
		(IdxX + 0.5f) * TileSizeUU - HalfWorldUU,
		(IdxY + 0.5f) * TileSizeUU - HalfWorldUU,
		0.0f
	);
	return GetActorTransform().TransformPosition(Local);
}

FTerrainCellState ATacticalMapGrid::BuildDefaultCellState(const FIntPoint& CellId, float ElevationMeters) const
{
	FTerrainCellState Cell;
	Cell.TerrainCellId = CellId;
	Cell.TerrainType = ETerrainType::Plains;
	Cell.ElevationMeters = ElevationMeters;
	Cell.WaterDepthMeters = 0.0f;
	Cell.bIsArtificialFacility = false;
	Cell.FacilityHP = 0.0f;
	Cell.bDestroyed = false;
	Cell.DefenseModifier = 0.0f;

	FTerrainTraversalRule LandRule;
	FTerrainTraversalRule AmphRule;
	FTerrainTraversalRule NavalRule;
	FTerrainTraversalRule AirRule;
	NavalRule.bCanTraverse = false;
	AirRule.MoveCost = 1.0f;
	AirRule.SpeedMultiplier = 1.0f;
	AirRule.bCanTraverse = true;

	Cell.TraversalRules.Add(EUnitMobilityType::Land, LandRule);
	Cell.TraversalRules.Add(EUnitMobilityType::Amphibious, AmphRule);
	Cell.TraversalRules.Add(EUnitMobilityType::Naval, NavalRule);
	Cell.TraversalRules.Add(EUnitMobilityType::Air, AirRule);
	ApplyTerrainTypeRules(Cell);
	return Cell;
}

void ATacticalMapGrid::ApplyTerrainTypeRules(FTerrainCellState& InOutCell) const
{
	FTerrainTraversalRule* LandRule = InOutCell.TraversalRules.Find(EUnitMobilityType::Land);
	FTerrainTraversalRule* AmphRule = InOutCell.TraversalRules.Find(EUnitMobilityType::Amphibious);
	FTerrainTraversalRule* NavalRule = InOutCell.TraversalRules.Find(EUnitMobilityType::Naval);
	FTerrainTraversalRule* AirRule = InOutCell.TraversalRules.Find(EUnitMobilityType::Air);
	if (!LandRule || !AmphRule || !NavalRule || !AirRule)
	{
		return;
	}

	*LandRule = FTerrainTraversalRule();
	*AmphRule = FTerrainTraversalRule();
	*NavalRule = FTerrainTraversalRule();
	*AirRule = FTerrainTraversalRule();
	NavalRule->bCanTraverse = false;
	AirRule->bCanTraverse = true;
	InOutCell.DefenseModifier = 0.0f;

	switch (InOutCell.TerrainType)
	{
	case ETerrainType::Road:
	case ETerrainType::Bridge:
		// Roads/Bridges provide movement speed bonus.
		// Requirement: Road/Bridge => x2 move speed.
		LandRule->SpeedMultiplier = 2.0f;
		AmphRule->SpeedMultiplier = 2.0f;
		NavalRule->SpeedMultiplier = 2.0f;
		AirRule->SpeedMultiplier = 2.0f;
		InOutCell.DefenseModifier = 0.05f;
		break;
	case ETerrainType::Railway:
		// Requirement: Railway => x3 move speed.
		LandRule->SpeedMultiplier = 3.0f;
		AmphRule->SpeedMultiplier = 3.0f;
		NavalRule->SpeedMultiplier = 3.0f;
		AirRule->SpeedMultiplier = 3.0f;
		break;
	case ETerrainType::River:
	case ETerrainType::Swamp:
	case ETerrainType::OpenWater:
	case ETerrainType::ShallowWater:
	case ETerrainType::DeepWater:
		LandRule->bCanTraverse = false;
		AmphRule->bCanTraverse = true;
		AmphRule->MoveCost = 1.2f;
		NavalRule->bCanTraverse = true;
		NavalRule->MoveCost = 1.0f;
		break;
	case ETerrainType::Forest:
		LandRule->MoveCost = 1.2f;
		AmphRule->MoveCost = 1.1f;
		InOutCell.DefenseModifier = 0.15f;
		break;
	case ETerrainType::Hills:
	case ETerrainType::Mountains:
		LandRule->MoveCost = 1.35f;
		AmphRule->MoveCost = 1.25f;
		InOutCell.DefenseModifier = 0.2f;
		break;
	case ETerrainType::Urban:
	case ETerrainType::Bunker:
		InOutCell.bIsArtificialFacility = true;
		InOutCell.FacilityHP = FMath::Max(InOutCell.FacilityHP, 100.0f);
		InOutCell.DefenseModifier = 0.25f;
		break;
	case ETerrainType::Airfield:
	case ETerrainType::Port:
		InOutCell.bIsArtificialFacility = true;
		InOutCell.FacilityHP = FMath::Max(InOutCell.FacilityHP, 120.0f);
		break;
	default:
		break;
	}
}

ETerrainType ATacticalMapGrid::TerrainTypeFromPalette(const FColor& Color) const
{
	// Palette mapping follows generator defaults.
	if (Color == FColor(255, 255, 255, 255)) return ETerrainType::Plains;
	if (Color == FColor(0, 128, 0, 255)) return ETerrainType::Forest;
	if (Color == FColor(128, 96, 64, 255)) return ETerrainType::Hills;
	if (Color == FColor(100, 100, 100, 255)) return ETerrainType::Mountains;
	if (Color == FColor(180, 180, 180, 255)) return ETerrainType::Urban;
	if (Color == FColor(80, 170, 220, 255)) return ETerrainType::River;
	if (Color == FColor(70, 120, 70, 255)) return ETerrainType::Swamp;
	if (Color == FColor(236, 217, 130, 255)) return ETerrainType::Desert;
	if (Color == FColor(242, 228, 170, 255)) return ETerrainType::Beach;
	if (Color == FColor(130, 100, 60, 255)) return ETerrainType::Road;
	if (Color == FColor(50, 50, 50, 255)) return ETerrainType::Railway;
	if (Color == FColor(20, 70, 170, 255)) return ETerrainType::OpenWater;
	if (Color == FColor(40, 130, 210, 255)) return ETerrainType::ShallowWater;
	if (Color == FColor(8, 36, 122, 255)) return ETerrainType::DeepWater;
	// Backward compatibility with old generated palette.
	if (Color == FColor(0, 0, 0, 255)) return ETerrainType::DeepWater;
	if (Color == FColor(120, 120, 140, 255)) return ETerrainType::Airfield;
	if (Color == FColor(20, 90, 120, 255)) return ETerrainType::Port;
	if (Color == FColor(160, 130, 90, 255)) return ETerrainType::Bridge;
	if (Color == FColor(90, 70, 50, 255)) return ETerrainType::Bunker;
	if (Color == FColor(120, 80, 80, 255)) return ETerrainType::Rubble;
	return ETerrainType::Plains;
}

void ATacticalMapGrid::BuildTestGrid(int32 HalfExtentTiles)
{
	TileDataMap.Empty();
	TerrainCellStateMap.Empty();
	GridHalfExtentTiles = FMath::Max(1, HalfExtentTiles);

	for (int32 X = -GridHalfExtentTiles; X <= GridHalfExtentTiles; ++X)
	{
		for (int32 Y = -GridHalfExtentTiles; Y <= GridHalfExtentTiles; ++Y)
		{
			const FIntPoint CellId(X, Y);
			const float ElevationMeters = FMath::PerlinNoise2D(FVector2D(static_cast<float>(X), static_cast<float>(Y)) * TerrainNoiseFrequency) * TerrainNoiseAmplitudeMeters;

			FTileData Tile;
			Tile.GridCoord = CellId;
			Tile.Terrain.TerrainType = ETerrainType::Plains;
			Tile.Terrain.MoveCost = 1.0f;
			Tile.Terrain.DefenseBonus = 0.0f;
			Tile.Terrain.StealthBonus = 0.0f;
			Tile.ElevationMeters = ElevationMeters;
			TileDataMap.Add(CellId, Tile);

			FTerrainCellState Cell = BuildDefaultCellState(CellId, ElevationMeters);
			TerrainCellStateMap.Add(CellId, Cell);
		}
	}

	RefreshMapMeshFromTerrain();
	ApplyContourMaterialParameters();
}

bool ATacticalMapGrid::TryLoadGrayscalePng(const FString& InPath, TArray<uint16>& OutPixels, int32& OutWidth, int32& OutHeight) const
{
	OutPixels.Reset();
	OutWidth = 0;
	OutHeight = 0;

	TArray<uint8> CompressedData;
	if (!FFileHelper::LoadFileToArray(CompressedData, *InPath))
	{
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
	{
		return false;
	}

	OutWidth = static_cast<int32>(ImageWrapper->GetWidth());
	OutHeight = static_cast<int32>(ImageWrapper->GetHeight());

	TArray<uint8> GrayData16;
	if (ImageWrapper->GetRaw(ERGBFormat::Gray, 16, GrayData16))
	{
		const int32 PixelCount = OutWidth * OutHeight;
		if (GrayData16.Num() >= PixelCount * 2)
		{
			TArray<uint16> BigEndianPixels;
			BigEndianPixels.SetNumUninitialized(PixelCount);
			TArray<uint16> LittleEndianPixels;
			LittleEndianPixels.SetNumUninitialized(PixelCount);
			for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
			{
				const int32 ByteIndex = PixelIndex * 2;
				const uint8 B0 = GrayData16[ByteIndex];
				const uint8 B1 = GrayData16[ByteIndex + 1];
				BigEndianPixels[PixelIndex] = static_cast<uint16>((B0 << 8) | B1);
				LittleEndianPixels[PixelIndex] = static_cast<uint16>((B1 << 8) | B0);
			}

			// Heuristic: choose decoding with smoother neighbor transitions.
			double SmoothScoreBig = 0.0;
			double SmoothScoreLittle = 0.0;
			const int32 SampleStep = FMath::Max(1, OutWidth / 64);
			for (int32 Y = 0; Y < OutHeight - 1; Y += SampleStep)
			{
				for (int32 X = 0; X < OutWidth - 1; X += SampleStep)
				{
					const int32 I = Y * OutWidth + X;
					SmoothScoreBig += FMath::Abs(static_cast<int32>(BigEndianPixels[I]) - static_cast<int32>(BigEndianPixels[I + 1]));
					SmoothScoreBig += FMath::Abs(static_cast<int32>(BigEndianPixels[I]) - static_cast<int32>(BigEndianPixels[I + OutWidth]));
					SmoothScoreLittle += FMath::Abs(static_cast<int32>(LittleEndianPixels[I]) - static_cast<int32>(LittleEndianPixels[I + 1]));
					SmoothScoreLittle += FMath::Abs(static_cast<int32>(LittleEndianPixels[I]) - static_cast<int32>(LittleEndianPixels[I + OutWidth]));
				}
			}

			OutPixels = (SmoothScoreBig <= SmoothScoreLittle) ? MoveTemp(BigEndianPixels) : MoveTemp(LittleEndianPixels);
			return true;
		}
	}

	TArray<uint8> GrayData8;
	if (ImageWrapper->GetRaw(ERGBFormat::Gray, 8, GrayData8))
	{
		OutPixels.SetNumUninitialized(GrayData8.Num());
		for (int32 Index = 0; Index < GrayData8.Num(); ++Index)
		{
			OutPixels[Index] = static_cast<uint16>(GrayData8[Index]) << 8;
		}
		return true;
	}

	return false;
}

bool ATacticalMapGrid::TryLoadColorPng(const FString& InPath, TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight) const
{
	OutPixels.Reset();
	OutWidth = 0;
	OutHeight = 0;

	TArray<uint8> CompressedData;
	if (!FFileHelper::LoadFileToArray(CompressedData, *InPath))
	{
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
	{
		return false;
	}

	OutWidth = static_cast<int32>(ImageWrapper->GetWidth());
	OutHeight = static_cast<int32>(ImageWrapper->GetHeight());
	TArray<uint8> BGRAData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, BGRAData))
	{
		return false;
	}

	const int32 PixelCount = OutWidth * OutHeight;
	OutPixels.SetNumUninitialized(PixelCount);
	for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
	{
		const int32 ByteIndex = PixelIndex * 4;
		OutPixels[PixelIndex] = FColor(BGRAData[ByteIndex + 2], BGRAData[ByteIndex + 1], BGRAData[ByteIndex + 0], BGRAData[ByteIndex + 3]);
	}
	return true;
}

bool ATacticalMapGrid::TryLoadFacilitiesJson(const FString& InPath)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *InPath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* RuntimeProfileObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("runtime_profile"), RuntimeProfileObject) && RuntimeProfileObject && RuntimeProfileObject->IsValid())
	{
		const TSharedPtr<FJsonObject>& Profile = *RuntimeProfileObject;
		if (Profile->HasField(TEXT("target_mode")))
		{
			const FString TargetMode = Profile->GetStringField(TEXT("target_mode"));
			bUseFlatContourMode = !TargetMode.Equals(TEXT("landscape"), ESearchCase::IgnoreCase);
		}
		if (Profile->HasField(TEXT("tile_size_meters")))
		{
			TileSizeMeters = static_cast<float>(Profile->GetNumberField(TEXT("tile_size_meters")));
		}
		if (Profile->HasField(TEXT("elevation_scale_meters")))
		{
			ElevationScaleMeters = static_cast<float>(Profile->GetNumberField(TEXT("elevation_scale_meters")));
		}
		if (Profile->HasField(TEXT("section_size_quads")))
		{
			LandscapeSectionSizeQuads = Profile->GetIntegerField(TEXT("section_size_quads"));
		}
		if (Profile->HasField(TEXT("sections_per_component")))
		{
			LandscapeSectionsPerComponent = Profile->GetIntegerField(TEXT("sections_per_component"));
		}
		if (Profile->HasField(TEXT("component_count_x")))
		{
			LandscapeComponentCountX = Profile->GetIntegerField(TEXT("component_count_x"));
		}
		if (Profile->HasField(TEXT("component_count_y")))
		{
			LandscapeComponentCountY = Profile->GetIntegerField(TEXT("component_count_y"));
		}
		if (Profile->HasField(TEXT("overall_resolution_x")))
		{
			LandscapeOverallResolutionX = Profile->GetIntegerField(TEXT("overall_resolution_x"));
		}
		if (Profile->HasField(TEXT("overall_resolution_y")))
		{
			LandscapeOverallResolutionY = Profile->GetIntegerField(TEXT("overall_resolution_y"));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Facilities = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("facilities"), Facilities) || !Facilities)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& FacilityValue : *Facilities)
	{
		if (!FacilityValue.IsValid() || FacilityValue->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> Facility = FacilityValue->AsObject();
		if (!Facility.IsValid())
		{
			continue;
		}

		const int32 X = Facility->GetIntegerField(TEXT("x"));
		const int32 Y = Facility->GetIntegerField(TEXT("y"));
		const FString TypeName = Facility->GetStringField(TEXT("type"));
		const double HP = Facility->GetNumberField(TEXT("hp"));
		const FIntPoint CellId(X, Y);

		if (FTerrainCellState* Cell = TerrainCellStateMap.Find(CellId))
		{
			Cell->bIsArtificialFacility = true;
			Cell->FacilityHP = static_cast<float>(HP);
			Cell->bDestroyed = false;

			if (TypeName == TEXT("urban")) Cell->TerrainType = ETerrainType::Urban;
			else if (TypeName == TEXT("road")) Cell->TerrainType = ETerrainType::Road;
			else if (TypeName == TEXT("railway")) Cell->TerrainType = ETerrainType::Railway;
			else if (TypeName == TEXT("airfield")) Cell->TerrainType = ETerrainType::Airfield;
			else if (TypeName == TEXT("port")) Cell->TerrainType = ETerrainType::Port;
			else if (TypeName == TEXT("bridge")) Cell->TerrainType = ETerrainType::Bridge;
			else if (TypeName == TEXT("bunker")) Cell->TerrainType = ETerrainType::Bunker;
			ApplyTerrainTypeRules(*Cell);
		}
	}

	return true;
}

bool ATacticalMapGrid::InitializeTerrainFromPipelineOutputs()
{
	TryAutoAssignContourMaterial();

	const FString HeightFullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), HeightmapPath);
	const FString TerrainFullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TerrainTypePath);
	const FString FacilityFullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), FacilitiesPath);
	const FString OverlayFullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TerrainOverlayPath);
	const FString WaterLayersFullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), WaterLayersPath);

	TArray<uint16> HeightPixels;
	int32 Width = 0;
	int32 Height = 0;
	if (!TryLoadGrayscalePng(HeightFullPath, HeightPixels, Width, Height))
	{
		bHeightmapLoaded = false;
		bUsedProceduralFallback = true;
		bVisualGeometryDeformed = false;
		BuildTestGrid(GridHalfExtentTiles);
		return false;
	}
	bHeightmapLoaded = true;
	bUsedProceduralFallback = false;
	bVisualGeometryDeformed = false;
	LoadedHeightmapWidth = Width;
	LoadedHeightmapHeight = Height;

	TArray<FColor> TypePixels;
	int32 TypeWidth = 0;
	int32 TypeHeight = 0;
	const bool bHasTerrainTypePng = TryLoadColorPng(TerrainFullPath, TypePixels, TypeWidth, TypeHeight) && TypeWidth == Width && TypeHeight == Height;

	TArray<FColor> OverlayPixels;
	int32 OverlayWidth = 0;
	int32 OverlayHeight = 0;
	const bool bHasOverlayPng = TryLoadColorPng(OverlayFullPath, OverlayPixels, OverlayWidth, OverlayHeight) && OverlayWidth == Width && OverlayHeight == Height;

	TArray<FColor> WaterLayerPixels;
	int32 WaterLayerWidth = 0;
	int32 WaterLayerHeight = 0;
	const bool bHasWaterLayersPng = TryLoadColorPng(WaterLayersFullPath, WaterLayerPixels, WaterLayerWidth, WaterLayerHeight) && WaterLayerWidth == Width && WaterLayerHeight == Height;

	RuntimeHeightTexture = BuildHeightTextureFrom16Bit(HeightPixels, Width, Height);
	const int32 Upscale = FMath::Clamp(DiscreteTextureUpscaleFactor, 1, 64);
	RuntimeTerrainTypeTexture = bHasTerrainTypePng ? BuildColorTextureFromRGBA(TypePixels, TypeWidth, TypeHeight, Upscale) : nullptr;
	RuntimeOverlayTexture = bHasOverlayPng ? BuildColorTextureFromRGBA(OverlayPixels, OverlayWidth, OverlayHeight, Upscale) : nullptr;
	RuntimeWaterLayersTexture = bHasWaterLayersPng ? BuildColorTextureFromRGBA(WaterLayerPixels, WaterLayerWidth, WaterLayerHeight, Upscale) : nullptr;

	TileDataMap.Empty();
	TerrainCellStateMap.Empty();
	const int32 HalfX = Width / 2;
	const int32 HalfY = Height / 2;
	GridHalfExtentTiles = FMath::Max(HalfX, HalfY);

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 PixelIndex = Y * Width + X;
			const int32 CellX = X - HalfX;
			const int32 CellY = Y - HalfY;
			const FIntPoint CellId(CellX, CellY);
			const float Height01 = static_cast<float>(HeightPixels[PixelIndex]) / 65535.0f;
			const float ElevationMeters = (Height01 - 0.5f) * ElevationScaleMeters;

			FTerrainCellState Cell = BuildDefaultCellState(CellId, ElevationMeters);
			if (bHasTerrainTypePng)
			{
				Cell.TerrainType = TerrainTypeFromPalette(TypePixels[PixelIndex]);
			}
			if (Cell.TerrainType == ETerrainType::River || Cell.TerrainType == ETerrainType::Swamp || Cell.TerrainType == ETerrainType::ShallowWater || Cell.TerrainType == ETerrainType::OpenWater || Cell.TerrainType == ETerrainType::DeepWater)
			{
				// Water depth tiers (meters) for visualization and rules:
				// River: 0~50, OpenWater: 50~80, ShallowWater: 80~200, DeepWater: >200
				// Swamp: treat as very shallow water (0~20)
				// NOTE: In flat contour mode we do NOT geometrically depress water in the heightmap,
				// so Height01 may not correlate with water depth. We still allow small modulation
				// but enforce non-zero minimum depths so HUD/visual layers work reliably.
				const float T = FMath::Clamp((0.50f - Height01) / 0.50f, 0.0f, 1.0f); // lower height => deeper
				if (Cell.TerrainType == ETerrainType::River)
				{
					Cell.WaterDepthMeters = FMath::Max(5.0f, FMath::Lerp(5.0f, 50.0f, T));
				}
				else if (Cell.TerrainType == ETerrainType::OpenWater)
				{
					Cell.WaterDepthMeters = FMath::Lerp(50.0f, 80.0f, T);
				}
				else if (Cell.TerrainType == ETerrainType::ShallowWater)
				{
					Cell.WaterDepthMeters = FMath::Lerp(80.0f, 200.0f, T);
				}
				else if (Cell.TerrainType == ETerrainType::DeepWater)
				{
					Cell.WaterDepthMeters = FMath::Lerp(220.0f, 400.0f, T);
				}
				else // Swamp
				{
					Cell.WaterDepthMeters = FMath::Max(2.0f, FMath::Lerp(2.0f, 20.0f, T));
				}
			}
			ApplyTerrainTypeRules(Cell);

			FTileData Tile;
			Tile.GridCoord = CellId;
			Tile.Terrain.TerrainType = Cell.TerrainType;
			Tile.Terrain.MoveCost = 1.0f;
			Tile.Terrain.DefenseBonus = Cell.DefenseModifier;
			Tile.ElevationMeters = ElevationMeters;

			TileDataMap.Add(CellId, Tile);
			TerrainCellStateMap.Add(CellId, Cell);
		}
	}

	TryLoadFacilitiesJson(FacilityFullPath);
	FixupWaterDepthPostPass();
	if (bEnableContourHeightLabels)
	{
		RuntimeContourLabelTexture = BuildContourLabelTexture(Width, Height, Upscale);
	}
	if (LandscapeOverallResolutionX == 0 || LandscapeOverallResolutionY == 0)
	{
		LandscapeOverallResolutionX = Width;
		LandscapeOverallResolutionY = Height;
	}

	// In flat contour mode, we keep geometry flat by design.
	if (bUseFlatContourMode)
	{
		bVisualGeometryDeformed = false;
	}
	RefreshMapMeshFromTerrain();
	ApplyContourMaterialParameters();
	ApplyRuntimeTextureInputsToMaterial();
	return true;
}

void ATacticalMapGrid::FixupWaterDepthPostPass()
{
	for (TPair<FIntPoint, FTerrainCellState>& Pair : TerrainCellStateMap)
	{
		FTerrainCellState& Cell = Pair.Value;
		const bool bIsWater =
			Cell.TerrainType == ETerrainType::River
			|| Cell.TerrainType == ETerrainType::Swamp
			|| Cell.TerrainType == ETerrainType::OpenWater
			|| Cell.TerrainType == ETerrainType::ShallowWater
			|| Cell.TerrainType == ETerrainType::DeepWater;

		if (!bIsWater)
		{
			continue;
		}

		if (Cell.WaterDepthMeters > 0.1f)
		{
			continue;
		}

		// Hard defaults to guarantee non-zero depths for HUD/visuals.
		switch (Cell.TerrainType)
		{
		case ETerrainType::River: Cell.WaterDepthMeters = 25.0f; break;
		case ETerrainType::OpenWater: Cell.WaterDepthMeters = 65.0f; break;
		case ETerrainType::ShallowWater: Cell.WaterDepthMeters = 140.0f; break;
		case ETerrainType::DeepWater: Cell.WaterDepthMeters = 260.0f; break;
		case ETerrainType::Swamp: Cell.WaterDepthMeters = 10.0f; break;
		default: break;
		}
	}
}

UTexture2D* ATacticalMapGrid::BuildContourLabelTexture(int32 Width, int32 Height, int32 UpscaleFactor)
{
	if (Width <= 0 || Height <= 0)
	{
		return nullptr;
	}

	// Make labels smaller by giving them more pixels per cell than other discrete textures.
	// This keeps UV alignment (same UV0) but reduces glyph size relative to a cell.
	const int32 Factor = FMath::Clamp(UpscaleFactor * 3, 1, 64);
	const int32 OutWidth = Width * Factor;
	const int32 OutHeight = Height * Factor;

	TArray<FColor> Pixels;
	Pixels.SetNumZeroed(OutWidth * OutHeight); // transparent

	auto DrawPixel = [&](int32 X, int32 Y, const FColor& C)
	{
		if (X < 0 || Y < 0 || X >= OutWidth || Y >= OutHeight)
		{
			return;
		}
		Pixels[Y * OutWidth + X] = C;
	};

	// 3x5 bitmap font for digits and signs (stable, easy to audit)
	auto GlyphRows = [](TCHAR Ch, const TCHAR*& R0, const TCHAR*& R1, const TCHAR*& R2, const TCHAR*& R3, const TCHAR*& R4) -> bool
	{
		// Each row is 3 chars: '1' = on, '0' = off
		switch (Ch)
		{
		case TEXT('0'): R0 = TEXT("111"); R1 = TEXT("101"); R2 = TEXT("101"); R3 = TEXT("101"); R4 = TEXT("111"); return true;
		case TEXT('1'): R0 = TEXT("010"); R1 = TEXT("110"); R2 = TEXT("010"); R3 = TEXT("010"); R4 = TEXT("111"); return true;
		case TEXT('2'): R0 = TEXT("111"); R1 = TEXT("001"); R2 = TEXT("111"); R3 = TEXT("100"); R4 = TEXT("111"); return true;
		case TEXT('3'): R0 = TEXT("111"); R1 = TEXT("001"); R2 = TEXT("111"); R3 = TEXT("001"); R4 = TEXT("111"); return true;
		case TEXT('4'): R0 = TEXT("101"); R1 = TEXT("101"); R2 = TEXT("111"); R3 = TEXT("001"); R4 = TEXT("001"); return true;
		case TEXT('5'): R0 = TEXT("111"); R1 = TEXT("100"); R2 = TEXT("111"); R3 = TEXT("001"); R4 = TEXT("111"); return true;
		case TEXT('6'): R0 = TEXT("111"); R1 = TEXT("100"); R2 = TEXT("111"); R3 = TEXT("101"); R4 = TEXT("111"); return true;
		case TEXT('7'): R0 = TEXT("111"); R1 = TEXT("001"); R2 = TEXT("010"); R3 = TEXT("010"); R4 = TEXT("010"); return true;
		case TEXT('8'): R0 = TEXT("111"); R1 = TEXT("101"); R2 = TEXT("111"); R3 = TEXT("101"); R4 = TEXT("111"); return true;
		case TEXT('9'): R0 = TEXT("111"); R1 = TEXT("101"); R2 = TEXT("111"); R3 = TEXT("001"); R4 = TEXT("111"); return true;
		case TEXT('+'): R0 = TEXT("010"); R1 = TEXT("010"); R2 = TEXT("111"); R3 = TEXT("010"); R4 = TEXT("010"); return true;
		case TEXT('-'): R0 = TEXT("000"); R1 = TEXT("000"); R2 = TEXT("111"); R3 = TEXT("000"); R4 = TEXT("000"); return true;
		default: return false;
		}
	};

	auto DrawGlyphClipped = [&](int32 BaseX, int32 BaseY, TCHAR Ch, int32 Scale, const FColor& C, int32 ClipMinX, int32 ClipMinY, int32 ClipMaxX, int32 ClipMaxY)
	{
		const TCHAR* R0 = nullptr; const TCHAR* R1 = nullptr; const TCHAR* R2 = nullptr; const TCHAR* R3 = nullptr; const TCHAR* R4 = nullptr;
		if (!GlyphRows(Ch, R0, R1, R2, R3, R4))
		{
			return;
		}

		const TCHAR* Rows[5] = { R0, R1, R2, R3, R4 };
		for (int32 Row = 0; Row < 5; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				if (Rows[Row][Col] != TEXT('1'))
				{
					continue;
				}
				for (int32 Sy = 0; Sy < Scale; ++Sy)
				{
					for (int32 Sx = 0; Sx < Scale; ++Sx)
					{
						const int32 Px = BaseX + Col * Scale + Sx;
						const int32 Py = BaseY + Row * Scale + Sy;
						if (Px < ClipMinX || Py < ClipMinY || Px >= ClipMaxX || Py >= ClipMaxY)
						{
							continue;
						}
						DrawPixel(Px, Py, C);
					}
				}
			}
		}
	};

	const float Interval = FMath::Max(1.0f, ContourLabelIntervalMeters);
	const float Tolerance = FMath::Max(0.0f, ContourLabelToleranceMeters);

	const int32 HalfX = Width / 2;
	const int32 HalfY = Height / 2;

	// Keep glyphs small and stable. Large scales will spill across cells and look like rectangles.
	const int32 GlyphScale = 1;
	const int32 GlyphW = 3 * GlyphScale;
	const int32 GlyphH = 5 * GlyphScale;
	const int32 BaseSpacing = 1 * GlyphScale;
	const int32 UnderlineGapPx = 1; // gap between digits and underline

	// Neighborhood de-dup rule:
	// For the same height bucket (Target), within Chebyshev radius <= 3 cells,
	// allow at most 2 labels.
	TMap<int32, TArray<FIntPoint>> PlacedByBucket;

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FIntPoint CellId(X - HalfX, Y - HalfY);
			const FTerrainCellState* Cell = TerrainCellStateMap.Find(CellId);
			if (!Cell)
			{
				continue;
			}

			const float Elev = Cell->ElevationMeters;
			const bool bIsHillOrMountain = (Cell->TerrainType == ETerrainType::Hills || Cell->TerrainType == ETerrainType::Mountains);
			const bool bIsWaterNonRiver =
				Cell->TerrainType == ETerrainType::OpenWater
				|| Cell->TerrainType == ETerrainType::ShallowWater
				|| Cell->TerrainType == ETerrainType::DeepWater
				|| Cell->TerrainType == ETerrainType::Swamp;
			const int32 Q = FMath::RoundToInt(Elev / Interval);
			const float Target = static_cast<float>(Q) * Interval;
			if (!bIsHillOrMountain && FMath::Abs(Elev - Target) > Tolerance)
			{
				continue;
			}

			{
				TArray<FIntPoint>& Placed = PlacedByBucket.FindOrAdd(Q);
				int32 NearbyCount = 0;
				for (const FIntPoint& P : Placed)
				{
					const int32 Dx = FMath::Abs(P.X - CellId.X);
					const int32 Dy = FMath::Abs(P.Y - CellId.Y);
					if (FMath::Max(Dx, Dy) <= 3)
					{
						NearbyCount++;
						if (NearbyCount >= 2)
						{
							break;
						}
					}
				}
				if (NearbyCount >= 2)
				{
					continue;
				}
				Placed.Add(CellId);
			}

			// Build label string like 0, +50, -100
			FString Label;
			if (bIsHillOrMountain)
			{
				// All hills/mountains are labeled with 1m precision (integer meters).
				Label = FString::Printf(TEXT("%.0f"), FMath::RoundToFloat(Elev));
			}
			else if (Target > 0.0f)
			{
				Label = FString::Printf(TEXT("+%.0f"), Target);
			}
			else if (Target < 0.0f)
			{
				Label = FString::Printf(TEXT("%.0f"), Target);
			}
			else
			{
				Label = TEXT("0");
			}

			const int32 CellBaseX = X * Factor;
			const int32 CellBaseY = Y * Factor;
			const int32 CellCenterX = CellBaseX + Factor / 2;
			const int32 CellCenterY = CellBaseY + Factor / 2;

			const FColor TextColor = bIsWaterNonRiver ? FColor(255, 255, 255, 255) : FColor(0, 0, 0, 255);
			const int32 ClipMinX = CellBaseX;
			const int32 ClipMinY = CellBaseY;
			const int32 ClipMaxX = CellBaseX + Factor;
			const int32 ClipMaxY = CellBaseY + Factor;

			// Build the label block in unrotated (normal) orientation, then rotate the whole block 90° CW,
			// including underline, to avoid per-glyph rotation artifacts.
			// Keep 1px spacing for 3/4-digit readability.
			const int32 Spacing = BaseSpacing;
			const int32 TextW = Label.Len() * GlyphW + (Label.Len() - 1) * Spacing;
			const int32 TextH = GlyphH;
			const int32 BlockW = TextW;
			const int32 BlockH = TextH + UnderlineGapPx + 1; // include underline row

			// Adaptive scale-down to keep the whole rotated label inside the cell.
			// We keep integer pixel placement for crispness (nearest-style).
			const float MaxDim = static_cast<float>(FMath::Max(BlockW, BlockH));
			const float Available = static_cast<float>(FMath::Max(1, Factor - 2)); // 1px margin
			const float ScaleDown = FMath::Clamp(Available / FMath::Max(MaxDim, 1.0f), 0.18f, 1.0f);
			const int32 ScaledBlockW = FMath::Max(1, FMath::CeilToInt(static_cast<float>(BlockW) * ScaleDown));
			const int32 ScaledBlockH = FMath::Max(1, FMath::CeilToInt(static_cast<float>(BlockH) * ScaleDown));

			const int32 RotBlockW = ScaledBlockH;
			const int32 RotBlockH = ScaledBlockW;
			const int32 OriginX = CellCenterX - RotBlockW / 2;
			const int32 OriginY = CellCenterY - RotBlockH / 2;

			auto DrawBlockPixelRotated = [&](int32 LocalX, int32 LocalY)
			{
				// Scale down in block-local space, then rotate 90° CW within scaled block.
				const int32 Sx = FMath::Clamp(FMath::FloorToInt(static_cast<float>(LocalX) * ScaleDown), 0, ScaledBlockW - 1);
				const int32 Sy = FMath::Clamp(FMath::FloorToInt(static_cast<float>(LocalY) * ScaleDown), 0, ScaledBlockH - 1);
				const int32 RotX = (ScaledBlockH - 1) - Sy;
				const int32 RotY = Sx;
				const int32 Px = OriginX + RotX;
				const int32 Py = OriginY + RotY;
				if (Px < ClipMinX || Py < ClipMinY || Px >= ClipMaxX || Py >= ClipMaxY)
				{
					return;
				}
				DrawPixel(Px, Py, TextColor);
			};

			// Draw glyphs into block-local coordinates.
			int32 BlockCursorX = 0;
			for (int32 I = 0; I < Label.Len(); ++I)
			{
				const TCHAR Ch = Label[I];
				const TCHAR* R0 = nullptr; const TCHAR* R1 = nullptr; const TCHAR* R2 = nullptr; const TCHAR* R3 = nullptr; const TCHAR* R4 = nullptr;
				if (GlyphRows(Ch, R0, R1, R2, R3, R4))
				{
					const TCHAR* Rows[5] = { R0, R1, R2, R3, R4 };
					for (int32 Row = 0; Row < 5; ++Row)
					{
						for (int32 Col = 0; Col < 3; ++Col)
						{
							if (Rows[Row][Col] != TEXT('1'))
							{
								continue;
							}
							// GlyphScale is 1 right now, keep logic general.
							for (int32 Sy = 0; Sy < GlyphScale; ++Sy)
							{
								for (int32 Sx = 0; Sx < GlyphScale; ++Sx)
								{
									DrawBlockPixelRotated(BlockCursorX + Col * GlyphScale + Sx, Row * GlyphScale + Sy);
								}
							}
						}
					}
				}
				BlockCursorX += GlyphW + Spacing;
			}

			// Underline in block-local coordinates (will rotate with the block).
			{
				const int32 UnderY = TextH + UnderlineGapPx;
				for (int32 LocalX = 0; LocalX < TextW; ++LocalX)
				{
					DrawBlockPixelRotated(LocalX, UnderY);
				}
			}
		}
	}

	// Create texture
	UTexture2D* Texture = UTexture2D::CreateTransient(OutWidth, OutHeight, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}

	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->CompressionSettings = TC_Default;
	Texture->SRGB = true;
	Texture->Filter = TF_Nearest;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

void ATacticalMapGrid::RefreshMapMeshFromTerrain()
{
	const int32 Dimension = FMath::Max(1, GridHalfExtentTiles * 2 + 1);
	const float WorldSizeUU = static_cast<float>(Dimension) * FMath::Max(1.0f, TileSizeMeters * 100.0f);
	MapMesh->SetWorldScale3D(FVector(WorldSizeUU / 100.0f, WorldSizeUU / 100.0f, 1.0f));
	MapMesh->SetCastShadow(false);

	float MinElevationMeters = 0.0f;
	float MaxElevationMeters = 0.0f;
	bool bFirst = true;
	for (const TPair<FIntPoint, FTerrainCellState>& Pair : TerrainCellStateMap)
	{
		if (bFirst)
		{
			MinElevationMeters = Pair.Value.ElevationMeters;
			MaxElevationMeters = Pair.Value.ElevationMeters;
			bFirst = false;
		}
		else
		{
			MinElevationMeters = FMath::Min(MinElevationMeters, Pair.Value.ElevationMeters);
			MaxElevationMeters = FMath::Max(MaxElevationMeters, Pair.Value.ElevationMeters);
		}
	}

	const float HeightRangeScale = bUseFlatContourMode
		? 1.0f
		: FMath::Clamp((MaxElevationMeters - MinElevationMeters) / FMath::Max(1.0f, ElevationScaleMeters), 0.2f, 3.0f);
	FVector MeshScale = MapMesh->GetComponentScale();
	MeshScale.Z = HeightRangeScale;
	MapMesh->SetWorldScale3D(MeshScale);
}

void ATacticalMapGrid::ApplyContourMaterialParameters()
{
	if (!MapMesh)
	{
		return;
	}

	if (TerrainSurfaceMaterial)
	{
		TerrainMaterialMID = UMaterialInstanceDynamic::Create(TerrainSurfaceMaterial, this);
		if (TerrainMaterialMID)
		{
			MapMesh->SetMaterial(0, TerrainMaterialMID);
		}
	}

	if (!TerrainMaterialMID)
	{
		return;
	}

	// Write both canonical and compatibility aliases so material-side naming mismatch is easy to rule out.
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ContourInterval"), ContourIntervalMeters);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ContourIntervalMeters"), ContourIntervalMeters);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ContourLineWidth"), ContourLineWidth);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ContourWidth"), ContourLineWidth);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ContourAAWidth"), ContourAAWidth);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ContourDepthOffset"), ContourDepthOffset);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ShowCoordinateAxes"), bShowCoordinateAxes ? 1.0f : 0.0f);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ElevationScaleMeters"), ElevationScaleMeters);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ElevationScale"), ElevationScaleMeters);
	ApplyMapViewModeToMaterial();
	ApplyDebugViewModeToMaterial();

	float AppliedContour = -1.0f;
	float AppliedContourMeters = -1.0f;
	float AppliedElevation = -1.0f;
	const bool bGotContour = TerrainMaterialMID->GetScalarParameterValue(TEXT("ContourInterval"), AppliedContour);
	const bool bGotContourMeters = TerrainMaterialMID->GetScalarParameterValue(TEXT("ContourIntervalMeters"), AppliedContourMeters);
	const bool bGotElevation = TerrainMaterialMID->GetScalarParameterValue(TEXT("ElevationScaleMeters"), AppliedElevation);
	UE_LOG(
		LogTacticalMapGridRuntime,
		Log,
		TEXT("MID contour params set: CI=%.3f (read:%s %.3f), CI_m=%.3f (read:%s %.3f), Elev=%.3f (read:%s %.3f)"),
		ContourIntervalMeters,
		bGotContour ? TEXT("ok") : TEXT("missing"),
		AppliedContour,
		ContourIntervalMeters,
		bGotContourMeters ? TEXT("ok") : TEXT("missing"),
		AppliedContourMeters,
		ElevationScaleMeters,
		bGotElevation ? TEXT("ok") : TEXT("missing"),
		AppliedElevation
	);
}

void ATacticalMapGrid::ApplyMapViewModeToMaterial()
{
	if (!TerrainMaterialMID)
	{
		return;
	}
	const float ModeScalar = (MapViewMode == EMapViewMode::Ocean) ? 1.0f : 0.0f;
	TerrainMaterialMID->SetScalarParameterValue(TEXT("MapViewMode"), ModeScalar);
	TerrainMaterialMID->SetVectorParameterValue(TEXT("WaterUnifiedColor"), FLinearColor(0.55f, 0.78f, 0.95f, 1.0f));
	TerrainMaterialMID->SetVectorParameterValue(TEXT("LandUnifiedColor"), FLinearColor(0.72f, 0.72f, 0.74f, 1.0f));
}

void ATacticalMapGrid::ApplyDebugViewModeToMaterial()
{
	if (!TerrainMaterialMID)
	{
		return;
	}
	TerrainMaterialMID->SetScalarParameterValue(TEXT("DebugViewMode"), static_cast<float>(static_cast<uint8>(DebugViewMode)));
}

void ATacticalMapGrid::SetMapViewMode(EMapViewMode NewMode)
{
	MapViewMode = NewMode;
	ApplyMapViewModeToMaterial();
}

void ATacticalMapGrid::SetDebugViewMode(EMapDebugViewMode NewMode)
{
	DebugViewMode = NewMode;
	ApplyDebugViewModeToMaterial();
}

void ATacticalMapGrid::CycleDebugViewMode()
{
	const uint8 Current = static_cast<uint8>(DebugViewMode);
	const uint8 Next = static_cast<uint8>((Current + 1) % 6);
	DebugViewMode = static_cast<EMapDebugViewMode>(Next);
	ApplyDebugViewModeToMaterial();
}

bool ATacticalMapGrid::GetCellStateAtWorldXY(float WorldX, float WorldY, FTerrainCellState& OutCellState) const
{
	const FIntPoint CellId = WorldToCell(FVector(WorldX, WorldY, GetActorLocation().Z));
	return TryGetCellStateById(CellId, OutCellState);
}

bool ATacticalMapGrid::TryGetCellStateById(const FIntPoint& CellId, FTerrainCellState& OutCellState) const
{
	const FTerrainCellState* Cell = TerrainCellStateMap.Find(CellId);
	if (!Cell)
	{
		return false;
	}

	OutCellState = *Cell;
	return true;
}

bool ATacticalMapGrid::AreWorldPositionsConnectedForRoadSupply(const FVector& WorldA, const FVector& WorldB) const
{
	const FIntPoint CellA = WorldToCell(WorldA);
	const FIntPoint CellB = WorldToCell(WorldB);
	TArray<FIntPoint> StartsA;
	TArray<FIntPoint> StartsB;
	CollectRoadNetworkEntryPoints(this, CellA, StartsA);
	CollectRoadNetworkEntryPoints(this, CellB, StartsB);
	if (StartsA.Num() == 0 || StartsB.Num() == 0)
	{
		return false;
	}

	TSet<FIntPoint> Goals;
	for (const FIntPoint& P : StartsB)
	{
		Goals.Add(P);
	}

	TQueue<FIntPoint> Queue;
	TSet<FIntPoint> Visited;
	for (const FIntPoint& S : StartsA)
	{
		Queue.Enqueue(S);
		Visited.Add(S);
	}

	static const FIntPoint Deltas[] = {
		FIntPoint(1, 0),
		FIntPoint(-1, 0),
		FIntPoint(0, 1),
		FIntPoint(0, -1)
	};

	while (!Queue.IsEmpty())
	{
		FIntPoint Current;
		Queue.Dequeue(Current);
		if (Goals.Contains(Current))
		{
			return true;
		}

		for (const FIntPoint& D : Deltas)
		{
			const FIntPoint Next(Current.X + D.X, Current.Y + D.Y);
			if (Visited.Contains(Next))
			{
				continue;
			}
			FTerrainCellState NextCell;
			if (!TryGetCellStateById(Next, NextCell) || !IsRoadNetworkTerrain(NextCell.TerrainType))
			{
				continue;
			}
			Visited.Add(Next);
			Queue.Enqueue(Next);
		}
	}

	return false;
}

bool ATacticalMapGrid::IsCellTraversableForMobility(EUnitMobilityType MobilityType, const FIntPoint& CellId) const
{
	FTerrainCellState Cell;
	if (!TryGetCellStateById(CellId, Cell))
	{
		return true;
	}

	if (const FTerrainTraversalRule* Rule = Cell.TraversalRules.Find(MobilityType))
	{
		return Rule->bCanTraverse;
	}

	return true;
}

namespace PathfindingDetail
{
	bool IsDiagonalNeighborAllowed(const ATacticalMapGrid* Grid, EUnitMobilityType MobilityType, const FIntPoint& From, const FIntPoint& To)
	{
		const FIntPoint D(To.X - From.X, To.Y - From.Y);
		if (FMath::Abs(D.X) != 1 || FMath::Abs(D.Y) != 1)
		{
			return true;
		}
		if (!Grid->IsCellTraversableForMobility(MobilityType, To))
		{
			return false;
		}
		const FIntPoint OrthoA(From.X + D.X, From.Y);
		const FIntPoint OrthoB(From.X, From.Y + D.Y);
		const bool bA = Grid->IsCellTraversableForMobility(MobilityType, OrthoA);
		const bool bB = Grid->IsCellTraversableForMobility(MobilityType, OrthoB);
		return bA || bB;
	}

	bool IsWorldChordTraversable(const ATacticalMapGrid* Grid, EUnitMobilityType MobilityType, FVector WorldA, FVector WorldB)
	{
		WorldA.Z = WorldB.Z;
		const float DistXY = FVector::Dist2D(WorldA, WorldB);
		const float TileUU = Grid->GetTileSizeUU();
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(DistXY / FMath::Max(20.0f, TileUU * 0.2f)), 2, 512);
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const float Alpha = static_cast<float>(Step) / static_cast<float>(Steps);
			const FVector Sample = FMath::Lerp(WorldA, WorldB, Alpha);
			const FIntPoint Cell = Grid->WorldToCell(Sample);
			if (!Grid->IsCellTraversableForMobility(MobilityType, Cell))
			{
				return false;
			}
		}
		return true;
	}

	void SimplifyCellPathToWorldWaypoints(const ATacticalMapGrid* Grid, EUnitMobilityType MobilityType, const TArray<FIntPoint>& ForwardCells, const FVector& GoalWorldLocation, TArray<FVector>& OutWorldWaypoints)
	{
		OutWorldWaypoints.Reset();
		if (ForwardCells.Num() < 2)
		{
			OutWorldWaypoints.Add(GoalWorldLocation);
			return;
		}

		TArray<FIntPoint> Slim;
		Slim.Reserve(ForwardCells.Num());
		Slim.Add(ForwardCells[0]);
		int32 i = 0;
		while (i < ForwardCells.Num() - 1)
		{
			int32 j = ForwardCells.Num() - 1;
			const FVector Wi = Grid->CellToWorldCenter(ForwardCells[i]);
			FVector WiUse = FVector(Wi.X, Wi.Y, GoalWorldLocation.Z);
			while (j > i + 1)
			{
				const FVector Wj = (j == ForwardCells.Num() - 1)
					? FVector(GoalWorldLocation.X, GoalWorldLocation.Y, GoalWorldLocation.Z)
					: FVector(Grid->CellToWorldCenter(ForwardCells[j]).X, Grid->CellToWorldCenter(ForwardCells[j]).Y, GoalWorldLocation.Z);
				if (IsWorldChordTraversable(Grid, MobilityType, WiUse, Wj))
				{
					break;
				}
				--j;
			}
			Slim.Add(ForwardCells[j]);
			i = j;
		}

		for (int32 k = 1; k < Slim.Num(); ++k)
		{
			if (k == Slim.Num() - 1)
			{
				OutWorldWaypoints.Add(FVector(GoalWorldLocation.X, GoalWorldLocation.Y, GoalWorldLocation.Z));
			}
			else
			{
				FVector W = Grid->CellToWorldCenter(Slim[k]);
				W.Z = GoalWorldLocation.Z;
				OutWorldWaypoints.Add(W);
			}
		}
	}
}

float ATacticalMapGrid::GetPathfindingEnterCost(EUnitMobilityType MobilityType, const FIntPoint& CellId) const
{
	FTerrainCellState Cell;
	if (!TryGetCellStateById(CellId, Cell))
	{
		return 1.0f;
	}

	const FTerrainTraversalRule* Rule = Cell.TraversalRules.Find(MobilityType);
	if (!Rule || !Rule->bCanTraverse)
	{
		return BIG_NUMBER;
	}

	float Cost = FMath::Max(0.05f, Rule->MoveCost);
	switch (Cell.TerrainType)
	{
	case ETerrainType::Road:
	case ETerrainType::Bridge:
		Cost *= 0.55f;
		break;
	case ETerrainType::Railway:
		Cost *= 0.4f;
		break;
	default:
		break;
	}
	return Cost;
}

bool ATacticalMapGrid::FindShortestMovePath(EUnitMobilityType MobilityType, FVector StartWorldLocation, FVector GoalWorldLocation, TArray<FVector>& OutWorldWaypoints) const
{
	OutWorldWaypoints.Reset();

	const FIntPoint StartCell = WorldToCell(StartWorldLocation);
	const FIntPoint GoalCell = WorldToCell(GoalWorldLocation);

	if (!IsCellTraversableForMobility(MobilityType, StartCell) || !IsCellTraversableForMobility(MobilityType, GoalCell))
	{
		return false;
	}

	if (StartCell == GoalCell)
	{
		OutWorldWaypoints.Add(GoalWorldLocation);
		return true;
	}

	const int32 Dimension = FMath::Max(1, GridHalfExtentTiles * 2 + 1);
	const int32 MaxIterations = Dimension * Dimension * 10;

	static const FIntPoint Deltas8[] = {
		FIntPoint(1, 0),
		FIntPoint(-1, 0),
		FIntPoint(0, 1),
		FIntPoint(0, -1),
		FIntPoint(1, 1),
		FIntPoint(1, -1),
		FIntPoint(-1, 1),
		FIntPoint(-1, -1)
	};

	auto HeuristicOctile = [&](const FIntPoint& A) -> float
	{
		const float Dx = FMath::Abs(static_cast<float>(A.X - GoalCell.X));
		const float Dy = FMath::Abs(static_cast<float>(A.Y - GoalCell.Y));
		const float DMin = FMath::Min(Dx, Dy);
		const float DMax = FMath::Max(Dx, Dy);
		static constexpr float Sqrt2 = 1.41421356f;
		const float Octile = DMax + (Sqrt2 - 1.0f) * DMin;
		return Octile * 0.02f;
	};

	TMap<FIntPoint, float> GScore;
	TMap<FIntPoint, float> FScore;
	TMap<FIntPoint, FIntPoint> CameFrom;
	TArray<FIntPoint> OpenList;
	TSet<FIntPoint> OpenSet;
	TSet<FIntPoint> ClosedSet;

	GScore.Add(StartCell, 0.0f);
	FScore.Add(StartCell, HeuristicOctile(StartCell));
	OpenList.Add(StartCell);
	OpenSet.Add(StartCell);

	int32 Iterations = 0;
	while (OpenList.Num() > 0 && Iterations++ < MaxIterations)
	{
		int32 BestIdx = 0;
		float BestF = BIG_NUMBER;
		for (int32 Idx = 0; Idx < OpenList.Num(); ++Idx)
		{
			const float FVal = FScore.FindRef(OpenList[Idx]);
			if (FVal < BestF)
			{
				BestF = FVal;
				BestIdx = Idx;
			}
		}

		const FIntPoint Current = OpenList[BestIdx];
		OpenList.RemoveAtSwap(BestIdx);
		OpenSet.Remove(Current);

		if (Current == GoalCell)
		{
			TArray<FIntPoint> ReverseCells;
			FIntPoint Trace = GoalCell;
			ReverseCells.Add(Trace);
			while (Trace != StartCell)
			{
				const FIntPoint* Prev = CameFrom.Find(Trace);
				if (!Prev)
				{
					return false;
				}
				Trace = *Prev;
				ReverseCells.Add(Trace);
			}

			TArray<FIntPoint> ForwardCells;
			ForwardCells.Reserve(ReverseCells.Num());
			for (int32 Idx = ReverseCells.Num() - 1; Idx >= 0; --Idx)
			{
				ForwardCells.Add(ReverseCells[Idx]);
			}

			PathfindingDetail::SimplifyCellPathToWorldWaypoints(this, MobilityType, ForwardCells, GoalWorldLocation, OutWorldWaypoints);
			return true;
		}

		ClosedSet.Add(Current);

		for (const FIntPoint& D : Deltas8)
		{
			const FIntPoint Neighbor(Current.X + D.X, Current.Y + D.Y);
			if (ClosedSet.Contains(Neighbor))
			{
				continue;
			}
			if (!PathfindingDetail::IsDiagonalNeighborAllowed(this, MobilityType, Current, Neighbor))
			{
				continue;
			}
			if (!IsCellTraversableForMobility(MobilityType, Neighbor))
			{
				continue;
			}

			static constexpr float Sqrt2Step = 1.41421356f;
			const float StepLen = (FMath::Abs(D.X) + FMath::Abs(D.Y) == 2) ? Sqrt2Step : 1.0f;
			const float* CurrentGPtr = GScore.Find(Current);
			const float CurrentG = CurrentGPtr ? *CurrentGPtr : 0.0f;
			const float TentativeG = CurrentG + StepLen * GetPathfindingEnterCost(MobilityType, Neighbor);
			const float* PrevGPtr = GScore.Find(Neighbor);
			const float PrevG = PrevGPtr ? *PrevGPtr : BIG_NUMBER;
			if (TentativeG >= PrevG)
			{
				continue;
			}

			CameFrom.FindOrAdd(Neighbor) = Current;
			GScore.FindOrAdd(Neighbor) = TentativeG;
			const float NewF = TentativeG + HeuristicOctile(Neighbor);
			FScore.FindOrAdd(Neighbor) = NewF;
			if (!OpenSet.Contains(Neighbor))
			{
				OpenSet.Add(Neighbor);
				OpenList.Add(Neighbor);
			}
		}
	}

	return false;
}

float ATacticalMapGrid::GetHeightAtWorldXY(float WorldX, float WorldY) const
{
	if (bUseFlatContourMode)
	{
		return GetActorLocation().Z;
	}

	FTerrainCellState Cell;
	if (!GetCellStateAtWorldXY(WorldX, WorldY, Cell))
	{
		return GetActorLocation().Z;
	}

	return GetActorLocation().Z + (Cell.ElevationMeters * 100.0f);
}

bool ATacticalMapGrid::ApplyFacilityDamage(float WorldX, float WorldY, float DamageAmount)
{
	const FIntPoint CellId = WorldToCell(FVector(WorldX, WorldY, GetActorLocation().Z));
	FTerrainCellState* Cell = TerrainCellStateMap.Find(CellId);
	if (!Cell || !Cell->bIsArtificialFacility || Cell->bDestroyed)
	{
		return false;
	}

	Cell->FacilityHP = FMath::Max(0.0f, Cell->FacilityHP - FMath::Max(0.0f, DamageAmount));
	if (Cell->FacilityHP <= 0.0f)
	{
		Cell->bDestroyed = true;
		Cell->TerrainType = ETerrainType::Rubble;

		if (FTerrainTraversalRule* LandRule = Cell->TraversalRules.Find(EUnitMobilityType::Land))
		{
			LandRule->SpeedMultiplier = 0.9f;
			LandRule->MoveCost = 1.2f;
		}
		if (FTerrainTraversalRule* AmphRule = Cell->TraversalRules.Find(EUnitMobilityType::Amphibious))
		{
			AmphRule->SpeedMultiplier = 0.9f;
			AmphRule->MoveCost = 1.2f;
		}
		Cell->DefenseModifier = 0.0f;
	}

	return true;
}
