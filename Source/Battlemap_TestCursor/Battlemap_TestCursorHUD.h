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

private:
	void RebuildFallbackContourCache(class ATacticalMapGrid* TacticalMapGrid);

	UPROPERTY(Transient)
	TWeakObjectPtr<class ATacticalMapGrid> CachedContourMap;

	TArray<TPair<FVector, FVector>> CachedFallbackContourSegments;
	float CachedContourInterval = -1.0f;
	int32 CachedGridHalfExtent = -1;
	float CachedContourDepthOffset = 0.0f;
};
