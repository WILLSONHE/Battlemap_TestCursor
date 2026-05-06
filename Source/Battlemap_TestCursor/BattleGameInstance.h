#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BattleCareerTypes.h"
#include "BattleTypes.h"
#include "BattleGameInstance.generated.h"

class UBattleCareerSaveGame;
class UUserWidget;
class UBattleMainMenuWidget;
class UBattleLoadoutScreenWidget;
class UBattleSettingsWidget;
class UBattleMissionDebriefWidget;
UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|UI")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|UI")
	TSubclassOf<UUserWidget> LoadoutWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|UI")
	TSubclassOf<UUserWidget> SettingsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|UI")
	TSubclassOf<UUserWidget> DebriefWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|UI")
	TSubclassOf<UUserWidget> BattlePauseMenuClass;

	UBattleCareerSaveGame* GetCareerSave();

	void LoadCareerFromDisk();
	void SaveCareerToDisk();

	void ApplyPostMissionExperience(EMissionOutcomeState Outcome);

	void ShowMainMenu(APlayerController* PC);
	void ShowLoadoutScreen(APlayerController* PC);
	void ShowSettingsScreen(APlayerController* PC, bool bHideMenusBelow = true);
	void ShowMissionDebrief(APlayerController* PC, const FMissionDebriefPayload& Payload);

	void ShowBattlePauseMenu(APlayerController* PC);
	void HandleEscapeDuringBattle(APlayerController* PC);
	void ClearAllMenuWidgets(APlayerController* PC);

	void DismissTopMenuLayer(APlayerController* PC);

	/** Remove all stacked UI and unpause; call before OpenLevel from debrief/travel so ESC is not blocked by stale stack. */
	void ClearMenuStackForLevelTravel(APlayerController* PC);

	/** Match ABattlemap_TestCursorPlayerController::BeginPlay — cursor visible during top-down gameplay. */
	static void ApplyBattleGameplayInputMode(APlayerController* PC);

	/** Cumulative XP thresholds for rank index (last entry is max rank cap). */
	static int32 GetExperienceForRank(int32 RankIndex);
	static FString GetRankDisplayName(int32 RankIndex);
	/** Design table: 军衔对应的「最高指挥规模」展示文案。 */
	static FString GetRankCommandScaleLabel(int32 RankIndex);
	/** Max `EFormationUnitScale` ordinal (0=\u73ed .. 8) allowed in loadout rows for this rank (inclusive). */
	static int32 GetMaxSelectableUnitScaleOrdinal(int32 RankIndex);
	static int32 GetXpToNextRank(int32 RankIndex, int32 CurrentXp);

protected:
	void SanitizeMenuStack();

	/** Hide widgets already on the stack so a full-screen overlay does not show through (transparent roots). */
	void HideStackedMenuWidgets();

	UPROPERTY(Transient)
	TObjectPtr<UBattleCareerSaveGame> CareerSave;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> MenuWidgetStack;
};
