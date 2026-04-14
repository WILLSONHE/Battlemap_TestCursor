#include "Battlemap_TestCursorHUD.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "BattleUnit.h"
#include "BattleTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
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

	TArray<FString> Lines;
	Lines.Add(TEXT("Battlemap_Test 最小测试关卡"));
	Lines.Add(TEXT("左键短按：单选单位或点地面清空"));
	Lines.Add(TEXT("左键按住拖动：框选单位"));
	Lines.Add(TEXT("右键短按：向当前选中单位下达移动命令"));
	Lines.Add(TEXT("右键按住拖动：平行于地面拖拽镜头"));
	Lines.Add(TEXT("鼠标滚轮：缩放镜头"));
	Lines.Add(TEXT("按住鼠标中键并左右拖动：旋转镜头"));
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
	Lines.Add(FString::Printf(TEXT("攻击目标：%s"), Snapshot.AttackTargetName.IsEmpty() ? TEXT("无") : *Snapshot.AttackTargetName));
	Lines.Add(Snapshot.AttackTargetName.IsEmpty() ? TEXT("目标血量：") : FString::Printf(TEXT("目标血量：%.0f"), Snapshot.AttackTargetHealth));
	Lines.Add(FString::Printf(TEXT("摄像机旋转 Pitch: %.1f  Roll: %.1f  Yaw: %.1f"), Snapshot.CameraPitch, Snapshot.CameraRoll, Snapshot.CameraYaw));
	Lines.Add(FString::Printf(TEXT("摄像机垂直距离：%.2f m"), Snapshot.CameraVerticalDistanceMeters));
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
		for (TActorIterator<ABattleUnit> It(World); It; ++It)
		{
			ABattleUnit* Unit = *It;
			if (!Unit || Unit->bFriendly)
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
