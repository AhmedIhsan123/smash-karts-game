#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "KartDamageType.generated.h"

/**
 * Data-only damage type, one Blueprint child per weapon (DT_Rockets, ...), set as DamageType in the weapon's DA_.
 * Authority: read on the server (elimination) and on clients (kill feed). Nothing replicates; the class is sent.
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

	/** Kill-feed label for a damage type class, "smash" if unknown. */
	static FText GetKillFeedName(TSubclassOf<UDamageType> DamageType);

	/** True if this damage type asks for the big death explosion. */
	static bool WantsBigExplosion(TSubclassOf<UDamageType> DamageType);
};
