#include "BattleLoadoutScreenWidget.h"
#include "BattleLoadoutSlotRowWidget.h"
#include "BattleGameInstance.h"
#include "BattleCareerSaveGame.h"
#include "BattleCareerTypes.h"
#include "BattleLoadoutCatalog.h"
#include "BattleLoadoutTableLayout.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FName GLoadoutNativeRootName(TEXT("BattleLoadoutNativeRoot"));

	const TCHAR* UnitScaleSuffixForSequence(EFormationUnitScale S)
	{
		switch (S)
		{
		case EFormationUnitScale::Squad: return TEXT("\u73ed");
		case EFormationUnitScale::Platoon: return TEXT("\u6392");
		case EFormationUnitScale::Company: return TEXT("\u8fde");
		case EFormationUnitScale::Battalion: return TEXT("\u8425");
		case EFormationUnitScale::Regiment: return TEXT("\u56e2");
		case EFormationUnitScale::Brigade: return TEXT("\u65c5");
		case EFormationUnitScale::Division: return TEXT("\u5e08");
		case EFormationUnitScale::Corps: return TEXT("\u519b");
		case EFormationUnitScale::ArmyGroup: return TEXT("\u96c6\u56e2\u519b");
		default: return TEXT("");
		}
	}

	int32 SiblingOrdinal1Based(int32 SlotIdx, const TArray<FPlayerLoadoutSlot>& Slots)
	{
		const int32 P = Slots[SlotIdx].ParentSlotIndex;
		TArray<int32> Sib;
		for (int32 j = 0; j < Slots.Num(); ++j)
		{
			if (Slots[j].ParentSlotIndex == P)
			{
				Sib.Add(j);
			}
		}
		Sib.Sort();
		const int32 Found = Sib.IndexOfByKey(SlotIdx);
		return Found == INDEX_NONE ? 1 : Found + 1;
	}

	void AddSizedHeaderCell(UWidgetTree* Tree, UHorizontalBox* Row, const TCHAR* Txt, float MinW, bool bCenterText = true)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		Box->SetMinDesiredWidth(MinW);
		Box->SetMaxDesiredWidth(MinW);
		UTextBlock* Lab = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		Lab->SetText(FText::FromString(Txt));
		Lab->SetJustification(bCenterText ? ETextJustify::Center : ETextJustify::Left);
		Lab->SetColorAndOpacity(FLinearColor::White);
		Lab->SetClipping(EWidgetClipping::ClipToBounds);
		Box->AddChild(Lab);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Box))
		{
			S->SetPadding(FMargin(BattleLoadoutTableLayout::SlotHPadding, 0.f, BattleLoadoutTableLayout::SlotHPadding, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
	}
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
	RootSlot->SetOffsets(FMargin(24.f, 48.f, 48.f, 48.f));
	RootSlot->SetAlignment(FVector2D(0.f, 0.f));

	RankText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RankText"));
	RankText->SetColorAndOpacity(FLinearColor::White);
	XpText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("XpText"));
	XpText->SetColorAndOpacity(FLinearColor::White);
	LoadoutCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoadoutCountText"));
	LoadoutCountText->SetColorAndOpacity(FLinearColor::White);
	MainVB->AddChildToVerticalBox(RankText)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	MainVB->AddChildToVerticalBox(XpText)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	MainVB->AddChildToVerticalBox(LoadoutCountText)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BtnRow"));
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
	if (UVerticalBoxSlot* BtnSlot = MainVB->AddChildToVerticalBox(BtnRow))
	{
		BtnSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	Scroll->SetAnimateWheelScrolling(true);
	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LoadoutListOuter"));
	Scroll->AddChild(Outer);
	if (UScrollBoxSlot* const OuterScrollSlot = Cast<UScrollBoxSlot>(Outer->Slot))
	{
		OuterScrollSlot->SetHorizontalAlignment(HAlign_Left);
	}
	if (UVerticalBoxSlot* ScrollSlot = MainVB->AddChildToVerticalBox(Scroll))
	{
		ScrollSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TableHeader"));
	AddSizedHeaderCell(WidgetTree, Header, TEXT("\u6218\u6597\u5e8f\u5217"), BattleLoadoutTableLayout::WBattleSeq, false);
	AddSizedHeaderCell(WidgetTree, Header, TEXT("\u5355\u4f4d\u89c4\u6a21"), BattleLoadoutTableLayout::WScale);
	AddSizedHeaderCell(WidgetTree, Header, TEXT("\u5355\u4f4d\u540d\u79f0 (\u53ef\u7f16\u8f91)"), BattleLoadoutTableLayout::WName);
	AddSizedHeaderCell(WidgetTree, Header, TEXT("\u5175\u79cd"), BattleLoadoutTableLayout::WBranch);
	AddSizedHeaderCell(WidgetTree, Header, TEXT("\u5206\u7c7b"), BattleLoadoutTableLayout::WClass);
	AddSizedHeaderCell(WidgetTree, Header, TEXT("\u5355\u4f4d"), BattleLoadoutTableLayout::WUnit);
	AddSizedHeaderCell(WidgetTree, Header, TEXT("\u5220\u9664"), BattleLoadoutTableLayout::WDel);
	AddSizedHeaderCell(WidgetTree, Header, TEXT("\u5355\u4f4d\u7f16\u7ec4"), BattleLoadoutTableLayout::WSubCount);
	AddSizedHeaderCell(WidgetTree, Header, TEXT("\u4e0b\u7ea7"), BattleLoadoutTableLayout::WInsert);
	USizeBox* HeaderWrap = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TableHeaderWrap"));
	HeaderWrap->SetMinDesiredWidth(BattleLoadoutTableLayout::TotalMinWidth());
	HeaderWrap->AddChild(Header);
	Outer->AddChildToVerticalBox(HeaderWrap)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	SlotList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotList"));
	Outer->AddChildToVerticalBox(SlotList);

	SyncMirrorFromSave();
	RebuildList();
	RefreshRankXpLabels();
	RefreshLoadoutCountLabel();
}

int32 UBattleLoadoutScreenWidget::GetCareerRankIndex() const
{
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		if (UBattleCareerSaveGame* S = GI->GetCareerSave())
		{
			return FMath::Clamp(S->MilitaryRankIndex, 0, 9);
		}
	}
	return 0;
}

int32 UBattleLoadoutScreenWidget::CountDirectChildren(int32 ParentSlotIndex) const
{
	int32 N = 0;
	for (const FPlayerLoadoutSlot& S : MirrorSlots)
	{
		if (S.ParentSlotIndex == ParentSlotIndex)
		{
			++N;
		}
	}
	return N;
}

FString UBattleLoadoutScreenWidget::ComputeBattleSequenceLabel(int32 InSlotIndex, const TArray<FPlayerLoadoutSlot>& Slots)
{
	if (!Slots.IsValidIndex(InSlotIndex))
	{
		return FString();
	}
	TArray<int32> PathLeafToRoot;
	int32 Cur = InSlotIndex;
	for (int32 G = 0; G < 64 && Slots.IsValidIndex(Cur); ++G)
	{
		PathLeafToRoot.Add(Cur);
		const int32 P = Slots[Cur].ParentSlotIndex;
		if (P == INDEX_NONE || !Slots.IsValidIndex(P))
		{
			break;
		}
		Cur = P;
	}
	for (int32 L = 0, R = PathLeafToRoot.Num() - 1; L < R; ++L, --R)
	{
		PathLeafToRoot.Swap(L, R);
	}
	FString Out;
	for (const int32 Idx : PathLeafToRoot)
	{
		const int32 Ord = SiblingOrdinal1Based(Idx, Slots);
		const TCHAR* const Suf = UnitScaleSuffixForSequence(Slots[Idx].UnitScale);
		Out.Append(FString::Printf(TEXT("%d%s"), Ord, Suf));
	}
	return Out;
}

FString UBattleLoadoutScreenWidget::GetBattleSequenceLabel(int32 InSlotIndex) const
{
	return ComputeBattleSequenceLabel(InSlotIndex, MirrorSlots);
}

int32 UBattleLoadoutScreenWidget::ComputeMaxAllowedScaleOrdinalForRow(int32 SlotIndex) const
{
	const int32 RMax = UBattleGameInstance::GetMaxSelectableUnitScaleOrdinal(GetCareerRankIndex());
	if (!MirrorSlots.IsValidIndex(SlotIndex))
	{
		return RMax;
	}
	const int32 P = MirrorSlots[SlotIndex].ParentSlotIndex;
	if (P != INDEX_NONE && MirrorSlots.IsValidIndex(P))
	{
		const int32 POrd = static_cast<int32>(MirrorSlots[P].UnitScale);
		return FMath::Clamp(POrd - 1, 0, RMax);
	}
	return RMax;
}

TArray<int32> UBattleLoadoutScreenWidget::BuildOrbatDisplayOrder(const TArray<FPlayerLoadoutSlot>& Slots)
{
	TArray<int32> Roots;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const int32 P = Slots[i].ParentSlotIndex;
		if (P == INDEX_NONE || !Slots.IsValidIndex(P))
		{
			Roots.Add(i);
		}
	}
	Roots.Sort();

	TArray<int32> Out;
	TFunction<void(int32)> Dfs = [&](int32 Idx)
	{
		Out.Add(Idx);
		TArray<int32> Children;
		for (int32 j = 0; j < Slots.Num(); ++j)
		{
			if (Slots[j].ParentSlotIndex == Idx)
			{
				Children.Add(j);
			}
		}
		Children.Sort();
		for (int32 C : Children)
		{
			Dfs(C);
		}
	};
	for (int32 R : Roots)
	{
		Dfs(R);
	}
	return Out;
}

void UBattleLoadoutScreenWidget::ApplyDefaultCatalogStrings(FPlayerLoadoutSlot& Slot)
{
	const TArray<FString> Br = BattleLoadoutCatalog::GetBranchOptions();
	Slot.LoadoutBranch = Br.Num() > 0 ? Br[0] : FString(TEXT("\u9646\u519b"));
	const TArray<FString> Cl = BattleLoadoutCatalog::GetClassOptions(Slot.LoadoutBranch);
	Slot.LoadoutClass = Cl.Num() > 0 ? Cl[0] : FString();
	const TArray<FString> Un = BattleLoadoutCatalog::GetUnitOptions(Slot.LoadoutBranch, Slot.LoadoutClass);
	Slot.LoadoutUnit = Un.Num() > 0 ? Un[0] : FString();
}

void UBattleLoadoutScreenWidget::ApplySpawnMappingFromCatalogStrings(FPlayerLoadoutSlot& Slot)
{
	const FString& B = Slot.LoadoutBranch;
	const FString& Cl = Slot.LoadoutClass;
	const FString& U = Slot.LoadoutUnit;
	if (B.Contains(TEXT("\u6d77\u519b")))
	{
		if (Cl.Contains(TEXT("\u6b65\u5175")))
		{
			Slot.Category = EUnitCategory::Infantry;
			Slot.UnitType = EUnitType::Infantry;
		}
		else if (Cl.Contains(TEXT("\u6f5c\u8247")) || Cl.Contains(TEXT("\u6f5c\u8240")) || U.Contains(TEXT("\u6f5c")))
		{
			Slot.Category = EUnitCategory::NavalSub;
			Slot.UnitType = EUnitType::Submarine;
		}
		else
		{
			Slot.Category = EUnitCategory::NavalSurface;
			Slot.UnitType = EUnitType::Destroyer;
		}
	}
	else if (B.Contains(TEXT("\u7a7a\u519b")))
	{
		Slot.Category = EUnitCategory::Aircraft;
		Slot.UnitType = EUnitType::AttackHelicopter;
	}
	else if (Cl.Contains(TEXT("\u5766\u514b")) || U.Contains(TEXT("\u5766\u514b")))
	{
		Slot.Category = EUnitCategory::Vehicle;
		Slot.UnitType = EUnitType::Tank;
	}
	else if (Cl.Contains(TEXT("\u6b65\u6218\u8f66")) || U.Contains(TEXT("\u6b65\u6218")) || U.Contains(TEXT("\u8f6e\u5f0f")))
	{
		Slot.Category = EUnitCategory::Vehicle;
		Slot.UnitType = EUnitType::Tank;
	}
	else
	{
		Slot.Category = EUnitCategory::Infantry;
		Slot.UnitType = EUnitType::Infantry;
	}
}

void UBattleLoadoutScreenWidget::RemoveSlotCascade(int32 SlotIndex)
{
	if (!MirrorSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	TSet<int32> Remove;
	TFunction<void(int32)> Collect = [&](int32 Root)
	{
		Remove.Add(Root);
		for (int32 i = 0; i < MirrorSlots.Num(); ++i)
		{
			if (MirrorSlots[i].ParentSlotIndex == Root)
			{
				Collect(i);
			}
		}
	};
	Collect(SlotIndex);

	TArray<int32> RemovedSorted;
	RemovedSorted.Reserve(Remove.Num());
	for (const int32 R : Remove)
	{
		RemovedSorted.Add(R);
	}
	RemovedSorted.Sort();

	auto CountRemovedBefore = [&RemovedSorted](int32 OldIdx) -> int32
	{
		int32 C = 0;
		for (const int32 R : RemovedSorted)
		{
			if (R < OldIdx)
			{
				++C;
			}
		}
		return C;
	};

	TArray<FPlayerLoadoutSlot> NewSlots;
	NewSlots.Reserve(MirrorSlots.Num() - Remove.Num());
	for (int32 OldI = 0; OldI < MirrorSlots.Num(); ++OldI)
	{
		if (Remove.Contains(OldI))
		{
			continue;
		}
		FPlayerLoadoutSlot S = MirrorSlots[OldI];
		const int32 OldP = S.ParentSlotIndex;
		if (OldP != INDEX_NONE)
		{
			if (Remove.Contains(OldP))
			{
				S.ParentSlotIndex = INDEX_NONE;
			}
			else
			{
				S.ParentSlotIndex = OldP - CountRemovedBefore(OldP);
			}
		}
		NewSlots.Add(MoveTemp(S));
	}
	MirrorSlots = MoveTemp(NewSlots);
	RebuildList();
	RefreshRankXpLabels();
	RefreshLoadoutCountLabel();
}

void UBattleLoadoutScreenWidget::InsertChildSlotAt(int32 ParentSlotIndex)
{
	if (!MirrorSlots.IsValidIndex(ParentSlotIndex))
	{
		return;
	}
	if (CountDirectChildren(ParentSlotIndex) >= MaxDirectChildrenPerUnit)
	{
		return;
	}
	if (MirrorSlots[ParentSlotIndex].UnitScale == EFormationUnitScale::Squad)
	{
		return;
	}
	FPlayerLoadoutSlot Ch;
	Ch.bEnabled = true;
	Ch.bSlotLabelUserOverride = false;
	Ch.ParentSlotIndex = ParentSlotIndex;
	Ch.UnitScale = EFormationUnitScale::Squad;
	ApplyDefaultCatalogStrings(Ch);
	ApplySpawnMappingFromCatalogStrings(Ch);
	MirrorSlots.Add(Ch);
	RebuildList();
	RefreshRankXpLabels();
	RefreshLoadoutCountLabel();
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
		FPlayerLoadoutSlot Co;
		Co.bEnabled = true;
		Co.bSlotLabelUserOverride = false;
		Co.ParentSlotIndex = INDEX_NONE;
		Co.UnitScale = EFormationUnitScale::Company;
		ApplyDefaultCatalogStrings(Co);
		ApplySpawnMappingFromCatalogStrings(Co);
		MirrorSlots.Add(Co);
		for (int32 i = 0; i < 2; ++i)
		{
			FPlayerLoadoutSlot Sq;
			Sq.bEnabled = true;
			Sq.bSlotLabelUserOverride = false;
			Sq.ParentSlotIndex = 0;
			Sq.UnitScale = EFormationUnitScale::Squad;
			ApplyDefaultCatalogStrings(Sq);
			ApplySpawnMappingFromCatalogStrings(Sq);
			MirrorSlots.Add(Sq);
		}
	}
	else
	{
		for (FPlayerLoadoutSlot& S : MirrorSlots)
		{
			if (S.LoadoutBranch.IsEmpty())
			{
				ApplyDefaultCatalogStrings(S);
			}
			ApplySpawnMappingFromCatalogStrings(S);
		}
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

void UBattleLoadoutScreenWidget::RefreshLoadoutCountLabel()
{
	if (!LoadoutCountText)
	{
		return;
	}
	LoadoutCountText->SetText(FText::FromString(FString::Printf(
		TEXT("\u7f16\u7ec4\u6761\u76ee\uff1a%d"), MirrorSlots.Num())));
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
			const int32 Ri = FMath::Clamp(S->MilitaryRankIndex, 0, 9);
			const FString RankName = UBattleGameInstance::GetRankDisplayName(Ri);
			const FString CmdScale = UBattleGameInstance::GetRankCommandScaleLabel(Ri);
			const int32 ToNext = UBattleGameInstance::GetXpToNextRank(Ri, S->Experience);
			RankText->SetText(FText::FromString(FString::Printf(
				TEXT("\u519b\u8854\uff1a%s  \u6700\u9ad8\u6307\u6325\u89c4\u6a21\uff1a%s"), *RankName, *CmdScale)));
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
	const TArray<int32> Ord = BuildOrbatDisplayOrder(MirrorSlots);
	for (int32 ActualIdx : Ord)
	{
		UBattleLoadoutSlotRowWidget* Row = CreateWidget<UBattleLoadoutSlotRowWidget>(this, UBattleLoadoutSlotRowWidget::StaticClass());
		Row->DeferredSetup(this, ActualIdx);
		SlotList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
	}
	RefreshLoadoutCountLabel();
}

void UBattleLoadoutScreenWidget::OnAddSlot()
{
	if (CountDirectChildren(INDEX_NONE) >= MaxDirectChildrenPerUnit)
	{
		return;
	}
	FPlayerLoadoutSlot S;
	S.bEnabled = true;
	S.bSlotLabelUserOverride = false;
	S.ParentSlotIndex = INDEX_NONE;
	S.UnitScale = EFormationUnitScale::Squad;
	ApplyDefaultCatalogStrings(S);
	ApplySpawnMappingFromCatalogStrings(S);
	MirrorSlots.Add(S);
	RebuildList();
	RefreshLoadoutCountLabel();
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
	RefreshLoadoutCountLabel();
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

namespace BattleOrbatStatic
{
	static bool LoadoutSlotHasChildLocal(int32 SlotIdx, const TArray<FPlayerLoadoutSlot>& Slots)
	{
		for (int32 j = 0; j < Slots.Num(); ++j)
		{
			if (Slots[j].ParentSlotIndex == SlotIdx)
			{
				return true;
			}
		}
		return false;
	}

	static bool SquadEligibleLocal(const FPlayerLoadoutSlot& Slot, const TArray<FPlayerLoadoutSlot>& Slots)
	{
		const int32 P = Slot.ParentSlotIndex;
		if (P == INDEX_NONE || !Slots.IsValidIndex(P))
		{
			return false;
		}
		return Slots[P].UnitScale != EFormationUnitScale::Squad;
	}

	static void CollectSquadsRecursive(int32 Idx, const TArray<FPlayerLoadoutSlot>& Slots, TArray<int32>& Out)
	{
		if (!Slots.IsValidIndex(Idx))
		{
			return;
		}
		const FPlayerLoadoutSlot& S = Slots[Idx];
		if (S.UnitScale == EFormationUnitScale::Squad)
		{
			if (S.bEnabled && !LoadoutSlotHasChildLocal(Idx, Slots) && SquadEligibleLocal(S, Slots))
			{
				Out.Add(Idx);
			}
			return;
		}
		TArray<int32> Ch;
		for (int32 j = 0; j < Slots.Num(); ++j)
		{
			if (Slots[j].ParentSlotIndex == Idx)
			{
				Ch.Add(j);
			}
		}
		Ch.Sort();
		for (int32 C : Ch)
		{
			CollectSquadsRecursive(C, Slots, Out);
		}
	}
}

TArray<int32> UBattleLoadoutScreenWidget::GetOrbatRootIndices(const TArray<FPlayerLoadoutSlot>& Slots)
{
	TArray<int32> Roots;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const int32 P = Slots[i].ParentSlotIndex;
		if (P == INDEX_NONE || !Slots.IsValidIndex(P))
		{
			Roots.Add(i);
		}
	}
	Roots.Sort();
	return Roots;
}

TArray<int32> UBattleLoadoutScreenWidget::GetOrbatChildIndices(int32 ParentSlotIndex, const TArray<FPlayerLoadoutSlot>& Slots)
{
	TArray<int32> Children;
	for (int32 j = 0; j < Slots.Num(); ++j)
	{
		if (Slots[j].ParentSlotIndex == ParentSlotIndex)
		{
			Children.Add(j);
		}
	}
	Children.Sort();
	return Children;
}

bool UBattleLoadoutScreenWidget::IsOrbatSlotStrictlyUnderParent(int32 ParentSlotIndex, int32 ChildSlotIndex, const TArray<FPlayerLoadoutSlot>& Slots)
{
	if (ParentSlotIndex == INDEX_NONE || ChildSlotIndex == INDEX_NONE || ParentSlotIndex == ChildSlotIndex)
	{
		return false;
	}
	int32 Walk = ChildSlotIndex;
	while (Slots.IsValidIndex(Walk))
	{
		Walk = Slots[Walk].ParentSlotIndex;
		if (Walk == ParentSlotIndex)
		{
			return true;
		}
	}
	return false;
}

void UBattleLoadoutScreenWidget::CollectEligibleBattleSquadsUnder(int32 SubtreeRootSlotIndex, const TArray<FPlayerLoadoutSlot>& Slots, TArray<int32>& OutSquads)
{
	OutSquads.Reset();
	if (!Slots.IsValidIndex(SubtreeRootSlotIndex))
	{
		return;
	}
	BattleOrbatStatic::CollectSquadsRecursive(SubtreeRootSlotIndex, Slots, OutSquads);
}
