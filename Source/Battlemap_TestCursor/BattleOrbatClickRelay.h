#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleOrbatClickRelay.generated.h"

class UBattleOrbatBattleWidget;

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleOrbatClickRelay : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UBattleOrbatBattleWidget> Owner = nullptr;

	int32 RowDepth = 0;
	int32 SlotIdx = INDEX_NONE;

	UFUNCTION()
	void Fire();
};
