#include "BattleMissionDebriefWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "BattleGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName GDebriefNativeRootName(TEXT("BattleDebriefNativeRoot"));
}

void UBattleMissionDebriefWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBattleMissionDebriefWidget::SetupDebrief(const FMissionDebriefPayload& Payload)
{
	CachedPayload = Payload;

	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->FindWidget(GDebriefNativeRootName))
	{
		return;
	}
	if (UWidget* OldRoot = WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(OldRoot);
		WidgetTree->RootWidget = nullptr;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), GDebriefNativeRootName);
	WidgetTree->RootWidget = Root;

	UBorder* BlackBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DebriefBlackBg"));
	BlackBg->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));
	BlackBg->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(BlackBg))
	{
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgSlot->SetOffsets(FMargin(0.f));
		BgSlot->SetZOrder(-1000);
	}

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVB"));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	const FString OutStr = CachedPayload.Outcome == EMissionOutcomeState::Victory
		? TEXT("\u80dc\u5229")
		: TEXT("\u5931\u8d25");
	Title->SetText(FText::FromString(FString::Printf(TEXT("\u4efb\u52a1\u7ed3\u7b97 \u2014 %s"), *OutStr)));
	Main->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	UTextBlock* Consumption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	Consumption->SetText(FText::FromString(FString::Printf(
		TEXT("\u6d88\u8017\uff1a\u5f39\u836f %d \u53d1\uff1b\u98df\u7269 %.1f\uff1b\u71c3\u6cb9 %.1f"),
		CachedPayload.TotalRoundsExpended,
		CachedPayload.TotalFoodConsumed,
		CachedPayload.TotalFuelConsumed)));
	Main->AddChildToVerticalBox(Consumption)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	auto AppendClassBlock = [&](const TCHAR* BlockTitle, const TArray<FDebriefClassCasualty>& List)
	{
		UTextBlock* H = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		H->SetText(FText::FromString(FString(BlockTitle)));
		Main->AddChildToVerticalBox(H)->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));
		if (List.Num() == 0)
		{
			UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
			Empty->SetText(FText::FromString(TEXT("\u2014 \u65e0")));
			Main->AddChildToVerticalBox(Empty)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
			return;
		}
		for (const FDebriefClassCasualty& C : List)
		{
			UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
			Row->SetText(FText::FromString(FString::Printf(TEXT("%s\uff1a\u51fb\u6bc1 %d"),
				*C.ClassName,
				C.DestroyedCount)));
			Main->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		}
	};
	AppendClassBlock(TEXT("\u6211\u65b9\u6309\u5206\u7c7b\u6218\u635f"), CachedPayload.FriendlyDestroyedByClass);
	AppendClassBlock(TEXT("\u654c\u65b9\u6309\u5206\u7c7b\u6218\u635f"), CachedPayload.EnemyDestroyedByClass);

	ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
	LineList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
	ScrollBox->AddChild(LineList);
	Main->AddChildToVerticalBox(ScrollBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	for (const FDebriefUnitLine& L : CachedPayload.Lines)
	{
		UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		const FString Side = L.bFriendly ? TEXT("\u53cb") : TEXT("\u654c");
		const FString Dest = L.bDestroyed ? TEXT("\u51fb\u6bc1") : TEXT("\u5b58\u6d3b");
		const float Lost = FMath::Max(0.f, L.HealthStart - L.HealthEnd);
		const FString ClassStr = L.LoadoutClass.IsEmpty() ? FString(TEXT("\u672a\u5206\u7c7b")) : L.LoadoutClass;
		Row->SetText(FText::FromString(FString::Printf(
			TEXT("%s [%s][%s] %s | HP %.0f\u2192%.0f (\u635f\u5931%.0f) | %s"),
			*L.UnitLabel,
			*Side,
			*ClassStr,
			*Dest,
			L.HealthStart,
			L.HealthEnd,
			Lost,
			L.bDestroyed ? TEXT("\u5168\u635f") : TEXT(""))));
		LineList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
	}

	UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BtnRow"));
	UButton* BtnMain = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
	UTextBlock* Tm = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	Tm->SetText(FText::FromString(TEXT("\u8fd4\u56de\u4e3b\u83dc\u5355")));
	BtnMain->AddChild(Tm);
	BtnMain->OnClicked.AddDynamic(this, &UBattleMissionDebriefWidget::OnReturnToMainMenu);
	BtnRow->AddChildToHorizontalBox(BtnMain)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));

	UButton* BtnAgain = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
	UTextBlock* Ta = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	Ta->SetText(FText::FromString(TEXT("\u518d\u6765\u4e00\u5c40")));
	BtnAgain->AddChild(Ta);
	BtnAgain->OnClicked.AddDynamic(this, &UBattleMissionDebriefWidget::OnRematch);
	BtnRow->AddChildToHorizontalBox(BtnAgain);

	Main->AddChildToVerticalBox(BtnRow);

	UBorder* ContentWrap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DebriefContentWrap"));
	ContentWrap->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.f));
	ContentWrap->SetPadding(FMargin(40.f));
	ContentWrap->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ContentWrap->SetContent(Main);
	if (UCanvasPanelSlot* WrapSlot = Root->AddChildToCanvas(ContentWrap))
	{
		WrapSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		WrapSlot->SetOffsets(FMargin(0.f));
		WrapSlot->SetZOrder(0);
	}
}

void UBattleMissionDebriefWidget::OnReturnToMainMenu()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->ClearMenuStackForLevelTravel(PC);
		}
	}
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_MainMenu")));
}

void UBattleMissionDebriefWidget::OnRematch()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->ClearMenuStackForLevelTravel(PC);
		}
	}
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_TestMinimal")));
}
