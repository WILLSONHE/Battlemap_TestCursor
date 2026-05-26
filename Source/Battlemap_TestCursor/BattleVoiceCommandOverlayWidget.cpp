#include "BattleVoiceCommandOverlayWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace
{
	const FName GVoiceOverlayRoot(TEXT("BattleVoiceOverlayRoot"));
}

TSharedRef<SWidget> UBattleVoiceCommandOverlayWidget::RebuildWidget()
{
	BuildShellIfNeeded();
	return Super::RebuildWidget();
}

void UBattleVoiceCommandOverlayWidget::BuildShellIfNeeded()
{
	if (!WidgetTree || WidgetTree->FindWidget(GVoiceOverlayRoot))
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), GVoiceOverlayRoot);
	WidgetTree->RootWidget = RootCanvas;

	CenterBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VoiceCmdBackdrop"));
	CenterBackdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	CenterBackdrop->SetVisibility(ESlateVisibility::Collapsed);

	CenterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VoiceCmdText"));
	CenterText->SetJustification(ETextJustify::Center);
	CenterText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo Font = CenterText->GetFont();
	Font.Size = 28;
	CenterText->SetFont(Font);
	CenterText->SetText(FText::GetEmpty());
	CenterBackdrop->AddChild(CenterText);

	if (UCanvasPanelSlot* BackSlot = RootCanvas->AddChildToCanvas(CenterBackdrop))
	{
		BackSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		BackSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BackSlot->SetAutoSize(true);
	}
}

void UBattleVoiceCommandOverlayWidget::SetOverlayText(const FString& Line, bool bIsError)
{
	BuildShellIfNeeded();
	if (!CenterText || !CenterBackdrop)
	{
		return;
	}
	CenterText->SetText(FText::FromString(Line));
	CenterText->SetColorAndOpacity(FSlateColor(bIsError ? FLinearColor(1.f, 0.35f, 0.35f) : FLinearColor::White));
	CenterBackdrop->SetVisibility(Line.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UBattleVoiceCommandOverlayWidget::SetListening(bool bListening)
{
	if (bListening)
	{
		SetOverlayText(TEXT("\u6309\u4f4f M \u8bf4\u8bdd\u2026"), false);
	}
	else if (CenterBackdrop && CenterBackdrop->GetVisibility() == ESlateVisibility::HitTestInvisible)
	{
		const FString Current = CenterText ? CenterText->GetText().ToString() : FString();
		if (Current.Contains(TEXT("\u76d1\u542c\u4e2d")))
		{
			SetOverlayText(FString(), false);
		}
	}
}
