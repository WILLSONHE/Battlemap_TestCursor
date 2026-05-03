#include "BattleLoadoutSlotRowWidget.h"
#include "BattleLoadoutScreenWidget.h"
#include "BattleCareerTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

namespace
{
	const FName GLoadoutRowNativeRootName(TEXT("BattleLoadoutRowNativeRoot"));
}

void UBattleLoadoutSlotRowWidget::DeferredSetup(UBattleLoadoutScreenWidget* InOwner, int32 InSlotIndex)
{
	Owner = InOwner;
	SlotIndex = InSlotIndex;

	if (!WidgetTree || !Owner || !Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	if (UWidget* OldRoot = WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(OldRoot);
		WidgetTree->RootWidget = nullptr;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), GLoadoutRowNativeRootName);
	WidgetTree->RootWidget = Row;

	UTextBlock* IdxLab = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	IdxLab->SetText(FText::FromString(FString::Printf(TEXT("#%d"), SlotIndex + 1)));
	Row->AddChildToHorizontalBox(IdxLab)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	UEditableText* Et = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), NAME_None);
	Et->SetText(FText::FromString(Owner->MirrorSlots[SlotIndex].SlotLabel));
	Et->OnTextCommitted.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnLabelCommitted);
	Row->AddChildToHorizontalBox(Et)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	UComboBoxString* Cb = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), NAME_None);
	Cb->AddOption(TEXT("Infantry"));
	Cb->AddOption(TEXT("Vehicle"));
	const bool bVeh = Owner->MirrorSlots[SlotIndex].Category == EUnitCategory::Vehicle;
	Cb->SetSelectedOption(bVeh ? TEXT("Vehicle") : TEXT("Infantry"));
	Cb->OnSelectionChanged.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnCategoryChanged);
	Row->AddChildToHorizontalBox(Cb)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	UCheckBox* Chk = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), NAME_None);
	Chk->SetIsChecked(Owner->MirrorSlots[SlotIndex].bEnabled);
	Chk->OnCheckStateChanged.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnEnabledChanged);
	Row->AddChildToHorizontalBox(Chk)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	UButton* Del = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
	UTextBlock* Dt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	Dt->SetText(FText::FromString(TEXT("\u5220\u9664")));
	Del->AddChild(Dt);
	Del->OnClicked.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnDeleteClicked);
	Row->AddChildToHorizontalBox(Del);
}

void UBattleLoadoutSlotRowWidget::OnLabelCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (Owner && Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		Owner->MirrorSlots[SlotIndex].SlotLabel = Text.ToString();
	}
}

void UBattleLoadoutSlotRowWidget::OnCategoryChanged(FString SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (!Owner || !Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	if (SelectedItem == TEXT("Vehicle"))
	{
		Owner->MirrorSlots[SlotIndex].Category = EUnitCategory::Vehicle;
		Owner->MirrorSlots[SlotIndex].UnitType = EUnitType::Tank;
	}
	else
	{
		Owner->MirrorSlots[SlotIndex].Category = EUnitCategory::Infantry;
		Owner->MirrorSlots[SlotIndex].UnitType = EUnitType::Infantry;
	}
}

void UBattleLoadoutSlotRowWidget::OnEnabledChanged(bool bIsChecked)
{
	if (Owner && Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		Owner->MirrorSlots[SlotIndex].bEnabled = bIsChecked;
	}
}

void UBattleLoadoutSlotRowWidget::OnDeleteClicked()
{
	if (Owner && Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		Owner->MirrorSlots.RemoveAt(SlotIndex);
		Owner->RebuildList();
		Owner->RefreshRankXpLabels();
	}
}
