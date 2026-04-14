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

	// Called every frame.
	virtual void Tick(float DeltaSeconds) override;

	void AdjustCameraZoom(float Delta);

	void AdjustCameraYaw(float DeltaYaw);

	void FocusOnWorldLocation(const FVector& WorldLocation);

	void PanCamera(const FVector2D& AxisInput, float PanSpeed);

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
	float MinZoomLength = 900.0f;

	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float MaxZoomLength = 8000.0f;
};

