#include "Battlemap_TestCursorHUD.h"
#include "Battlemap_TestCursorGameMode.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "BattleOrbatBattleWidget.h"
#include "BattleUnit.h"
#include "BattleECMZoneComponent.h"
#include "BattleTypes.h"
#include "TacticalMapGrid.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/HitResult.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

void ABattlemap_TestCursorHUD::DrawHUD()
{
	Super::DrawHUD();

	ABattlemap_TestCursorPlayerController* BattleController = Cast<ABattlemap_TestCursorPlayerController>(GetOwningPlayerController());
	if (!BattleController || !Canvas)
	{
		return;
	}

	const FDebugBattleSnapshot Snapshot = BattleController->BuildDebugSnapshot();

	UFont* DrawFont = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!DrawFont)
	{
		static ConstructorHelpers::FObjectFinder<UFont> FontObject(TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		DrawFont = FontObject.Succeeded() ? FontObject.Object : nullptr;
	}

	const UEnum* EnumPtr = StaticEnum<ECommsState>();
	const FString CommsName = EnumPtr ? EnumPtr->GetNameStringByValue(static_cast<int64>(Snapshot.CommsState)) : TEXT("Unknown");
	const UEnum* RuntimeStateEnum = StaticEnum<EUnitRuntimeState>();
	const FString RuntimeStateName = RuntimeStateEnum ? RuntimeStateEnum->GetNameStringByValue(static_cast<int64>(Snapshot.SelectedUnitRuntimeState)) : TEXT("Unknown");

	TArray<FString> Lines;
	Lines.Add(TEXT("Battlemap_Test 最小测试关卡"));

	const UEnum* MissionOutcomeEnum = StaticEnum<EMissionOutcomeState>();
	const FString OutcomeName = MissionOutcomeEnum ? MissionOutcomeEnum->GetNameStringByValue(static_cast<int64>(Snapshot.ActiveMissionOutcome)) : TEXT("Unknown");
	const UEnum* MissionTypeEnum = StaticEnum<EMissionType>();
	const FString MissionTypeName = MissionTypeEnum ? MissionTypeEnum->GetNameStringByValue(static_cast<int64>(Snapshot.ActiveMissionType)) : TEXT("Unknown");
	Lines.Add(FString::Printf(TEXT("任务：%s  │  结果：%s"), *MissionTypeName, *OutcomeName));
	if (Snapshot.ActiveMissionType == EMissionType::Defense && Snapshot.DefenseHoldDurationSeconds > 0.0f && Snapshot.ActiveMissionOutcome == EMissionOutcomeState::InProgress)
	{
		const float Remain = FMath::Max(0.0f, Snapshot.DefenseHoldDurationSeconds - Snapshot.MissionElapsedSeconds);
		Lines.Add(FString::Printf(TEXT("防御计时：已过 %.1fs / 目标 %.1fs  │  剩余 %.1fs"), Snapshot.MissionElapsedSeconds, Snapshot.DefenseHoldDurationSeconds, Remain));
	}
	else
	{
		Lines.Add(FString::Printf(TEXT("任务经过时间：%.1fs"), Snapshot.MissionElapsedSeconds));
	}
	Lines.Add(FString::Printf(TEXT("平衡版本：%s  │  ECM干扰感知系数(表/ini)：%.2f  │  补给消耗/秒 食物 %.4f 燃油 %.4f"),
		Snapshot.ActiveBalanceVersion.IsEmpty() ? TEXT("—") : *Snapshot.ActiveBalanceVersion,
		Snapshot.EcmJammedDetectionRangeScale,
		Snapshot.SupplyFoodConsumePerSecond,
		Snapshot.SupplyFuelConsumePerSecond));

	Lines.Add(TEXT("左键短按：单选单位或点地面清空"));
	Lines.Add(TEXT("左键按住拖动：框选单位"));
	Lines.Add(TEXT("右键短按：向当前选中单位下达命令"));
	Lines.Add(TEXT("Ctrl+右键短按地面：对设施单元施加伤害"));
	Lines.Add(TEXT("右键按住拖动：旋转镜头"));
	Lines.Add(TEXT("左Alt+右键拖动：平行于地面拖拽镜头"));
	Lines.Add(TEXT("鼠标滚轮：缩放镜头"));
	Lines.Add(TEXT("空格：镜头聚焦当前选中单位"));
	Lines.Add(TEXT("Q：切换当前选中单位通讯状态"));
	Lines.Add(TEXT(" "));
	Lines.Add(FString::Printf(TEXT("选中数量：%d"), Snapshot.SelectedUnitCount));
	Lines.Add(FString::Printf(TEXT("主单位：%s"), Snapshot.SelectedUnitName.IsEmpty() ? TEXT("无") : *Snapshot.SelectedUnitName));
	Lines.Add(FString::Printf(TEXT("位置：X %.0f  Y %.0f  Z %.0f"), Snapshot.SelectedUnitLocation.X, Snapshot.SelectedUnitLocation.Y, Snapshot.SelectedUnitLocation.Z));
	Lines.Add(FString::Printf(TEXT("通讯：%s"), *CommsName));
	Lines.Add(FString::Printf(TEXT("生命：%.0f"), Snapshot.Health));
	Lines.Add(FString::Printf(TEXT("食物：%.0f  燃油：%.0f"), Snapshot.Food, Snapshot.Fuel));
	Lines.Add(FString::Printf(TEXT("弹药：%d/%d"), Snapshot.CurrentAmmo, Snapshot.MaxAmmo));
	Lines.Add(FString::Printf(TEXT("状态：%s"), *RuntimeStateName));
	Lines.Add(FString::Printf(TEXT("攻击冷却：%.2fs  重装填剩余：%.2fs"), Snapshot.SelectedUnitAttackCooldown, Snapshot.SelectedUnitReloadRemaining));
	Lines.Add(FString::Printf(TEXT("攻击目标：%s"), Snapshot.AttackTargetName.IsEmpty() ? TEXT("无") : *Snapshot.AttackTargetName));
	Lines.Add(Snapshot.AttackTargetName.IsEmpty() ? TEXT("目标血量：") : FString::Printf(TEXT("目标血量：%.0f"), Snapshot.AttackTargetHealth));
	Lines.Add(FString::Printf(TEXT("战斗事件：%s"), Snapshot.SelectedUnitLastCombatEvent.IsEmpty() ? TEXT("无") : *Snapshot.SelectedUnitLastCombatEvent));
	Lines.Add(FString::Printf(TEXT("摄像机旋转 Pitch: %.1f  Roll: %.1f  Yaw: %.1f"), Snapshot.CameraPitch, Snapshot.CameraRoll, Snapshot.CameraYaw));
	Lines.Add(FString::Printf(TEXT("摄像机垂直距离：%.2f m"), Snapshot.CameraVerticalDistanceMeters));
	Lines.Add(FString::Printf(TEXT("战争迷雾：当前可见敌军 %d"), Snapshot.VisibleEnemyCount));
	if (ATacticalMapGrid* TacticalMapGrid = BattleController->GetTacticalMapGrid())
	{
		Lines.Add(FString::Printf(TEXT("等高线参数：间距 %.1f  线宽 %.2f  深度偏移 %.2f"),
			TacticalMapGrid->ContourIntervalMeters,
			TacticalMapGrid->ContourLineWidth,
			TacticalMapGrid->ContourDepthOffset));

		FHitResult TerrainHit;
		if (BattleController->GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, TerrainHit))
		{
			FTerrainCellState CellState;
			if (TacticalMapGrid->GetCellStateAtWorldXY(TerrainHit.Location.X, TerrainHit.Location.Y, CellState))
			{
				const UEnum* TerrainTypeEnum = StaticEnum<ETerrainType>();
				const FString TerrainName = TerrainTypeEnum ? TerrainTypeEnum->GetNameStringByValue(static_cast<int64>(CellState.TerrainType)) : TEXT("Unknown");
				Lines.Add(FString::Printf(TEXT("地形格：(%d,%d) %s  高程 %.1fm  水深 %.1fm"),
					CellState.TerrainCellId.X,
					CellState.TerrainCellId.Y,
					*TerrainName,
					CellState.ElevationMeters,
					CellState.WaterDepthMeters));
				Lines.Add(FString::Printf(TEXT("坡度(最大邻格变化率)：%.3f"), CellState.SlopeToMaxNeighbor));
				if (CellState.bIsArtificialFacility)
				{
					Lines.Add(FString::Printf(TEXT("设施状态：HP %.1f  Destroyed %s"),
						CellState.FacilityHP,
						CellState.bDestroyed ? TEXT("Yes") : TEXT("No")));
				}

				// Slope diagnostics: max elevation delta against 4-neighbors.
				float MaxNeighborDeltaMeters = 0.0f;
				const FIntPoint NeighborOffsets[4] = {
					FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)
				};
				for (const FIntPoint& Offset : NeighborOffsets)
				{
					FTerrainCellState Neighbor;
					if (TacticalMapGrid->TryGetCellStateById(CellState.TerrainCellId + Offset, Neighbor))
					{
						MaxNeighborDeltaMeters = FMath::Max(MaxNeighborDeltaMeters, FMath::Abs(CellState.ElevationMeters - Neighbor.ElevationMeters));
					}
				}
				Lines.Add(FString::Printf(TEXT("坡度诊断：邻格最大高差 %.2f m"), MaxNeighborDeltaMeters));

				// Mapping diagnostics: World -> Cell -> PNG pixel -> UV
				const int32 Width = TacticalMapGrid->LoadedHeightmapWidth;
				const int32 Height = TacticalMapGrid->LoadedHeightmapHeight;
				if (Width > 0 && Height > 0)
				{
					const int32 HalfX = Width / 2;
					const int32 HalfY = Height / 2;
					const int32 PixelX = CellState.TerrainCellId.X + HalfX;
					const int32 PixelY = CellState.TerrainCellId.Y + HalfY;
					const bool bPixelInRange = (PixelX >= 0 && PixelX < Width && PixelY >= 0 && PixelY < Height);

					const float UFromPixel = bPixelInRange ? (static_cast<float>(PixelX) + 0.5f) / static_cast<float>(Width) : -1.0f;
					const float VFromPixel = bPixelInRange ? (static_cast<float>(PixelY) + 0.5f) / static_cast<float>(Height) : -1.0f;

					const FVector Local = TacticalMapGrid->GetActorTransform().InverseTransformPosition(TerrainHit.Location);
					const float TileSizeUU = TacticalMapGrid->GetTileSizeUU();
					const float CellXFromWorld = Local.X / TileSizeUU;
					const float CellYFromWorld = Local.Y / TileSizeUU;
					const float UFromWorld = (CellXFromWorld + static_cast<float>(HalfX) + 0.5f) / static_cast<float>(Width);
					const float VFromWorld = (CellYFromWorld + static_cast<float>(HalfY) + 0.5f) / static_cast<float>(Height);

					Lines.Add(FString::Printf(TEXT("映射诊断: Cell->Pixel=(%d,%d)  InRange=%s"),
						PixelX, PixelY, bPixelInRange ? TEXT("Yes") : TEXT("No")));
					Lines.Add(FString::Printf(TEXT("映射诊断: UV(Pixel)=%.4f, %.4f  UV(World)=%.4f, %.4f"),
						UFromPixel, VFromPixel, UFromWorld, VFromWorld));
					Lines.Add(FString::Printf(TEXT("映射诊断: dUV=%.4f, %.4f  LocalXY=(%.1f, %.1f) UU"),
						UFromWorld - UFromPixel, VFromWorld - VFromPixel, Local.X, Local.Y));
				}
			}
		}
	}
	Lines.Add(FString::Printf(TEXT("提示：%s"), Snapshot.LastHint.IsEmpty() ? TEXT("无") : *Snapshot.LastHint));

	float MouseScreenX = 0.f;
	float MouseScreenY = 0.f;
	if (BattleController->GetMousePosition(MouseScreenX, MouseScreenY))
	{
		Lines.Add(FString::Printf(TEXT("鼠标屏幕坐标：%.0f, %.0f"), MouseScreenX, MouseScreenY));
	}
	else
	{
		Lines.Add(TEXT("鼠标屏幕坐标：（无法读取）"));
	}

	if (UWorld* HUDWorld = BattleController->GetWorld())
	{
		if (ABattlemap_TestCursorGameMode* GM = Cast<ABattlemap_TestCursorGameMode>(HUDWorld->GetAuthGameMode()))
		{
			if (UBattleOrbatBattleWidget* Orbat = GM->GetBattleOrbatBattleWidget())
			{
				Lines.Add(TEXT("战斗序列按钮屏幕位置："));
				Orbat->AppendDebugScreenPositions(Lines, BattleController);
			}
		}
	}

	for (const FString& UnitName : Snapshot.SelectedUnitNames)
	{
		Lines.Add(FString::Printf(TEXT("- %s"), *UnitName));
	}

	float DrawY = 32.0f;
	for (const FString& Line : Lines)
	{
		DrawText(Line, FLinearColor::White, 30.0f, DrawY, DrawFont, 1.1f, false);
		DrawY += 20.0f;
	}

	if (Snapshot.bIsBoxSelecting)
	{
		const float MinX = FMath::Min(Snapshot.SelectionBoxStart.X, Snapshot.SelectionBoxEnd.X);
		const float MinY = FMath::Min(Snapshot.SelectionBoxStart.Y, Snapshot.SelectionBoxEnd.Y);
		const float Width = FMath::Abs(Snapshot.SelectionBoxEnd.X - Snapshot.SelectionBoxStart.X);
		const float Height = FMath::Abs(Snapshot.SelectionBoxEnd.Y - Snapshot.SelectionBoxStart.Y);
		const FLinearColor SelectionOutlineColor(0.0f, 1.0f, 1.0f, 1.0f);
		DrawRect(FLinearColor(0.1f, 0.7f, 1.0f, 0.15f), MinX, MinY, Width, Height);
		DrawLine(MinX, MinY, MinX + Width, MinY, SelectionOutlineColor, 1.5f);
		DrawLine(MinX + Width, MinY, MinX + Width, MinY + Height, SelectionOutlineColor, 1.5f);
		DrawLine(MinX + Width, MinY + Height, MinX, MinY + Height, SelectionOutlineColor, 1.5f);
		DrawLine(MinX, MinY + Height, MinX, MinY, SelectionOutlineColor, 1.5f);
	}

	const float EcmLineThickness = 2.0f;
	const FLinearColor EcmLineColor(0.0f, 0.0f, 0.0f, 0.95f);
	const int32 EcmSegmentCount = 48;

	UWorld* World = BattleController->GetWorld();
	if (World)
	{
		for (TActorIterator<ABattleUnit> It(World); It; ++It)
		{
			ABattleUnit* Unit = *It;
			if (!Unit || !Unit->IsAlive() || Unit->IsHidden())
			{
				continue;
			}

			if (!Unit->EcmZone || Unit->EcmZone->JamRadiusUU <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float EcmRadius = Unit->EcmZone->JamRadiusUU;
			const FVector Origin = Unit->EcmZone->GetComponentLocation();
			FVector2D PrevScreen = FVector2D::ZeroVector;
			bool bHasPrev = false;
			for (int32 Segment = 0; Segment <= EcmSegmentCount; ++Segment)
			{
				const float Angle = (static_cast<float>(Segment) / static_cast<float>(EcmSegmentCount)) * 2.0f * PI;
				const FVector WorldPoint = Origin + FVector(FMath::Cos(Angle) * EcmRadius, FMath::Sin(Angle) * EcmRadius, 0.0f);
				FVector2D ScreenPoint;
				if (!BattleController->ProjectWorldLocationToScreen(WorldPoint, ScreenPoint, true))
				{
					bHasPrev = false;
					continue;
				}

				if (bHasPrev)
				{
					DrawLine(PrevScreen.X, PrevScreen.Y, ScreenPoint.X, ScreenPoint.Y, EcmLineColor, EcmLineThickness);
				}

				PrevScreen = ScreenPoint;
				bHasPrev = true;
			}
		}
	}

	const TArray<ABattleUnit*>& SelectedUnits = BattleController->GetSelectedUnits();
	for (ABattleUnit* Unit : SelectedUnits)
	{
		if (!Unit)
		{
			continue;
		}

		const int32 SegmentCount = 48;
		const float Radius = Unit->AttackRange;
		FVector2D PrevScreen = FVector2D::ZeroVector;
		bool bHasPrev = false;

		for (int32 Segment = 0; Segment <= SegmentCount; ++Segment)
		{
			const float Angle = (static_cast<float>(Segment) / static_cast<float>(SegmentCount)) * 2.0f * PI;
			const FVector WorldPoint = Unit->GetActorLocation() + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
			FVector2D ScreenPoint;
			if (!BattleController->ProjectWorldLocationToScreen(WorldPoint, ScreenPoint, true))
			{
				bHasPrev = false;
				continue;
			}

			if (bHasPrev)
			{
				DrawLine(PrevScreen.X, PrevScreen.Y, ScreenPoint.X, ScreenPoint.Y, FLinearColor(1.0f, 0.2f, 0.2f, 0.9f), 2.0f);
			}

			PrevScreen = ScreenPoint;
			bHasPrev = true;
		}

		const int32 DetectionSegmentCount = 48;
		const float DetectionRadius = Unit->DetectionRange;
		FVector2D PrevDetectionScreen = FVector2D::ZeroVector;
		bool bHasPrevDetection = false;
		for (int32 Segment = 0; Segment <= DetectionSegmentCount; ++Segment)
		{
			const float Angle = (static_cast<float>(Segment) / static_cast<float>(DetectionSegmentCount)) * 2.0f * PI;
			const FVector WorldPoint = Unit->GetActorLocation() + FVector(FMath::Cos(Angle) * DetectionRadius, FMath::Sin(Angle) * DetectionRadius, 0.0f);
			FVector2D ScreenPoint;
			if (!BattleController->ProjectWorldLocationToScreen(WorldPoint, ScreenPoint, true))
			{
				bHasPrevDetection = false;
				continue;
			}

			if (bHasPrevDetection)
			{
				DrawLine(PrevDetectionScreen.X, PrevDetectionScreen.Y, ScreenPoint.X, ScreenPoint.Y, FLinearColor(1.0f, 1.0f, 0.0f, 0.9f), 2.0f);
			}

			PrevDetectionScreen = ScreenPoint;
			bHasPrevDetection = true;
		}
	}

	if (World)
	{
		for (TActorIterator<ABattleUnit> It(World); It; ++It)
		{
			ABattleUnit* Unit = *It;
			if (!Unit || Unit->bFriendly || Unit->IsHidden())
			{
				continue;
			}

			const int32 DetectionSegmentCount = 48;
			const float DetectionRadius = Unit->DetectionRange;
			FVector2D PrevDetectionScreen = FVector2D::ZeroVector;
			bool bHasPrevDetection = false;
			for (int32 Segment = 0; Segment <= DetectionSegmentCount; ++Segment)
			{
				const float Angle = (static_cast<float>(Segment) / static_cast<float>(DetectionSegmentCount)) * 2.0f * PI;
				const FVector WorldPoint = Unit->GetActorLocation() + FVector(FMath::Cos(Angle) * DetectionRadius, FMath::Sin(Angle) * DetectionRadius, 0.0f);
				FVector2D ScreenPoint;
				if (!BattleController->ProjectWorldLocationToScreen(WorldPoint, ScreenPoint, true))
				{
					bHasPrevDetection = false;
					continue;
				}

				if (bHasPrevDetection)
				{
					DrawLine(PrevDetectionScreen.X, PrevDetectionScreen.Y, ScreenPoint.X, ScreenPoint.Y, FLinearColor(1.0f, 1.0f, 0.0f, 0.9f), 2.0f);
				}

				PrevDetectionScreen = ScreenPoint;
				bHasPrevDetection = true;
			}
		}
	}

	auto DrawWhiteHoverRing = [&](ABattleUnit* Unit)
	{
		if (!Unit || !Unit->IsAlive())
		{
			return;
		}
		const float HoverRadius = Unit->GetHoverCircleRadius();
		const int32 HoverSegmentCount = 48;
		FVector2D PrevScreen = FVector2D::ZeroVector;
		bool bHasPrev = false;
		for (int32 Segment = 0; Segment <= HoverSegmentCount; ++Segment)
		{
			const float Angle = (static_cast<float>(Segment) / static_cast<float>(HoverSegmentCount)) * 2.0f * PI;
			const FVector WorldPoint = Unit->GetActorLocation() + FVector(FMath::Cos(Angle) * HoverRadius, FMath::Sin(Angle) * HoverRadius, 0.0f);
			FVector2D ScreenPoint;
			if (!BattleController->ProjectWorldLocationToScreen(WorldPoint, ScreenPoint, true))
			{
				bHasPrev = false;
				continue;
			}
			if (bHasPrev)
			{
				DrawLine(PrevScreen.X, PrevScreen.Y, ScreenPoint.X, ScreenPoint.Y, FLinearColor::White, 5.0f);
			}
			PrevScreen = ScreenPoint;
			bHasPrev = true;
		}
	};

	for (ABattleUnit* Sel : BattleController->GetSelectedUnits())
	{
		if (Sel && Sel->bFriendly)
		{
			DrawWhiteHoverRing(Sel);
		}
	}

	FHitResult HoverHit;
	if (BattleController->GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, HoverHit))
	{
		if (ABattleUnit* HoveredUnit = Cast<ABattleUnit>(HoverHit.GetActor()))
		{
			DrawWhiteHoverRing(HoveredUnit);
		}
	}
}
