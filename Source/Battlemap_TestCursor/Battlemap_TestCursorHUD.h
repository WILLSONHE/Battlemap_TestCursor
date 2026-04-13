#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Battlemap_TestCursorHUD.generated.h"

UCLASS()
class BATTLEMAP_TESTCURSOR_API ABattlemap_TestCursorHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
