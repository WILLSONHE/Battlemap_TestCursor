#include "BattleMainMenuPlayerController.h"
#include "BattleGameInstance.h"
#include "TimerManager.h"

void ABattleMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Next tick: viewport / owning player are reliably ready for AddToViewport (UE 5.x).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ABattleMainMenuPlayerController::ShowMainMenuDeferred));
	}
}

void ABattleMainMenuPlayerController::ShowMainMenuDeferred()
{
	if (UBattleGameInstance* GI = Cast<UBattleGameInstance>(GetGameInstance()))
	{
		GI->ShowMainMenu(this);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BattleMainMenu: GameInstance is not UBattleGameInstance. In DefaultEngine.ini set GameInstanceClass under [/Script/EngineSettings.GameMapsSettings] (UE 5.x), e.g. GameInstanceClass=/Script/Battlemap_TestCursor.BattleGameInstance"));
	}
}
