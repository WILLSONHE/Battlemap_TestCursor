#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleCareerTypes.h"
#include "BattleMissionDebriefWidget.generated.h"

class UButton;
class UScrollBox;
class UVerticalBox;

UCLASS()
class BATTLEMAP_TESTCURSOR_API UBattleMissionDebriefWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupDebrief(const FMissionDebriefPayload& Payload);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	TObjectPtr<UScrollBox> ScrollBox;

	UPROPERTY()
	TObjectPtr<UVerticalBox> LineList;

	FMissionDebriefPayload CachedPayload;

	UFUNCTION()
	void OnReturnToMainMenu();

	UFUNCTION()
	void OnRematch();
};
