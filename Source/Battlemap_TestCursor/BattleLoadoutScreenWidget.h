#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleCareerTypes.h"
#include "BattleLoadoutScreenWidget.generated.h"

class UVerticalBox;
class UHorizontalBox;

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleLoadoutScreenWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class UBattleLoadoutSlotRowWidget;

public:
	static constexpr int32 MaxDirectChildrenPerUnit = 11;

	/** Read-only ORBAT label from current tree (e.g. 1\u84251\u8fde3\u73ed). */
	FString GetBattleSequenceLabel(int32 SlotIndex) const;

	/** Same as GetBattleSequenceLabel but for an arbitrary slot array (e.g. career save before UI build). */
	static FString ComputeBattleSequenceLabel(int32 SlotIndex, const TArray<FPlayerLoadoutSlot>& Slots);

	int32 GetCareerRankIndex() const;
	/** Count rows whose parent is ParentSlotIndex (use INDEX_NONE for top-level). */
	int32 CountDirectChildren(int32 ParentSlotIndex) const;
	/** Max unit-scale ordinal (0..8) allowed on this row given rank + parent chain. */
	int32 ComputeMaxAllowedScaleOrdinalForRow(int32 SlotIndex) const;
	void RemoveSlotCascade(int32 SlotIndex);
	void InsertChildSlotAt(int32 ParentSlotIndex);
	static TArray<int32> BuildOrbatDisplayOrder(const TArray<FPlayerLoadoutSlot>& Slots);
	static void ApplySpawnMappingFromCatalogStrings(FPlayerLoadoutSlot& Slot);
	static void ApplyDefaultCatalogStrings(FPlayerLoadoutSlot& Slot);

	/** ORBAT rows with no parent (or invalid parent index). */
	static TArray<int32> GetOrbatRootIndices(const TArray<FPlayerLoadoutSlot>& Slots);
	/** Direct child slot indices of ParentSlotIndex. */
	static TArray<int32> GetOrbatChildIndices(int32 ParentSlotIndex, const TArray<FPlayerLoadoutSlot>& Slots);
	/** All eligible deployable \u73ed (leaf squads) under SubtreeRootSlotIndex. */
	static void CollectEligibleBattleSquadsUnder(int32 SubtreeRootSlotIndex, const TArray<FPlayerLoadoutSlot>& Slots, TArray<int32>& OutSquads);

	/** True if ChildSlotIndex is a strict descendant of ParentSlotIndex in the ORBAT parent chain. */
	static bool IsOrbatSlotStrictlyUnderParent(int32 ParentSlotIndex, int32 ChildSlotIndex, const TArray<FPlayerLoadoutSlot>& Slots);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void BuildNativeLoadoutShellIfNeeded();

	UPROPERTY()
	TObjectPtr<UVerticalBox> SlotList;

	UPROPERTY()
	TObjectPtr<class UTextBlock> RankText;

	UPROPERTY()
	TObjectPtr<class UTextBlock> XpText;

	UPROPERTY()
	TObjectPtr<class UTextBlock> LoadoutCountText;

	TArray<FPlayerLoadoutSlot> MirrorSlots;

	void RebuildList();
	void SyncMirrorFromSave();
	void PushMirrorToSave();

	UFUNCTION()
	void OnAddSlot();

	UFUNCTION()
	void OnClearSlots();

	UFUNCTION()
	void OnSaveClicked();

	UFUNCTION()
	void OnBackClicked();

	void RefreshRankXpLabels();
	void RefreshLoadoutCountLabel();
};
