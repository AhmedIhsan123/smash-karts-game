#pragma once

#include "CoreMinimal.h"
#include "KartWeapon.h"
#include "ProjectileWeapon.generated.h"

/** Archetype A: Rockets, Bullets, Seeker, Cannon Balls, Lob-Grenuke, Snowball, Nuke. */
UCLASS()
class SMASHKARTS_API AProjectileWeapon : public AKartWeapon
{
	GENERATED_BODY()

protected:
	// One shot = ProjectilesPerShot projectiles side by side
	virtual void FireOnce(float ChargeAlpha) override;

	// The one place that spawns a projectile (keeps the door open for client prediction later)
	void SpawnProjectile(float ChargeAlpha, float SideOffset, float YawOffset);
};
