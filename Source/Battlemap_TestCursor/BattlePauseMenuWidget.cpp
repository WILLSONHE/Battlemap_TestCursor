#include "BattlePauseMenuWidget.h"
#include "BattleGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName GPauseNativeRootName(TEXT("BattlePauseNativeRoot"));
}

TSharedRef<SWidget> UBattlePauseMenuWidget::RebuildWidget()
{
	BuildNativePauseIfNeeded();
	return Super::RebuildWidget();
}

void UBattlePauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBattlePauseMenuWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(World, false);
	}
	Super::NativeDestruct();
}

void UBattlePauseMenuWidget::BuildNativePauseIfNeeded()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->FindWidget(GPauseNativeRootName))
	{
		return;
	}
	if (UWidget* OldRoot = WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(OldRoot);
		WidgetTree->RootWidget = nullptr;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), GPauseNativeRootName);
	WidgetTree->RootWidget = Root;

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseVB"));
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(VB))
	{
		CanvasSlot->SetAnchors(FAnchors(0.35f, 0.3f, 0.65f, 0.7f));
		CanvasSlot->SetOffsets(FMargin(0.f));
	}

	auto AddButton = [&](const TCHAR* Label)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
		UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		Txt->SetText(FText::FromString(FString(Label)));
		Btn->AddChild(Txt);
		VB->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		return Btn;
	};

	UButton* BtnContinue = AddButton(TEXT("\u7ee7\u7eed\u6e38\u620f"));
	BtnContinue->OnClicked.AddDynamic(this, &UBattlePauseMenuWidget::OnContinueClicked);

	UButton* BtnSettings = AddButton(TEXT("\u8bbe\u7f6e"));
	BtnSettings->OnClicked.AddDynamic(this, &UBattlePauseMenuWidget::OnSettingsClicked);

	UButton* BtnMain = AddButton(TEXT("\u8fd4\u56de\u4e3b\u83dc\u5355"));
	BtnMain->OnClicked.AddDynamic(this, &UBattlePauseMenuWidget::OnMainMenuClicked);
}

void UBattlePauseMenuWidget::OnContinueClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->DismissTopMenuLayer(PC);
		}
	}
}

void UBattlePauseMenuWidget::OnSettingsClicked()
{
	SetVisibility(ESlateVisibility::Hidden);
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->ShowSettingsScreen(PC, false);
		}
	}
}

void UBattlePauseMenuWidget::OnMainMenuClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UWorld* World = PC->GetWorld())
		{
			UGameplayStatics::SetGamePaused(World, false);
		}
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->ClearAllMenuWidgets(PC);
		}
		UGameplayStatics::OpenLevel(this, FName(TEXT("L_MainMenu")));
	}
}
