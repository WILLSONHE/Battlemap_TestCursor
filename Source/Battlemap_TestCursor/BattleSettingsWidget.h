#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleSettingsWidget.generated.h"

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void BuildNativeSettingsIfNeeded();

	UFUNCTION()
	void OnBackClicked();
};
