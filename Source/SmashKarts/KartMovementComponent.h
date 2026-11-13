// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KartMovementComponent.generated.h"

class AKartCharacter;
class UKartStatusComponent;

/**
 * Kart movement on top of CMC (prediction + correction for free).
 *
 * Authority: server and owning client both run this simulation for the owning client's moves.
 * Replicates: nothing new; it only reads replicated state (match state, status flags), so client and server
 * compute the same move. A status flag flip reaches the client a little later than the server: that costs
 * at most one small correction, which is accepted.
 * Client prediction: everything here (freeze stop, popped-tire grip, speed multipliers, reverse cap, wall bounce).
 */
UCLASS()
class SMASHKARTS_API UKartMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UKartMovementComponent();

	// How fast velocity swings toward the kart's facing while coasting (higher = snappier)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kart|Coasting", meta = (ClampMin = "0"))
	float CoastAlignRate;

	// --- Status effects ---
	/** Frozen: how fast the kart's velocity drops to zero (higher = stops sooner). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kart|Status", meta = (ClampMin = "0"))
	float FrozenStopRate;

	/** Popped tires: grip used for both throttle (GroundFriction) and coasting (CoastAlignRate). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kart|Status", meta = (ClampMin = "0"))
	float PoppedTiresGrip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kart|Status", meta = (ClampMin = "0"))
	float PoppedTiresSpeedMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kart|Status", meta = (ClampMin = "0"))
	float SpikySpeedMultiplier;

	// --- Handling ---
	/** Max reverse speed as a fraction of max forward speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kart|Handling", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxReverseSpeedFactor;

	/** Head-on wall hit: fraction of the impact speed bounced back. 0 = no bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kart|Handling", meta = (ClampMin = "0", ClampMax = "1"))
	float WallBounceFactor;

	/** Speed into the wall (cm/s) needed for a bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kart|Handling", meta = (ClampMin = "0"))
	float WallBounceMinSpeed;

	/** How head-on the hit must be: cosine of the max angle between velocity and the wall normal (1 = dead on). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kart|Handling", meta = (ClampMin = "0", ClampMax = "1"))
	float WallBounceHeadOnDot;

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual float GetMaxSpeed() const override;

	/** No throttle while the kart is input-locked (countdown, results, smashed, frozen). Runs on both sides. */
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;

	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

	/** Corrections received by this client (development stat, logged). */
	int32 GetCorrectionCount() const { return CorrectionCount; }

protected:
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;

	virtual void OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation,
		FVector NewVelocity, FMovementBaseInterfaceData* NewMovementBaseInterfaceData, FName NewBaseBoneName, bool bHasBase,
		bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;

private:
	const UKartStatusComponent* GetStatus() const;
	bool HasStatusFlag(uint8 Bit) const;

	bool bPendingWallBounce = false;
	FVector PendingWallBounceVelocity = FVector::ZeroVector;
	int32 CorrectionCount = 0;
};
