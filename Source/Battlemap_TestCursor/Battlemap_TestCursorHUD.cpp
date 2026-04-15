#include "Battlemap_TestCursorHUD.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "BattleUnit.h"
#include "BattleTypes.h"
#include "TacticalMapGrid.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/HitResult.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

void ABattlemap_TestCursorHUD::RebuildFallbackContourCache(ATacticalMapGrid* TacticalMapGrid)
{
	CachedFallbackContourSegments.Reset();
	if (!TacticalMapGrid)
	{
		return;
	}

	const int32 MinCell = -TacticalMapGrid->GridHalfExtentTiles;
	const int32 MaxCell = TacticalMapGrid->GridHalfExtentTiles;
	const float Interval = FMath::Max(0.1f, TacticalMapGrid->ContourIntervalMeters);

	auto ElevAt = [TacticalMapGrid](const FIntPoint& CellId, float& OutElevation) -> bool
	{
		FTerrainCellState Cell;
		if (!TacticalMapGrid->TryGetCellStateById(CellId, Cell))
		{
			return false;
		}
		OutElevation = Cell.ElevationMeters;
		return true;
	};

	CachedFallbackContourSegments.Reserve((MaxCell - MinCell) * (MaxCell - MinCell));
	for (int32 Y = MinCell; Y < MaxCell; ++Y)
	{
		for (int32 X = MinCell; X < MaxCell; ++X)
		{
			const FIntPoint C00(X, Y);
			const FIntPoint C10(X + 1, Y);
			const FIntPoint C01(X, Y + 1);
			const FIntPoint C11(X + 1, Y + 1);

			float H00 = 0.0f;
			float H10 = 0.0f;
			float H01 = 0.0f;
			float H11 = 0.0f;
			if (!ElevAt(C00, H00) || !ElevAt(C10, H10) || !ElevAt(C01, H01) || !ElevAt(C11, H11))
			{
				continue;
			}

			const float MinH = FMath::Min(FMath::Min(H00, H10), FMath::Min(H01, H11));
			const float MaxH = FMath::Max(FMath::Max(H00, H10), FMath::Max(H01, H11));
			const int32 L0 = FMath::FloorToInt(MinH / Interval);
			const int32 L1 = FMath::FloorToInt(MaxH / Interval);
			if (L0 == L1)
			{
				continue;
			}

			const FVector P00 = TacticalMapGrid->CellToWorldCenter(C00);
			const FVector P10 = TacticalMapGrid->CellToWorldCenter(C10);
			const FVector P01 = TacticalMapGrid->CellToWorldCenter(C01);
			const FVector P11 = TacticalMapGrid->CellToWorldCenter(C11);

			for (int32 Level = L0; Level <= L1; ++Level)
			{
				const float IsoH = static_cast<float>(Level) * Interval;
				TArray<FVector> CrossPoints;
				CrossPoints.Reserve(4);

				auto TryEdge = [&](const FVector& A, const FVector& B, float HA, float HB)
				{
					const bool bCross = (HA <= IsoH && HB > IsoH) || (HA > IsoH && HB <= IsoH);
					if (!bCross || FMath::IsNearlyEqual(HA, HB))
					{
						return;
					}
					const float T = (IsoH - HA) / (HB - HA);
					CrossPoints.Add(FMath::Lerp(A, B, FMath::Clamp(T, 0.0f, 1.0f)) + FVector(0.0f, 0.0f, TacticalMapGrid->ContourDepthOffset));
				};

				TryEdge(P00, P10, H00, H10);
				TryEdge(P10, P11, H10, H11);
				TryEdge(P11, P01, H11, H01);
				TryEdge(P01, P00, H01, H00);

				if (CrossPoints.Num() == 2)
				{
					CachedFallbackContourSegments.Add(TPair<FVector, FVector>(CrossPoints[0], CrossPoints[1]));
				}
				else if (CrossPoints.Num() == 4)
				{
					CachedFallbackContourSegments.Add(TPair<FVector, FVector>(CrossPoints[0], CrossPoints[1]));
					CachedFallbackContourSegments.Add(TPair<FVector, FVector>(CrossPoints[2], CrossPoints[3]));
				}
			}
		}
	}
}

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
		Lines.Add(FString::Printf(TEXT("高度图加载：%s  回退模式：%s  几何起伏：%s"),
			TacticalMapGrid->bHeightmapLoaded ? TEXT("成功") : TEXT("失败"),
			TacticalMapGrid->bUsedProceduralFallback ? TEXT("是") : TEXT("否"),
			TacticalMapGrid->bVisualGeometryDeformed ? TEXT("是") : TEXT("否")));
		Lines.Add(FString::Printf(TEXT("地形显示模式：%s"),
			TacticalMapGrid->bUseFlatContourMode ? TEXT("平面+等高线材质") : TEXT("Landscape导入模式")));
		Lines.Add(FString::Printf(TEXT("等高线材质配置：%s"),
			TacticalMapGrid->IsContourMaterialConfigured() ? TEXT("已配置") : TEXT("未配置")));
		Lines.Add(FString::Printf(TEXT("材质输入链：%s"),
			TacticalMapGrid->bRuntimeTextureInputReady ? TEXT("Height/Terrain 纹理已喂入") : TEXT("未就绪")));
		Lines.Add(FString::Printf(TEXT("HUD后备等高线：%s"),
			TacticalMapGrid->bEnableHudContourFallback ? TEXT("开启") : TEXT("关闭(推荐)")));
		Lines.Add(FString::Printf(TEXT("高度图分辨率：%d x %d"),
			TacticalMapGrid->LoadedHeightmapWidth,
			TacticalMapGrid->LoadedHeightmapHeight));
		Lines.Add(FString::Printf(TEXT("Landscape建议参数：Section=%d  Sections/Comp=%d  Comp=%d x %d  Overall=%d x %d"),
			TacticalMapGrid->LandscapeSectionSizeQuads,
			TacticalMapGrid->LandscapeSectionsPerComponent,
			TacticalMapGrid->LandscapeComponentCountX,
			TacticalMapGrid->LandscapeComponentCountY,
			TacticalMapGrid->LandscapeOverallResolutionX,
			TacticalMapGrid->LandscapeOverallResolutionY));
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
			}
		}
	}
	Lines.Add(FString::Printf(TEXT("提示：%s"), Snapshot.LastHint.IsEmpty() ? TEXT("无") : *Snapshot.LastHint));
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

	UWorld* World = BattleController->GetWorld();
	if (World)
	{
		ATacticalMapGrid* TacticalMapGrid = BattleController->GetTacticalMapGrid();
		if (TacticalMapGrid && TacticalMapGrid->bEnableHudContourFallback && !TacticalMapGrid->IsContourMaterialConfigured())
		{
			const bool bNeedRebuild =
				(CachedContourMap.Get() != TacticalMapGrid)
				|| !FMath::IsNearlyEqual(CachedContourInterval, TacticalMapGrid->ContourIntervalMeters)
				|| (CachedGridHalfExtent != TacticalMapGrid->GridHalfExtentTiles)
				|| !FMath::IsNearlyEqual(CachedContourDepthOffset, TacticalMapGrid->ContourDepthOffset);

			if (bNeedRebuild)
			{
				CachedContourMap = TacticalMapGrid;
				CachedContourInterval = TacticalMapGrid->ContourIntervalMeters;
				CachedGridHalfExtent = TacticalMapGrid->GridHalfExtentTiles;
				CachedContourDepthOffset = TacticalMapGrid->ContourDepthOffset;
				RebuildFallbackContourCache(TacticalMapGrid);
			}

			const float ContourThickness = FMath::Clamp(TacticalMapGrid->ContourLineWidth, 1.0f, 3.0f);
			const float TileSizeUU = TacticalMapGrid->GetTileSizeUU();
			for (const TPair<FVector, FVector>& Segment : CachedFallbackContourSegments)
			{
				FVector2D ScreenA;
				FVector2D ScreenB;
				if (!BattleController->ProjectWorldLocationToScreen(Segment.Key, ScreenA, true)
					|| !BattleController->ProjectWorldLocationToScreen(Segment.Value, ScreenB, true))
				{
					continue;
				}
				DrawLine(ScreenA.X, ScreenA.Y, ScreenB.X, ScreenB.Y, FLinearColor(0.0f, 0.0f, 0.0f, 0.95f), ContourThickness);
			}

			// Draw a local axis hint in fallback mode.
			const FVector AxisOrigin = TacticalMapGrid->CellToWorldCenter(FIntPoint::ZeroValue) + FVector(0.0f, 0.0f, TacticalMapGrid->ContourDepthOffset + 2.0f);
			FVector2D AxisO, AxisX, AxisY;
			if (BattleController->ProjectWorldLocationToScreen(AxisOrigin, AxisO, true)
				&& BattleController->ProjectWorldLocationToScreen(AxisOrigin + FVector(TileSizeUU * 2.0f, 0.0f, 0.0f), AxisX, true)
				&& BattleController->ProjectWorldLocationToScreen(AxisOrigin + FVector(0.0f, TileSizeUU * 2.0f, 0.0f), AxisY, true))
			{
				DrawLine(AxisO.X, AxisO.Y, AxisX.X, AxisX.Y, FLinearColor::Red, 2.0f);
				DrawLine(AxisO.X, AxisO.Y, AxisY.X, AxisY.Y, FLinearColor::Green, 2.0f);
			}
		}

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

	FHitResult HoverHit;
	if (BattleController->GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, HoverHit))
	{
		ABattleUnit* HoveredUnit = Cast<ABattleUnit>(HoverHit.GetActor());
		if (HoveredUnit)
		{
			const float HoverRadius = HoveredUnit->GetHoverCircleRadius();
			const int32 HoverSegmentCount = 48;
			FVector2D PrevScreen = FVector2D::ZeroVector;
			bool bHasPrev = false;

			for (int32 Segment = 0; Segment <= HoverSegmentCount; ++Segment)
			{
				const float Angle = (static_cast<float>(Segment) / static_cast<float>(HoverSegmentCount)) * 2.0f * PI;
				const FVector WorldPoint = HoveredUnit->GetActorLocation() + FVector(FMath::Cos(Angle) * HoverRadius, FMath::Sin(Angle) * HoverRadius, 0.0f);
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
		}
	}
}
