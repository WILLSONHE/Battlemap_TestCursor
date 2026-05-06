#include "BattleUnit.h"
#include "BattleCommandComponent.h"
#include "BattleDetectionComponent.h"
#include "BattleCommsComponent.h"
#include "BattleSupplyComponent.h"
#include "BattleBalanceDeveloperSettings.h"
#include "Battlemap_TestCursorGameMode.h"
#include "BattleEnemyTacticalBrainComponent.h"
#include "BattleECMZoneComponent.h"
#include "MoveCommandMarkerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Object.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TacticalMapGrid.h"

namespace BattleUnitMeshPaths
{
	static const TCHAR* AliveTankAsset = TEXT("/Game/ActualContent/SM_Tank.SM_Tank");
	static const TCHAR* DestroyedPlaceholderCube = TEXT("/Engine/BasicShapes/Cube.Cube");
}

ABattleUnit::ABattleUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetActorTickEnabled(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	UnitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UnitMesh"));
	UnitMesh->SetupAttachment(SceneRoot);
	UnitMesh->SetCollisionProfileName(TEXT("Pawn"));
	UnitMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UnitMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	UnitMesh->SetMobility(EComponentMobility::Movable);
	UnitMesh->SetCastShadow(true);
	UnitMesh->bCastDynamicShadow = true;
	UnitMesh->bCastStaticShadow = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		UnitMesh->SetStaticMesh(CubeMesh.Object);
		UnitMesh->SetWorldScale3D(FVector(0.25f));
	}

	SetActorEnableCollision(true);
	MoveCommandMarker = nullptr;
	RuntimeState = EUnitRuntimeState::Idle;
	LastCombatEvent = TEXT("无");

	CommandComponent = CreateDefaultSubobject<UBattleCommandComponent>(TEXT("CommandComponent"));
	DetectionComponent = CreateDefaultSubobject<UBattleDetectionComponent>(TEXT("DetectionComponent"));
	CommsComponent = CreateDefaultSubobject<UBattleCommsComponent>(TEXT("CommsComponent"));
	SupplyComponent = CreateDefaultSubobject<UBattleSupplyComponent>(TEXT("SupplyComponent"));

	EcmZone = CreateDefaultSubobject<UBattleECMZoneComponent>(TEXT("EcmZone"));
	if (EcmZone)
	{
		EcmZone->SetupAttachment(RootComponent);
		EcmZone->JamRadiusUU = 0.0f;
		EcmZone->bAffectsFriendliesOnly = true;
	}
}

void ABattleUnit::BeginPlay()
{
	Super::BeginPlay();
	MovementFacingAnchor = GetActorLocation();
	bMovementFacingAnchorInitialized = true;
	if (IsAlive())
	{
		ApplyAliveCombatMesh();
	}
	else
	{
		ApplyDestroyedPlaceholderMesh();
	}
}

void ABattleUnit::ApplyAliveCombatMesh()
{
	if (!UnitMesh)
	{
		return;
	}
	if (UStaticMesh* TankMesh = LoadObject<UStaticMesh>(nullptr, BattleUnitMeshPaths::AliveTankAsset))
	{
		UnitMesh->SetStaticMesh(TankMesh);
	}
}

void ABattleUnit::ApplyDestroyedPlaceholderMesh()
{
	if (!UnitMesh || bDestroyedPlaceholderMeshApplied)
	{
		return;
	}
	if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, BattleUnitMeshPaths::DestroyedPlaceholderCube))
	{
		UnitMesh->SetStaticMesh(CubeMesh);
	}
	bDestroyedPlaceholderMeshApplied = true;
}

void ABattleUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsAlive())
	{
		RuntimeState = EUnitRuntimeState::Dead;
		UpdateMeshScaleVisual();
		return;
	}

	AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaSeconds);
	AutoEngageScanCooldown = FMath::Max(0.0f, AutoEngageScanCooldown - DeltaSeconds);
	HitFlashRemaining = FMath::Max(0.0f, HitFlashRemaining - DeltaSeconds);
	if (ReloadRemaining > 0.0f)
	{
		ReloadRemaining = FMath::Max(0.0f, ReloadRemaining - DeltaSeconds);
		RuntimeState = EUnitRuntimeState::Reload;
		if (ReloadRemaining <= 0.0f)
		{
			CurrentAmmo = MaxAmmo;
			LastCombatEvent = FString::Printf(TEXT("%s 完成重装填。"), *UnitLabel);
			if (bHasActiveCommand && ActiveCommand.CommandType == ECommandType::Attack && AttackTarget && AttackTarget->IsAlive())
			{
				RuntimeState = EUnitRuntimeState::Attack;
			}
			else
			{
				RuntimeState = EUnitRuntimeState::Idle;
			}
		}
	}

	ProcessActiveCommand(DeltaSeconds);
	TryAutoEngage(DeltaSeconds);

	if (SupplyComponent && IsAlive())
	{
		bool bRoadConnected = false;
		if (ATacticalMapGrid* Grid = ResolveTacticalMapGrid())
		{
			if (UWorld* World = GetWorld())
			{
				if (ABattlemap_TestCursorGameMode* GM = Cast<ABattlemap_TestCursorGameMode>(World->GetAuthGameMode()))
				{
					bRoadConnected = Grid->AreWorldPositionsConnectedForRoadSupply(GetActorLocation(), GM->GetSupplyRoadAnchorWorld());
				}
			}
		}
		SupplyComponent->ConsumeForUnitType(UnitData.Category, bRoadConnected, DeltaSeconds);
	}

	UpdateUnitMeshFacingFromMovement();
	UpdateMeshScaleVisual();
}

void ABattleUnit::UpdateUnitMeshFacingFromMovement()
{
	if (!UnitMesh || bDestroyedPlaceholderMeshApplied)
	{
		return;
	}
	FVector2D FacingDir2D(0.f, 0.f);
	bool bHaveFacing = false;

	if (AttackTarget && AttackTarget->IsAlive())
	{
		const FVector2D ToTarget(
			AttackTarget->GetActorLocation().X - GetActorLocation().X,
			AttackTarget->GetActorLocation().Y - GetActorLocation().Y);
		if (ToTarget.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			FacingDir2D = ToTarget.GetSafeNormal();
			bHaveFacing = true;
		}
		MovementFacingAnchor = GetActorLocation();
	}
	else
	{
		const FVector Post = GetActorLocation();
		if (!bMovementFacingAnchorInitialized)
		{
			MovementFacingAnchor = Post;
			bMovementFacingAnchorInitialized = true;
			return;
		}
		const FVector2D D(Post.X - MovementFacingAnchor.X, Post.Y - MovementFacingAnchor.Y);
		MovementFacingAnchor = Post;
		if (D.SizeSquared() >= 4.f)
		{
			FacingDir2D = D.GetSafeNormal();
			bHaveFacing = true;
		}
	}

	if (!bHaveFacing)
	{
		return;
	}
	// Tank asset: local +Y is forward, +X is lateral; align +Y with horizontal aim / velocity.
	const float WorldYawDeg = FMath::RadiansToDegrees(FMath::Atan2(FacingDir2D.Y, FacingDir2D.X)) - 90.f;
	UnitMesh->SetWorldRotation(FRotator(0.f, WorldYawDeg, 0.f));
}

void ABattleUnit::ApplyDamageValue(float DamageValue)
{
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageValue);
	HitFlashRemaining = 0.16f;
	LastCombatEvent = FString::Printf(TEXT("%s 受到 %.0f 点伤害。"), *UnitLabel, DamageValue);
	if (!IsAlive())
	{
		RuntimeState = EUnitRuntimeState::Dead;
		LastCombatEvent = FString::Printf(TEXT("%s 已被击毁。"), *UnitLabel);
		ApplyDestroyedPlaceholderMesh();
	}
}

bool ABattleUnit::IsAlive() const
{
	return CurrentHealth > 0.0f;
}

void ABattleUnit::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;

	if (!UnitMesh)
	{
		return;
	}

	UnitMesh->SetRenderCustomDepth(bSelected);
	UpdateMeshScaleVisual();

	if (MoveCommandMarker)
	{
		MoveCommandMarker->SetMarkerVisible(bSelected);
	}
}

bool ABattleUnit::IssueMoveCommandInterrupt(const FVector& TargetLocation, ECommandPriority Priority)
{
	ClearMovePath();
	if (!TryPopulateMovePathFromGoal(TargetLocation))
	{
		LastCombatEvent = TEXT("没有可靠通行路径");
		return false;
	}

	if (CommandComponent)
	{
		CommandComponent->RemoveCommandsByType(ECommandType::Move);
	}

	ActiveCommand.CommandType = ECommandType::Move;
	ActiveCommand.Priority = Priority;
	ActiveCommand.TargetLocation = TargetLocation;
	ActiveCommand.bDirectCommand = true;
	AttackTarget = nullptr;
	bHasActiveCommand = true;
	MovePathPointIndex = 0;
	RuntimeState = EUnitRuntimeState::Move;
	LastCombatEvent = FString::Printf(TEXT("%s 执行机动命令。"), *UnitLabel);
	SpawnOrReplaceMoveMarker(TargetLocation);
	return true;
}

void ABattleUnit::IssueAttackCommandInterrupt(ABattleUnit* TargetUnit, ECommandPriority Priority)
{
	if (!TargetUnit || TargetUnit == this)
	{
		return;
	}

	if (CommandComponent)
	{
		CommandComponent->RemoveCommandsByType(ECommandType::Attack);
		CommandComponent->RemoveCommandsByType(ECommandType::Move);
	}

	DestroyMoveMarker();
	ClearMovePath();
	AttackTarget = TargetUnit;
	ActiveCommand.CommandType = ECommandType::Attack;
	ActiveCommand.Priority = Priority;
	ActiveCommand.TargetLocation = TargetUnit->GetActorLocation();
	ActiveCommand.bDirectCommand = true;
	bHasActiveCommand = true;
	RuntimeState = EUnitRuntimeState::Attack;
	LastCombatEvent = FString::Printf(TEXT("%s 锁定目标 %s。"), *UnitLabel, *TargetUnit->UnitLabel);
}

ATacticalMapGrid* ABattleUnit::ResolveTacticalMapGrid() const
{
	if (CachedMapGrid.IsValid())
	{
		return CachedMapGrid.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ATacticalMapGrid> It(World); It; ++It)
	{
		ATacticalMapGrid* Grid = *It;
		if (Grid)
		{
			CachedMapGrid = Grid;
			return Grid;
		}
	}

	return nullptr;
}

bool ABattleUnit::TryGetTraversalRuleAt(const FVector& WorldLocation, FTerrainTraversalRule& OutRule) const
{
	ATacticalMapGrid* Grid = ResolveTacticalMapGrid();
	if (!Grid)
	{
		OutRule = FTerrainTraversalRule();
		return false;
	}

	FTerrainCellState Cell;
	if (!Grid->GetCellStateAtWorldXY(WorldLocation.X, WorldLocation.Y, Cell))
	{
		OutRule = FTerrainTraversalRule();
		return false;
	}

	if (const FTerrainTraversalRule* Rule = Cell.TraversalRules.Find(MobilityType))
	{
		OutRule = *Rule;
		return true;
	}

	OutRule = FTerrainTraversalRule();
	return false;
}

bool ABattleUnit::IsWaterTerrainType(ETerrainType TerrainType) const
{
	return TerrainType == ETerrainType::River
		|| TerrainType == ETerrainType::Swamp
		|| TerrainType == ETerrainType::ShallowWater
		|| TerrainType == ETerrainType::OpenWater
		|| TerrainType == ETerrainType::DeepWater;
}

bool ABattleUnit::IsInfrastructureTerrainType(ETerrainType TerrainType) const
{
	return TerrainType == ETerrainType::Road
		|| TerrainType == ETerrainType::Bridge
		|| TerrainType == ETerrainType::Railway;
}

bool ABattleUnit::CanLandTraverseWaterCell(const FVector& WorldLocation) const
{
	ATacticalMapGrid* Grid = ResolveTacticalMapGrid();
	if (!Grid)
	{
		return false;
	}

	const FIntPoint TargetCellId = Grid->WorldToCell(WorldLocation);
	FTerrainCellState TargetCell;
	if (!Grid->TryGetCellStateById(TargetCellId, TargetCell) || !IsWaterTerrainType(TargetCell.TerrainType))
	{
		return false;
	}

	// Rule 1/4: land can enter water cell if adjacent to land,
	// and also allow crossings where adjacent infrastructure (road/bridge/railway) exists.
	static const FIntPoint NeighborOffsets[] = {
		FIntPoint(1, 0),
		FIntPoint(-1, 0),
		FIntPoint(0, 1),
		FIntPoint(0, -1)
	};
	for (const FIntPoint& Offset : NeighborOffsets)
	{
		const FIntPoint NeighborId(TargetCellId.X + Offset.X, TargetCellId.Y + Offset.Y);
		FTerrainCellState NeighborCell;
		if (!Grid->TryGetCellStateById(NeighborId, NeighborCell))
		{
			continue;
		}

		if (!IsWaterTerrainType(NeighborCell.TerrainType) || IsInfrastructureTerrainType(NeighborCell.TerrainType))
		{
			return true;
		}
	}
	return false;
}

bool ABattleUnit::IsTraversableAt(const FVector& WorldLocation) const
{
	FTerrainTraversalRule Rule;
	if (!TryGetTraversalRuleAt(WorldLocation, Rule))
	{
		return true;
	}
	if (Rule.bCanTraverse)
	{
		return true;
	}

	return false;
}

bool ABattleUnit::TryResolveSlidingMoveStep(const FVector& FromWorld, const FVector& DesiredDeltaXY, FVector& OutAcceptedDeltaXY) const
{
	FVector D = FVector(DesiredDeltaXY.X, DesiredDeltaXY.Y, 0.0f);
	const float LenSq = D.SizeSquared2D();
	if (LenSq <= KINDA_SMALL_NUMBER)
	{
		OutAcceptedDeltaXY = FVector::ZeroVector;
		return false;
	}

	const float Len = FMath::Sqrt(LenSq);
	const FVector Dir = D / Len;

	if (IsTraversableAt(FromWorld + D))
	{
		OutAcceptedDeltaXY = D;
		return true;
	}

	const FVector DX(D.X, 0.0f, 0.0f);
	if (!FMath::IsNearlyZero(D.X) && IsTraversableAt(FromWorld + DX))
	{
		OutAcceptedDeltaXY = DX;
		return true;
	}

	const FVector DY(0.0f, D.Y, 0.0f);
	if (!FMath::IsNearlyZero(D.Y) && IsTraversableAt(FromWorld + DY))
	{
		OutAcceptedDeltaXY = DY;
		return true;
	}

	float Lo = 0.0f;
	float Hi = Len;
	for (int32 Iter = 0; Iter < 12; ++Iter)
	{
		const float Mid = (Lo + Hi) * 0.5f;
		if (IsTraversableAt(FromWorld + Dir * Mid))
		{
			Lo = Mid;
		}
		else
		{
			Hi = Mid;
		}
	}

	if (Lo > 2.0f)
	{
		OutAcceptedDeltaXY = Dir * Lo;
		return true;
	}

	OutAcceptedDeltaXY = FVector::ZeroVector;
	return false;
}

float ABattleUnit::GetSpeedMultiplierAt(const FVector& WorldLocation) const
{
	FTerrainTraversalRule Rule;
	if (TryGetTraversalRuleAt(WorldLocation, Rule))
	{
		return FMath::Max(0.1f, Rule.SpeedMultiplier);
	}
	return 1.0f;
}

float ABattleUnit::GetHoverCircleRadius() const
{
	float UnitRadius = MinHoverCircleRadius;
	if (UnitMesh)
	{
		UnitRadius = FMath::Max(UnitRadius, UnitMesh->Bounds.SphereRadius);
	}
	else
	{
		FVector Origin = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		GetActorBounds(true, Origin, Extent);
		UnitRadius = FMath::Max(UnitRadius, Extent.Size());
	}

	return UnitRadius * HoverCircleScale;
}

bool ABattleUnit::AcquireNextCommand()
{
	if (!CommandComponent)
	{
		return false;
	}

	FActiveCommand Next;
	while (CommandComponent->TryPopNextCommand(Next))
	{
		if (Next.CommandType != ECommandType::Move)
		{
			ActiveCommand = Next;
			ClearMovePath();
			bHasActiveCommand = true;
			return true;
		}

		ActiveCommand = Next;
		if (TryPopulateMovePathFromGoal(Next.TargetLocation))
		{
			MovePathPointIndex = 0;
			bHasActiveCommand = true;
			SpawnOrReplaceMoveMarker(ActiveCommand.TargetLocation);
			return true;
		}

		LastCombatEvent = TEXT("没有可靠通行路径");
	}

	return false;
}

void ABattleUnit::ClearMovePath()
{
	MovePathWorldWaypoints.Reset();
	MovePathPointIndex = 0;
}

bool ABattleUnit::TryPopulateMovePathFromGoal(const FVector& GoalWorld)
{
	MovePathWorldWaypoints.Reset();
	MovePathPointIndex = 0;

	if (ATacticalMapGrid* Grid = ResolveTacticalMapGrid())
	{
		if (!Grid->FindShortestMovePath(MobilityType, GetActorLocation(), GoalWorld, MovePathWorldWaypoints))
		{
			return false;
		}

		const float TrimDist = FMath::Max(50.0f, Grid->GetTileSizeUU() * 0.4f);
		while (MovePathWorldWaypoints.Num() > 0 && FVector::Dist2D(GetActorLocation(), MovePathWorldWaypoints[0]) < TrimDist)
		{
			MovePathWorldWaypoints.RemoveAt(0);
		}

		if (MovePathWorldWaypoints.Num() == 0)
		{
			if (FVector::Dist2D(GetActorLocation(), GoalWorld) < TrimDist)
			{
				MovePathWorldWaypoints.Add(GoalWorld);
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	MovePathWorldWaypoints.Add(GoalWorld);
	return true;
}

void ABattleUnit::ProcessActiveCommand(float DeltaSeconds)
{
	if (!bHasActiveCommand)
	{
		bHasActiveCommand = AcquireNextCommand();
	}

	if (!bHasActiveCommand)
	{
		if (ReloadRemaining <= 0.0f && RuntimeState != EUnitRuntimeState::Dead)
		{
			RuntimeState = EUnitRuntimeState::Idle;
		}
		return;
	}

	if (ActiveCommand.CommandType != ECommandType::Move)
	{
		if (ActiveCommand.CommandType == ECommandType::Attack)
		{
			ProcessAttackCommand(DeltaSeconds);
			return;
		}

		bHasActiveCommand = false;
		RuntimeState = EUnitRuntimeState::Idle;
		return;
	}
	RuntimeState = EUnitRuntimeState::Move;

	const FVector CurrentLocation = GetActorLocation();
	FVector SubGoal = ActiveCommand.TargetLocation;
	if (MovePathWorldWaypoints.Num() > 0 && MovePathPointIndex < MovePathWorldWaypoints.Num())
	{
		SubGoal = MovePathWorldWaypoints[MovePathPointIndex];
	}

	const FVector Delta = SubGoal - CurrentLocation;
	const float Distance2D = FVector::Dist2D(CurrentLocation, SubGoal);
	if (Distance2D <= 10.0f)
	{
		SetActorLocation(SubGoal, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
		if (MovePathWorldWaypoints.Num() > 0)
		{
			MovePathPointIndex++;
			if (MovePathPointIndex >= MovePathWorldWaypoints.Num())
			{
				DestroyMoveMarker();
				bHasActiveCommand = false;
				ClearMovePath();
				RuntimeState = EUnitRuntimeState::Idle;
			}
		}
		else
		{
			DestroyMoveMarker();
			bHasActiveCommand = false;
			ClearMovePath();
			RuntimeState = EUnitRuntimeState::Idle;
		}
		return;
	}

	FVector CandidateStep = Delta.GetSafeNormal() * (MoveSpeed * GetSpeedMultiplierAt(CurrentLocation)) * DeltaSeconds;
	CandidateStep.Z = 0.0f;
	FVector Step = CandidateStep;
	if (!TryResolveSlidingMoveStep(CurrentLocation, CandidateStep, Step) || Step.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		bHasActiveCommand = false;
		ClearMovePath();
		DestroyMoveMarker();
		RuntimeState = EUnitRuntimeState::Idle;
		LastCombatEvent = FString::Printf(TEXT("%s 当前地形不可通行，机动命令中止。"), *UnitLabel);
		return;
	}

	if (Step.Size2D() >= Distance2D)
	{
		SetActorLocation(SubGoal, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
		if (MovePathWorldWaypoints.Num() > 0)
		{
			MovePathPointIndex++;
			if (MovePathPointIndex >= MovePathWorldWaypoints.Num())
			{
				DestroyMoveMarker();
				bHasActiveCommand = false;
				ClearMovePath();
				RuntimeState = EUnitRuntimeState::Idle;
			}
		}
		else
		{
			DestroyMoveMarker();
			bHasActiveCommand = false;
			ClearMovePath();
			RuntimeState = EUnitRuntimeState::Idle;
		}
	}
	else
	{
		SetActorLocation(CurrentLocation + Step, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
	}
}

void ABattleUnit::ProcessAttackCommand(float DeltaSeconds)
{
	if (ReloadRemaining > 0.0f)
	{
		RuntimeState = EUnitRuntimeState::Reload;
		return;
	}

	if (!AttackTarget || !AttackTarget->IsAlive())
	{
		bHasActiveCommand = false;
		LastCombatEvent = FString::Printf(TEXT("%s 丢失攻击目标。"), *UnitLabel);
		AttackTarget = nullptr;
		RuntimeState = EUnitRuntimeState::Idle;
		return;
	}

	RuntimeState = EUnitRuntimeState::Attack;

	const float Distance = FVector::Dist2D(GetActorLocation(), AttackTarget->GetActorLocation());
	const float MaxAcquireRange = DetectionRange;
	if (!bFriendly && Distance > MaxAcquireRange)
	{
		LastCombatEvent = FString::Printf(TEXT("%s 目标超出探测范围。"), *UnitLabel);
		AttackTarget = nullptr;
		bHasActiveCommand = false;
		RuntimeState = EUnitRuntimeState::Idle;
		return;
	}

	if (Distance > AttackRange)
	{
		const FVector CurrentLocation = GetActorLocation();
		const FVector Direction = (AttackTarget->GetActorLocation() - CurrentLocation).GetSafeNormal2D();
		FVector CandidateStep = Direction * (MoveSpeed * GetSpeedMultiplierAt(CurrentLocation)) * DeltaSeconds;
		CandidateStep.Z = 0.0f;
		FVector Step = CandidateStep;
		if (!TryResolveSlidingMoveStep(CurrentLocation, CandidateStep, Step) || Step.SizeSquared2D() <= KINDA_SMALL_NUMBER)
		{
			LastCombatEvent = FString::Printf(TEXT("%s 无法穿越当前地形，追击中止。"), *UnitLabel);
			bHasActiveCommand = false;
			RuntimeState = EUnitRuntimeState::Idle;
			return;
		}
		SetActorLocation(CurrentLocation + Step, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
		return;
	}

	if (AttackCooldownRemaining > 0.0f)
	{
		return;
	}

	if (CurrentAmmo <= 0)
	{
		StartReload();
		return;
	}

	AttackTarget->ApplyDamageValue(AttackDamage);
	CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
	AttackCooldownRemaining = AttackCooldown;
	LastCombatEvent = FString::Printf(TEXT("%s 命中 %s，造成 %.0f 伤害。"), *UnitLabel, *AttackTarget->UnitLabel, AttackDamage);

	if (CurrentAmmo <= 0)
	{
		StartReload();
	}

	if (!AttackTarget->IsAlive())
	{
		LastCombatEvent = FString::Printf(TEXT("%s 消灭目标。"), *UnitLabel);
		if (ATacticalMapGrid* Grid = ResolveTacticalMapGrid())
		{
			Grid->ApplyFacilityDamage(AttackTarget->GetActorLocation().X, AttackTarget->GetActorLocation().Y, AttackDamage);
		}
		AttackTarget = nullptr;
		bHasActiveCommand = false;
		RuntimeState = EUnitRuntimeState::Idle;
	}
}

void ABattleUnit::SpawnOrReplaceMoveMarker(const FVector& TargetLocation)
{
	DestroyMoveMarker();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	MoveCommandMarker = World->SpawnActor<AMoveCommandMarkerActor>(AMoveCommandMarkerActor::StaticClass(), TargetLocation, FRotator::ZeroRotator);
	if (MoveCommandMarker)
	{
		MoveCommandMarker->SetMarkerVisible(IsSelected());
	}
}

void ABattleUnit::DestroyMoveMarker()
{
	if (!MoveCommandMarker)
	{
		return;
	}

	MoveCommandMarker->Destroy();
	MoveCommandMarker = nullptr;
}

void ABattleUnit::EnforceMinimumUnitSpacing()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector CurrentLocation = GetActorLocation();
	const float MyRadius = GetHoverCircleRadius();
	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* Other = *It;
		if (!Other || Other == this)
		{
			continue;
		}

		const FVector OtherLocation = Other->GetActorLocation();
		const float MinAllowedDistance = MyRadius + Other->GetHoverCircleRadius();
		FVector Delta = CurrentLocation - OtherLocation;
		Delta.Z = 0.0f;
		float Dist2D = Delta.Size();
		if (Dist2D >= MinAllowedDistance)
		{
			continue;
		}

		FVector PushDir = Delta.GetSafeNormal2D();
		if (Dist2D <= KINDA_SMALL_NUMBER)
		{
			const float RandomYaw = FMath::FRandRange(0.0f, 2.0f * PI);
			PushDir = FVector(FMath::Cos(RandomYaw), FMath::Sin(RandomYaw), 0.0f);
			Dist2D = 0.0f;
		}

		const float PushDistance = MinAllowedDistance - Dist2D;
		FVector PushDelta = PushDir * PushDistance;
		PushDelta.Z = 0.0f;
		FVector NewLocation = CurrentLocation + PushDelta;
		if (!IsTraversableAt(NewLocation))
		{
			float Lo = 0.0f;
			float Hi = PushDistance;
			for (int32 Bin = 0; Bin < 10; ++Bin)
			{
				const float Mid = (Lo + Hi) * 0.5f;
				const FVector TryLoc = CurrentLocation + PushDir * Mid;
				if (IsTraversableAt(TryLoc))
				{
					Lo = Mid;
				}
				else
				{
					Hi = Mid;
				}
			}
			if (Lo <= 2.0f)
			{
				continue;
			}
			NewLocation = CurrentLocation + PushDir * Lo;
		}
		SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
		CurrentLocation = NewLocation;
	}
}

void ABattleUnit::TryAutoEngage(float DeltaSeconds)
{
	if (bFriendly || !IsAlive() || bHasActiveCommand || CurrentAmmo <= 0 || ReloadRemaining > 0.0f)
	{
		return;
	}

	if (AutoEngageScanCooldown > 0.0f)
	{
		return;
	}

	AutoEngageScanCooldown = AutoEngageScanInterval;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ABattleUnit* BestTarget = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* Candidate = *It;
		if (!Candidate || Candidate == this || !Candidate->IsAlive() || !Candidate->bFriendly)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
		// Enemy AI uses full DetectionRange. Friendly Jammed (e.g. inside this unit's ECM) must not shrink
		// acquisition below the authored range, or units never engage while still "inside" the HUD ring.
		const float MaxPerceiveRange = DetectionRange;
		if (DistSq > FMath::Square(MaxPerceiveRange))
		{
			continue;
		}

		if (DistSq < BestDistanceSq)
		{
			BestDistanceSq = DistSq;
			BestTarget = Candidate;
		}
	}

	if (BestTarget)
	{
		IssueAttackCommandInterrupt(BestTarget, ECommandPriority::High);
	}
}

void ABattleUnit::StartReload()
{
	if (ReloadRemaining > 0.0f || MaxAmmo <= 0)
	{
		return;
	}

	ReloadRemaining = FMath::Max(0.1f, ReloadDuration);
	RuntimeState = EUnitRuntimeState::Reload;
	LastCombatEvent = FString::Printf(TEXT("%s 弹药耗尽，开始重装填。"), *UnitLabel);
}

void ABattleUnit::UpdateMeshScaleVisual()
{
	if (!UnitMesh)
	{
		return;
	}

	// Uniform scale; selection does not change model size.
	const float Core = 0.22f;
	FVector BaseScale = FVector(Core);
	if (RuntimeState == EUnitRuntimeState::Dead)
	{
		BaseScale *= 0.75f;
	}

	if (HitFlashRemaining > 0.0f)
	{
		BaseScale *= 1.1f;
	}

	UnitMesh->SetWorldScale3D(BaseScale);
}

ABattleVehicleUnit::ABattleVehicleUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TacticalBrain = ObjectInitializer.CreateDefaultSubobject<UBattleEnemyTacticalBrainComponent>(this, TEXT("TacticalBrain"));
	if (EcmZone)
	{
		// 设为 >0 时由 GameMode 在每帧 PostActorTick 中统一结算友军 Jam（见 UBattleECMZoneComponent::UpdateFriendlyJamForWorld）。
		EcmZone->JamRadiusUU = 0.0f;
		EcmZone->bAffectsFriendliesOnly = true;
	}
}

