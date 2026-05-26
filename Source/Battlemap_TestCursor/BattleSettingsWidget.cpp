#include "BattleSettingsWidget.h"
#include "BattleGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FName GSettingsNativeRootName(TEXT("BattleSettingsNativeRoot"));
	const FName GMicComboName(TEXT("MicCombo"));
	const FName GInputLevelBarName(TEXT("InputLevelBar"));
	const FName GMicStatusName(TEXT("MicStatusText"));
}

TSharedRef<SWidget> UBattleSettingsWidget::RebuildWidget()
{
	BuildNativeSettingsIfNeeded();
	return Super::RebuildWidget();
}

void UBattleSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeSettingsIfNeeded();
	RefreshMicrophoneUI();
}

void UBattleSettingsWidget::NativeDestruct()
{
	if (bTestingMicrophone)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
			{
				GI->EndMicrophoneLevelTest();
			}
		}
	}
	bTestingMicrophone = false;
	Super::NativeDestruct();
}

void UBattleSettingsWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bTestingMicrophone || !InputLevelBar)
	{
		return;
	}

	UBattleGameInstance* GI = nullptr;
	if (APlayerController* PC = GetOwningPlayer())
	{
		GI = Cast<UBattleGameInstance>(PC->GetGameInstance());
	}
	if (!GI)
	{
		return;
	}

	FString MeterError;
	const float Peak = GI->QuerySelectedMicrophonePeakLevel(&MeterError);
	// Boost quiet mics for a visible meter (actual capture peak is often small).
	const float Target = FMath::Clamp(FMath::Sqrt(Peak) * 1.35f, 0.f, 1.f);
	SmoothedPeakLevel = FMath::FInterpTo(SmoothedPeakLevel, Target, InDeltaTime, 12.f);
	InputLevelBar->SetPercent(SmoothedPeakLevel);

	if (MicStatusText)
	{
		if (!MeterError.IsEmpty())
		{
			MicStatusText->SetText(FText::FromString(MeterError));
		}
		else
		{
			const int32 Pct = FMath::RoundToInt(SmoothedPeakLevel * 100.f);
			MicStatusText->SetText(FText::FromString(FString::Printf(TEXT("\u8f93\u5165\u97f3\u91cf\uff1a%d%%\uff08\u6309\u4f4f\u6309\u94ae\u6d4b\u8bd5\uff09"), Pct)));
		}
	}
}

void UBattleSettingsWidget::BuildNativeSettingsIfNeeded()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->FindWidget(GSettingsNativeRootName))
	{
		MicrophoneCombo = Cast<UComboBoxString>(WidgetTree->FindWidget(GMicComboName));
		InputLevelBar = Cast<UProgressBar>(WidgetTree->FindWidget(GInputLevelBarName));
		MicStatusText = Cast<UTextBlock>(WidgetTree->FindWidget(GMicStatusName));
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

	auto AddLabel = [&](const TCHAR* Text, float BottomPad)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		Label->SetText(FText::FromString(Text));
		VB->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPad));
	};

	AddLabel(TEXT("\u8bbe\u7f6e"), 24.f);
	AddLabel(TEXT("\u8bed\u97f3\u547d\u4ee4\u9ea6\u514b\u98ce\uff08\u6218\u6597\u5185\u6309\u4f4f M \u8bf4\u300c\u547d\u4ee4\u300d\uff09"), 12.f);

	MicrophoneCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), GMicComboName);
	MicrophoneCombo->OnSelectionChanged.AddDynamic(this, &UBattleSettingsWidget::OnMicrophoneSelectionChanged);
	VB->AddChildToVerticalBox(MicrophoneCombo)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	UButton* RefreshBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RefreshMicBtn"));
	UTextBlock* RefreshLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	RefreshLbl->SetText(FText::FromString(TEXT("\u5237\u65b0\u9ea6\u514b\u98ce\u5217\u8868")));
	RefreshBtn->AddChild(RefreshLbl);
	RefreshBtn->OnClicked.AddDynamic(this, &UBattleSettingsWidget::OnRefreshMicrophonesClicked);
	VB->AddChildToVerticalBox(RefreshBtn)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	AddLabel(TEXT("\u8f93\u5165\u97f3\u91cf\u6d4b\u8bd5"), 8.f);

	InputLevelBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), GInputLevelBarName);
	InputLevelBar->SetPercent(0.f);
	VB->AddChildToVerticalBox(InputLevelBar)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	MicStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), GMicStatusName);
	MicStatusText->SetText(FText::FromString(TEXT("\u6309\u4f4f\u300c\u6d4b\u8bd5\u9ea6\u514b\u98ce\u300d\u67e5\u770b\u8f93\u5165\u97f3\u91cf")));
	VB->AddChildToVerticalBox(MicStatusText)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	UButton* TestBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TestMicBtn"));
	UTextBlock* TestLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	TestLbl->SetText(FText::FromString(TEXT("\u6d4b\u8bd5\u9ea6\u514b\u98ce\uff08\u6309\u4f4f\uff09")));
	TestBtn->AddChild(TestLbl);
	TestBtn->OnPressed.AddDynamic(this, &UBattleSettingsWidget::OnTestMicrophonePressed);
	TestBtn->OnReleased.AddDynamic(this, &UBattleSettingsWidget::OnTestMicrophoneReleased);
	VB->AddChildToVerticalBox(TestBtn)->SetPadding(FMargin(0.f, 0.f, 0.f, 32.f));

	UButton* Back = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
	UTextBlock* Bt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	Bt->SetText(FText::FromString(TEXT("\u8fd4\u56de")));
	Back->AddChild(Bt);
	Back->OnClicked.AddDynamic(this, &UBattleSettingsWidget::OnBackClicked);
	VB->AddChildToVerticalBox(Back);
}

void UBattleSettingsWidget::RefreshMicrophoneUI()
{
	PopulateMicrophoneCombo();
}

void UBattleSettingsWidget::PopulateMicrophoneCombo()
{
	if (!MicrophoneCombo)
	{
		return;
	}

	UBattleGameInstance* GI = nullptr;
	if (APlayerController* PC = GetOwningPlayer())
	{
		GI = Cast<UBattleGameInstance>(PC->GetGameInstance());
	}
	if (!GI)
	{
		return;
	}

	FString ListError;
	if (!GI->RefreshMicrophoneDeviceList(&ListError))
	{
		MicrophoneCombo->ClearOptions();
		MicrophoneCombo->AddOption(TEXT("\uff08\u672a\u627e\u5230\u9ea6\u514b\u98ce\u8bbe\u5907\uff09"));
		if (MicStatusText)
		{
			MicStatusText->SetText(FText::FromString(
				ListError.IsEmpty() ? TEXT("\u672a\u627e\u5230\u53ef\u7528\u9ea6\u514b\u98ce\u3002") : ListError));
		}
		return;
	}

	const FString Previous = GI->GetSelectedMicrophoneDisplayName();
	MicrophoneCombo->ClearOptions();
	for (const FBattleAudioInputDeviceInfo& Device : GI->GetCachedMicrophoneDevices())
	{
		MicrophoneCombo->AddOption(Device.DisplayName);
	}

	if (!Previous.IsEmpty() && MicrophoneCombo->FindOptionIndex(Previous) != INDEX_NONE)
	{
		MicrophoneCombo->SetSelectedOption(Previous);
	}
	else if (GI->GetCachedMicrophoneDevices().Num() > 0)
	{
		const FString First = GI->GetCachedMicrophoneDevices()[0].DisplayName;
		MicrophoneCombo->SetSelectedOption(First);
		GI->SetSelectedMicrophoneByDisplayName(First);
	}

	if (MicStatusText && !bTestingMicrophone)
	{
		MicStatusText->SetText(FText::FromString(TEXT("\u6309\u4f4f\u300c\u6d4b\u8bd5\u9ea6\u514b\u98ce\u300d\u67e5\u770b\u8f93\u5165\u97f3\u91cf")));
	}
}

void UBattleSettingsWidget::OnBackClicked()
{
	bTestingMicrophone = false;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->DismissTopMenuLayer(PC);
		}
	}
}

void UBattleSettingsWidget::OnRefreshMicrophonesClicked()
{
	RefreshMicrophoneUI();
}

void UBattleSettingsWidget::OnMicrophoneSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType == ESelectInfo::Direct)
	{
		return;
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->SetSelectedMicrophoneByDisplayName(SelectedItem);
		}
	}
}

void UBattleSettingsWidget::OnTestMicrophonePressed()
{
	SmoothedPeakLevel = 0.f;
	if (InputLevelBar)
	{
		InputLevelBar->SetPercent(0.f);
	}

	UBattleGameInstance* GI = nullptr;
	if (APlayerController* PC = GetOwningPlayer())
	{
		GI = Cast<UBattleGameInstance>(PC->GetGameInstance());
	}
	if (!GI)
	{
		return;
	}

	FString StartError;
	if (!GI->BeginMicrophoneLevelTest(&StartError))
	{
		bTestingMicrophone = false;
		if (MicStatusText)
		{
			MicStatusText->SetText(FText::FromString(
				StartError.IsEmpty() ? TEXT("\u65e0\u6cd5\u6253\u5f00\u9ea6\u514b\u98ce\u8f93\u5165\u6d41\u3002") : StartError));
		}
		return;
	}

	bTestingMicrophone = true;
	if (MicStatusText)
	{
		MicStatusText->SetText(FText::FromString(TEXT("\u6b63\u5728\u76d1\u542c\u2026\u8bf7\u5bf9\u7740\u9ea6\u514b\u98ce\u8bf4\u8bdd")));
	}
}

void UBattleSettingsWidget::OnTestMicrophoneReleased()
{
	bTestingMicrophone = false;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->EndMicrophoneLevelTest();
		}
	}
	if (InputLevelBar)
	{
		InputLevelBar->SetPercent(0.f);
	}
	if (MicStatusText)
	{
		MicStatusText->SetText(FText::FromString(TEXT("\u6309\u4f4f\u300c\u6d4b\u8bd5\u9ea6\u514b\u98ce\u300d\u67e5\u770b\u8f93\u5165\u97f3\u91cf")));
	}
}
