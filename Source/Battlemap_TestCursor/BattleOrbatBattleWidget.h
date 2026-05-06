#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleCareerTypes.h"
#include "BattleOrbatBattleWidget.generated.h"

class UVerticalBox;
class UButton;
class UTextBlock;
class USpacer;
class UCanvasPanel;
class UHorizontalBox;
class UBattleOrbatClickRelay;
class APlayerController;

/** Bottom-centered interactive ORBAT / \u6218\u6597\u5e8f\u5217 strip during battle. */
UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleOrbatBattleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupWithLoadout(const TArray<FPlayerLoadoutSlot>& InSlots);

	/** Sync UI after map click cleared unit selection (nav only; PC clears deployment slots). */
	void ResetOrbatNavUIOnly();

	/** HUD debug: formation buttons only (Back excluded). Parallel with OrbatDebugSlotIndices. */
	void AppendDebugScreenPositions(TArray<FString>& OutLines, APlayerController* PC) const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void BuildShellIfNeeded();
	void RebuildRows();
	void PushSelectionToPlayerController();
	void UpdateSelectionFromNavStack();

public:
	void DispatchOrbatSlotClicked(int32 RowDepth, int32 SlotIdx);

protected:
	UFUNCTION()
	void OnBackClicked();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

	UPROPERTY()
	TObjectPtr<UVerticalBox> MainColumn = nullptr;

	/** Title + back button; compact width, centered in the strip. */
	UPROPERTY()
	TObjectPtr<UHorizontalBox> HeaderRow = nullptr;

	/** Absorbs space above drill rows so drill + root stay against the bottom of the strip. */
	UPROPERTY()
	TObjectPtr<USpacer> TopPushSpacer = nullptr;

	UPROPERTY()
	TObjectPtr<UVerticalBox> RowsInner = nullptr;

	/** Top-level ORBAT buttons along the bottom of the strip (below drill rows). */
	UPROPERTY()
	TObjectPtr<UHorizontalBox> RootRowBox = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> BtnBack = nullptr;

	TArray<FPlayerLoadoutSlot> Slots;
	TArray<int32> NavStack;
	TArray<int32> SelectedSquadIndices;

	UPROPERTY()
	TArray<TObjectPtr<UBattleOrbatClickRelay>> OrbatClickRelays;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> OrbatDebugButtons;

	UPROPERTY()
	TArray<int32> OrbatDebugSlotIndices;
};
