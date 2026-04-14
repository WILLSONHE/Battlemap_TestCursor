#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BattleTypes.h"
#include "BattleUnit.generated.h"

class UBattleCommandComponent;
class UBattleDetectionComponent;
class UBattleCommsComponent;
class UBattleSupplyComponent;
class USceneComponent;
class UStaticMeshComponent;
class AMoveCommandMarkerActor;

UCLASS(Blueprintable)
class BATTLEMAP_TESTCURSOR_API ABattleUnit : public APawn
{
	GENERATED_BODY()

public:
	ABattleUnit();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	FUnitBaseData UnitData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	FString UnitLabel = TEXT("Unit");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	EStealthState StealthState = EStealthState::Visible;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	bool bFriendly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	float MoveSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Combat")
	float AttackRange = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Combat")
	float AttackDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Combat")
	float AttackCooldown = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void ApplyDamageValue(float DamageValue);

	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool IsAlive() const;

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool IsSelected() const { return bSelected; }

	UFUNCTION(BlueprintCallable, Category = "Battle|Command")
	void IssueMoveCommandInterrupt(const FVector& TargetLocation, ECommandPriority Priority = ECommandPriority::High);

	UFUNCTION(BlueprintCallable, Category = "Battle|Command")
	void IssueAttackCommandInterrupt(ABattleUnit* TargetUnit, ECommandPriority Priority = ECommandPriority::High);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Components")
	UBattleCommandComponent* CommandComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Components")
	UBattleDetectionComponent* DetectionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Components")
	UBattleCommsComponent* CommsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Components")
	UBattleSupplyComponent* SupplyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Visual")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Visual")
	UStaticMeshComponent* UnitMesh;

	UPROPERTY()
	AMoveCommandMarkerActor* MoveCommandMarker;

protected:
	void ProcessActiveCommand(float DeltaSeconds);
	void ProcessAttackCommand(float DeltaSeconds);

	void SpawnOrReplaceMoveMarker(const FVector& TargetLocation);

	void DestroyMoveMarker();

	bool AcquireNextCommand();

	UPROPERTY()
	FActiveCommand ActiveCommand;

	bool bHasActiveCommand = false;
	bool bSelected = false;
	float AttackCooldownRemaining = 0.0f;

	UPROPERTY()
	ABattleUnit* AttackTarget = nullptr;
};

UCLASS()
class BATTLEMAP_TESTCURSOR_API ABattleInfantryUnit : public ABattleUnit
{
	GENERATED_BODY()
};

UCLASS()
class BATTLEMAP_TESTCURSOR_API ABattleVehicleUnit : public ABattleUnit
{
	GENERATED_BODY()
};

UCLASS()
class BATTLEMAP_TESTCURSOR_API ABattleAircraftUnit : public ABattleUnit
{
	GENERATED_BODY()
};

UCLASS()
class BATTLEMAP_TESTCURSOR_API ABattleNavalSurfaceUnit : public ABattleUnit
{
	GENERATED_BODY()
};

UCLASS()
class BATTLEMAP_TESTCURSOR_API ABattleSubmarineUnit : public ABattleUnit
{
	GENERATED_BODY()
};

UCLASS()
class BATTLEMAP_TESTCURSOR_API ABattleFacilityUnit : public ABattleUnit
{
	GENERATED_BODY()
};
