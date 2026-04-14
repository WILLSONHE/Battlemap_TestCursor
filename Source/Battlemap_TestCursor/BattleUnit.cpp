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
	ProcessActiveCommand(DeltaSeconds);
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
		DestroyMoveMarker();
		bHasActiveCommand = false;
		return;
	}

	const FVector Step = Delta.GetSafeNormal() * MoveSpeed * DeltaSeconds;
	if (Step.Size() >= Distance)
	{
		SetActorLocation(ActiveCommand.TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		DestroyMoveMarker();
		bHasActiveCommand = false;
	}
	else
	{
		SetActorLocation(CurrentLocation + Step, false, nullptr, ETeleportType::TeleportPhysics);
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
	if (Distance > AttackRange)
	{
		const FVector Direction = (AttackTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		SetActorLocation(GetActorLocation() + Direction * MoveSpeed * DeltaSeconds, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	if (AttackCooldownRemaining > 0.0f)
	{
		return;
	}

	AttackTarget->ApplyDamageValue(AttackDamage);
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
