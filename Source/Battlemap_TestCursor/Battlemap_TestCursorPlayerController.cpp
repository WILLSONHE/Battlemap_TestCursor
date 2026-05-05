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
#include "Battlemap_TestCursorGameMode.h"
#include "BattleGameInstance.h"
#include "BattleBalanceTableTypes.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Camera/CameraComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Light.h"
#include "Components/LightComponent.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

ABattlemap_TestCursorPlayerController::ABattlemap_TestCursorPlayerController()
{
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;
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
	bRightMouseHeld = false;
	bAltHeld = false;
	bHasDraggedSelection = false;
	bIsBoxSelecting = false;
	PendingPanInput = FVector2D::ZeroVector;
	LastMouseScreenPosition = FVector2D::ZeroVector;
	bHasLastMouseScreenPosition = false;
	SelectionBoxStart = FVector2D::ZeroVector;
	SelectionBoxEnd = FVector2D::ZeroVector;
	RightClickPressScreenPosition = FVector2D::ZeroVector;
	LastAttackTarget = nullptr;
	FogUpdateCooldown = 0.0f;
	FogUpdateInterval = 0.2f;
	CachedVisibleEnemyCount = 0;

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

	// Keep scene lighting/shadows enabled. We control shadow behavior per-mesh.
}

void ABattlemap_TestCursorPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateFogOfWar(DeltaTime);

	float MouseX = 0.0f;
	float MouseY = 0.0f;

	if (!FMath::IsNearlyZero(KeyboardForwardInput) || !FMath::IsNearlyZero(KeyboardRightInput))
	{
		if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
		{
			const FVector2D KeyboardPanInput(KeyboardRightInput, KeyboardForwardInput);
			BattleCharacter->PanCamera(KeyboardPanInput, KeyboardPanSpeed);
		}
	}

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

	if (bRightMouseHeld)
	{
		const FVector2D MouseDelta = CurrentMousePosition - LastMouseScreenPosition;
		const float RightDragDistance = FVector2D::Distance(CurrentMousePosition, RightClickPressScreenPosition);
		if (RightDragDistance >= RightDragThreshold && !MouseDelta.IsNearlyZero())
		{
			bHasDraggedSelection = true;
			if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
			{
				if (bAltHeld)
				{
					BattleCharacter->PanCamera(FVector2D(-MouseDelta.X, MouseDelta.Y), AltMapDragSensitivity);
				}
				else
				{
					BattleCharacter->AdjustCameraLookYaw(MouseDelta.X * RightMouseRotateSensitivity);
					BattleCharacter->AdjustCameraLookPitch(-MouseDelta.Y * RightMouseRotateSensitivity);
				}
			}
		}
	}

	if (bLeftMouseHeld)
	{
		SelectionBoxEnd = CurrentMousePosition;
		if (!bIsBoxSelecting && FVector2D::Distance(SelectionBoxStart, SelectionBoxEnd) >= SelectionDragThreshold)
		{
			bIsBoxSelecting = true;
		}
	}

	LastMouseScreenPosition = CurrentMousePosition;
}

void ABattlemap_TestCursorPlayerController::UpdateFogOfWar(float DeltaTime)
{
	if (!bEnableFogOfWar)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			CachedVisibleEnemyCount = 0;
			return;
		}

		int32 VisibleEnemyCount = 0;
		for (TActorIterator<ABattleUnit> It(World); It; ++It)
		{
			ABattleUnit* Unit = *It;
			if (!Unit || Unit->bFriendly || !Unit->IsAlive())
			{
				continue;
			}
			Unit->SetActorHiddenInGame(false);
			Unit->SetActorEnableCollision(true);
			if (Unit->UnitMesh)
			{
				Unit->UnitMesh->SetVisibility(true, true);
				Unit->UnitMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
			++VisibleEnemyCount;
		}
		CachedVisibleEnemyCount = VisibleEnemyCount;
		return;
	}

	FogUpdateCooldown = FMath::Max(0.0f, FogUpdateCooldown - DeltaTime);
	if (FogUpdateCooldown > 0.0f)
	{
		return;
	}

	FogUpdateCooldown = FogUpdateInterval;
	ApplyEnemyVisibilityForFog();
}

void ABattlemap_TestCursorPlayerController::ApplyEnemyVisibilityForFog()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<ABattleUnit*> FriendlyUnits;
	FriendlyUnits.Reserve(16);
	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* Unit = *It;
		if (!Unit || !Unit->IsAlive() || !Unit->bFriendly)
		{
			continue;
		}
		FriendlyUnits.Add(Unit);
	}

	int32 VisibleEnemyCount = 0;
	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* EnemyUnit = *It;
		if (!EnemyUnit || !EnemyUnit->IsAlive() || EnemyUnit->bFriendly)
		{
			continue;
		}

		bool bVisibleToFriendlies = false;
		for (ABattleUnit* FriendlyUnit : FriendlyUnits)
		{
			if (!FriendlyUnit)
			{
				continue;
			}

			const float DistSq = FVector::DistSquared2D(FriendlyUnit->GetActorLocation(), EnemyUnit->GetActorLocation());
			if (DistSq <= FMath::Square(FriendlyUnit->DetectionRange))
			{
				bVisibleToFriendlies = true;
				break;
			}
		}

		EnemyUnit->SetActorHiddenInGame(!bVisibleToFriendlies);
		EnemyUnit->SetActorEnableCollision(bVisibleToFriendlies);
		if (EnemyUnit->UnitMesh)
		{
			EnemyUnit->UnitMesh->SetVisibility(bVisibleToFriendlies, true);
			EnemyUnit->UnitMesh->SetCollisionEnabled(bVisibleToFriendlies ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}

		if (bVisibleToFriendlies)
		{
			++VisibleEnemyCount;
		}
	}

	CachedVisibleEnemyCount = VisibleEnemyCount;
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
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnRightMousePressed);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ABattlemap_TestCursorPlayerController::OnRightMouseReleased);
		InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnToggleCommsTriggered);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnEscapeBattleMenu);
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnZoomIn);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnZoomOut);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnRotatePressed);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &ABattlemap_TestCursorPlayerController::OnRotateReleased);
		InputComponent->BindAxisKey(EKeys::MouseX, this, &ABattlemap_TestCursorPlayerController::OnRotateAxis);
		InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnMoveForwardPressed);
		InputComponent->BindKey(EKeys::W, IE_Released, this, &ABattlemap_TestCursorPlayerController::OnMoveForwardReleased);
		InputComponent->BindKey(EKeys::S, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnMoveBackwardPressed);
		InputComponent->BindKey(EKeys::S, IE_Released, this, &ABattlemap_TestCursorPlayerController::OnMoveBackwardReleased);
		InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnMoveRightPressed);
		InputComponent->BindKey(EKeys::D, IE_Released, this, &ABattlemap_TestCursorPlayerController::OnMoveRightReleased);
		InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnMoveLeftPressed);
		InputComponent->BindKey(EKeys::A, IE_Released, this, &ABattlemap_TestCursorPlayerController::OnMoveLeftReleased);
		InputComponent->BindKey(EKeys::LeftAlt, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnAltPressed);
		InputComponent->BindKey(EKeys::LeftAlt, IE_Released, this, &ABattlemap_TestCursorPlayerController::OnAltReleased);
		InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnFocusSelectedUnit);
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnMapViewLand);
		InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnMapViewOcean);
		InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnCycleDebugViewMode);
		InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnDebugViewFinal);
		InputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnDebugViewTerrainTypes);
		InputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnDebugViewWaterLayers);
		InputComponent->BindKey(EKeys::NumPadThree, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnDebugViewIsWaterMask);
		InputComponent->BindKey(EKeys::NumPadFour, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnDebugViewBaseTint);
		InputComponent->BindKey(EKeys::NumPadFive, IE_Pressed, this, &ABattlemap_TestCursorPlayerController::OnDebugViewContourMask);
	}
}

void ABattlemap_TestCursorPlayerController::OnInputStarted()
{
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
	PendingPanInput = FVector2D::ZeroVector;
	bShowMouseCursor = true;
	FollowTime = 0.f;
}

void ABattlemap_TestCursorPlayerController::OnLeftMousePressed()
{
	bLeftMouseHeld = true;
	bIsBoxSelecting = false;
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (GetMousePosition(MouseX, MouseY))
	{
		SelectionBoxStart = FVector2D(MouseX, MouseY);
		SelectionBoxEnd = SelectionBoxStart;
	}
	OnSetDestinationTriggered();
}

void ABattlemap_TestCursorPlayerController::OnLeftMouseReleased()
{
	bLeftMouseHeld = false;
	if (bIsBoxSelecting)
	{
		UpdateBoxSelection();
		bIsBoxSelecting = false;
		return;
	}

	UpdateSelectionFromCursor();
	if (SelectedUnit)
	{
		SetStatusHint(FString::Printf(TEXT("已选中单位：%s。"), *SelectedUnit->UnitLabel));
	}
	else if (FXCursor)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
	}
}

void ABattlemap_TestCursorPlayerController::OnRightMousePressed()
{
	bRightMouseHeld = true;
	OnInputStarted();
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (GetMousePosition(MouseX, MouseY))
	{
		RightClickPressScreenPosition = FVector2D(MouseX, MouseY);
	}
	OnSetDestinationTriggered();
}

void ABattlemap_TestCursorPlayerController::OnRightMouseReleased()
{
	bRightMouseHeld = false;
	const bool bWasDragging = bHasDraggedSelection;
	OnSetDestinationReleased();
	if (!bWasDragging)
	{
		OnCommandTriggered();
	}
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
	ABattleUnit* ClickedUnit = nullptr;
	if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
	{
		CachedDestination = Hit.Location;
		ClickedUnit = Cast<ABattleUnit>(Hit.GetActor());
	}

	if (SelectedUnits.Num() == 0)
	{
		SetStatusHint(TEXT("命令失败：当前未选中单位。"));
		return;
	}

	const bool bAttackCommand = ClickedUnit && !ClickedUnit->bFriendly;
	const bool bTryDamageFacility = !bAttackCommand && TacticalMapGrid && IsInputKeyDown(EKeys::LeftControl);
	if (bTryDamageFacility)
	{
		float TotalDamage = 0.0f;
		for (ABattleUnit* Unit : SelectedUnits)
		{
			if (Unit)
			{
				TotalDamage += FMath::Max(0.0f, Unit->AttackDamage);
			}
		}

		if (TacticalMapGrid->ApplyFacilityDamage(CachedDestination.X, CachedDestination.Y, TotalDamage))
		{
			SetStatusHint(FString::Printf(TEXT("已对设施造成 %.0f 点伤害。"), TotalDamage));
			return;
		}
	}

	int32 IssuedCount = 0;
	int32 SkippedAttackNoCommsCount = 0;
	int32 MovePathFailedCount = 0;
	FVector2D FormationForward = FVector2D::ZeroVector;
	FVector FormationOrigin = CachedDestination;
	if (!bAttackCommand)
	{
		FVector GroupCenter = FVector::ZeroVector;
		int32 ValidUnitCount = 0;
		for (ABattleUnit* Unit : SelectedUnits)
		{
			if (!Unit)
			{
				continue;
			}
			GroupCenter += Unit->GetActorLocation();
			++ValidUnitCount;
		}
		if (ValidUnitCount > 0)
		{
			GroupCenter /= static_cast<float>(ValidUnitCount);
			FormationForward = FVector2D(CachedDestination.X - GroupCenter.X, CachedDestination.Y - GroupCenter.Y).GetSafeNormal();
		}

		if (FormationForward.IsNearlyZero())
		{
			if (const ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
			{
				if (const UCameraComponent* Camera = BattleCharacter->GetTopDownCameraComponent())
				{
					const float CameraYawRad = FMath::DegreesToRadians(Camera->GetComponentRotation().Yaw);
					FormationForward = FVector2D(FMath::Cos(CameraYawRad), FMath::Sin(CameraYawRad));
				}
			}
		}
		if (FormationForward.IsNearlyZero())
		{
			FormationForward = FVector2D(1.0f, 0.0f);
		}
	}

	for (ABattleUnit* Unit : SelectedUnits)
	{
		if (!Unit || !Unit->CommandComponent)
		{
			continue;
		}

		if (!Unit->CommsComponent || !Unit->CommsComponent->IsCommandEnabled())
		{
			if (bAttackCommand)
			{
				SkippedAttackNoCommsCount++;
			}
			continue;
		}

		if (bAttackCommand)
		{
			Unit->IssueAttackCommandInterrupt(ClickedUnit, ECommandPriority::High);
		}
		else
		{
			FVector MoveTarget = FormationOrigin;
			if (bEnableFormationMove && SelectedUnits.Num() > 1)
			{
				const int32 SlotIndex = IssuedCount;
				const int32 ColumnCount = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(SelectedUnits.Num())));
				const int32 RowIndex = SlotIndex / FMath::Max(1, ColumnCount);
				const int32 ColIndex = SlotIndex % FMath::Max(1, ColumnCount);
				const float CenteredCol = static_cast<float>(ColIndex) - (static_cast<float>(ColumnCount - 1) * 0.5f);
				const FVector2D Forward = FormationForward.GetSafeNormal();
				const FVector2D Right(Forward.Y, -Forward.X);
				const FVector2D Offset2D = (Right * (CenteredCol * FormationLateralSpacing)) - (Forward * (static_cast<float>(RowIndex) * FormationDepthSpacing));
				MoveTarget.X += Offset2D.X;
				MoveTarget.Y += Offset2D.Y;
			}
			if (!Unit->IssueMoveCommandInterrupt(MoveTarget, ECommandPriority::High))
			{
				MovePathFailedCount++;
				continue;
			}
		}

		++IssuedCount;
	}

	if (bAttackCommand)
	{
		LastAttackTarget = ClickedUnit;
		FString Msg = FString::Printf(TEXT("已向 %d 个单位下达攻击命令。"), IssuedCount);
		if (SkippedAttackNoCommsCount > 0)
		{
			Msg += FString::Printf(TEXT(" %d 个单位因通讯不可用未下达。"), SkippedAttackNoCommsCount);
		}
		SetStatusHint(Msg);
	}
	else
	{
		if (MovePathFailedCount > 0 && IssuedCount == 0)
		{
			SetStatusHint(TEXT("没有可靠通行路径"));
		}
		else if (MovePathFailedCount > 0)
		{
			if (bEnableFormationMove && IssuedCount > 1)
			{
				SetStatusHint(FString::Printf(TEXT("已向 %d 个单位下达编队移动命令。%d 个单位没有可靠通行路径。"), IssuedCount, MovePathFailedCount));
			}
			else
			{
				SetStatusHint(FString::Printf(TEXT("已向 %d 个单位下达移动命令。%d 个单位没有可靠通行路径。"), IssuedCount, MovePathFailedCount));
			}
		}
		else if (bEnableFormationMove && IssuedCount > 1)
		{
			SetStatusHint(FString::Printf(TEXT("已向 %d 个单位下达编队移动命令。"), IssuedCount));
		}
		else
		{
			SetStatusHint(FString::Printf(TEXT("已向 %d 个单位下达移动命令。"), IssuedCount));
		}
	}
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
	TArray<ABattleUnit*> NewSelection;
	if (InSelectedUnit)
	{
		NewSelection.Add(InSelectedUnit);
	}
	SetSelectedUnits(NewSelection);
}

void ABattlemap_TestCursorPlayerController::SetSelectedUnits(const TArray<ABattleUnit*>& InSelectedUnits)
{
	for (ABattleUnit* Unit : SelectedUnits)
	{
		if (Unit)
		{
			Unit->SetSelected(false);
		}
	}

	SelectedUnits.Empty();
	for (ABattleUnit* Unit : InSelectedUnits)
	{
		if (Unit)
		{
			SelectedUnits.AddUnique(Unit);
		}
	}

	for (ABattleUnit* Unit : SelectedUnits)
	{
		if (Unit)
		{
			Unit->SetSelected(true);
		}
	}

	SelectedUnit = SelectedUnits.Num() > 0 ? SelectedUnits[0] : nullptr;
}

FDebugBattleSnapshot ABattlemap_TestCursorPlayerController::BuildDebugSnapshot() const
{
	FDebugBattleSnapshot Snapshot;
	Snapshot.LastHint = StatusHint;
	Snapshot.bIsBoxSelecting = bIsBoxSelecting;
	Snapshot.SelectionBoxStart = SelectionBoxStart;
	Snapshot.SelectionBoxEnd = SelectionBoxEnd;
	Snapshot.SelectedUnitCount = SelectedUnits.Num();
	for (ABattleUnit* Unit : SelectedUnits)
	{
		if (Unit)
		{
			Snapshot.SelectedUnitNames.Add(Unit->UnitLabel);
		}
	}

	if (SelectedUnit)
	{
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

		Snapshot.CurrentAmmo = SelectedUnit->CurrentAmmo;
		Snapshot.MaxAmmo = SelectedUnit->MaxAmmo;
		Snapshot.SelectedUnitRuntimeState = SelectedUnit->GetRuntimeState();
		Snapshot.SelectedUnitAttackCooldown = SelectedUnit->GetAttackCooldownRemaining();
		Snapshot.SelectedUnitReloadRemaining = SelectedUnit->GetReloadRemaining();
		Snapshot.SelectedUnitLastCombatEvent = SelectedUnit->GetLastCombatEvent();
	}

	if (const ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
	{
		if (const UCameraComponent* Camera = BattleCharacter->GetTopDownCameraComponent())
		{
			const FRotator CameraWorldRotation = Camera->GetComponentRotation();
			Snapshot.CameraPitch = CameraWorldRotation.Pitch;
			Snapshot.CameraRoll = CameraWorldRotation.Roll;
			Snapshot.CameraYaw = CameraWorldRotation.Yaw;
		}

		Snapshot.CameraVerticalDistanceMeters = BattleCharacter->GetVerticalDistanceToReferencePlane() / 100.0f;
	}
	Snapshot.VisibleEnemyCount = CachedVisibleEnemyCount;

	FHitResult HoverHit;
	if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, HoverHit))
	{
		if (ABattleUnit* HoverUnit = Cast<ABattleUnit>(HoverHit.GetActor()))
		{
			Snapshot.AttackTargetName = HoverUnit->UnitLabel;
			Snapshot.AttackTargetHealth = HoverUnit->CurrentHealth;
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (ABattlemap_TestCursorGameMode* BattleGM = Cast<ABattlemap_TestCursorGameMode>(World->GetAuthGameMode()))
		{
			Snapshot.ActiveMissionType = BattleGM->GetMissionType();
			Snapshot.ActiveMissionOutcome = BattleGM->GetMissionOutcome();
			Snapshot.MissionElapsedSeconds = BattleGM->GetMissionElapsedSeconds();
			Snapshot.DefenseHoldDurationSeconds = BattleGM->GetDefenseHoldDurationSeconds();

			const FBattleBalanceTableRow BalanceRow = BattleGM->GetEffectiveBattleBalanceRow();
			Snapshot.ActiveBalanceVersion = BalanceRow.BalanceVersion;
			Snapshot.EcmJammedDetectionRangeScale = BalanceRow.EcmJammedDetectionRangeScale;
			Snapshot.SupplyFoodConsumePerSecond = BalanceRow.SupplyFoodConsumePerSecond;
			Snapshot.SupplyFuelConsumePerSecond = BalanceRow.SupplyFuelConsumePerSecond;
		}
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
		if (HitUnit->bFriendly)
		{
			SetSelectedUnit(HitUnit);
			CachedDestination = HitUnit->GetActorLocation();
			return;
		}

		SetStatusHint(TEXT("敌方单位不可被选中。"));
		return;
	}

	ClearSelectionInternal(false);
	CachedDestination = Hit.Location;
	SetStatusHint(TEXT("已取消选择，并记录新的地面目标点。"));
}

void ABattlemap_TestCursorPlayerController::UpdateBoxSelection()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector2D Min(FMath::Min(SelectionBoxStart.X, SelectionBoxEnd.X), FMath::Min(SelectionBoxStart.Y, SelectionBoxEnd.Y));
	const FVector2D Max(FMath::Max(SelectionBoxStart.X, SelectionBoxEnd.X), FMath::Max(SelectionBoxStart.Y, SelectionBoxEnd.Y));
	TArray<ABattleUnit*> BoxSelectedUnits;

	for (TActorIterator<ABattleUnit> It(World); It; ++It)
	{
		ABattleUnit* Unit = *It;
		if (!Unit || !Unit->bFriendly)
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!ProjectWorldLocationToScreen(Unit->GetActorLocation(), ScreenPosition, true))
		{
			continue;
		}

		if (ScreenPosition.X >= Min.X && ScreenPosition.X <= Max.X
			&& ScreenPosition.Y >= Min.Y && ScreenPosition.Y <= Max.Y)
		{
			BoxSelectedUnits.Add(Unit);
		}
	}

	SetSelectedUnits(BoxSelectedUnits);
	SetStatusHint(FString::Printf(TEXT("框选到 %d 个单位。"), SelectedUnits.Num()));
}

void ABattlemap_TestCursorPlayerController::ClearSelectionInternal(bool bClearHint)
{
	SetSelectedUnits(TArray<ABattleUnit*>());
	if (bClearHint)
	{
		SetStatusHint(TEXT("已清除当前选中单位。"));
	}
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
	if (ABattlemap_TestCursorCharacter* BattleCharacter = Cast<ABattlemap_TestCursorCharacter>(GetPawn()))
	{
		BattleCharacter->ResetCameraOrientation();

		if (SelectedUnit)
		{
			BattleCharacter->FocusOnWorldLocation(SelectedUnit->GetActorLocation());
			SetStatusHint(FString::Printf(TEXT("镜头已聚焦并重置到 %s。"), *SelectedUnit->UnitLabel));
		}
		else
		{
			SetStatusHint(TEXT("镜头角度已重置。"));
		}
	}
}

void ABattlemap_TestCursorPlayerController::OnMoveForwardPressed()
{
	KeyboardForwardInput += 1.0f;
}

void ABattlemap_TestCursorPlayerController::OnMoveForwardReleased()
{
	KeyboardForwardInput -= 1.0f;
}

void ABattlemap_TestCursorPlayerController::OnMoveBackwardPressed()
{
	KeyboardForwardInput -= 1.0f;
}

void ABattlemap_TestCursorPlayerController::OnMoveBackwardReleased()
{
	KeyboardForwardInput += 1.0f;
}

void ABattlemap_TestCursorPlayerController::OnMoveRightPressed()
{
	KeyboardRightInput += 1.0f;
}

void ABattlemap_TestCursorPlayerController::OnMoveRightReleased()
{
	KeyboardRightInput -= 1.0f;
}

void ABattlemap_TestCursorPlayerController::OnMoveLeftPressed()
{
	KeyboardRightInput -= 1.0f;
}

void ABattlemap_TestCursorPlayerController::OnMoveLeftReleased()
{
	KeyboardRightInput += 1.0f;
}

void ABattlemap_TestCursorPlayerController::OnAltPressed()
{
	bAltHeld = true;
}

void ABattlemap_TestCursorPlayerController::OnAltReleased()
{
	bAltHeld = false;
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
	ClearSelectionInternal(true);
}

void ABattlemap_TestCursorPlayerController::OnEscapeBattleMenu()
{
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		GI->HandleEscapeDuringBattle(this);
	}
}

void ABattlemap_TestCursorPlayerController::OnMapViewLand()
{
	if (TacticalMapGrid)
	{
		TacticalMapGrid->SetMapViewMode(EMapViewMode::Land);
	}
}

void ABattlemap_TestCursorPlayerController::OnMapViewOcean()
{
	if (TacticalMapGrid)
	{
		TacticalMapGrid->SetMapViewMode(EMapViewMode::Ocean);
	}
}

void ABattlemap_TestCursorPlayerController::OnCycleDebugViewMode()
{
	if (!TacticalMapGrid)
	{
		return;
	}
	TacticalMapGrid->CycleDebugViewMode();
	const EMapDebugViewMode Mode = TacticalMapGrid->GetDebugViewMode();
	const UEnum* EnumPtr = StaticEnum<EMapDebugViewMode>();
	const FString ModeName = EnumPtr ? EnumPtr->GetNameStringByValue(static_cast<int64>(Mode)) : TEXT("Unknown");
	SetStatusHint(FString::Printf(TEXT("DebugViewMode => %s"), *ModeName));
}

void ABattlemap_TestCursorPlayerController::OnDebugViewFinal()
{
	if (TacticalMapGrid)
	{
		TacticalMapGrid->SetDebugViewMode(EMapDebugViewMode::Final);
		SetStatusHint(TEXT("DebugViewMode => Final"));
	}
}

void ABattlemap_TestCursorPlayerController::OnDebugViewTerrainTypes()
{
	if (TacticalMapGrid)
	{
		TacticalMapGrid->SetDebugViewMode(EMapDebugViewMode::TerrainTypes);
		SetStatusHint(TEXT("DebugViewMode => TerrainTypes"));
	}
}

void ABattlemap_TestCursorPlayerController::OnDebugViewWaterLayers()
{
	if (TacticalMapGrid)
	{
		TacticalMapGrid->SetDebugViewMode(EMapDebugViewMode::WaterLayers);
		SetStatusHint(TEXT("DebugViewMode => WaterLayers"));
	}
}

void ABattlemap_TestCursorPlayerController::OnDebugViewIsWaterMask()
{
	if (TacticalMapGrid)
	{
		TacticalMapGrid->SetDebugViewMode(EMapDebugViewMode::IsWaterMask);
		SetStatusHint(TEXT("DebugViewMode => IsWaterMask"));
	}
}

void ABattlemap_TestCursorPlayerController::OnDebugViewBaseTint()
{
	if (TacticalMapGrid)
	{
		TacticalMapGrid->SetDebugViewMode(EMapDebugViewMode::BaseTint);
		SetStatusHint(TEXT("DebugViewMode => BaseTint"));
	}
}

void ABattlemap_TestCursorPlayerController::OnDebugViewContourMask()
{
	if (TacticalMapGrid)
	{
		TacticalMapGrid->SetDebugViewMode(EMapDebugViewMode::ContourMask);
		SetStatusHint(TEXT("DebugViewMode => ContourMask"));
	}
}
