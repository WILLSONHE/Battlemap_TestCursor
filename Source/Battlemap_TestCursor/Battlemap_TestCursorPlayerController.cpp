// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battlemap_TestCursorPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Battlemap_TestCursorCharacter.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "TacticalMapGrid.h"
#include "BattleUnit.h"
#include "BattleCommandComponent.h"
#include "BattleCommsComponent.h"
#include "BattleSupplyComponent.h"
#include "BattleTypes.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

ABattlemap_TestCursorPlayerController::ABattlemap_TestCursorPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
	ShortPressThreshold = 0.25f;
	TacticalMapGrid = nullptr;
	SelectedUnit = nullptr;
	StatusHint = TEXT("左键选择单位/地面，右键下达移动，Q 切换通讯状态。");
	bRotateHeld = false;
	bLeftMouseHeld = false;
	bHasDraggedSelection = false;
	PendingPanInput = FVector2D::ZeroVector;
	LastMouseScreenPosition = FVector2D::ZeroVector;
	bHasLastMouseScreenPosition = false;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingContextAsset(TEXT("/Game/TopDown/Input/IMC_Default.IMC_Default"));
	if (MappingContextAsset.Succeeded())
	{
		DefaultMappingContext = MappingContextAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ClickActionAsset(TEXT("/Game/TopDown/Input/Actions/IA_SetDestination_Click.IA_SetDestination_Click"));
	if (ClickActionAsset.Succeeded())
	{
		SetDestinationClickAction = ClickActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> TouchActionAsset(TEXT("/Game/TopDown/Input/Actions/IA_SetDestination_Touch.IA_SetDestination_Touch"));
	if (TouchActionAsset.Succeeded())
	{
		SetDestinationTouchAction = TouchActionAsset.Object;
	}
}

void ABattlemap_TestCursorPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	bShowMouseCursor = true;
}

void ABattlemap_TestCursorPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		bHasLastMouseScreenPosition = false;
		return;
	}

	const FVector2D CurrentMousePosition(MouseX, MouseY);
	if (!bHasLastMouseScreenPosition)
	{
		LastMouseScreenPosition = CurrentMousePosition;
		bHasLastMouseScreenPosition = true;
		return;
	}

	if (bLeftMouseHeld)
	{
		const FVector2D MouseDelta = CurrentMousePosition - LastMouseScreenPosition;
		if (!MouseDelta.IsNearlyZero())
		{
			bHasDraggedSelection = true;
			if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
			{
				BattleCharacter->PanCamera(FVector2D(-MouseDelta.X, MouseDelta.Y), 5.0f);
			}
		}
	}

	LastMouseScreenPosition = CurrentMousePosition;
}

void ABattlemap_TestCursorPlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// Add Input Mapping Context
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Setup touch input events
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &ABattlemap_TestCursorPlayerController::OnInputStarted);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &ABattlemap_TestCursorPlayerController::OnTouchTriggered);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &ABattlemap_TestCursorPlayerController::OnTouchReleased);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &ABattlemap_TestCursorPlayerController::OnTouchReleased);

		if (ZoomAction)
		{
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ABattlemap_TestCursorPlayerController::OnZoomTriggered);
		}

		if (RotateMapAction)
		{
			EnhancedInputComponent->BindAction(RotateMapAction, ETriggerEvent::Triggered, this, &ABattlemap_TestCursorPlayerController::OnRotateMapTriggered);
		}

		if (CommandUnitAction)
		{
			EnhancedInputComponent->BindAction(CommandUnitAction, ETriggerEvent::Triggered, this, &ABattlemap_TestCursorPlayerController::OnCommandTriggered);
		}

		if (ToggleCommsAction)
		{
			EnhancedInputComponent->BindAction(ToggleCommsAction, ETriggerEvent::Triggered, this, &ABattlemap_TestCursorPlayerController::OnToggleCommsTriggered);
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnLeftMousePressed);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ABattlemap_TestCursorPlayerController::OnLeftMouseReleased);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnCommandTriggered);
		InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnToggleCommsTriggered);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnClearSelection);
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnZoomIn);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnZoomOut);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnRotatePressed);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &ABattlemap_TestCursorPlayerController::OnRotateReleased);
		InputComponent->BindAxisKey(EKeys::MouseX, this, &ABattlemap_TestCursorPlayerController::OnRotateAxis);
		InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnFocusSelectedUnit);
	}
}

void ABattlemap_TestCursorPlayerController::OnInputStarted()
{
	bLeftMouseHeld = true;
	bHasDraggedSelection = false;
	bShowMouseCursor = true;
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (GetMousePosition(MouseX, MouseY))
	{
		LastMouseScreenPosition = FVector2D(MouseX, MouseY);
		bHasLastMouseScreenPosition = true;
	}
	StopMovement();
}

// Triggered every frame when the input is held down
void ABattlemap_TestCursorPlayerController::OnSetDestinationTriggered()
{
	// We flag that the input is being pressed
	FollowTime += GetWorld()->GetDeltaSeconds();
	
	// We look for the location in the world where the player has pressed the input
	FHitResult Hit;
	bool bHitSuccessful = false;
	if (bIsTouch)
	{
		bHitSuccessful = GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit);
	}
	else
	{
		bHitSuccessful = GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit);
	}

	// If we hit a surface, cache the location
	if (bHitSuccessful)
	{
		CachedDestination = Hit.Location;
	}
}

void ABattlemap_TestCursorPlayerController::OnSetDestinationReleased()
{
	bLeftMouseHeld = false;
	PendingPanInput = FVector2D::ZeroVector;
	bShowMouseCursor = true;

	if (bHasDraggedSelection)
	{
		FollowTime = 0.f;
		return;
	}

	UpdateSelectionFromCursor();

	// If it was a short press
	if (FollowTime <= ShortPressThreshold)
	{
		if (!SelectedUnit)
		{
			UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, CachedDestination);
		}
		else
		{
			SetStatusHint(FString::Printf(TEXT("已选中单位：%s。右键可下达移动命令。"), *SelectedUnit->UnitLabel));
		}

		if (FXCursor)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
		}
	}

	FollowTime = 0.f;
}

void ABattlemap_TestCursorPlayerController::OnLeftMousePressed()
{
	OnInputStarted();
	OnSetDestinationTriggered();
}

void ABattlemap_TestCursorPlayerController::OnLeftMouseReleased()
{
	OnSetDestinationReleased();
}

// Triggered every frame when the input is held down
void ABattlemap_TestCursorPlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void ABattlemap_TestCursorPlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
}

void ABattlemap_TestCursorPlayerController::OnZoomTriggered(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
	{
		BattleCharacter->AdjustCameraZoom(AxisValue * -150.0f);
	}
}

void ABattlemap_TestCursorPlayerController::OnRotateMapTriggered(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
	{
		BattleCharacter->AdjustCameraYaw(AxisValue);
	}
}

void ABattlemap_TestCursorPlayerController::OnCommandTriggered()
{
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
	{
		CachedDestination = Hit.Location;
	}

	if (!SelectedUnit
		|| !SelectedUnit->CommandComponent
		|| !SelectedUnit->CommsComponent
		|| !SelectedUnit->CommsComponent->IsCommandEnabled())
	{
		SetStatusHint(TEXT("命令失败：当前未选中单位或单位通讯中断。"));
		return;
	}

	FActiveCommand MoveCommand;
	MoveCommand.CommandType = ECommandType::Move;
	MoveCommand.Priority = ECommandPriority::High;
	MoveCommand.TargetLocation = CachedDestination;
	MoveCommand.TargetLocation.Z = SelectedUnit->GetActorLocation().Z;
	MoveCommand.bDirectCommand = true;
	SelectedUnit->CommandComponent->EnqueueCommand(MoveCommand);
	SetStatusHint(FString::Printf(TEXT("已向 %s 下达移动命令。"), *SelectedUnit->UnitLabel));
}

void ABattlemap_TestCursorPlayerController::OnToggleCommsTriggered()
{
	if (!SelectedUnit || !SelectedUnit->CommsComponent)
	{
		SetStatusHint(TEXT("没有选中单位，无法切换通讯状态。"));
		return;
	}

	ECommsState& State = SelectedUnit->CommsComponent->CommsChannel.State;
	switch (State)
	{
	case ECommsState::Online:
		State = ECommsState::Jammed;
		break;
	case ECommsState::Jammed:
		State = ECommsState::Lost;
		break;
	default:
		State = ECommsState::Online;
		break;
	}

	const UEnum* EnumPtr = StaticEnum<ECommsState>();
	const FString CommsStateName = EnumPtr ? EnumPtr->GetNameStringByValue(static_cast<int64>(State)) : TEXT("Unknown");
	SetStatusHint(FString::Printf(TEXT("%s 通讯状态切换为 %s。"), *SelectedUnit->UnitLabel, *CommsStateName));
}

void ABattlemap_TestCursorPlayerController::SetTacticalMapGrid(ATacticalMapGrid* InTacticalMapGrid)
{
	TacticalMapGrid = InTacticalMapGrid;
}

void ABattlemap_TestCursorPlayerController::SetSelectedUnit(ABattleUnit* InSelectedUnit)
{
	if (SelectedUnit == InSelectedUnit)
	{
		return;
	}

	if (SelectedUnit)
	{
		SelectedUnit->SetSelected(false);
	}

	SelectedUnit = InSelectedUnit;

	if (SelectedUnit)
	{
		SelectedUnit->SetSelected(true);
	}
}

FDebugBattleSnapshot ABattlemap_TestCursorPlayerController::BuildDebugSnapshot() const
{
	FDebugBattleSnapshot Snapshot;
	Snapshot.LastHint = StatusHint;
	if (!SelectedUnit)
	{
		return Snapshot;
	}

	Snapshot.SelectedUnitName = SelectedUnit->UnitLabel;
	Snapshot.SelectedUnitLocation = SelectedUnit->GetActorLocation();
	Snapshot.Health = SelectedUnit->CurrentHealth;

	if (SelectedUnit->CommsComponent)
	{
		Snapshot.CommsState = SelectedUnit->CommsComponent->CommsChannel.State;
	}

	if (SelectedUnit->SupplyComponent)
	{
		Snapshot.Food = SelectedUnit->SupplyComponent->Food;
		Snapshot.Fuel = SelectedUnit->SupplyComponent->Fuel;
	}

	return Snapshot;
}

void ABattlemap_TestCursorPlayerController::UpdateSelectionFromCursor()
{
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
	{
		return;
	}

	if (ABattleUnit* HitUnit = Cast<ABattleUnit>(Hit.GetActor()))
	{
		SetSelectedUnit(HitUnit);
		CachedDestination = HitUnit->GetActorLocation();
		return;
	}

	SetSelectedUnit(nullptr);
	CachedDestination = Hit.Location;
	SetStatusHint(TEXT("已取消选择，并记录新的地面目标点。"));
}

void ABattlemap_TestCursorPlayerController::SetStatusHint(const FString& NewHint)
{
	StatusHint = NewHint;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, StatusHint);
	}
}

void ABattlemap_TestCursorPlayerController::OnZoomIn()
{
	if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
	{
		BattleCharacter->AdjustCameraZoom(-200.0f);
	}
}

void ABattlemap_TestCursorPlayerController::OnZoomOut()
{
	if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
	{
		BattleCharacter->AdjustCameraZoom(200.0f);
	}
}

void ABattlemap_TestCursorPlayerController::OnRotatePressed()
{
	bRotateHeld = true;
}

void ABattlemap_TestCursorPlayerController::OnRotateReleased()
{
	bRotateHeld = false;
}

void ABattlemap_TestCursorPlayerController::OnRotateAxis(float AxisValue)
{
	if (!bRotateHeld)
	{
		return;
	}

	if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
	{
		BattleCharacter->AdjustCameraYaw(AxisValue * 2.0f);
	}
}

void ABattlemap_TestCursorPlayerController::OnFocusSelectedUnit()
{
	if (!SelectedUnit)
	{
		SetStatusHint(TEXT("当前没有选中单位。"));
		return;
	}

	if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
	{
		BattleCharacter->FocusOnWorldLocation(SelectedUnit->GetActorLocation());
		SetStatusHint(FString::Printf(TEXT("镜头已聚焦到 %s。"), *SelectedUnit->UnitLabel));
	}
}

void ABattlemap_TestCursorPlayerController::OnMouseXWhilePanning(float AxisValue)
{
	// Deprecated: camera panning now uses PlayerTick mouse delta.
}

void ABattlemap_TestCursorPlayerController::OnMouseYWhilePanning(float AxisValue)
{
	// Deprecated: camera panning now uses PlayerTick mouse delta.
}

void ABattlemap_TestCursorPlayerController::OnClearSelection()
{
	SetSelectedUnit(nullptr);
	SetStatusHint(TEXT("已清除当前选中单位。"));
}
