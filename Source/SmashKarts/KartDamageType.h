#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "KartDamageType.generated.h"

/**
 * Data-only damage type, one Blueprint child per weapon (DT_Rockets, ...), set as DamageType in the weapon's DA_.
 * Authority: read on the server (elimination, knockback) and on clients (kill feed). Nothing replicates; the class is sent.
 */
UCLASS(Blueprintable, Const)
class SMASHKARTS_API UKartDamageType : public UDamageType
{
	GENERATED_BODY()

public:
	/** Name shown in the kill feed: "A [Rockets] B" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart")
	FText KillFeedName;

	/** Victim plays the big death explosion (visual only). Nuke / Sticky Bomb. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart")
	bool bBigDeathExplosion = false;

	/** Horizontal push (cm/s) on a kart that survives the hit, away from the blast. Scaled by splash falloff. 0 = none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Knockback", meta = (ClampMin = "0"))
	float KnockbackStrength = 0.f;

	/** Upward part of the push (cm/s), scaled like KnockbackStrength. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Knockback", meta = (ClampMin = "0"))
	float KnockbackUpward = 0.f;

	/** Ignores damage reduction (Spiky -10%). Contact smashes use this so a smash always smashes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart")
	bool bIgnoreDamageReduction = false;

	static bool IgnoresDamageReduction(TSubclassOf<UDamageType> DamageType);

	/** Kill-feed label for a damage type class, "smash" if unknown. */
	static FText GetKillFeedName(TSubclassOf<UDamageType> DamageType);

	/** True if this damage type asks for the big death explosion. */
	static bool WantsBigExplosion(TSubclassOf<UDamageType> DamageType);

	/** Knockback numbers for a damage type class (0 / 0 for generic damage). */
	static void GetKnockback(TSubclassOf<UDamageType> DamageType, float& OutStrength, float& OutUpward);
};
