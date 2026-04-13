#include "Battlemap_TestCursorHUD.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "BattleTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
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
	Lines.Add(TEXT("左键短按：选择单位或记录目标点"));
	Lines.Add(TEXT("左键按住拖动：平行于地面拖拽镜头"));
	Lines.Add(TEXT("右键：向当前选中单位下达移动命令"));
	Lines.Add(TEXT("鼠标滚轮：缩放镜头"));
	Lines.Add(TEXT("按住鼠标中键并左右拖动：旋转镜头"));
	Lines.Add(TEXT("空格：镜头聚焦当前选中单位"));
	Lines.Add(TEXT("Q：切换当前选中单位通讯状态"));
	Lines.Add(TEXT(" "));
	Lines.Add(FString::Printf(TEXT("选中单位：%s"), Snapshot.SelectedUnitName.IsEmpty() ? TEXT("无") : *Snapshot.SelectedUnitName));
	Lines.Add(FString::Printf(TEXT("位置：X %.0f  Y %.0f  Z %.0f"), Snapshot.SelectedUnitLocation.X, Snapshot.SelectedUnitLocation.Y, Snapshot.SelectedUnitLocation.Z));
	Lines.Add(FString::Printf(TEXT("通讯：%s"), *CommsName));
	Lines.Add(FString::Printf(TEXT("生命：%.0f"), Snapshot.Health));
	Lines.Add(FString::Printf(TEXT("食物：%.0f  燃油：%.0f"), Snapshot.Food, Snapshot.Fuel));
	Lines.Add(FString::Printf(TEXT("提示：%s"), Snapshot.LastHint.IsEmpty() ? TEXT("无") : *Snapshot.LastHint));

	float DrawY = 32.0f;
	for (const FString& Line : Lines)
	{
		DrawText(Line, FLinearColor::White, 30.0f, DrawY, DrawFont, 1.1f, false);
		DrawY += 20.0f;
	}
}
