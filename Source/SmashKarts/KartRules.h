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

	/** Trims, strips control characters and clamps a player name. Empty input → empty output. */
	SMASHKARTS_API FString SanitizePlayerName(const FString& InName, int32 MaxLength = 16);
}

namespace KartTime
{
	/** Server world time in seconds, valid on server and clients (GameState-synced). 0 without a world. */
	SMASHKARTS_API double ServerNow(const UObject* WorldContext);
}
