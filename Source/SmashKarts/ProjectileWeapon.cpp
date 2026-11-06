#include "ProjectileWeapon.h"
#include "KartWeaponData.h"
#include "KartProjectile.h"
#include "KartCharacter.h"

void AProjectileWeapon::FireOnce(float ChargeAlpha)
{
	if (!WeaponData || !WeaponData->ProjectileClass)
	{
		return;
	}

	// Center the row on the kart: 3 projectiles → offsets -1, 0, +1 × SideSpacing
	const int32 Count = FMath::Max(1, WeaponData->ProjectilesPerShot);
	for (int32 i = 0; i < Count; ++i)
	{
		const float SideOffset = (i - (Count - 1) * 0.5f) * WeaponData->SideSpacing;
		SpawnProjectile(ChargeAlpha, SideOffset);
	}
}

void AProjectileWeapon::SpawnProjectile(float ChargeAlpha, float SideOffset)
{
	AKartCharacter* Kart = GetOwnerKart();
	if (!Kart)
	{
		return;
	}

	FRotator Rotation = Kart->GetActorRotation();
	Rotation.Yaw += FMath::FRandRange(-WeaponData->SpreadDegrees, WeaponData->SpreadDegrees);

	// SpawnOffset is local to the kart: X forward, Y right, Z up
	const FVector LocalOffset = WeaponData->SpawnOffset + FVector(0.f, SideOffset, 0.f);
	const FVector Location = Kart->GetActorTransform().TransformPosition(LocalOffset);
	const FTransform SpawnTransform(Rotation, Location);

	// Tier 2 (charge throw) will turn ChargeAlpha into a speed here
	const float Speed = WeaponData->ProjectileSpeed;

	AKartProjectile* Projectile = GetWorld()->SpawnActorDeferred<AKartProjectile>(
		WeaponData->ProjectileClass, SpawnTransform, Kart, Kart,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (Projectile)
	{
		Projectile->InitProjectile(WeaponData, Speed);
		Projectile->FinishSpawning(SpawnTransform);
	}
}
