#pragma once

#include "CoreMinimal.h"
#include "Shakes/DefaultCameraShakeBase.h"
#include "KartFeedback.generated.h"

class APawn;

/** Short jolt when your kart takes damage (scaled by the damage). Perlin noise, set up in C++ (no asset). */
UCLASS()
class SMASHKARTS_API UKartHitShake : public UDefaultCameraShakeBase
{
	GENERATED_BODY()

public:
	UKartHitShake(const FObjectInitializer& ObjectInitializer);
};

/** Rumble from an explosion near you (scaled by blast size and distance). */
UCLASS()
class SMASHKARTS_API UKartExplosionShake : public UDefaultCameraShakeBase
{
	GENERATED_BODY()

public:
	UKartExplosionShake(const FObjectInitializer& ObjectInitializer);
};

/** Tiny kick when you fire. */
UCLASS()
class SMASHKARTS_API UKartFireShake : public UDefaultCameraShakeBase
{
	GENERATED_BODY()

public:
	UKartFireShake(const FObjectInitializer& ObjectInitializer);
};

/**
 * Game feel: camera shakes and the HUD's red damage flash. Called from RepNotifies and multicasts that already
 * run on every machine, so it only touches local player controllers; nothing replicates. Shakes respect the
 * player's Screen Shake setting (UKartGameInstance).
 */
namespace KartFeedback
{
	/** Kart lost Amount health. Only does something on the machine that controls it: shake + red flash. */
	SMASHKARTS_API void KartDamaged(APawn* Kart, float Amount);

	/**
	 * Something exploded at Location. Strength is the explosion FX scale (bullet 0.15, rocket 0.6, Sticky Bomb 2,
	 * Nuke 5): it sets both how far the shake reaches and how hard it is (capped). Every local player near it shakes.
	 */
	SMASHKARTS_API void Explosion(const UObject* WorldContext, const FVector& Location, float Strength);

	/** The local player fired a weapon. */
	SMASHKARTS_API void Fired(APawn* Kart);

	/** Shake strength for an explosion of Strength at Distance (0 when out of reach). Pure, for tests. */
	SMASHKARTS_API float ExplosionShakeScale(float Strength, float Distance);
}
