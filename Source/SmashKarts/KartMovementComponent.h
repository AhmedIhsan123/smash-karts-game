// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KartMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class SMASHKARTS_API UKartMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// How fast velocity swings toward the kart's facing while coasting (higher = snappier)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kart|Coasting", meta = (ClampMin = "0"))
	float CoastAlignRate = 5.f;

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
};
