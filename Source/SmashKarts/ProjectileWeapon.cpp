#include "ProjectileWeapon.h"
#include "KartWeaponData.h"
#include "KartProjectile.h"
#include "KartCharacter.h"
#include "KartRules.h"

float AProjectileWeapon::GetLaunchSpeed(float ChargeAlpha) const
{
	// Charge throw: hold time (0..1, already clamped on the server) picks the throw speed
	return WeaponData->bChargeThrow
		? FMath::Lerp(WeaponData->ChargeSpeedMin, WeaponData->ChargeSpeedMax, FMath::Clamp(ChargeAlpha, 0.f, 1.f))
		: WeaponData->ProjectileSpeed;
}

void AProjectileWeapon::FireOnce(float ChargeAlpha)
{
	if (!WeaponData || !WeaponData->ProjectileClass)
	{
		return;
	}

	const int32 Count = FMath::Max(1, WeaponData->ProjectilesPerShot);
	const float BaseSpeed = GetLaunchSpeed(ChargeAlpha);
	const bool bFan = WeaponData->VolleySpreadDegrees > 0.f && Count > 1;
	const bool bLine = WeaponData->VolleyLineSpacing > 0.f && Count > 1;
	const float Gravity = FMath::Abs(GetWorld()->GetGravityZ()) * WeaponData->ProjectileGravityScale;

	for (int32 i = 0; i < Count; ++i)
	{
		// Line volley (Cannon Balls): all straight ahead, ball 0 farthest, each next one lands VolleyLineSpacing closer
		if (bLine)
		{
			const float SpeedScale = KartRules::LineVolleySpeedScale(BaseSpeed, WeaponData->LaunchPitchDegrees, Gravity, i, WeaponData->VolleyLineSpacing);
			SpawnProjectile(BaseSpeed * SpeedScale, 0.f, 0.f);
			continue;
		}

		// Centre the row on the kart: 3 projectiles → offsets -1, 0, +1 × SideSpacing (or yaw steps when fanned)
		const float Slot = i - (Count - 1) * 0.5f;
		const float SideOffset = bFan ? 0.f : Slot * WeaponData->SideSpacing;
		const float YawOffset = bFan ? Slot * (WeaponData->VolleySpreadDegrees / (Count - 1)) : 0.f;
		SpawnProjectile(BaseSpeed, SideOffset, YawOffset);
	}
}

void AProjectileWeapon::SpawnProjectile(float Speed, float SideOffset, float YawOffset)
{
	AKartCharacter* Kart = GetOwnerKart();
	if (!Kart)
	{
		return;
	}

	FRotator Rotation = Kart->GetActorRotation();
	Rotation.Yaw += YawOffset + FMath::FRandRange(-WeaponData->SpreadDegrees, WeaponData->SpreadDegrees);
	Rotation.Pitch = WeaponData->LaunchPitchDegrees;
	Rotation.Roll = 0.f;

	// SpawnOffset is local to the kart: X forward, Y right, Z up
	const FVector LocalOffset = WeaponData->SpawnOffset + FVector(0.f, SideOffset, 0.f);
	const FVector Location = Kart->GetActorTransform().TransformPosition(LocalOffset);
	const FTransform SpawnTransform(Rotation, Location);

	AKartProjectile* Projectile = GetWorld()->SpawnActorDeferred<AKartProjectile>(
		WeaponData->ProjectileClass, SpawnTransform, Kart, Kart,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (Projectile)
	{
		// Throws inherit the kart's speed so a lob from a moving kart still lands ahead of it
		const FVector InheritedVelocity = WeaponData->bChargeThrow ? Kart->GetVelocity() * FVector(1.f, 1.f, 0.f) : FVector::ZeroVector;
		Projectile->InitProjectile(WeaponData, Speed, InheritedVelocity);
		Projectile->FinishSpawning(SpawnTransform);
	}
}
