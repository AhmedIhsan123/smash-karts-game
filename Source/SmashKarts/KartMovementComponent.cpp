// Fill out your copyright notice in the Description page of Project Settings.


#include "KartMovementComponent.h"
#include "KartCharacter.h"
#include "KartStatusComponent.h"
#include "SmashKarts.h"
#include "GameFramework/Pawn.h"

// Kart tuning defaults live here; BP_Kart only holds asset references
UKartMovementComponent::UKartMovementComponent()
{
	MaxWalkSpeed = 900.f;
	MaxAcceleration = 400.f;
	BrakingDecelerationWalking = 100.f;
	BrakingFrictionFactor = 0.3f;
	GroundFriction = 1.5f;
	GravityScale = 0.5f;
	AirControl = 0.f;
	MaxStepHeight = 30.f;
	CoastAlignRate = 1.5f;
	bOrientRotationToMovement = false;
	SetWalkableFloorAngle(50.f);

	FrozenStopRate = 8.f;
	PoppedTiresGrip = 0.3f;
	PoppedTiresSpeedMultiplier = 0.6f;
	SpikySpeedMultiplier = 1.25f;

	MaxReverseSpeedFactor = 0.5f;
	WallBounceFactor = 0.3f;
	WallBounceMinSpeed = 250.f;
	WallBounceHeadOnDot = 0.6f;
}

const UKartStatusComponent* UKartMovementComponent::GetStatus() const
{
	const AKartCharacter* Kart = Cast<AKartCharacter>(CharacterOwner);
	return Kart ? Kart->GetStatusComponent() : nullptr;
}

bool UKartMovementComponent::HasStatusFlag(uint8 Bit) const
{
	const UKartStatusComponent* Status = GetStatus();
	return Status && (Status->GetFlags() & Bit) != 0;
}

float UKartMovementComponent::GetMaxSpeed() const
{
	float Speed = Super::GetMaxSpeed();
	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling)
	{
		if (HasStatusFlag(UKartStatusComponent::ToBit(EKartStatus::TiresPopped)))
		{
			Speed *= PoppedTiresSpeedMultiplier;
		}
		if (HasStatusFlag(UKartStatusComponent::ToBit(EKartStatus::Spiky)))
		{
			Speed *= SpikySpeedMultiplier;
		}
	}
	return Speed;
}

void UKartMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Frozen: bleed off all horizontal speed quickly (input is already zeroed by ConstrainInputAcceleration)
	if (HasStatusFlag(UKartStatusComponent::ToBit(EKartStatus::Frozen)))
	{
		const FVector Horizontal = FMath::VInterpTo(FVector(Velocity.X, Velocity.Y, 0.f), FVector::ZeroVector, DeltaTime, FrozenStopRate);
		Velocity.X = Horizontal.X;
		Velocity.Y = Horizontal.Y;
		return;
	}

	// Popped tires: very little grip on and off the throttle
	const bool bPopped = HasStatusFlag(UKartStatusComponent::ToBit(EKartStatus::TiresPopped));
	const float AlignRate = bPopped ? PoppedTiresGrip : CoastAlignRate;
	if (bPopped)
	{
		Friction = FMath::Min(Friction, PoppedTiresGrip);
	}

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

		// Calculate the interpolation factor based on DeltaTime and the coasting grip
		const float Alpha = FMath::Clamp(DeltaTime * AlignRate, 0.f, 1.f);
		const FVector CurrentHorizontal = FVector(Velocity.X, Velocity.Y, 0.f);

		// Interpolate between current velocity and target velocity
		const FVector NewVelocity = FMath::Lerp(CurrentHorizontal, TargetVelocity, Alpha).GetSafeNormal2D() * Speed;
		Velocity.X = NewVelocity.X;
		Velocity.Y = NewVelocity.Y;
	}

	// Call the parent class's CalcVelocity to handle normal movement
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);

	// Reversing is slower than driving forward
	if (IsMovingOnGround() && MaxReverseSpeedFactor < 1.f)
	{
		const FVector Forward = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
		const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
		const float ForwardSpeed = FVector::DotProduct(Horizontal, Forward);
		const float MaxReverse = GetMaxSpeed() * MaxReverseSpeedFactor;
		if (ForwardSpeed < -MaxReverse)
		{
			// Remove only the excess backwards component, keep any sideways slide
			const FVector Clamped = Horizontal + Forward * (-MaxReverse - ForwardSpeed);
			Velocity.X = Clamped.X;
			Velocity.Y = Clamped.Y;
		}
	}
}

FVector UKartMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	// Same rule on server and owning client: both read replicated state (bEliminated, match state, Frozen)
	if (const AKartCharacter* Kart = Cast<AKartCharacter>(CharacterOwner))
	{
		if (Kart->IsInputLocked())
		{
			return FVector::ZeroVector;
		}
	}
	return Super::ConstrainInputAcceleration(InputAcceleration);
}

void UKartMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);

	// Tiny bounce off walls on head-on hits. Kart-to-kart bumps keep the default (blocked, momentum stops).
	if (WallBounceFactor <= 0.f || !IsMovingOnGround() || Cast<APawn>(Hit.GetActor()))
	{
		return;
	}
	const FVector Normal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f);
	if (FMath::Abs(Hit.ImpactNormal.Z) > 0.5f || Normal.IsNearlyZero())
	{
		return; // floor / ramp lip, not a wall
	}
	const FVector WallNormal = Normal.GetSafeNormal();
	const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
	const float IntoWall = -FVector::DotProduct(Horizontal, WallNormal);
	const float HeadOn = Horizontal.IsNearlyZero() ? 0.f : -FVector::DotProduct(Horizontal.GetSafeNormal(), WallNormal);
	if (IntoWall >= WallBounceMinSpeed && HeadOn >= WallBounceHeadOnDot)
	{
		bPendingWallBounce = true;
		PendingWallBounceVelocity = WallNormal * IntoWall * WallBounceFactor;
	}
}

void UKartMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	bPendingWallBounce = false;
	Super::PhysWalking(deltaTime, Iterations);

	// PhysWalking rebuilds Velocity from the actual move, so the bounce is applied afterwards
	if (bPendingWallBounce)
	{
		bPendingWallBounce = false;
		Velocity.X = PendingWallBounceVelocity.X;
		Velocity.Y = PendingWallBounceVelocity.Y;
	}
}

void UKartMovementComponent::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
	FVector NewLocation, FVector NewVelocity, FMovementBaseInterfaceData* NewMovementBaseInterfaceData, FName NewBaseBoneName,
	bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	++CorrectionCount;
	UE_LOG(LogSmashKarts, Log, TEXT("[Net] Correction #%d: %.0f cm off, server vel %.0f"), CorrectionCount,
		UpdatedComponent ? FVector::Dist(UpdatedComponent->GetComponentLocation(), NewLocation) : -1.f, NewVelocity.Size());

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewMovementBaseInterfaceData, NewBaseBoneName,
		bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);
}
