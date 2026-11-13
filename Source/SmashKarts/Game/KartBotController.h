#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "KartBotController.generated.h"

class AKartCharacter;

/**
 * Simple practice bot. Unarmed: drives to the nearest loot box it can reach. Armed: hunts the nearest kart and
 * uses the weapon the way it's meant to be used (aims projectiles, drops traps when chased, starts buffs and the
 * machine gun up close, sets the charge from the distance). Steers around walls and backs up when stuck.
 * Idles in the lobby and obeys every input lock (countdown, freeze, results) like a player.
 *
 * Authority: server only (AI controllers never exist on clients).
 * Replicates: nothing itself. Its kart and PlayerState (name, colour, score) replicate like a human player's.
 * Client prediction: none; the kart moves on the server and clients see it like any other remote kart.
 */
UCLASS()
class SMASHKARTS_API AKartBotController : public AAIController
{
	GENERATED_BODY()

public:
	AKartBotController();

	virtual void Tick(float DeltaSeconds) override;

protected:
	/** Max turning speed, degrees per second at full speed (players turn about 100). */
	UPROPERTY(EditDefaultsOnly, Category = "Bot", meta = (ClampMin = "1"))
	float TurnRate = 110.f;

	/** Projectiles fire when the target is within this many degrees of straight ahead. */
	UPROPERTY(EditDefaultsOnly, Category = "Bot", meta = (ClampMin = "0"))
	float FireConeDegrees = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot", meta = (ClampMin = "0"))
	float FireRange = 3500.f;

	/** Buffs without an aura (Mace Orbit, Invincibility) start when an enemy is this close. */
	UPROPERTY(EditDefaultsOnly, Category = "Bot", meta = (ClampMin = "0"))
	float CloseRange = 700.f;

	/** Traps drop when an enemy is behind and this close. */
	UPROPERTY(EditDefaultsOnly, Category = "Bot", meta = (ClampMin = "0"))
	float TrapRange = 1500.f;

	/** Seconds a shot has to stay lined up before the bot fires (keeps them beatable). */
	UPROPERTY(EditDefaultsOnly, Category = "Bot", meta = (ClampMin = "0"))
	float ReactionTime = 0.25f;

	/** Uses the weapon anyway after holding it this long. */
	UPROPERTY(EditDefaultsOnly, Category = "Bot", meta = (ClampMin = "0"))
	float MaxHoldTime = 8.f;

	/** How far ahead it looks for walls (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Bot", meta = (ClampMin = "50"))
	float WallLookAhead = 600.f;

	/** Loot boxes more than this far above the kart (on platforms) are skipped. */
	UPROPERTY(EditDefaultsOnly, Category = "Bot", meta = (ClampMin = "0"))
	float MaxClimb = 120.f;

private:
	AActor* PickTarget(const AKartCharacter* Kart, bool& bOutIsEnemy) const;
	/** Best heading to drive: open space first, then close to TargetYaw. OutAhead = free distance straight ahead. */
	float ChooseHeading(const AKartCharacter* Kart, float TargetYaw, float& OutAhead) const;
	void ThinkWeapon(AKartCharacter* Kart, const AActor* Target, bool bTargetIsEnemy);
	bool CanSee(const AKartCharacter* Kart, const AActor* Target) const;
	void ResetState();

	TWeakObjectPtr<AActor> CurrentTarget;
	bool bTargetIsEnemy = false;
	double RetargetTime = 0.0;
	double StuckSince = -1.0;
	double ReverseUntil = 0.0;
	float ReverseSteer = 1.f;
	double AimedSince = -1.0;
	double HoldingSince = -1.0;
	TWeakObjectPtr<const AActor> StuckTarget;
	double LastStuckTime = -100.0;
	TWeakObjectPtr<const AActor> IgnoredTarget;
	double IgnoreUntil = 0.0;
};
