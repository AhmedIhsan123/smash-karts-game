// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KartMovementComponent.generated.h"

/**
 * Kart movement on top of CMC (prediction + correction for free).
 * Anything that changes velocity runs inside this simulation, on both the server and the owning client,
 * and only reads replicated state (match state, status flags) so client and server compute the same move.
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

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	/** No throttle while the kart is input-locked (countdown, results, smashed). Runs on both sides. */
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;
};
