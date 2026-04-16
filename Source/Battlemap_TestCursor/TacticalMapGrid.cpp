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
	Texture->CompressionSettings = TC_Default;
	Texture->SRGB = false;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FColor* Dest = static_cast<FColor*>(Data);
	for (int32 PixelIndex = 0; PixelIndex < HeightPixels.Num(); ++PixelIndex)
	{
		const uint8 V = static_cast<uint8>(HeightPixels[PixelIndex] >> 8);
		Dest[PixelIndex] = FColor(V, V, V, 255);
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
	Texture->CompressionSettings = TC_Default;
	Texture->SRGB = true;
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

	TerrainMaterialMID->SetScalarParameterValue(TEXT("HeightmapWidth"), static_cast<float>(LoadedHeightmapWidth));
	TerrainMaterialMID->SetScalarParameterValue(TEXT("HeightmapHeight"), static_cast<float>(LoadedHeightmapHeight));
	TerrainMaterialMID->SetScalarParameterValue(TEXT("TileSizeMeters"), TileSizeMeters);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ElevationScaleMeters"), ElevationScaleMeters);
	bRuntimeTextureInputReady = RuntimeHeightTexture != nullptr;
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
	RuntimeHeightTexture = BuildHeightTextureFrom16Bit(HeightPixels, Width, Height);
	const int32 Upscale = FMath::Clamp(DiscreteTextureUpscaleFactor, 1, 64);
	RuntimeTerrainTypeTexture = bHasTerrainTypePng ? BuildColorTextureFromRGBA(TypePixels, TypeWidth, TypeHeight, Upscale) : nullptr;

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
				Cell.WaterDepthMeters = FMath::Max(0.0f, (0.2f - Height01) * ElevationScaleMeters);
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

	TerrainMaterialMID->SetScalarParameterValue(TEXT("ContourInterval"), ContourIntervalMeters);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ContourLineWidth"), ContourLineWidth);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ContourDepthOffset"), ContourDepthOffset);
	TerrainMaterialMID->SetScalarParameterValue(TEXT("ShowCoordinateAxes"), bShowCoordinateAxes ? 1.0f : 0.0f);
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
