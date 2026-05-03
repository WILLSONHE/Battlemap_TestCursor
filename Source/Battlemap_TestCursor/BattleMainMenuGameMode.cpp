#include "BattleMainMenuGameMode.h"
#include "BattleMainMenuPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

ABattleMainMenuGameMode::ABattleMainMenuGameMode()
{
	PlayerControllerClass = ABattleMainMenuPlayerController::StaticClass();
	// nullptr DefaultPawnClass causes "SpawnActor failed because no class was specified" when PIE spawns the player pawn.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	HUDClass = nullptr;
}
