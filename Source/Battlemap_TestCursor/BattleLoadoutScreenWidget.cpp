#include "BattleLoadoutScreenWidget.h"
#include "BattleLoadoutSlotRowWidget.h"
#include "BattleGameInstance.h"
#include "BattleCareerSaveGame.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FName GLoadoutNativeRootName(TEXT("BattleLoadoutNativeRoot"));
}

TSharedRef<SWidget> UBattleLoadoutScreenWidget::RebuildWidget()
{
	BuildNativeLoadoutShellIfNeeded();
	return Super::RebuildWidget();
}

void UBattleLoadoutScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBattleLoadoutScreenWidget::BuildNativeLoadoutShellIfNeeded()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->FindWidget(GLoadoutNativeRootName))
	{
		return;
	}
	if (UWidget* OldRoot = WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(OldRoot);
		WidgetTree->RootWidget = nullptr;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), GLoadoutNativeRootName);
	WidgetTree->RootWidget = Root;

	UVerticalBox* MainVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVB"));
	UCanvasPanelSlot* RootSlot = Root->AddChildToCanvas(MainVB);
	RootSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	RootSlot->SetOffsets(FMargin(48.f));

	RankText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RankText"));
	XpText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("XpText"));
	MainVB->AddChildToVerticalBox(RankText)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	MainVB->AddChildToVerticalBox(XpText)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
	SlotList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotList"));
	Scroll->AddChild(SlotList);
	MainVB->AddChildToVerticalBox(Scroll)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BtnRow"));
	// AddDynamic requires a compile-time member pointer literal; do not pass a variable Fn (Delegate.h assert).
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		L->SetText(FText::FromString(TEXT("\u6dfb\u52a0\u69fd\u4f4d")));
		B->AddChild(L);
		BtnRow->AddChildToHorizontalBox(B)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		B->OnClicked.AddDynamic(this, &UBattleLoadoutScreenWidget::OnAddSlot);
	}
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		L->SetText(FText::FromString(TEXT("\u6e05\u7a7a\u9ed8\u8ba4")));
		B->AddChild(L);
		BtnRow->AddChildToHorizontalBox(B)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		B->OnClicked.AddDynamic(this, &UBattleLoadoutScreenWidget::OnClearSlots);
	}
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		L->SetText(FText::FromString(TEXT("\u4fdd\u5b58")));
		B->AddChild(L);
		BtnRow->AddChildToHorizontalBox(B)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		B->OnClicked.AddDynamic(this, &UBattleLoadoutScreenWidget::OnSaveClicked);
	}
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		L->SetText(FText::FromString(TEXT("\u8fd4\u56de")));
		B->AddChild(L);
		BtnRow->AddChildToHorizontalBox(B)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		B->OnClicked.AddDynamic(this, &UBattleLoadoutScreenWidget::OnBackClicked);
	}
	MainVB->AddChildToVerticalBox(BtnRow);

	SyncMirrorFromSave();
	RebuildList();
	RefreshRankXpLabels();
}

void UBattleLoadoutScreenWidget::SyncMirrorFromSave()
{
	MirrorSlots.Reset();
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		if (UBattleCareerSaveGame* S = GI->GetCareerSave())
		{
			MirrorSlots = S->FriendlyLoadoutSlots;
		}
	}
	if (MirrorSlots.Num() == 0)
	{
		FPlayerLoadoutSlot A;
		A.SlotLabel = TEXT("Alpha-1");
		A.Category = EUnitCategory::Infantry;
		A.UnitType = EUnitType::Infantry;
		A.bEnabled = true;
		MirrorSlots.Add(A);
	}
}

void UBattleLoadoutScreenWidget::PushMirrorToSave()
{
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		if (UBattleCareerSaveGame* S = GI->GetCareerSave())
		{
			S->FriendlyLoadoutSlots = MirrorSlots;
		}
	}
}

void UBattleLoadoutScreenWidget::RefreshRankXpLabels()
{
	if (!RankText || !XpText)
	{
		return;
	}
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		if (UBattleCareerSaveGame* S = GI->GetCareerSave())
		{
			const FString RankName = UBattleGameInstance::GetRankDisplayName(S->MilitaryRankIndex);
			const int32 ToNext = UBattleGameInstance::GetXpToNextRank(S->MilitaryRankIndex, S->Experience);
			RankText->SetText(FText::FromString(FString::Printf(TEXT("\u519b\u8854\uff1a%s"), *RankName)));
			XpText->SetText(FText::FromString(FString::Printf(TEXT("\u7ecf\u9a8c\uff1a%d  \u8ddd\u4e0b\u4e00\u7ea7\u8fd8\u9700\uff1a%d"), S->Experience, ToNext)));
		}
	}
}

void UBattleLoadoutScreenWidget::RebuildList()
{
	if (!SlotList)
	{
		return;
	}
	SlotList->ClearChildren();

	for (int32 i = 0; i < MirrorSlots.Num(); ++i)
	{
		UBattleLoadoutSlotRowWidget* Row = CreateWidget<UBattleLoadoutSlotRowWidget>(this, UBattleLoadoutSlotRowWidget::StaticClass());
		Row->DeferredSetup(this, i);
		SlotList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));
	}
}

void UBattleLoadoutScreenWidget::OnAddSlot()
{
	if (MirrorSlots.Num() >= MaxSlots)
	{
		return;
	}
	FPlayerLoadoutSlot S;
	S.bEnabled = true;
	S.SlotLabel = FString::Printf(TEXT("Unit-%d"), MirrorSlots.Num() + 1);
	S.Category = EUnitCategory::Infantry;
	S.UnitType = EUnitType::Infantry;
	MirrorSlots.Add(S);
	RebuildList();
}

void UBattleLoadoutScreenWidget::OnClearSlots()
{
	MirrorSlots.Reset();
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		if (UBattleCareerSaveGame* S = GI->GetCareerSave())
		{
			S->ResetToDefaultRoster();
		}
	}
	SyncMirrorFromSave();
	RebuildList();
	RefreshRankXpLabels();
}

void UBattleLoadoutScreenWidget::OnSaveClicked()
{
	PushMirrorToSave();
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		GI->SaveCareerToDisk();
	}
	RefreshRankXpLabels();
}

void UBattleLoadoutScreenWidget::OnBackClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(PC->GetGameInstance()))
		{
			GI->DismissTopMenuLayer(PC);
		}
	}
}
