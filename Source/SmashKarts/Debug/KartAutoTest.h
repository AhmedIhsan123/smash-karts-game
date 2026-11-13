#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Scripted end-to-end match test for listen-server runs (development builds only; a no-op in Shipping).
 * Runs on the host: waits for N players, starts a match, eliminates a client by damage and by falls,
 * checks scoring, respawn and spawn protection, ends the match, plays again and returns to the lobby.
 * Every step logs "[AutoTest] ..." to LogSmashKarts; the run ends with "[AutoTest] PASS" or "[AutoTest] FAIL".
 * Start it with: SK.AutoTest <players> [quit]
 */
class SMASHKARTS_API FKartAutoTest
{
public:
	static void Start(UWorld* World, int32 ExpectedPlayers, bool bQuitWhenDone);
};
