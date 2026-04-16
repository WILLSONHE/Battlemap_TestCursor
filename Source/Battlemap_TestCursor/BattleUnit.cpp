#include "BattleUnit.h"
#include "BattleCommandComponent.h"
#include "BattleDetectionComponent.h"
#include "BattleCommsComponent.h"
#include "BattleSupplyComponent.h"
#include "MoveCommandMarkerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TacticalMapGrid.h"

ABattleUnit::ABattleUnit()
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
	UnitMesh->SetCastShadow(false);
	UnitMesh->bCastDynamicShadow = false;
	UnitMesh->bCastStaticShadow = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		UnitMesh->SetStaticMesh(CubeMesh.Object);
		UnitMesh->SetWorldScale3D(FVector(0.8f, 0.8f, 0.25f));
	}

	SetActorEnableCollision(true);
	MoveCommandMarker = nullptr;
	RuntimeState = EUnitRuntimeState::Idle;
	LastCombatEvent = TEXT("无");

	CommandComponent = CreateDefaultSubobject<UBattleCommandComponent>(TEXT("CommandComponent"));
	DetectionComponent = CreateDefaultSubobject<UBattleDetectionComponent>(TEXT("DetectionComponent"));
	CommsComponent = CreateDefaultSubobject<UBattleCommsComponent>(TEXT("CommsComponent"));
	SupplyComponent = CreateDefaultSubobject<UBattleSupplyComponent>(TEXT("SupplyComponent"));
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
	UpdateMeshScaleVisual();
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
	const FVector Scale = bSelected ? FVector(0.95f, 0.95f, 0.3f) : FVector(0.8f, 0.8f, 0.25f);
	UnitMesh->SetWorldScale3D(Scale);

	if (MoveCommandMarker)
	{
		MoveCommandMarker->SetMarkerVisible(bSelected);
	}
}

void ABattleUnit::IssueMoveCommandInterrupt(const FVector& TargetLocation, ECommandPriority Priority)
{
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
	RuntimeState = EUnitRuntimeState::Move;
	LastCombatEvent = FString::Printf(TEXT("%s 执行机动命令。"), *UnitLabel);
	SpawnOrReplaceMoveMarker(TargetLocation);
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

	if (MobilityType == EUnitMobilityType::Land && CanLandTraverseWaterCell(WorldLocation))
	{
		return true;
	}

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

	return CommandComponent->TryPopNextCommand(ActiveCommand);
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
	const FVector Delta = ActiveCommand.TargetLocation - CurrentLocation;
	const float Distance = Delta.Size();
	if (Distance <= 10.0f)
	{
		SetActorLocation(ActiveCommand.TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
		DestroyMoveMarker();
		bHasActiveCommand = false;
		RuntimeState = EUnitRuntimeState::Idle;
		return;
	}

	const FVector CandidateStep = Delta.GetSafeNormal() * (MoveSpeed * GetSpeedMultiplierAt(CurrentLocation)) * DeltaSeconds;
	const FVector CandidateLocation = CurrentLocation + CandidateStep;
	if (!IsTraversableAt(CandidateLocation))
	{
		bHasActiveCommand = false;
		RuntimeState = EUnitRuntimeState::Idle;
		LastCombatEvent = FString::Printf(TEXT("%s 当前地形不可通行，机动命令中止。"), *UnitLabel);
		return;
	}

	const FVector Step = CandidateStep;
	if (Step.Size() >= Distance)
	{
		SetActorLocation(ActiveCommand.TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
		DestroyMoveMarker();
		bHasActiveCommand = false;
		RuntimeState = EUnitRuntimeState::Idle;
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
	if (!bFriendly && Distance > DetectionRange)
	{
		LastCombatEvent = FString::Printf(TEXT("%s 目标超出探测范围。"), *UnitLabel);
		AttackTarget = nullptr;
		bHasActiveCommand = false;
		RuntimeState = EUnitRuntimeState::Idle;
		return;
	}

	if (Distance > AttackRange)
	{
		const FVector Direction = (AttackTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const FVector CandidateLocation = GetActorLocation() + Direction * (MoveSpeed * GetSpeedMultiplierAt(GetActorLocation())) * DeltaSeconds;
		if (!IsTraversableAt(CandidateLocation))
		{
			LastCombatEvent = FString::Printf(TEXT("%s 无法穿越当前地形，追击中止。"), *UnitLabel);
			bHasActiveCommand = false;
			RuntimeState = EUnitRuntimeState::Idle;
			return;
		}
		SetActorLocation(CandidateLocation, false, nullptr, ETeleportType::TeleportPhysics);
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
		CurrentLocation += PushDir * PushDistance;
		SetActorLocation(CurrentLocation, false, nullptr, ETeleportType::TeleportPhysics);
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
		if (DistSq > FMath::Square(DetectionRange))
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

	FVector BaseScale = bSelected ? FVector(0.95f, 0.95f, 0.3f) : FVector(0.8f, 0.8f, 0.25f);
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
