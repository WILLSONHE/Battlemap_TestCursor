#include "BattleUnit.h"
#include "BattleCommandComponent.h"
#include "BattleDetectionComponent.h"
#include "BattleCommsComponent.h"
#include "BattleSupplyComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

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

	CommandComponent = CreateDefaultSubobject<UBattleCommandComponent>(TEXT("CommandComponent"));
	DetectionComponent = CreateDefaultSubobject<UBattleDetectionComponent>(TEXT("DetectionComponent"));
	CommsComponent = CreateDefaultSubobject<UBattleCommsComponent>(TEXT("CommsComponent"));
	SupplyComponent = CreateDefaultSubobject<UBattleSupplyComponent>(TEXT("SupplyComponent"));
}

void ABattleUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
		bHasActiveCommand = false;
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector Delta = ActiveCommand.TargetLocation - CurrentLocation;
	const float Distance = Delta.Size();
	if (Distance <= 10.0f)
	{
		SetActorLocation(ActiveCommand.TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		bHasActiveCommand = false;
		return;
	}

	const FVector Step = Delta.GetSafeNormal() * MoveSpeed * DeltaSeconds;
	if (Step.Size() >= Distance)
	{
		SetActorLocation(ActiveCommand.TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		bHasActiveCommand = false;
	}
	else
	{
		SetActorLocation(CurrentLocation + Step, false, nullptr, ETeleportType::TeleportPhysics);
	}
}
