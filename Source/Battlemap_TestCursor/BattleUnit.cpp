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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		UnitMesh->SetStaticMesh(CubeMesh.Object);
		UnitMesh->SetWorldScale3D(FVector(0.8f, 0.8f, 0.25f));
	}

	SetActorEnableCollision(true);
	MoveCommandMarker = nullptr;

	CommandComponent = CreateDefaultSubobject<UBattleCommandComponent>(TEXT("CommandComponent"));
	DetectionComponent = CreateDefaultSubobject<UBattleDetectionComponent>(TEXT("DetectionComponent"));
	CommsComponent = CreateDefaultSubobject<UBattleCommsComponent>(TEXT("CommsComponent"));
	SupplyComponent = CreateDefaultSubobject<UBattleSupplyComponent>(TEXT("SupplyComponent"));
}

void ABattleUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaSeconds);
	AutoEngageScanCooldown = FMath::Max(0.0f, AutoEngageScanCooldown - DeltaSeconds);
	ProcessActiveCommand(DeltaSeconds);
	TryAutoEngage(DeltaSeconds);
}

void ABattleUnit::ApplyDamageValue(float DamageValue)
{
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageValue);
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
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector Delta = ActiveCommand.TargetLocation - CurrentLocation;
	const float Distance = Delta.Size();
	if (Distance <= 10.0f)
	{
		SetActorLocation(ActiveCommand.TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
		DestroyMoveMarker();
		bHasActiveCommand = false;
		return;
	}

	const FVector Step = Delta.GetSafeNormal() * MoveSpeed * DeltaSeconds;
	if (Step.Size() >= Distance)
	{
		SetActorLocation(ActiveCommand.TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
		DestroyMoveMarker();
		bHasActiveCommand = false;
	}
	else
	{
		SetActorLocation(CurrentLocation + Step, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
	}
}

void ABattleUnit::ProcessAttackCommand(float DeltaSeconds)
{
	if (!AttackTarget || !AttackTarget->IsAlive())
	{
		bHasActiveCommand = false;
		AttackTarget = nullptr;
		return;
	}

	const float Distance = FVector::Dist2D(GetActorLocation(), AttackTarget->GetActorLocation());
	if (!bFriendly && Distance > DetectionRange)
	{
		AttackTarget = nullptr;
		bHasActiveCommand = false;
		return;
	}

	if (Distance > AttackRange)
	{
		const FVector Direction = (AttackTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		SetActorLocation(GetActorLocation() + Direction * MoveSpeed * DeltaSeconds, false, nullptr, ETeleportType::TeleportPhysics);
		EnforceMinimumUnitSpacing();
		return;
	}

	if (AttackCooldownRemaining > 0.0f)
	{
		return;
	}

	if (CurrentAmmo <= 0)
	{
		bHasActiveCommand = false;
		return;
	}

	AttackTarget->ApplyDamageValue(AttackDamage);
	CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
	AttackCooldownRemaining = AttackCooldown;

	if (!AttackTarget->IsAlive())
	{
		AttackTarget = nullptr;
		bHasActiveCommand = false;
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
	if (bFriendly || !IsAlive() || bHasActiveCommand || CurrentAmmo <= 0)
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
