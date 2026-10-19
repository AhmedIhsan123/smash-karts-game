// Fill out your copyright notice in the Description page of Project Settings.


#include "KartMovementComponent.h"

void UKartMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Custom behavior: if the player is not pressing any input, we want to align the velocity with the kart's facing direction.
	if (IsMovingOnGround() && Acceleration.IsNearlyZero() && !Velocity.IsNearlyZero())
	{
		// Get the forward direction of the kart (the direction it's facing) and flatten it to 2D (ignore vertical component) and normalize it.
		const FVector Forward = UpdatedComponent->GetForwardVector().GetSafeNormal2D();

		// Get the current speed in 2D (ignore vertical component)
		const float Speed = Velocity.Size2D();

		// Determine if the velocity is in the same direction as the forward vector
		const float Sign = FVector::DotProduct(Velocity.GetSafeNormal2D(), Forward) >= 0.f ? 1.f : -1.f;
		
		// Calculate the target velocity aligned with the forward direction
		const FVector TargetVelocity = Forward * Speed * Sign;

		// Calculate the interpolation factor based on DeltaTime and CoastAlignRate
		const float Alpha = FMath::Clamp(DeltaTime * CoastAlignRate, 0.f, 1.f);
		const FVector CurrentHorizontal = FVector(Velocity.X, Velocity.Y, 0.f);

		// Interpolate between current velocity and target velocity
		const FVector NewVelocity = FMath::Lerp(CurrentHorizontal, TargetVelocity, Alpha).GetSafeNormal2D() * Speed; 
		Velocity.X = NewVelocity.X;
		Velocity.Y = NewVelocity.Y;
	}
	
	// Call the parent class's CalcVelocity to handle normal movement
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}