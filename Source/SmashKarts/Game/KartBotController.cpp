#include "Game/KartBotController.h"
#include "Game/KartGameState.h"
#include "KartCharacter.h"
#include "KartWeapon.h"
#include "KartWeaponComponent.h"
#include "KartWeaponData.h"
#include "LootBox.h"
#include "TrapWeapon.h"
#include "TurretWeapon.h"
#include "BuffWeapon.h"
#include "SmashKarts.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AKartBotController::AKartBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true; // name, colour and score like a player
	// We steer by setting the control rotation ourselves; by default AAIController overwrites it with the pawn's
	// rotation every tick, which would undo all steering. The kart then follows it (bUseControllerRotationYaw)
	bSetControlRotationFromPawnOrientation = false;
}

void AKartBotController::ResetState()
{
	StuckSince = -1.0;
	ReverseUntil = 0.0;
	AimedSince = -1.0;
}

void AKartBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AKartCharacter* Kart = Cast<AKartCharacter>(GetPawn());
	const AKartGameState* GS = GetWorld()->GetGameState<AKartGameState>();
	if (!Kart || Kart->IsEliminated() || Kart->IsInputLocked() || !GS || !GS->IsPlaying())
	{
		ResetState();
		HoldingSince = -1.0;
		return; // lobby, countdown, results, frozen or smashed: hands off
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= RetargetTime || !CurrentTarget.IsValid())
	{
		CurrentTarget = PickTarget(Kart, bTargetIsEnemy);
		RetargetTime = Now + 0.5;
	}
	const AActor* Target = CurrentTarget.Get();

	FRotator Control = GetControlRotation();
	const float Heading = static_cast<float>(Control.Yaw);
	float TargetYaw = Heading;
	if (Target)
	{
		TargetYaw = static_cast<float>((Target->GetActorLocation() - Kart->GetActorLocation()).GetSafeNormal2D().Rotation().Yaw);
	}
	float Ahead = WallLookAhead;
	const float DriveYaw = ChooseHeading(Kart, TargetYaw, Ahead);
	float YawDelta = FMath::FindDeltaAngleDegrees(Heading, DriveYaw);

	float Throttle = 1.f;
	const float Speed = static_cast<float>(Kart->GetVelocity().Size2D());
	if (Ahead < 220.f && FMath::Abs(YawDelta) > 60.f)
	{
		Throttle = -0.6f; // boxed in and the way out is to the side: back up while turning (3-point turn)
	}
	else if (FMath::Abs(YawDelta) > 100.f && Speed > 500.f)
	{
		Throttle = 0.5f; // hairpin at speed
	}

	// Stuck against something anyway: back up while turning, then try again
	if (Now < ReverseUntil)
	{
		Throttle = -1.f;
		YawDelta = ReverseSteer * 90.f;
	}
	else if (Speed < 60.f)
	{
		if (StuckSince < 0.0)
		{
			StuckSince = Now;
		}
		else if (Now - StuckSince > 1.0)
		{
			ReverseUntil = Now + 0.8;
			ReverseSteer = FMath::RandBool() ? 1.f : -1.f;
			StuckSince = -1.0;
			// Stuck twice on the way to the same thing: forget it for a while and go somewhere else
			if (Target && Target == StuckTarget.Get() && Now - LastStuckTime < 6.0)
			{
				IgnoredTarget = StuckTarget;
				IgnoreUntil = Now + 4.0;
				RetargetTime = 0.0;
			}
			StuckTarget = Target;
			LastStuckTime = Now;
			UE_LOG(LogSmashKarts, Verbose, TEXT("[Bot] %s stuck at %s heading %.0f, target %s, backing up"), PlayerState ? *PlayerState->GetPlayerName() : *GetName(),
				*Kart->GetActorLocation().ToCompactString(), Heading, *GetNameSafe(Target));
		}
	}
	else
	{
		StuckSince = -1.0;
	}

	// Steer like a kart: no turning on the spot
	const float SpeedFactor = FMath::Clamp(Speed / 250.f, 0.4f, 1.f);
	const float MaxTurn = TurnRate * SpeedFactor * DeltaSeconds;
	Control.Yaw = FRotator::NormalizeAxis(Control.Yaw + FMath::Clamp(YawDelta, -MaxTurn, MaxTurn));
	Control.Pitch = 0.f;
	Control.Roll = 0.f;
	SetControlRotation(Control);
	Kart->AddMovementInput(Kart->GetActorForwardVector(), Throttle);

	ThinkWeapon(Kart, Target, bTargetIsEnemy);
}

AActor* AKartBotController::PickTarget(const AKartCharacter* Kart, bool& bOutIsEnemy) const
{
	const FVector Here = Kart->GetActorLocation();
	const AActor* Ignored = GetWorld()->GetTimeSeconds() < IgnoreUntil ? IgnoredTarget.Get() : nullptr;
	const bool bArmed = Kart->GetWeaponComponent() && Kart->GetWeaponComponent()->HasWeapon();
	AActor* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();

	if (!bArmed)
	{
		for (TActorIterator<ALootBox> It(GetWorld()); It; ++It)
		{
			if (!It->IsActive() || It->GetActorLocation().Z - Here.Z > MaxClimb || *It == Ignored)
			{
				continue;
			}
			const double DistSq = FVector::DistSquared2D(Here, It->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = *It;
			}
		}
		if (Best)
		{
			bOutIsEnemy = false;
			return Best;
		}
	}

	for (TActorIterator<AKartCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == Kart || It->IsEliminated() || It->HasStatus(EKartStatus::SpawnProtected) || *It == Ignored)
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(Here, It->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}
	bOutIsEnemy = Best != nullptr;
	return Best;
}

float AKartBotController::ChooseHeading(const AKartCharacter* Kart, float TargetYaw, float& OutAhead) const
{
	// Look in 9 directions around the current heading (-120..+120 degrees) with a kart-sized sphere (so wall ends
	// and corners count). Nearly blocked directions are out; among the rest, open space and pointing at the
	// target both score, so the bot slides along walls toward the gaps. The sweep runs a little below the kart's
	// centre so 50 cm curbs count as walls.
	const FVector Start = Kart->GetActorLocation() - FVector(0.f, 0.f, 10.f);
	const float Heading = static_cast<float>(GetControlRotation().Yaw);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BotLook), false, Kart);
	const FCollisionShape Body = FCollisionShape::MakeSphere(40.f);

	float BestScore = -1.0e9f;
	float BestYaw = TargetYaw;
	OutAhead = WallLookAhead;
	for (int32 Step = -4; Step <= 4; ++Step)
	{
		const float Yaw = Heading + Step * 30.f;
		FHitResult Hit;
		const FVector End = Start + FRotator(0.f, Yaw, 0.f).Vector() * WallLookAhead;
		const float Free = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, Body, Params)
			? static_cast<float>(Hit.Distance) : WallLookAhead;
		const float Toward = FMath::Cos(FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Yaw, TargetYaw)));
		float Score = (Free / WallLookAhead) * 1.2f + Toward * 0.8f - FMath::Abs(Step) * 0.03f;
		if (Free < 220.f)
		{
			Score -= 2.f; // about to hit something: never pick this unless everything is blocked
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			BestYaw = Yaw;
		}
		if (Step == 0)
		{
			OutAhead = Free;
		}
	}
	return BestYaw;
}

bool AKartBotController::CanSee(const AKartCharacter* Kart, const AActor* Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BotSight), false, Kart);
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Kart->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
}

void AKartBotController::ThinkWeapon(AKartCharacter* Kart, const AActor* Target, bool bIsEnemy)
{
	UKartWeaponComponent* WeaponComp = Kart->GetWeaponComponent();
	const AKartWeapon* Weapon = WeaponComp ? WeaponComp->GetCurrentWeapon() : nullptr;
	const UKartWeaponData* Data = Weapon ? Weapon->GetWeaponData() : nullptr;
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Data || !Data->WeaponClass)
	{
		HoldingSince = -1.0;
		AimedSince = -1.0;
		return;
	}
	if (HoldingSince < 0.0)
	{
		HoldingSince = Now;
	}
	if (Weapon->GetActiveTimeRemaining() > 0.f)
	{
		return; // machine gun / buff already running
	}

	bool bWantFire = false;
	float Charge = 0.f;
	if (bIsEnemy && Target)
	{
		const FVector To = Target->GetActorLocation() - Kart->GetActorLocation();
		const float Distance = static_cast<float>(To.Size2D());
		const float Angle = FMath::Abs(FMath::FindDeltaAngleDegrees(static_cast<float>(Kart->GetActorRotation().Yaw), static_cast<float>(To.Rotation().Yaw)));
		const UClass* Archetype = Data->WeaponClass;

		if (Archetype->IsChildOf(ATrapWeapon::StaticClass()))
		{
			bWantFire = Distance < TrapRange && Angle > 120.f; // being chased: drop it in their path
		}
		else if (Archetype->IsChildOf(ATurretWeapon::StaticClass()))
		{
			bWantFire = Distance < Data->LockRange && CanSee(Kart, Target);
		}
		else if (Archetype->IsChildOf(ABuffWeapon::StaticClass()))
		{
			const float Range = Data->AuraInterval > 0.f ? Data->AuraRadius : CloseRange;
			bWantFire = Distance < Range && CanSee(Kart, Target);
		}
		else
		{
			const float Cone = Data->bHoming ? 45.f : FireConeDegrees;
			bWantFire = Angle < Cone && Distance < FireRange && CanSee(Kart, Target);
			if (Data->bChargeThrow && Data->ChargeSpeedMax > Data->ChargeSpeedMin)
			{
				// Throw speed for this distance on flat ground: v = sqrt(d * g / sin(2 * pitch))
				const float Gravity = FMath::Abs(GetWorld()->GetGravityZ()) * FMath::Max(0.1f, Data->ProjectileGravityScale);
				const float Sin2 = FMath::Max(0.1f, FMath::Sin(FMath::DegreesToRadians(2.f * Data->LaunchPitchDegrees)));
				const float Speed = FMath::Sqrt(Distance * Gravity / Sin2);
				Charge = FMath::Clamp((Speed - Data->ChargeSpeedMin) / (Data->ChargeSpeedMax - Data->ChargeSpeedMin), 0.f, 1.f);
			}
		}
	}
	if (!bWantFire && Now - HoldingSince > MaxHoldTime)
	{
		bWantFire = true; // don't sit on a weapon forever
	}

	if (!bWantFire)
	{
		AimedSince = -1.0;
		return;
	}
	if (AimedSince < 0.0)
	{
		AimedSince = Now;
	}
	if (Now - AimedSince >= ReactionTime)
	{
		UE_LOG(LogSmashKarts, Verbose, TEXT("[Bot] %s fires %s (charge %.2f, held %.1f s)"), PlayerState ? *PlayerState->GetPlayerName() : *GetName(),
			*GetNameSafe(Data), Charge, Now - HoldingSince);
		WeaponComp->FireFromServer(Charge);
		AimedSince = -1.0;
	}
}
