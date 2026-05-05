#include "BattleLoadoutSlotRowWidget.h"
#include "BattleLoadoutScreenWidget.h"
#include "BattleLoadoutCatalog.h"
#include "BattleLoadoutTableLayout.h"
#include "BattleCareerTypes.h"
#include "BattleTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Styling/SlateColor.h"

namespace
{
	const FName GLoadoutRowNativeRootName(TEXT("BattleLoadoutRowNativeRoot"));

	const TCHAR* ScaleLabelForOrdinal(int32 O)
	{
		static const TCHAR* const Labels[] = {
			TEXT("\u73ed"),
			TEXT("\u6392"),
			TEXT("\u8fde"),
			TEXT("\u8425"),
			TEXT("\u56e2/\u5408\u6210\u8425"),
			TEXT("\u65c5"),
			TEXT("\u5e08"),
			TEXT("\u519b"),
			TEXT("\u96c6\u56e2\u519b")
		};
		return Labels[FMath::Clamp(O, 0, 8)];
	}

	int32 OrdinalFromScaleLabel(const FString& S)
	{
		for (int32 O = 0; O <= 8; ++O)
		{
			if (S == ScaleLabelForOrdinal(O))
			{
				return O;
			}
		}
		return 0;
	}

	int32 ComputeDepth(int32 Idx, const TArray<FPlayerLoadoutSlot>& Slots)
	{
		int32 D = 0;
		int32 Cur = Idx;
		for (int32 Guard = 0; Guard < 64; ++Guard)
		{
			const int32 P = Slots[Cur].ParentSlotIndex;
			if (P == INDEX_NONE || !Slots.IsValidIndex(P))
			{
				break;
			}
			++D;
			Cur = P;
		}
		return D;
	}

	int32 FindOptionIndexLocal(const UComboBoxString* Cb, const FString& Opt)
	{
		if (!Cb)
		{
			return INDEX_NONE;
		}
		for (int32 I = 0; I < Cb->GetOptionCount(); ++I)
		{
			if (Cb->GetOptionAtIndex(I) == Opt)
			{
				return I;
			}
		}
		return INDEX_NONE;
	}

	void FillScaleCombo(UComboBoxString* Cb, int32 MaxOrdinalInclusive, EFormationUnitScale Current)
	{
		if (!Cb)
		{
			return;
		}
		Cb->ClearOptions();
		for (int32 O = 0; O <= MaxOrdinalInclusive; ++O)
		{
			Cb->AddOption(ScaleLabelForOrdinal(O));
		}
		const int32 CurO = FMath::Clamp(static_cast<int32>(Current), 0, MaxOrdinalInclusive);
		Cb->SetSelectedOption(ScaleLabelForOrdinal(CurO));
	}

	void ApplyWhiteCombo(UComboBoxString* Cb)
	{
		if (!Cb)
		{
			return;
		}
		FComboBoxStyle St = Cb->GetWidgetStyle();
		St.ComboButtonStyle.ButtonStyle.SetNormalForeground(FSlateColor(FLinearColor::White));
		St.ComboButtonStyle.ButtonStyle.SetHoveredForeground(FSlateColor(FLinearColor::White));
		St.ComboButtonStyle.ButtonStyle.SetPressedForeground(FSlateColor(FLinearColor::White));
		St.ComboButtonStyle.ButtonStyle.SetDisabledForeground(FSlateColor(FLinearColor::White));
		Cb->SetWidgetStyle(St);

		FTableRowStyle Its = Cb->GetItemStyle();
		Its.SetTextColor(FSlateColor(FLinearColor::White));
		Its.SetSelectedTextColor(FSlateColor(FLinearColor::White));
		Cb->SetItemStyle(Its);
	}

	void ApplyWhiteEditable(UEditableText* Et)
	{
		if (!Et)
		{
			return;
		}
		FEditableTextStyle Es = Et->WidgetStyle;
		Es.SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Et->SetWidgetStyle(Es);
	}

	UHorizontalBoxSlot* AddSized(UWidgetTree* Tree, UHorizontalBox* Row, UWidget* Inner, float MinW)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		Box->SetMinDesiredWidth(MinW);
		Box->SetMaxDesiredWidth(MinW);
		Box->AddChild(Inner);
		UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Box);
		if (S)
		{
			S->SetPadding(FMargin(BattleLoadoutTableLayout::SlotHPadding, 0.f, BattleLoadoutTableLayout::SlotHPadding, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		return S;
	}
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

	const int32 Depth = ComputeDepth(SlotIndex, Owner->MirrorSlots);
	const int32 MaxOrd = Owner->ComputeMaxAllowedScaleOrdinalForRow(SlotIndex);
	FPlayerLoadoutSlot& RowSlot = Owner->MirrorSlots[SlotIndex];
	{
		const int32 CurO = static_cast<int32>(RowSlot.UnitScale);
		if (CurO > MaxOrd)
		{
			RowSlot.UnitScale = static_cast<EFormationUnitScale>(MaxOrd);
		}
	}
	if (!RowSlot.bSlotLabelUserOverride)
	{
		RowSlot.SlotLabel = Owner->GetBattleSequenceLabel(SlotIndex);
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), NAME_None);

	{
		UTextBlock* SeqLab = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		SeqLab->SetText(FText::FromString(Owner->GetBattleSequenceLabel(SlotIndex)));
		SeqLab->SetColorAndOpacity(FLinearColor::White);
		SeqLab->SetJustification(ETextJustify::Left);
		SeqLab->SetClipping(EWidgetClipping::ClipToBounds);
		AddSized(WidgetTree, Row, SeqLab, BattleLoadoutTableLayout::WBattleSeq);
	}

	{
		USizeBox* ScaleCol = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ScaleCol->SetMinDesiredWidth(BattleLoadoutTableLayout::WScale);
		ScaleCol->SetMaxDesiredWidth(BattleLoadoutTableLayout::WScale);
		UHorizontalBox* ScaleH = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), NAME_None);
		const float PadW = FMath::Min(static_cast<float>(Depth) * 18.f, BattleLoadoutTableLayout::WScale - 64.f);
		USizeBox* PadBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		PadBox->SetMinDesiredWidth(FMath::Max(0.f, PadW));
		PadBox->SetMaxDesiredWidth(FMath::Max(0.f, PadW));
		ScaleH->AddChildToHorizontalBox(PadBox);
		UComboBoxString* CbScale = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), NAME_None);
		FillScaleCombo(CbScale, MaxOrd, RowSlot.UnitScale);
		ApplyWhiteCombo(CbScale);
		CbScale->OnSelectionChanged.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnScaleChanged);
		const float ComboW = FMath::Max(56.f, BattleLoadoutTableLayout::WScale - PadW);
		USizeBox* ComboWrap = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ComboWrap->SetMinDesiredWidth(ComboW);
		ComboWrap->SetMaxDesiredWidth(ComboW);
		ComboWrap->AddChild(CbScale);
		ScaleH->AddChildToHorizontalBox(ComboWrap);
		ScaleCol->AddChild(ScaleH);
		if (UHorizontalBoxSlot* Ss = Row->AddChildToHorizontalBox(ScaleCol))
		{
			Ss->SetPadding(FMargin(BattleLoadoutTableLayout::SlotHPadding, 0.f, BattleLoadoutTableLayout::SlotHPadding, 0.f));
			Ss->SetVerticalAlignment(VAlign_Center);
		}
	}

	UEditableText* Et = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), NAME_None);
	Et->SetText(FText::FromString(RowSlot.SlotLabel));
	ApplyWhiteEditable(Et);
	Et->OnTextCommitted.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnLabelCommitted);
	AddSized(WidgetTree, Row, Et, BattleLoadoutTableLayout::WName);

	const bool bSquadRow = RowSlot.UnitScale == EFormationUnitScale::Squad;

	UComboBoxString* CbBranch = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), NAME_None);
	for (const FString& B : BattleLoadoutCatalog::GetBranchOptions())
	{
		CbBranch->AddOption(B);
	}
	if (!RowSlot.LoadoutBranch.IsEmpty())
	{
		CbBranch->SetSelectedOption(RowSlot.LoadoutBranch);
	}
	else
	{
		CbBranch->SetSelectedIndex(0);
	}
	ApplyWhiteCombo(CbBranch);
	CbBranch->OnSelectionChanged.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnBranchChanged);
	{
		USizeBox* BrBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		BrBox->SetMinDesiredWidth(BattleLoadoutTableLayout::WBranch);
		BrBox->SetMaxDesiredWidth(BattleLoadoutTableLayout::WBranch);
		BrBox->AddChild(CbBranch);
		BrBox->SetVisibility(bSquadRow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		if (UHorizontalBoxSlot* Sb = Row->AddChildToHorizontalBox(BrBox))
		{
			Sb->SetPadding(FMargin(BattleLoadoutTableLayout::SlotHPadding, 0.f, BattleLoadoutTableLayout::SlotHPadding, 0.f));
			Sb->SetVerticalAlignment(VAlign_Center);
		}
	}

	UComboBoxString* CbClass = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), NAME_None);
	{
		const FString Br = CbBranch->GetSelectedOption();
		for (const FString& C : BattleLoadoutCatalog::GetClassOptions(Br))
		{
			CbClass->AddOption(C);
		}
		if (!RowSlot.LoadoutClass.IsEmpty() && FindOptionIndexLocal(CbClass, RowSlot.LoadoutClass) != INDEX_NONE)
		{
			CbClass->SetSelectedOption(RowSlot.LoadoutClass);
		}
		else if (CbClass->GetOptionCount() > 0)
		{
			CbClass->SetSelectedIndex(0);
		}
	}
	ApplyWhiteCombo(CbClass);
	CbClass->OnSelectionChanged.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnClassChanged);
	{
		USizeBox* ClBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ClBox->SetMinDesiredWidth(BattleLoadoutTableLayout::WClass);
		ClBox->SetMaxDesiredWidth(BattleLoadoutTableLayout::WClass);
		ClBox->AddChild(CbClass);
		ClBox->SetVisibility(bSquadRow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		if (UHorizontalBoxSlot* Sc = Row->AddChildToHorizontalBox(ClBox))
		{
			Sc->SetPadding(FMargin(BattleLoadoutTableLayout::SlotHPadding, 0.f, BattleLoadoutTableLayout::SlotHPadding, 0.f));
			Sc->SetVerticalAlignment(VAlign_Center);
		}
	}

	UComboBoxString* CbUnit = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), NAME_None);
	{
		const FString Br = CbBranch->GetSelectedOption();
		const FString Cl = CbClass->GetSelectedOption();
		for (const FString& U : BattleLoadoutCatalog::GetUnitOptions(Br, Cl))
		{
			CbUnit->AddOption(U);
		}
		if (!RowSlot.LoadoutUnit.IsEmpty() && FindOptionIndexLocal(CbUnit, RowSlot.LoadoutUnit) != INDEX_NONE)
		{
			CbUnit->SetSelectedOption(RowSlot.LoadoutUnit);
		}
		else if (CbUnit->GetOptionCount() > 0)
		{
			CbUnit->SetSelectedIndex(0);
		}
	}
	ApplyWhiteCombo(CbUnit);
	CbUnit->OnSelectionChanged.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnUnitChanged);
	{
		USizeBox* UnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		UnBox->SetMinDesiredWidth(BattleLoadoutTableLayout::WUnit);
		UnBox->SetMaxDesiredWidth(BattleLoadoutTableLayout::WUnit);
		UnBox->AddChild(CbUnit);
		UnBox->SetVisibility(bSquadRow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		if (UHorizontalBoxSlot* Su = Row->AddChildToHorizontalBox(UnBox))
		{
			Su->SetPadding(FMargin(BattleLoadoutTableLayout::SlotHPadding, 0.f, BattleLoadoutTableLayout::SlotHPadding, 0.f));
			Su->SetVerticalAlignment(VAlign_Center);
		}
	}

	UButton* Del = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
	UTextBlock* Dt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	Dt->SetText(FText::FromString(TEXT("\u5220\u9664")));
	Dt->SetColorAndOpacity(FLinearColor::White);
	Del->AddChild(Dt);
	Del->OnClicked.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnDeleteClicked);
	AddSized(WidgetTree, Row, Del, BattleLoadoutTableLayout::WDel);

	{
		const int32 Ch = Owner->CountDirectChildren(SlotIndex);
		UTextBlock* SubLab = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		SubLab->SetText(FText::FromString(FString::Printf(
			TEXT("%d/%d"), Ch, UBattleLoadoutScreenWidget::MaxDirectChildrenPerUnit)));
		SubLab->SetColorAndOpacity(FLinearColor::White);
		SubLab->SetJustification(ETextJustify::Center);
		SubLab->SetClipping(EWidgetClipping::ClipToBounds);
		USizeBox* SubBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		SubBox->SetMinDesiredWidth(BattleLoadoutTableLayout::WSubCount);
		SubBox->SetMaxDesiredWidth(BattleLoadoutTableLayout::WSubCount);
		SubBox->AddChild(SubLab);
		SubBox->SetVisibility(bSquadRow ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
		if (UHorizontalBoxSlot* Ss = Row->AddChildToHorizontalBox(SubBox))
		{
			Ss->SetPadding(FMargin(BattleLoadoutTableLayout::SlotHPadding, 0.f, BattleLoadoutTableLayout::SlotHPadding, 0.f));
			Ss->SetVerticalAlignment(VAlign_Center);
		}
	}

	UButton* BtnIns = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
	UTextBlock* It = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
	It->SetText(FText::FromString(TEXT("\u4e0b\u7ea7")));
	It->SetColorAndOpacity(FLinearColor::White);
	BtnIns->AddChild(It);
	BtnIns->OnClicked.AddDynamic(this, &UBattleLoadoutSlotRowWidget::OnInsertChildClicked);
	{
		const int32 Ch = Owner->CountDirectChildren(SlotIndex);
		const bool bCanMore = RowSlot.UnitScale > EFormationUnitScale::Squad && Ch < UBattleLoadoutScreenWidget::MaxDirectChildrenPerUnit;
		BtnIns->SetVisibility(bCanMore ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	AddSized(WidgetTree, Row, BtnIns, BattleLoadoutTableLayout::WInsert);

	USizeBox* RowWrap = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), GLoadoutRowNativeRootName);
	RowWrap->SetMinDesiredWidth(BattleLoadoutTableLayout::TotalMinWidth());
	RowWrap->AddChild(Row);
	WidgetTree->RootWidget = RowWrap;
}

void UBattleLoadoutSlotRowWidget::OnScaleChanged(FString SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (!Owner || !Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	const int32 MaxOrd = Owner->ComputeMaxAllowedScaleOrdinalForRow(SlotIndex);
	const int32 O = FMath::Clamp(OrdinalFromScaleLabel(SelectedItem), 0, MaxOrd);
	Owner->MirrorSlots[SlotIndex].UnitScale = static_cast<EFormationUnitScale>(O);
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}
	Owner->RebuildList();
}

void UBattleLoadoutSlotRowWidget::OnBranchChanged(FString SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}
	if (!Owner || !Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	Owner->MirrorSlots[SlotIndex].LoadoutBranch = SelectedItem;
	const TArray<FString> Cl = BattleLoadoutCatalog::GetClassOptions(SelectedItem);
	Owner->MirrorSlots[SlotIndex].LoadoutClass = Cl.Num() > 0 ? Cl[0] : FString();
	const TArray<FString> Un = BattleLoadoutCatalog::GetUnitOptions(Owner->MirrorSlots[SlotIndex].LoadoutBranch, Owner->MirrorSlots[SlotIndex].LoadoutClass);
	Owner->MirrorSlots[SlotIndex].LoadoutUnit = Un.Num() > 0 ? Un[0] : FString();
	UBattleLoadoutScreenWidget::ApplySpawnMappingFromCatalogStrings(Owner->MirrorSlots[SlotIndex]);
	Owner->RebuildList();
}

void UBattleLoadoutSlotRowWidget::OnClassChanged(FString SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}
	if (!Owner || !Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	Owner->MirrorSlots[SlotIndex].LoadoutClass = SelectedItem;
	const FString& Br = Owner->MirrorSlots[SlotIndex].LoadoutBranch;
	const TArray<FString> Un = BattleLoadoutCatalog::GetUnitOptions(Br, SelectedItem);
	Owner->MirrorSlots[SlotIndex].LoadoutUnit = Un.Num() > 0 ? Un[0] : FString();
	UBattleLoadoutScreenWidget::ApplySpawnMappingFromCatalogStrings(Owner->MirrorSlots[SlotIndex]);
	Owner->RebuildList();
}

void UBattleLoadoutSlotRowWidget::OnUnitChanged(FString SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}
	if (!Owner || !Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	Owner->MirrorSlots[SlotIndex].LoadoutUnit = SelectedItem;
	UBattleLoadoutScreenWidget::ApplySpawnMappingFromCatalogStrings(Owner->MirrorSlots[SlotIndex]);
}

void UBattleLoadoutSlotRowWidget::OnLabelCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (!Owner || !Owner->MirrorSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	FPlayerLoadoutSlot& S = Owner->MirrorSlots[SlotIndex];
	const FString T = Text.ToString().TrimStartAndEnd();
	if (T.IsEmpty())
	{
		S.bSlotLabelUserOverride = false;
		S.SlotLabel = Owner->GetBattleSequenceLabel(SlotIndex);
	}
	else
	{
		S.bSlotLabelUserOverride = true;
		S.SlotLabel = T;
	}
	Owner->RebuildList();
}

void UBattleLoadoutSlotRowWidget::OnInsertChildClicked()
{
	if (Owner)
	{
		Owner->InsertChildSlotAt(SlotIndex);
	}
}

void UBattleLoadoutSlotRowWidget::OnDeleteClicked()
{
	if (Owner)
	{
		Owner->RemoveSlotCascade(SlotIndex);
	}
}
