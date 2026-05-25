#include "BattleOrbatBattleWidget.h"
#include "BattleOrbatClickRelay.h"
#include "BattleLoadoutScreenWidget.h"
#include "Battlemap_TestCursorPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/SlateBlueprintLibrary.h"

namespace
{
	const FName GOrbatBattleRootName(TEXT("BattleOrbatNativeRootV3"));

	/** Viewport pixel Y for top edge of root-level buttons (HUD debug target). */
	constexpr float GOrbatRootRowScreenY = 1213.f;

	/** Viewport pixel Y for top edge of title + back row. */
	constexpr float GOrbatHeaderRowScreenY = 1283.f;

	/** Pin widget top edge to a fixed viewport Y by offset from viewport bottom (works at any height >= target). */
	void SetCanvasTopAtTargetScreenY(UWidget* Widget, float ViewportHeight, float TargetTopY)
	{
		if (!Widget || ViewportHeight <= 1.f)
		{
			return;
		}
		const float OffsetFromBottom = FMath::Max(4.f, ViewportHeight - TargetTopY);
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			Slot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
			Slot->SetAlignment(FVector2D(0.5f, 0.f));
			Slot->SetPosition(FVector2D(0.f, -OffsetFromBottom));
			Slot->SetAutoSize(true);
		}
	}

	/** Pin widget bottom edge to the same viewport Y as root row top (drill stack grows upward). */
	void SetCanvasBottomAtTargetScreenY(UWidget* Widget, float ViewportHeight, float TargetBottomY)
	{
		if (!Widget || ViewportHeight <= 1.f)
		{
			return;
		}
		const float OffsetFromBottom = FMath::Max(4.f, ViewportHeight - TargetBottomY);
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			Slot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
			Slot->SetAlignment(FVector2D(0.5f, 1.f));
			Slot->SetPosition(FVector2D(0.f, -OffsetFromBottom));
			Slot->SetAutoSize(true);
		}
	}

	float GetViewportHeightForOrbat(const UBattleOrbatBattleWidget* Widget)
	{
		if (!Widget)
		{
			return 0.f;
		}
		if (APlayerController* PC = Widget->GetOwningPlayer())
		{
			int32 SizeX = 0;
			int32 SizeY = 0;
			PC->GetViewportSize(SizeX, SizeY);
			if (SizeY > 0)
			{
				return static_cast<float>(SizeY);
			}
		}
		const FVector2D LocalSize = Widget->GetCachedGeometry().GetLocalSize();
		return LocalSize.Y > 1.f ? LocalSize.Y : 0.f;
	}

	void PinWidgetFullViewport(UCanvasPanel* Canvas, UWidget* Widget)
	{
		if (!Canvas || !Widget)
		{
			return;
		}
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget))
		{
			Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			Slot->SetOffsets(FMargin(0.f));
		}
	}
}

TSharedRef<SWidget> UBattleOrbatBattleWidget::RebuildWidget()
{
	BuildShellIfNeeded();
	return Super::RebuildWidget();
}

void UBattleOrbatBattleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D::ZeroVector);
	LastLayoutViewportSize = FVector2D::ZeroVector;
	RebuildRows();
	ApplyOrbatCanvasLayout();
}

void UBattleOrbatBattleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (APlayerController* PC = GetOwningPlayer())
	{
		int32 SizeX = 0;
		int32 SizeY = 0;
		PC->GetViewportSize(SizeX, SizeY);
		ViewportSize = FVector2D(static_cast<float>(SizeX), static_cast<float>(SizeY));
	}
	if (ViewportSize.Y < 1.f)
	{
		ViewportSize = MyGeometry.GetLocalSize();
	}
	if (!ViewportSize.Equals(LastLayoutViewportSize, 0.5f))
	{
		LastLayoutViewportSize = ViewportSize;
		ApplyOrbatCanvasLayout();
	}
}

void UBattleOrbatBattleWidget::SetupWithLoadout(const TArray<FPlayerLoadoutSlot>& InSlots)
{
	Slots = InSlots;
	NavStack.Reset();
	SelectedSquadIndices.Reset();
	UpdateSelectionFromNavStack();
	RebuildRows();
}

void UBattleOrbatBattleWidget::ResetOrbatNavUIOnly()
{
	NavStack.Reset();
	SelectedSquadIndices.Reset();
	RebuildRows();
}

void UBattleOrbatBattleWidget::BuildShellIfNeeded()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->FindWidget(GOrbatBattleRootName))
	{
		return;
	}
	if (UWidget* OldRoot = WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(OldRoot);
		WidgetTree->RootWidget = nullptr;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), GOrbatBattleRootName);
	WidgetTree->RootWidget = RootCanvas;

	ViewportFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OrbatViewportFill"));
	ViewportFill->SetVisibility(ESlateVisibility::HitTestInvisible);
	ViewportFill->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	PinWidgetFullViewport(RootCanvas, ViewportFill);

	RowsInner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OrbatRowsInner"));
	RootCanvas->AddChildToCanvas(RowsInner);

	RootRowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("OrbatRootRow"));
	RootCanvas->AddChildToCanvas(RootRowBox);

	HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("OrbatHeader"));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OrbatTitle"));
	Title->SetText(FText::FromString(TEXT("\u6218\u6597\u5e8f\u5217")));
	if (UHorizontalBoxSlot* TitleSlot = HeaderRow->AddChildToHorizontalBox(Title))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	BtnBack = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OrbatBack"));
	UTextBlock* BackTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	BackTxt->SetText(FText::FromString(TEXT("\u8fd4\u56de\u4e0a\u4e00\u7ea7")));
	BtnBack->AddChild(BackTxt);
	BtnBack->OnClicked.AddDynamic(this, &UBattleOrbatBattleWidget::OnBackClicked);
	if (UHorizontalBoxSlot* BackSlot = HeaderRow->AddChildToHorizontalBox(BtnBack))
	{
		BackSlot->SetVerticalAlignment(VAlign_Center);
	}

	RootCanvas->AddChildToCanvas(HeaderRow);
}

void UBattleOrbatBattleWidget::ApplyOrbatCanvasLayout()
{
	if (!RootCanvas)
	{
		return;
	}
	const float ViewportH = GetViewportHeightForOrbat(this);
	if (ViewportH <= 1.f)
	{
		return;
	}
	SetCanvasBottomAtTargetScreenY(RowsInner, ViewportH, GOrbatRootRowScreenY);
	SetCanvasTopAtTargetScreenY(RootRowBox, ViewportH, GOrbatRootRowScreenY);
	SetCanvasTopAtTargetScreenY(HeaderRow, ViewportH, GOrbatHeaderRowScreenY);
}

void UBattleOrbatBattleWidget::OnBackClicked()
{
	if (NavStack.Num() == 0)
	{
		return;
	}
	NavStack.Pop();
	UpdateSelectionFromNavStack();
	RebuildRows();
}

void UBattleOrbatBattleWidget::DispatchOrbatSlotClicked(int32 RowDepth, int32 SlotIdx)
{
	if (!Slots.IsValidIndex(SlotIdx))
	{
		return;
	}
	NavStack.SetNum(RowDepth);
	NavStack.Add(SlotIdx);
	const FPlayerLoadoutSlot& S = Slots[SlotIdx];
	if (S.UnitScale == EFormationUnitScale::Squad)
	{
		SelectedSquadIndices = { SlotIdx };
	}
	else
	{
		UBattleLoadoutScreenWidget::CollectEligibleBattleSquadsUnder(SlotIdx, Slots, SelectedSquadIndices);
	}
	PushSelectionToPlayerController();
	RebuildRows();
}

void UBattleOrbatBattleWidget::UpdateSelectionFromNavStack()
{
	if (NavStack.Num() == 0)
	{
		SelectedSquadIndices.Reset();
		PushSelectionToPlayerController();
		return;
	}
	const int32 Tip = NavStack.Last();
	if (!Slots.IsValidIndex(Tip))
	{
		SelectedSquadIndices.Reset();
		PushSelectionToPlayerController();
		return;
	}
	const FPlayerLoadoutSlot& S = Slots[Tip];
	if (S.UnitScale == EFormationUnitScale::Squad)
	{
		SelectedSquadIndices = { Tip };
	}
	else
	{
		UBattleLoadoutScreenWidget::CollectEligibleBattleSquadsUnder(Tip, Slots, SelectedSquadIndices);
	}
	PushSelectionToPlayerController();
}

void UBattleOrbatBattleWidget::PushSelectionToPlayerController()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABattlemap_TestCursorPlayerController* BPC = Cast<ABattlemap_TestCursorPlayerController>(PC))
		{
			BPC->SetDeploymentSquadSlotSelection(SelectedSquadIndices);
			BPC->SetDeploymentOrbatTipSlot(NavStack.Num() > 0 ? NavStack.Last() : INDEX_NONE);
			BPC->RefreshWorldSelectionForDeploymentSlots();
		}
	}
}

void UBattleOrbatBattleWidget::RebuildRows()
{
	if (!RowsInner || !RootRowBox || !BtnBack)
	{
		return;
	}

	RowsInner->ClearChildren();
	RootRowBox->ClearChildren();
	OrbatClickRelays.Reset();
	OrbatDebugButtons.Reset();
	OrbatDebugSlotIndices.Reset();
	BtnBack->SetVisibility(NavStack.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	auto AddSlotButtonToRow = [&](UHorizontalBox* Row, int32 Idx, int32 RowDepth)
	{
		if (!Slots.IsValidIndex(Idx))
		{
			return;
		}
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
		UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		const FString Lab = UBattleLoadoutScreenWidget::ComputeBattleSequenceLabel(Idx, Slots);
		Txt->SetText(FText::FromString(Lab));
		Btn->AddChild(Txt);
		UBattleOrbatClickRelay* Relay = NewObject<UBattleOrbatClickRelay>(this);
		Relay->Owner = this;
		Relay->RowDepth = RowDepth;
		Relay->SlotIdx = Idx;
		OrbatClickRelays.Add(Relay);
		Btn->OnClicked.AddDynamic(Relay, &UBattleOrbatClickRelay::Fire);
		if (UHorizontalBoxSlot* BtnSlot = Row->AddChildToHorizontalBox(Btn))
		{
			BtnSlot->SetPadding(FMargin(4.f, 2.f));
		}
		OrbatDebugButtons.Add(Btn);
		OrbatDebugSlotIndices.Add(Idx);
	};

	struct FOrbatRowSpec
	{
		TArray<int32> Indices;
		int32 RowDepth = 0;
	};
	TArray<FOrbatRowSpec> Specs;
	const TArray<int32> Roots = UBattleLoadoutScreenWidget::GetOrbatRootIndices(Slots);
	Specs.Add({ Roots, 0 });

	for (int32 D = 0; D < NavStack.Num(); ++D)
	{
		const int32 ParentIdx = NavStack[D];
		const TArray<int32> Ch = UBattleLoadoutScreenWidget::GetOrbatChildIndices(ParentIdx, Slots);
		Specs.Add({ Ch, D + 1 });
	}

	for (int32 Idx : Roots)
	{
		AddSlotButtonToRow(RootRowBox, Idx, 0);
	}

	for (int32 Si = Specs.Num() - 1; Si >= 1; --Si)
	{
		const TArray<int32>& Indices = Specs[Si].Indices;
		const int32 RowDepth = Specs[Si].RowDepth;
		if (Indices.Num() == 0)
		{
			continue;
		}
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), NAME_None);
		for (int32 Idx : Indices)
		{
			AddSlotButtonToRow(Row, Idx, RowDepth);
		}
		if (Row->GetChildrenCount() > 0)
		{
			if (UVerticalBoxSlot* InnerSlot = RowsInner->AddChildToVerticalBox(Row))
			{
				InnerSlot->SetPadding(FMargin(0.f, 4.f));
			}
		}
	}

	RowsInner->SetVisibility(NavStack.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ApplyOrbatCanvasLayout();
	InvalidateLayoutAndVolatility();
}

void UBattleOrbatBattleWidget::AppendDebugScreenPositions(TArray<FString>& OutLines, APlayerController* PC) const
{
	if (!PC)
	{
		OutLines.Add(TEXT("  （无 PlayerController）"));
		return;
	}

	const int32 N = FMath::Min(OrbatDebugButtons.Num(), OrbatDebugSlotIndices.Num());
	for (int32 i = 0; i < N; ++i)
	{
		UButton* Btn = OrbatDebugButtons[i];
		if (!Btn || !Btn->IsVisible())
		{
			continue;
		}
		const int32 SlotIdx = OrbatDebugSlotIndices[i];
		const FString Lab = Slots.IsValidIndex(SlotIdx)
			? UBattleLoadoutScreenWidget::ComputeBattleSequenceLabel(SlotIdx, Slots)
			: TEXT("?");
		const FGeometry& Geo = Btn->GetCachedGeometry();
		const FVector2D Abs = Geo.GetAbsolutePosition();
		const FVector2D Size = Geo.GetLocalSize();
		FVector2D Pix = FVector2D::ZeroVector;
		FVector2D VP = FVector2D::ZeroVector;
		USlateBlueprintLibrary::AbsoluteToViewport(PC, Abs, Pix, VP);
		OutLines.Add(FString::Printf(TEXT("  [%d] 槽%d \"%s\" 左上(%.0f,%.0f) 宽%.0fx高%.0f"),
			i, SlotIdx, *Lab, Pix.X, Pix.Y, Size.X, Size.Y));
	}
	if (N == 0)
	{
		OutLines.Add(TEXT("  （无序列按钮）"));
	}
}
