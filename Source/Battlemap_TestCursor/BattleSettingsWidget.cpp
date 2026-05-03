#include "BattleSettingsWidget.h"
#include "BattleGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FName GSettingsNativeRootName(TEXT("BattleSettingsNativeRoot"));
}

TSharedRef<SWidget> UBattleSettingsWidget::RebuildWidget()
{
	BuildNativeSettingsIfNeeded();
	return Super::RebuildWidget();
}

void UBattleSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBattleSettingsWidget::BuildNativeSettingsIfNeeded()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->FindWidget(GSettingsNativeRootName))
	{
		return;
	}
	if (UWidget* OldRoot = WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(OldRoot);
		WidgetTree->RootWidget = nullptr;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), GSettingsNativeRootName);
	WidgetTree->RootWidget = Root;

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VB"));
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(VB))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(48.f));
	}

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	Hint->SetText(FText::FromString(TEXT("\u8bbe\u7f6e\uff08\u5360\u4f4d\uff09\uff1a\u97f3\u91cf\u3001\u5168\u5c4f\u7b49\u5c06\u5728\u540e\u7eed\u7248\u672c\u63a5\u5165\u3002")));
	VB->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));

	UButton* Back = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
	UTextBlock* Bt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	Bt->SetText(FText::FromString(TEXT("\u8fd4\u56de")));
	Back->AddChild(Bt);
	Back->OnClicked.AddDynamic(this, &UBattleSettingsWidget::OnBackClicked);
	VB->AddChildToVerticalBox(Back);
}

void UBattleSettingsWidget::OnBackClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->DismissTopMenuLayer(PC);
		}
	}
}
