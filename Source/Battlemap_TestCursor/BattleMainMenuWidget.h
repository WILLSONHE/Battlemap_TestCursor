#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleMainMenuWidget.generated.h"

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Build UMG before Super::RebuildWidget — otherwise UE caches SSpacer and the menu never paints. */
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void BuildNativeMainMenuIfNeeded();

	UFUNCTION()
	void OnStartGameClicked();

	UFUNCTION()
	void OnLoadoutClicked();

	UFUNCTION()
	void OnSettingsClicked();

	UFUNCTION()
	void OnQuitClicked();
};
