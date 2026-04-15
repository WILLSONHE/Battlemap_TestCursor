// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battlemap_TestCursorCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TacticalMapGrid.h"

namespace
{
	static float ResolveTerrainReferenceZ(const UWorld* World, const FVector& WorldLocation, float FallbackZ)
	{
		if (!World)
		{
			return FallbackZ;
		}

		for (TActorIterator<ATacticalMapGrid> It(World); It; ++It)
		{
			if (const ATacticalMapGrid* Grid = *It)
			{
				return Grid->GetHeightAtWorldXY(WorldLocation.X, WorldLocation.Y);
			}
		}

		return FallbackZ;
	}
}

ABattlemap_TestCursorCharacter::ABattlemap_TestCursorCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Rotate character to moving direction
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;
	GetCharacterMovement()->GravityScale = 0.0f;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true); // Don't want arm to rotate when character does
	CameraBoom->TargetArmLength = 6000.0f;
	CameraBoom->SetRelativeRotation(FRotator(DefaultCameraPitch, DefaultCameraYaw, 0.f));
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ABattlemap_TestCursorCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!TopDownCameraComponent)
	{
		return;
	}

	// Post-process overrides disabled to avoid unintended darkening.

	const float ClampedDefaultDistance = FMath::Clamp(DefaultVerticalDistanceToPlane, MinVerticalDistanceToPlane, MaxVerticalDistanceToPlane);
	ZoomReferencePlaneZ = ResolveTerrainReferenceZ(GetWorld(), GetActorLocation(), ZoomReferencePlaneZ);
	const float CurrentVerticalDistance = TopDownCameraComponent->GetComponentLocation().Z - ZoomReferencePlaneZ;
	const float ZOffset = ClampedDefaultDistance - CurrentVerticalDistance;
	if (!FMath::IsNearlyZero(ZOffset))
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, ZOffset), false);
	}
}

void ABattlemap_TestCursorCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ABattlemap_TestCursorCharacter::AdjustCameraZoom(float Delta)
{
	if (!TopDownCameraComponent)
	{
		return;
	}

	ZoomReferencePlaneZ = ResolveTerrainReferenceZ(GetWorld(), GetActorLocation(), ZoomReferencePlaneZ);
	const float CurrentVerticalDistance = TopDownCameraComponent->GetComponentLocation().Z - ZoomReferencePlaneZ;
	const float NewVerticalDistance = FMath::Clamp(CurrentVerticalDistance + Delta, MinVerticalDistanceToPlane, MaxVerticalDistanceToPlane);
	const float ZOffset = NewVerticalDistance - CurrentVerticalDistance;
	if (!FMath::IsNearlyZero(ZOffset))
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, ZOffset), false);
	}
}

void ABattlemap_TestCursorCharacter::AdjustCameraYaw(float DeltaYaw)
{
	if (!CameraBoom)
	{
		return;
	}

	const FRotator CurrentRotation = CameraBoom->GetRelativeRotation();
	CameraBoom->SetRelativeRotation(FRotator(CurrentRotation.Pitch, CurrentRotation.Yaw + DeltaYaw, CurrentRotation.Roll));
}

void ABattlemap_TestCursorCharacter::AdjustCameraPitch(float DeltaPitch)
{
	if (!CameraBoom)
	{
		return;
	}

	const FRotator CurrentRotation = CameraBoom->GetRelativeRotation();
	const float NewPitch = FMath::Clamp(CurrentRotation.Pitch + DeltaPitch, -89.0f, -10.0f);
	CameraBoom->SetRelativeRotation(FRotator(NewPitch, CurrentRotation.Yaw, CurrentRotation.Roll));
}

void ABattlemap_TestCursorCharacter::AdjustCameraLookYaw(float DeltaYaw)
{
	if (!TopDownCameraComponent)
	{
		return;
	}

	const FRotator CurrentWorldRotation = TopDownCameraComponent->GetComponentRotation();
	TopDownCameraComponent->SetWorldRotation(FRotator(CurrentWorldRotation.Pitch, CurrentWorldRotation.Yaw + DeltaYaw, 0.0f));
}

void ABattlemap_TestCursorCharacter::AdjustCameraLookPitch(float DeltaPitch)
{
	if (!TopDownCameraComponent)
	{
		return;
	}

	const FRotator CurrentWorldRotation = TopDownCameraComponent->GetComponentRotation();
	const float NewPitch = FMath::Clamp(CurrentWorldRotation.Pitch + DeltaPitch, MinFreeLookPitch, MaxFreeLookPitch);
	TopDownCameraComponent->SetWorldRotation(FRotator(NewPitch, CurrentWorldRotation.Yaw, 0.0f));
}

void ABattlemap_TestCursorCharacter::FocusOnWorldLocation(const FVector& WorldLocation)
{
	SetActorLocation(FVector(WorldLocation.X, WorldLocation.Y, GetActorLocation().Z));
}

void ABattlemap_TestCursorCharacter::PanCamera(const FVector2D& AxisInput, float PanSpeed)
{
	if (AxisInput.IsNearlyZero())
	{
		return;
	}

	const float CameraYaw = TopDownCameraComponent ? TopDownCameraComponent->GetComponentRotation().Yaw : GetActorRotation().Yaw;
	const FRotator CameraYawRotation(0.0f, CameraYaw, 0.0f);
	const FVector Forward = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::Y);
	const FVector Delta = ((Forward * AxisInput.Y) + (Right * AxisInput.X)) * PanSpeed;

	SetActorLocation(GetActorLocation() + FVector(Delta.X, Delta.Y, 0.0f), false);
}

void ABattlemap_TestCursorCharacter::ResetCameraOrientation()
{
	if (!CameraBoom || !TopDownCameraComponent)
	{
		return;
	}

	CameraBoom->SetRelativeRotation(FRotator(DefaultCameraPitch, DefaultCameraYaw, 0.0f));
	TopDownCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
	const FRotator CameraWorldRotation = TopDownCameraComponent->GetComponentRotation();
	TopDownCameraComponent->SetWorldRotation(FRotator(CameraWorldRotation.Pitch, CameraWorldRotation.Yaw, 0.0f));
}

float ABattlemap_TestCursorCharacter::GetVerticalDistanceToReferencePlane() const
{
	if (!TopDownCameraComponent)
	{
		return 0.0f;
	}

	const float DynamicReferenceZ = ResolveTerrainReferenceZ(GetWorld(), GetActorLocation(), ZoomReferencePlaneZ);
	return FMath::Abs(TopDownCameraComponent->GetComponentLocation().Z - DynamicReferenceZ);
}
