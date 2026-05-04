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
	void DeferredSetup(UBattleLoadoutScreenWidget* InOwner, int32 InSlotIndex);

protected:
	UPROPERTY()
	TObjectPtr<UBattleLoadoutScreenWidget> Owner;

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	UFUNCTION()
	void OnDeleteClicked();

	UFUNCTION()
	void OnInsertChildClicked();

	UFUNCTION()
	void OnLabelCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void OnScaleChanged(FString SelectedItem, ESelectInfo::Type SelectInfo);

	UFUNCTION()
	void OnBranchChanged(FString SelectedItem, ESelectInfo::Type SelectInfo);

	UFUNCTION()
	void OnClassChanged(FString SelectedItem, ESelectInfo::Type SelectInfo);

	UFUNCTION()
	void OnUnitChanged(FString SelectedItem, ESelectInfo::Type SelectInfo);
};
