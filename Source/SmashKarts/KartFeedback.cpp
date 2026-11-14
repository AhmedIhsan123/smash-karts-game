#include "KartFeedback.h"
#include "Game/KartGameInstance.h"
#include "UI/KartHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

namespace
{
	void SetupPattern(UCameraShakeBase& Shake, float Duration, float BlendIn, float BlendOut,
		const FVector& LocationAmplitude, float LocationFrequency, const FVector& RotationAmplitude /* pitch, yaw, roll */, float RotationFrequency)
	{
		Shake.bSingleInstance = true; // a new hit restarts the shake instead of stacking copies
		UPerlinNoiseCameraShakePattern* Pattern = Cast<UPerlinNoiseCameraShakePattern>(Shake.GetRootShakePattern());
		if (!Pattern)
		{
			return;
		}
		Pattern->Duration = Duration;
		Pattern->BlendInTime = BlendIn;
		Pattern->BlendOutTime = BlendOut;
		Pattern->LocationAmplitudeMultiplier = 1.f;
		Pattern->LocationFrequencyMultiplier = 1.f;
		Pattern->X.Amplitude = LocationAmplitude.X;
		Pattern->Y.Amplitude = LocationAmplitude.Y;
		Pattern->Z.Amplitude = LocationAmplitude.Z;
		Pattern->X.Frequency = Pattern->Y.Frequency = Pattern->Z.Frequency = LocationFrequency;
		Pattern->RotationAmplitudeMultiplier = 1.f;
		Pattern->RotationFrequencyMultiplier = 1.f;
		Pattern->Pitch.Amplitude = RotationAmplitude.X;
		Pattern->Yaw.Amplitude = RotationAmplitude.Y;
		Pattern->Roll.Amplitude = RotationAmplitude.Z;
		Pattern->Pitch.Frequency = Pattern->Yaw.Frequency = Pattern->Roll.Frequency = RotationFrequency;
		Pattern->FOV.Amplitude = 0.f;
	}

	bool ShakeEnabled(const APlayerController* PC)
	{
		const UWorld* World = PC ? PC->GetWorld() : nullptr;
		const UKartGameInstance* GI = World ? World->GetGameInstance<UKartGameInstance>() : nullptr;
		return !GI || GI->IsScreenShakeEnabled();
	}

	void StartShake(APlayerController* PC, TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
	{
		if (PC && PC->IsLocalController() && PC->PlayerCameraManager && Scale > 0.02f && ShakeEnabled(PC))
		{
			PC->PlayerCameraManager->StartCameraShake(ShakeClass, Scale);
		}
	}

	APlayerController* LocalControllerOf(APawn* Kart)
	{
		APlayerController* PC = Kart ? Cast<APlayerController>(Kart->GetController()) : nullptr;
		return (PC && PC->IsLocalController()) ? PC : nullptr;
	}
}

UKartHitShake::UKartHitShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetupPattern(*this, 0.35f, 0.02f, 0.2f, FVector(4.f, 4.f, 3.f), 30.f, FVector(1.0f, 1.0f, 1.5f), 25.f);
}

UKartExplosionShake::UKartExplosionShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetupPattern(*this, 0.6f, 0.04f, 0.35f, FVector(8.f, 8.f, 6.f), 20.f, FVector(1.6f, 1.6f, 2.f), 18.f);
}

UKartFireShake::UKartFireShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetupPattern(*this, 0.12f, 0.01f, 0.08f, FVector(2.5f, 0.5f, 0.5f), 40.f, FVector(0.5f, 0.2f, 0.2f), 35.f);
}

float KartFeedback::ExplosionShakeScale(float Strength, float Distance)
{
	if (Strength <= 0.f)
	{
		return 0.f;
	}
	// Reach grows with the blast (bullet ~8 m, rocket ~15 m, Nuke ~45 m); strength is capped so a Nuke doesn't blur the screen
	const float Reach = 2000.f * FMath::Sqrt(Strength);
	const float Falloff = 1.f - FMath::Max(0.f, Distance) / Reach;
	return Falloff <= 0.f ? 0.f : FMath::Min(Strength, 2.f) * Falloff * Falloff;
}

void KartFeedback::KartDamaged(APawn* Kart, float Amount)
{
	APlayerController* PC = LocalControllerOf(Kart);
	if (!PC || Amount <= 0.f)
	{
		return;
	}
	StartShake(PC, UKartHitShake::StaticClass(), FMath::Clamp(Amount / 40.f, 0.35f, 1.6f));
	if (AKartHUD* HUD = PC->GetHUD<AKartHUD>())
	{
		HUD->FlashDamage(Amount);
	}
}

void KartFeedback::Explosion(const UObject* WorldContext, const FVector& Location, float Strength)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController())
		{
			continue;
		}
		// Distance from your kart (the camera trails behind it); spectating / dead: from the camera
		const APawn* Pawn = PC->GetPawn();
		const FVector From = Pawn ? Pawn->GetActorLocation() : (PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Location);
		StartShake(PC, UKartExplosionShake::StaticClass(), ExplosionShakeScale(Strength, FVector::Dist(From, Location)));
	}
}

void KartFeedback::Fired(APawn* Kart)
{
	StartShake(LocalControllerOf(Kart), UKartFireShake::StaticClass(), 1.f);
}
