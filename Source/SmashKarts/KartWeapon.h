#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KartWeapon.generated.h"

class UKartWeaponData;
class AKartCharacter;
class UStaticMeshComponent;

/**
 * Base class for a held weapon. Spawned by UKartWeaponComponent on pickup (server),
 * attached to the kart, destroyed when used up. One subclass per archetype.
 *
 * Standard flow (Projectile, Trap): press Fire → BurstCount shots, BurstInterval apart,
 * each shot calls FireOnce() → then one use is consumed.
 * Turret overrides Fire() completely.
 */
UCLASS(Abstract)
class SMASHKARTS_API AKartWeapon : public AActor
{
	GENERATED_BODY()

public:
	AKartWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Server: called right after spawning
	virtual void InitWeapon(UKartWeaponData* InData);

	// Server: the owner pressed Fire. ChargeAlpha is 0..1 (for lobbers later)
	virtual void Fire(float ChargeAlpha);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UKartWeaponData* GetWeaponData() const { return WeaponData; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetUsesLeft() const { return UsesLeft; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponData)
	TObjectPtr<UKartWeaponData> WeaponData;

	UPROPERTY(Replicated)
	int32 UsesLeft = 0;

	UFUNCTION()
	void OnRep_WeaponData();

	void ApplyWeaponVisuals();

	// Server: one shot of a burst. Archetypes override this (spawn projectiles, drop a trap...)
	virtual void FireOnce(float ChargeAlpha) {}

	// Server: true if FireInterval has passed since the last press
	bool CanFireNow() const;

	// Server: remember the time of this press
	void MarkFired();

	// Server: use one charge; when none are left the weapon removes itself
	void ConsumeUse();

	// Server: tell the weapon component we're done, then destroy
	void FinishWeapon();

	AKartCharacter* GetOwnerKart() const;

private:
	void BurstTick();

	float LastFireTime = -1000.f;
	int32 BurstShotsLeft = 0;
	float BurstCharge = 0.f;
	FTimerHandle BurstTimer;
};
