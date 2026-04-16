// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Battlemap_TestCursorCharacter.generated.h"

UCLASS(Blueprintable)
class ABattlemap_TestCursorCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABattlemap_TestCursorCharacter();

	virtual void BeginPlay() override;

	// Called every frame.
	virtual void Tick(float DeltaSeconds) override;

	void AdjustCameraZoom(float Delta);

	void AdjustCameraYaw(float DeltaYaw);

	void AdjustCameraPitch(float DeltaPitch);

	void AdjustCameraLookYaw(float DeltaYaw);

	void AdjustCameraLookPitch(float DeltaPitch);

	void FocusOnWorldLocation(const FVector& WorldLocation);

	void PanCamera(const FVector2D& AxisInput, float PanSpeed);

	void ResetCameraOrientation();

	float GetVerticalDistanceToReferencePlane() const;

	/** Returns TopDownCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

private:
	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float MinVerticalDistanceToPlane = 500.0f;

	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float MaxVerticalDistanceToPlane = 15000.0f;

	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float ZoomReferencePlaneZ = 0.0f;

	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float DefaultVerticalDistanceToPlane = 8000.0f;

	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float DefaultCameraPitch = -90.0f;

	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float DefaultCameraYaw = 0.0f;

	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float MinFreeLookPitch = -89.0f;

	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float MaxFreeLookPitch = 89.0f;
};
