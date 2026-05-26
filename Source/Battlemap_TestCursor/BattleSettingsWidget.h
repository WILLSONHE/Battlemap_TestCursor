#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleSettingsWidget.generated.h"

class UComboBoxString;
class UProgressBar;
class UTextBlock;

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildNativeSettingsIfNeeded();
	void RefreshMicrophoneUI();
	void PopulateMicrophoneCombo();

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnRefreshMicrophonesClicked();

	UFUNCTION()
	void OnMicrophoneSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnTestMicrophonePressed();

	UFUNCTION()
	void OnTestMicrophoneReleased();

	UPROPERTY()
	TObjectPtr<UComboBoxString> MicrophoneCombo;

	UPROPERTY()
	TObjectPtr<UProgressBar> InputLevelBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> MicStatusText;

	bool bTestingMicrophone = false;
	float SmoothedPeakLevel = 0.f;
};
