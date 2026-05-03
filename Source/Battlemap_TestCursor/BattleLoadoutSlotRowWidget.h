#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleLoadoutSlotRowWidget.generated.h"

class UBattleLoadoutScreenWidget;

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleLoadoutSlotRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call immediately after CreateWidget; NativeConstruct runs before Owner is known, so UI is built here. */
	void DeferredSetup(UBattleLoadoutScreenWidget* InOwner, int32 InSlotIndex);

protected:
	UPROPERTY()
	TObjectPtr<UBattleLoadoutScreenWidget> Owner;

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	UFUNCTION()
	void OnDeleteClicked();

	UFUNCTION()
	void OnLabelCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void OnCategoryChanged(FString SelectedItem, ESelectInfo::Type SelectInfo);

	UFUNCTION()
	void OnEnabledChanged(bool bIsChecked);
};
