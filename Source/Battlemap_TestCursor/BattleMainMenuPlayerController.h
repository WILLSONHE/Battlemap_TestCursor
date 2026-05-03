#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BattleMainMenuPlayerController.generated.h"

UCLASS()
class BATTLEMAP_TESTCURSOR_API ABattleMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	void ShowMainMenuDeferred();
};
