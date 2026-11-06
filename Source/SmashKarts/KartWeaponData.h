#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KartWeaponData.generated.h"

class AKartWeapon;
class AKartProjectile;
class AKartTrap;
class UStaticMesh;
class UTexture2D;
class UDamageType;

/**
 * One asset per weapon (DA_Rockets, DA_Mines, ...).
 * Only the fields for the weapon's archetype matter; the rest are ignored.
 */
UCLASS(BlueprintType)
class SMASHKARTS_API UKartWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---------------- Common ----------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UTexture2D> Icon;

	// Relative chance to roll this weapon from a loot box
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float DropWeight = 10.f;

	// Which behavior class to spawn: ProjectileWeapon, TrapWeapon or TurretWeapon
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AKartWeapon> WeaponClass;

	// Mesh shown on the kart while held (optional)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMesh> HeldMesh;

	// Held mesh position relative to the kart's Mesh component
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FVector HeldMeshOffset = FVector(0.f, 0.f, 80.f);

	// Shots / drops before the weapon is used up (Machine Gun: 1)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1"))
	int32 Uses = 3;

	// Minimum seconds between two presses of Fire
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float FireInterval = 0.4f;

	// Shots fired by ONE press, one after another (Mines: 3, Bullets: 3). 1 = single shot
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1"))
	int32 BurstCount = 1;

	// Seconds between the shots of one burst (0 = all at once)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float BurstInterval = 0.4f;

	// Optional data-only damage type (kill feed later). Empty = generic damage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<UDamageType> DamageType;

	// ---------------- Projectile ----------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<AKartProjectile> ProjectileClass;

	// Spawn point relative to the kart (X forward, Z up)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FVector SpawnOffset = FVector(120.f, 0.f, 10.f);

	// Projectiles fired side by side in each shot (Rockets: 3)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1"))
	int32 ProjectilesPerShot = 1;

	// Sideways gap between side-by-side projectiles (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float SideSpacing = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float ProjectileSpeed = 3000.f;

	// Random yaw spread, +/- degrees
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float SpreadDegrees = 2.f;

	// 0 = flies straight, >0 = lobbed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float ProjectileGravityScale = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float ProjectileLifetime = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float DirectDamage = 50.f;

	// 0 = no splash
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float SplashRadius = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float SplashMaxDamage = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float SplashMinDamage = 5.f;

	// ---------------- Trap ----------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	TSubclassOf<AKartTrap> TrapClass;

	// How far behind the kart the trap is dropped
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	float DropDistance = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	float TrapDamage = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	float TriggerRadius = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	float ArmingDelay = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	float TrapLifetime = 30.f;

	// ---------------- Turret ----------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret")
	float TurretDuration = 6.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret")
	float LockRange = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret")
	float ShotsPerSecond = 26.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret")
	float DamageNear = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret")
	float DamageFar = 0.3f;

	// DamageType, or the generic UDamageType if none is set
	TSubclassOf<UDamageType> GetDamageTypeOrDefault() const;
};
