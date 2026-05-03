#include "BattleMissionDebriefWidget.h"
#include "Blueprint/WidgetTree.h"
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

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVB"));
	if (UCanvasPanelSlot* MainSlot = Root->AddChildToCanvas(Main))
	{
		MainSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		MainSlot->SetOffsets(FMargin(40.f));
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	const FString OutStr = CachedPayload.Outcome == EMissionOutcomeState::Victory
		? TEXT("\u80dc\u5229")
		: TEXT("\u5931\u8d25");
	Title->SetText(FText::FromString(FString::Printf(TEXT("\u4efb\u52a1\u7ed3\u7b97 \u2014 %s"), *OutStr)));
	Main->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

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
		Row->SetText(FText::FromString(FString::Printf(
			TEXT("%s [%s] %s | HP %.0f\u2192%.0f (\u635f\u5931%.0f) | %s"),
			*L.UnitLabel,
			*Side,
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
}

void UBattleMissionDebriefWidget::OnReturnToMainMenu()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_MainMenu")));
}

void UBattleMissionDebriefWidget::OnRematch()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_TestMinimal")));
}
