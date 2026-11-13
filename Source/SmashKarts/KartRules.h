#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * Pure gameplay rules with no world access, so they can be covered by automation tests
 * (Source/SmashKarts/Tests). Everything here is deterministic for given inputs.
 */
namespace KartRules
{
	/** Weighted roll. Roll01 is in [0, 1). Weights <= 0 never win. INDEX_NONE if nothing can win. */
	SMASHKARTS_API int32 PickWeightedIndex(const TArray<float>& Weights, float Roll01);

	/** One scoreboard row for sorting. Id is whatever the caller needs to map back (array index). */
	struct FStandingEntry
	{
		float Score = 0.f;
		int32 Deaths = 0;
		int32 JoinOrder = 0;
		int32 Id = 0;
	};

	/** Best first: higher score, then fewer deaths, then earlier join. */
	SMASHKARTS_API void SortStandings(TArray<FStandingEntry>& Entries);

	/** "M:SS", seconds rounded up so "0:00" only shows once time is really up. Negative → "0:00". */
	SMASHKARTS_API FString FormatMatchTime(float Seconds);

	/** Linear falloff: NearDamage at distance 0, FarDamage at MaxRange and beyond. */
	SMASHKARTS_API float DamageByDistance(float Distance, float MaxRange, float NearDamage, float FarDamage);

	/** Picks randomly among the TopN highest scores. Rand01 is in [0, 1). INDEX_NONE if Scores is empty. */
	SMASHKARTS_API int32 PickSpawnIndex(const TArray<float>& Scores, int32 TopN, float Rand01);

	/**
	 * Speed multiplier for ball Index of a lobbed volley that lands in a line: ball 0 flies the full distance
	 * (BaseSpeed at PitchDegrees), each next ball lands Spacing cm closer (never closer than MinFraction of it).
	 * Flat-ground ballistics: distance = v^2 sin(2 pitch) / gravity, so speed scales with sqrt(distance).
	 */
	SMASHKARTS_API float LineVolleySpeedScale(float BaseSpeed, float PitchDegrees, float Gravity, int32 Index, float Spacing, float MinFraction = 0.15f);

	/** Trims, strips control characters and clamps a player name. Empty input → empty output. */
	SMASHKARTS_API FString SanitizePlayerName(const FString& InName, int32 MaxLength = 16);

	/** One kart's side of a contact check. */
	struct FContactSide
	{
		bool bInvincible = false;
		bool bSpiky = false;
		bool bFrozen = false;
	};

	struct FContactResult
	{
		bool bAHitsB = false;
		bool bBHitsA = false;
	};

	/**
	 * Contact smash rules (priority Invincible > Spiky > normal):
	 * - an Invincible kart smashes a non-invincible kart it touches; two Invincible karts just bounce
	 * - a Spiky kart smashes a non-invincible kart its maces reach; two Spiky karts smash each other
	 * - ramming a Frozen kart smashes it (an Invincible kart can never be frozen)
	 * bAReachesB = B is inside A's mace reach; bARamsB = A drove into B fast enough.
	 */
	SMASHKARTS_API FContactResult ResolveContact(const FContactSide& A, const FContactSide& B, bool bTouching,
		bool bAReachesB, bool bBReachesA, bool bARamsB, bool bBRamsA);

	/**
	 * Zaps an aura buff gets: one right away, then one per Interval while the buff lasts
	 * (Tesla Coil 5 s / 0.5 s = 10). Tolerant of float error (0.7f / 0.1f gives 7, not 6). 0 if Interval <= 0.
	 */
	SMASHKARTS_API int32 AuraZapCount(float Duration, float Interval);
}

namespace KartTime
{
	/** Server world time in seconds, valid on server and clients (GameState-synced). 0 without a world. */
	SMASHKARTS_API double ServerNow(const UObject* WorldContext);
}
