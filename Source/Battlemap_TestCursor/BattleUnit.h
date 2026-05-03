#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BattleTypes.h"
#include "BattleUnit.generated.h"

class UBattleCommandComponent;
class UBattleDetectionComponent;
class UBattleCommsComponent;
class UBattleSupplyComponent;
class UBattleEnemyTacticalBrainComponent;
class UBattleECMZoneComponent;
class USceneComponent;
class UStaticMeshComponent;
class AMoveCommandMarkerActor;
class ATacticalMapGrid;

UCLASS(Blueprintable)
class BATTLEMAP_TESTCURSOR_API ABattleUnit : public APawn
{
	GENERATED_BODY()

public:
	ABattleUnit(const FObjectInitializer& ObjectInitializer);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Movement")
	EUnitMobilityType MobilityType = EUnitMobilityType::Land;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Combat")
	float AttackRange = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Combat")
	float AttackDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Combat")
	float AttackCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Detection")
	float DetectionRange = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Combat")
	int32 MaxAmmo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Combat")
	int32 CurrentAmmo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Combat")
	float ReloadDuration = 2.5f;

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void ApplyDamageValue(float DamageValue);

	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool IsAlive() const;

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool IsSelected() const { return bSelected; }

	UFUNCTION(BlueprintCallable, Category = "Battle|Command")
	bool IssueMoveCommandInterrupt(const FVector& TargetLocation, ECommandPriority Priority = ECommandPriority::High);

	UFUNCTION(BlueprintCallable, Category = "Battle|Command")
	void IssueAttackCommandInterrupt(ABattleUnit* TargetUnit, ECommandPriority Priority = ECommandPriority::High);

	UFUNCTION(BlueprintPure, Category = "Battle|Visual")
	float GetHoverCircleRadius() const;

	UFUNCTION(BlueprintPure, Category = "Battle|Debug")
	EUnitRuntimeState GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintPure, Category = "Battle|Debug")
	float GetAttackCooldownRemaining() const { return AttackCooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Battle|Debug")
	float GetReloadRemaining() const { return ReloadRemaining; }

	UFUNCTION(BlueprintPure, Category = "Battle|Debug")
	const FString& GetLastCombatEvent() const { return LastCombatEvent; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Components")
	UBattleCommandComponent* CommandComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Components")
	UBattleDetectionComponent* DetectionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Components")
	UBattleCommsComponent* CommsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Components")
	UBattleSupplyComponent* SupplyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|ECM")
	UBattleECMZoneComponent* EcmZone = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Visual")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Visual")
	UStaticMeshComponent* UnitMesh;

	UPROPERTY()
	AMoveCommandMarkerActor* MoveCommandMarker;

protected:
	void ProcessActiveCommand(float DeltaSeconds);
	void ProcessAttackCommand(float DeltaSeconds);
	void EnforceMinimumUnitSpacing();
	void TryAutoEngage(float DeltaSeconds);
	ATacticalMapGrid* ResolveTacticalMapGrid() const;
	bool TryGetTraversalRuleAt(const FVector& WorldLocation, FTerrainTraversalRule& OutRule) const;
	bool IsWaterTerrainType(ETerrainType TerrainType) const;
	bool IsInfrastructureTerrainType(ETerrainType TerrainType) const;
	bool CanLandTraverseWaterCell(const FVector& WorldLocation) const;
	bool IsTraversableAt(const FVector& WorldLocation) const;
	float GetSpeedMultiplierAt(const FVector& WorldLocation) const;

	/** If DesiredDeltaXY is blocked by a cell edge, try axis-only or partial length along the same heading (wall slide). */
	bool TryResolveSlidingMoveStep(const FVector& FromWorld, const FVector& DesiredDeltaXY, FVector& OutAcceptedDeltaXY) const;

	void SpawnOrReplaceMoveMarker(const FVector& TargetLocation);

	void DestroyMoveMarker();

	void StartReload();

	void UpdateMeshScaleVisual();

	bool AcquireNextCommand();

	/** Computes grid path into MovePathWorldWaypoints; no grid => straight to goal. */
	bool TryPopulateMovePathFromGoal(const FVector& GoalWorld);

	void ClearMovePath();

	UPROPERTY()
	TArray<FVector> MovePathWorldWaypoints;

	int32 MovePathPointIndex = 0;

	UPROPERTY()
	FActiveCommand ActiveCommand;

	bool bHasActiveCommand = false;
	bool bSelected = false;
	float AttackCooldownRemaining = 0.0f;
	float HoverCircleScale = 1.35f;
	float MinHoverCircleRadius = 120.0f;
	float AutoEngageScanCooldown = 0.0f;
	float AutoEngageScanInterval = 0.5f;
	float ReloadRemaining = 0.0f;
	float HitFlashRemaining = 0.0f;
	EUnitRuntimeState RuntimeState = EUnitRuntimeState::Idle;
	FString LastCombatEvent;

	UPROPERTY()
	ABattleUnit* AttackTarget = nullptr;

	mutable TWeakObjectPtr<ATacticalMapGrid> CachedMapGrid;
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

public:
	ABattleVehicleUnit(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|AI")
	UBattleEnemyTacticalBrainComponent* TacticalBrain = nullptr;
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
