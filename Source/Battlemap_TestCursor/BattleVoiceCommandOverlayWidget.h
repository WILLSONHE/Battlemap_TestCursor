#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleVoiceCommandOverlayWidget.generated.h"

class UTextBlock;
class UBorder;
class UCanvasPanel;

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleVoiceCommandOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOverlayText(const FString& Line, bool bIsError = false);
	void SetListening(bool bListening);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void BuildShellIfNeeded();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

	UPROPERTY()
	TObjectPtr<UBorder> CenterBackdrop = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> CenterText = nullptr;
};
