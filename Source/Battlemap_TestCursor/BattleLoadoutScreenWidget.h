#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleCareerTypes.h"
#include "BattleLoadoutScreenWidget.generated.h"

class UVerticalBox;

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleLoadoutScreenWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class UBattleLoadoutSlotRowWidget;

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

	TArray<FPlayerLoadoutSlot> MirrorSlots;

	static constexpr int32 MaxSlots = 8;

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
};
