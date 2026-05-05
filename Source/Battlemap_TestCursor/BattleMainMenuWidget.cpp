#include "BattleMainMenuWidget.h"
#include "BattleGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName GMainMenuNativeRootName(TEXT("BattleMainMenuNativeRoot"));
}

TSharedRef<SWidget> UBattleMainMenuWidget::RebuildWidget()
{
	// NativeConstruct runs AFTER this; if we only built there, RebuildWidget would cache SSpacer forever.
	BuildNativeMainMenuIfNeeded();
	return Super::RebuildWidget();
}

void UBattleMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBattleMainMenuWidget::BuildNativeMainMenuIfNeeded()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->FindWidget(GMainMenuNativeRootName))
	{
		return;
	}
	if (UWidget* OldRoot = WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(OldRoot);
		WidgetTree->RootWidget = nullptr;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), GMainMenuNativeRootName);
	WidgetTree->RootWidget = Root;

	UBorder* BlackBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuBlackBg"));
	BlackBg->SetBrushColor(FLinearColor::Black);
	BlackBg->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(BlackBg))
	{
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgSlot->SetOffsets(FMargin(0.f));
		BgSlot->SetZOrder(-1000);
	}

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuVB"));

	auto AddLabel = [&](const TCHAR* Txt)
	{
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		Title->SetText(FText::FromString(Txt));
		VB->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
	};

	AddLabel(TEXT("Battlemap"));

	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BtnStart"));
		UTextBlock* Lab = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TxtStart"));
		Lab->SetText(FText::FromString(TEXT("\u5f00\u59cb\u6e38\u620f")));
		Btn->AddChild(Lab);
		VB->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.f, 8.f, 0.f, 8.f));
		Btn->OnClicked.AddDynamic(this, &UBattleMainMenuWidget::OnStartGameClicked);
	}
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BtnLoadout"));
		UTextBlock* Lab = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TxtLoadout"));
		Lab->SetText(FText::FromString(TEXT("\u90e8\u961f\u7f16\u7ec4")));
		Btn->AddChild(Lab);
		VB->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.f, 8.f, 0.f, 8.f));
		Btn->OnClicked.AddDynamic(this, &UBattleMainMenuWidget::OnLoadoutClicked);
	}
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BtnSettings"));
		UTextBlock* Lab = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TxtSettings"));
		Lab->SetText(FText::FromString(TEXT("\u8bbe\u7f6e")));
		Btn->AddChild(Lab);
		VB->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.f, 8.f, 0.f, 8.f));
		Btn->OnClicked.AddDynamic(this, &UBattleMainMenuWidget::OnSettingsClicked);
	}
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BtnQuit"));
		UTextBlock* Lab = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TxtQuit"));
		Lab->SetText(FText::FromString(TEXT("\u9000\u51fa")));
		Btn->AddChild(Lab);
		VB->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.f, 8.f, 0.f, 8.f));
		Btn->OnClicked.AddDynamic(this, &UBattleMainMenuWidget::OnQuitClicked);
	}

	UBorder* ContentWrap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuContentWrap"));
	ContentWrap->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.f));
	ContentWrap->SetPadding(FMargin(80.f));
	ContentWrap->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ContentWrap->SetContent(VB);
	if (UCanvasPanelSlot* WrapSlot = Root->AddChildToCanvas(ContentWrap))
	{
		WrapSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		WrapSlot->SetOffsets(FMargin(0.f));
		WrapSlot->SetZOrder(0);
	}
}

void UBattleMainMenuWidget::OnStartGameClicked()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_TestMinimal")));
}

void UBattleMainMenuWidget::OnLoadoutClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->ShowLoadoutScreen(PC);
		}
	}
}

void UBattleMainMenuWidget::OnSettingsClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->ShowSettingsScreen(PC);
		}
	}
}

void UBattleMainMenuWidget::OnQuitClicked()
{
	FGenericPlatformMisc::RequestExit(false);
}
