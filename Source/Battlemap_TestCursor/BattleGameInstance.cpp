#include "BattleGameInstance.h"
#include "BattleCareerSaveGame.h"
#include "BattleBalanceDeveloperSettings.h"
#include "BattleMainMenuWidget.h"
#include "BattleLoadoutScreenWidget.h"
#include "BattleSettingsWidget.h"
#include "BattleMissionDebriefWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"

namespace
{
	const TArray<int32> RankXpThresholds = { 0, 100, 250, 500, 900, 1500, 2500 };
	const TArray<FString> RankNames = {
		TEXT("\u5217\u5175"),
		TEXT("\u4e0b\u58eb"),
		TEXT("\u4e2d\u58eb"),
		TEXT("\u4e0a\u58eb"),
		TEXT("\u5c11\u5c09"),
		TEXT("\u4e2d\u5c09"),
		TEXT("\u4e0a\u5c09")
	};
}

void UBattleGameInstance::SanitizeMenuStack()
{
	for (int32 i = MenuWidgetStack.Num() - 1; i >= 0; --i)
	{
		if (!IsValid(MenuWidgetStack[i]))
		{
			MenuWidgetStack.RemoveAt(i);
		}
	}
}

void UBattleGameInstance::HideStackedMenuWidgets()
{
	for (UUserWidget* W : MenuWidgetStack)
	{
		if (IsValid(W))
		{
			W->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UBattleGameInstance::Init()
{
	Super::Init();

	if (!MainMenuWidgetClass)
	{
		MainMenuWidgetClass = UBattleMainMenuWidget::StaticClass();
	}
	if (!LoadoutWidgetClass)
	{
		LoadoutWidgetClass = UBattleLoadoutScreenWidget::StaticClass();
	}
	if (!SettingsWidgetClass)
	{
		SettingsWidgetClass = UBattleSettingsWidget::StaticClass();
	}
	if (!DebriefWidgetClass)
	{
		DebriefWidgetClass = UBattleMissionDebriefWidget::StaticClass();
	}
}

void UBattleGameInstance::LoadCareerFromDisk()
{
	if (CareerSave)
	{
		return;
	}

	if (UGameplayStatics::DoesSaveGameExist(UBattleCareerSaveGame::SlotName(), 0))
	{
		if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(UBattleCareerSaveGame::SlotName(), 0))
		{
			CareerSave = Cast<UBattleCareerSaveGame>(Loaded);
		}
	}

	if (!CareerSave)
	{
		CareerSave = Cast<UBattleCareerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBattleCareerSaveGame::StaticClass()));
		CareerSave->ResetToDefaultRoster();
		SaveCareerToDisk();
	}

	if (CareerSave->FriendlyLoadoutSlots.Num() == 0)
	{
		CareerSave->ResetToDefaultRoster();
		SaveCareerToDisk();
	}
}

void UBattleGameInstance::SaveCareerToDisk()
{
	if (CareerSave)
	{
		UGameplayStatics::SaveGameToSlot(CareerSave, UBattleCareerSaveGame::SlotName(), 0);
	}
}

int32 UBattleGameInstance::GetExperienceForRank(int32 RankIndex)
{
	if (!RankXpThresholds.IsValidIndex(RankIndex))
	{
		return RankXpThresholds.Last();
	}
	return RankXpThresholds[RankIndex];
}

FString UBattleGameInstance::GetRankDisplayName(int32 RankIndex)
{
	if (!RankNames.IsValidIndex(RankIndex))
	{
		return RankNames.Last();
	}
	return RankNames[RankIndex];
}

int32 UBattleGameInstance::GetXpToNextRank(int32 RankIndex, int32 CurrentXp)
{
	const int32 NextRank = RankIndex + 1;
	if (!RankXpThresholds.IsValidIndex(NextRank))
	{
		return 0;
	}
	const int32 NeedTotal = RankXpThresholds[NextRank];
	return FMath::Max(0, NeedTotal - CurrentXp);
}

void UBattleGameInstance::ApplyPostMissionExperience(EMissionOutcomeState Outcome)
{
	LoadCareerFromDisk();
	if (!CareerSave)
	{
		return;
	}

	const UBattleBalanceDeveloperSettings* Bal = GetDefault<UBattleBalanceDeveloperSettings>();
	const int32 Delta = Outcome == EMissionOutcomeState::Victory ? Bal->MissionVictoryExperience : Bal->MissionDefeatExperience;
	CareerSave->Experience += Delta;

	while (CareerSave->MilitaryRankIndex + 1 < RankXpThresholds.Num()
		&& CareerSave->Experience >= RankXpThresholds[CareerSave->MilitaryRankIndex + 1])
	{
		CareerSave->MilitaryRankIndex++;
	}

	SaveCareerToDisk();
}

void UBattleGameInstance::ShowMainMenu(APlayerController* PC)
{
	if (!PC || !MainMenuWidgetClass)
	{
		return;
	}

	SanitizeMenuStack();
	LoadCareerFromDisk();

	if (UUserWidget* W = CreateWidget<UUserWidget>(PC, MainMenuWidgetClass))
	{
		W->AddToViewport(100);
		MenuWidgetStack.Add(W);
		// Do not SetWidgetToFocus on the UserWidget root — TakeWidget() is often not focusable (LogPlayerController error).
		FInputModeUIOnly Mode;
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

void UBattleGameInstance::ShowLoadoutScreen(APlayerController* PC)
{
	if (!PC || !LoadoutWidgetClass)
	{
		return;
	}
	SanitizeMenuStack();
	LoadCareerFromDisk();
	HideStackedMenuWidgets();
	if (UUserWidget* W = CreateWidget<UUserWidget>(PC, LoadoutWidgetClass))
	{
		W->AddToViewport(200);
		MenuWidgetStack.Add(W);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(W->TakeWidget());
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

void UBattleGameInstance::ShowSettingsScreen(APlayerController* PC)
{
	if (!PC || !SettingsWidgetClass)
	{
		return;
	}
	SanitizeMenuStack();
	HideStackedMenuWidgets();
	if (UUserWidget* W = CreateWidget<UUserWidget>(PC, SettingsWidgetClass))
	{
		W->AddToViewport(200);
		MenuWidgetStack.Add(W);
		FInputModeUIOnly Mode;
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

void UBattleGameInstance::ShowMissionDebrief(APlayerController* PC, const FMissionDebriefPayload& Payload)
{
	if (!PC || !DebriefWidgetClass)
	{
		return;
	}
	SanitizeMenuStack();
	HideStackedMenuWidgets();
	if (UBattleMissionDebriefWidget* W = CreateWidget<UBattleMissionDebriefWidget>(PC, DebriefWidgetClass))
	{
		W->SetupDebrief(Payload);
		W->AddToViewport(500);
		MenuWidgetStack.Add(W);
		FInputModeUIOnly Mode;
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

UBattleCareerSaveGame* UBattleGameInstance::GetCareerSave()
{
	LoadCareerFromDisk();
	return CareerSave;
}

void UBattleGameInstance::DismissTopMenuLayer(APlayerController* PC)
{
	if (MenuWidgetStack.Num() == 0)
	{
		return;
	}

	if (UUserWidget* Top = MenuWidgetStack.Pop())
	{
		Top->RemoveFromParent();
	}

	if (MenuWidgetStack.Num() > 0)
	{
		if (UUserWidget* NewTop = MenuWidgetStack.Last())
		{
			NewTop->SetVisibility(ESlateVisibility::Visible);
		}
	}

	if (MenuWidgetStack.Num() == 0 && PC)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	else if (MenuWidgetStack.Num() > 0 && PC)
	{
		if (MenuWidgetStack.Last())
		{
			FInputModeUIOnly Mode;
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = true;
		}
	}
}
