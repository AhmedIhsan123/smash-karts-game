#pragma once

#include "CoreMinimal.h"
#include "KartWeapon.h"
#include "BuffWeapon.generated.h"

class AKartCharacter;

/**
 * Archetype D: buffs (Mace Orbit, Tesla Coil, later Invincibility). One press gives the kart
 * WeaponData->BuffStatus for BuffDuration seconds. The weapon stays held while the buff runs (the HUD counts
 * down, loot boxes are ignored) and removes itself when the status ends.
 * Optional aura (Tesla Coil): every AuraInterval the server zaps every enemy kart within AuraRadius that it can see.
 *
 * Authority: server only (applies the status, picks zap targets, applies the damage).
 * Replicates: the status through the kart's UKartStatusComponent, ActiveEndServerTime (base class, HUD),
 * and each zap as an unreliable multicast with its targets so every machine draws the beams.
 * Client prediction: none. The speed buff (Spiky) is read from the replicated flag by the movement component
 * on both sides; all visuals follow replicated state (UKartStatusFXComponent).
 */
UCLASS()
class SMASHKARTS_API ABuffWeapon : public AKartWeapon
{
	GENERATED_BODY()

public:
	virtual void Fire(float ChargeAlpha) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Server: ends the weapon once the status is gone (expired or cleared). */
	void CheckBuff();

	/** Server: one zap of the aura. */
	void AuraTick();

	/** Server: true if nothing solid is between the kart and the target. */
	bool CanSee(const AKartCharacter* Kart, const AKartCharacter* Target) const;

	/** Every machine: draw the zap beams from our kart to these karts. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Zap(const TArray<AActor*>& Targets);

private:
	bool bActive = false;
	int32 ZapsLeft = 0;
	FTimerHandle CheckTimer;
	FTimerHandle AuraTimer;
};
