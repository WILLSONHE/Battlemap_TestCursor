#pragma once

#include "CoreMinimal.h"
#include "BattleTypes.generated.h"

UENUM(BlueprintType)
enum class ETerrainType : uint8
{
	Plains,
	Forest,
	Hills,
	Mountains,
	Urban,
	River,
	Swamp,
	Desert,
	Beach,
	Road,
	Railway,
	OpenWater,
	ShallowWater,
	DeepWater,
	Airfield,
	Port,
	Bridge,
	Bunker,
	Rubble
};

UENUM(BlueprintType)
enum class EUnitMobilityType : uint8
{
	Land,
	Amphibious,
	Naval,
	Air
};

UENUM(BlueprintType)
enum class EUnitCategory : uint8
{
	Infantry,
	Vehicle,
	Aircraft,
	NavalSurface,
	NavalSub,
	Facility
};

UENUM(BlueprintType)
enum class EUnitType : uint8
{
	Infantry,
	Tank,
	AttackHelicopter,
	TransportHelicopter,
	Destroyer,
	Submarine,
	Facility
};

UENUM(BlueprintType)
enum class ERank : uint8
{
	SquadLeader,
	PlatoonLeader,
	CompanyCommander,
	BattalionCommander,
	RegimentCommander,
	BrigadeCommander,
	DivisionCommander,
	ArmyCommander
};

UENUM(BlueprintType)
enum class EStealthState : uint8
{
	Visible,
	Stealthed,
	Detected
};

UENUM(BlueprintType)
enum class ECommsState : uint8
{
	Online,
	Jammed,
	Lost
};

UENUM(BlueprintType)
enum class EEquipmentType : uint8
{
	Rifle,
	TankGun,
	AntiTank,
	Rocket,
	Missile,
	Torpedo,
	Mine
};

UENUM(BlueprintType)
enum class ECommandType : uint8
{
	Move,
	Attack,
	Defend,
	Patrol,
	Recon,
	Support
};

UENUM(BlueprintType)
enum class ECommandPriority : uint8
{
	Critical,
	High,
	Normal,
	Low
};

UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	Setup,
	Planning,
	Execution,
	Review
};

UENUM(BlueprintType)
enum class EMissionType : uint8
{
	Assault,
	Defense,
	Recon,
	Escort
};

UENUM(BlueprintType)
enum class EUnitRuntimeState : uint8
{
	Idle,
	Move,
	Attack,
	Reload,
	Dead
};

USTRUCT(BlueprintType)
struct FTileTerrainData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETerrainType TerrainType = ETerrainType::Plains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveCost = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefenseBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StealthBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CoverLevel = 0;
};

USTRUCT(BlueprintType)
struct FTileData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint GridCoord = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTileTerrainData Terrain;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ElevationMeters = 0.0f;

};

USTRUCT(BlueprintType)
struct FTerrainTraversalRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveCost = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanTraverse = true;
};

USTRUCT(BlueprintType)
struct FTerrainCellState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint TerrainCellId = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETerrainType TerrainType = ETerrainType::Plains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ElevationMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WaterDepthMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SlopeToMaxNeighbor = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsArtificialFacility = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FacilityHP = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDestroyed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefenseModifier = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EUnitMobilityType, FTerrainTraversalRule> TraversalRules;
};

USTRUCT(BlueprintType)
struct FUnitEquipmentSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEquipmentType EquipmentType = EEquipmentType::Rifle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Capacity = 1;
};

USTRUCT(BlueprintType)
struct FUnitCommsChannel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Frequency = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECommsState State = ECommsState::Online;
};

USTRUCT(BlueprintType)
struct FScaleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxScale = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefaultScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FTerrainResistance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETerrainType TerrainType = ETerrainType::Plains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Resistance = 1.0f;
};

USTRUCT(BlueprintType)
struct FCommanderProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CommanderName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERank Rank = ERank::PlatoonLeader;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Experience = 0;
};

USTRUCT(BlueprintType)
struct FUnitBaseData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName UnitId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUnitCategory Category = EUnitCategory::Infantry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUnitType UnitType = EUnitType::Infantry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Armor = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BaseDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BaseDetectionRangeKm = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 StealthLevel = 0;
};

USTRUCT(BlueprintType)
struct FPlatoonData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> SquadUnitIds;
};

USTRUCT(BlueprintType)
struct FCompanyData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FPlatoonData> Platoons;
};

USTRUCT(BlueprintType)
struct FActiveCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECommandType CommandType = ECommandType::Move;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECommandPriority Priority = ECommandPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDirectCommand = true;
};

USTRUCT(BlueprintType)
struct FDetectionResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDetected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EffectiveDetectionLevel = 0.0f;
};

USTRUCT(BlueprintType)
struct FDebugBattleSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> SelectedUnitNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SelectedUnitName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SelectedUnitCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SelectedUnitLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECommsState CommsState = ECommsState::Online;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Health = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Food = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Fuel = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CurrentAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LastHint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString AttackTargetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackTargetHealth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraRoll = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraVerticalDistanceMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 VisibleEnemyCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUnitRuntimeState SelectedUnitRuntimeState = EUnitRuntimeState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SelectedUnitAttackCooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SelectedUnitReloadRemaining = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SelectedUnitLastCombatEvent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsBoxSelecting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D SelectionBoxStart = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D SelectionBoxEnd = FVector2D::ZeroVector;
};
