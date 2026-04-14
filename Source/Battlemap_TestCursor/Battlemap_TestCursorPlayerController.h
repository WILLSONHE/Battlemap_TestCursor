// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "BattleTypes.h"
#include "Battlemap_TestCursorPlayerController.generated.h"

/** Forward declaration to improve compiling times */
class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class ATacticalMapGrid;
class ABattleUnit;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS()
class ABattlemap_TestCursorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABattlemap_TestCursorPlayerController();

	/** Time Threshold to know if it was a short press */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	float ShortPressThreshold;

	/** FX Class that we will spawn when clicking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UNiagaraSystem* FXCursor;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;
	
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* SetDestinationClickAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* SetDestinationTouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* ZoomAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* RotateMapAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* CommandUnitAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* ToggleCommsAction;

protected:
	/** True if the controlled character should navigate to the mouse cursor. */
	uint32 bMoveToMouseCursor : 1;

	virtual void SetupInputComponent() override;
	
	// To add mapping context
	virtual void BeginPlay();
	virtual void PlayerTick(float DeltaTime) override;

	/** Input handlers for SetDestination action. */
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchTriggered();
	void OnTouchReleased();
	void OnZoomTriggered(const FInputActionValue& Value);
	void OnRotateMapTriggered(const FInputActionValue& Value);
	void OnCommandTriggered();
	void OnToggleCommsTriggered();
	void OnZoomIn();
	void OnZoomOut();
	void OnRotatePressed();
	void OnRotateReleased();
	void OnRotateAxis(float AxisValue);
	void OnFocusSelectedUnit();
	void OnLeftMousePressed();
	void OnLeftMouseReleased();
	void OnRightMousePressed();
	void OnRightMouseReleased();
	void OnMouseXWhilePanning(float AxisValue);
	void OnMouseYWhilePanning(float AxisValue);
	void OnClearSelection();

public:
	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	void SetTacticalMapGrid(ATacticalMapGrid* InTacticalMapGrid);

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	void SetSelectedUnit(ABattleUnit* InSelectedUnit);

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	void SetSelectedUnits(const TArray<ABattleUnit*>& InSelectedUnits);

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	FDebugBattleSnapshot BuildDebugSnapshot() const;

private:
	void UpdateSelectionFromCursor();
	void UpdateBoxSelection();
	void ClearSelectionInternal(bool bClearHint);

	void SetStatusHint(const FString& NewHint);

	FVector CachedDestination;

	bool bIsTouch; // Is it a touch device
	float FollowTime; // For how long it has been pressed

	UPROPERTY(EditAnywhere, Category = "Battle")
	ATacticalMapGrid* TacticalMapGrid;

	UPROPERTY(EditAnywhere, Category = "Battle")
	ABattleUnit* SelectedUnit;

	UPROPERTY()
	TArray<ABattleUnit*> SelectedUnits;

	FString StatusHint;
	bool bRotateHeld = false;
	bool bLeftMouseHeld = false;
	bool bRightMouseHeld = false;
	bool bHasDraggedSelection = false;
	bool bIsBoxSelecting = false;
	FVector2D PendingPanInput = FVector2D::ZeroVector;
	FVector2D LastMouseScreenPosition = FVector2D::ZeroVector;
	bool bHasLastMouseScreenPosition = false;
	FVector2D SelectionBoxStart = FVector2D::ZeroVector;
	FVector2D SelectionBoxEnd = FVector2D::ZeroVector;
	float SelectionDragThreshold = 8.0f;
};


