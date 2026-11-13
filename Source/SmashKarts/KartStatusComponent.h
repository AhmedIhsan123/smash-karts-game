#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KartStatusComponent.generated.h"

/** Status effects a kart can carry. Order matters: it is the bit index in the replicated mask. */
UENUM(BlueprintType)
enum class EKartStatus : uint8
{
	Frozen,
	TiresPopped,
	Spiky,
	Invincible,
	Tesla,
	SpawnProtected,
	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EKartStatus, EKartStatus::Count);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKartStatusChanged, uint8 /*OldFlags*/, uint8 /*NewFlags*/);

/**
 * Timed status flags on a kart (freeze, popped tires, buffs, spawn protection).
 *
 * Authority: only the server applies, clears and expires statuses (TickComponent runs on the server only).
 * Replicates: a bitmask (RepNotify) + one end time per status in server world time (<0 = until cleared).
 * Client prediction: none here; UKartMovementComponent reads the replicated flags on both sides,
 * so movement effects stay prediction-safe apart from a small correction when a flag flips.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SMASHKARTS_API UKartStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKartStatusComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Server: turn a status on. Duration <= 0 keeps it until ClearStatus/ClearAll (e.g. popped tires until respawn). */
	void ApplyStatus(EKartStatus Status, float Duration);

	/** Server */
	void ClearStatus(EKartStatus Status);

	/** Server: clears everything (elimination). */
	void ClearAll();

	UFUNCTION(BlueprintPure, Category = "Status")
	bool HasStatus(EKartStatus Status) const;

	/** Seconds left; -1 = until cleared, 0 = not active. Valid on every machine. */
	UFUNCTION(BlueprintPure, Category = "Status")
	float GetTimeRemaining(EKartStatus Status) const;

	uint8 GetFlags() const { return StatusFlags; }

	/** Fires on every machine when the mask changes (HUD, VFX). */
	FOnKartStatusChanged OnStatusChanged;

	static uint8 ToBit(EKartStatus Status) { return static_cast<uint8>(1u << static_cast<uint8>(Status)); }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_StatusFlags)
	uint8 StatusFlags = 0;

	/** Server world time each status ends at. Index = EKartStatus. <0 = until cleared. */
	UPROPERTY(Replicated)
	TArray<double> EndServerTimes;

	UFUNCTION()
	void OnRep_StatusFlags(uint8 OldFlags);

private:
	void SetFlags(uint8 NewFlags);
};
