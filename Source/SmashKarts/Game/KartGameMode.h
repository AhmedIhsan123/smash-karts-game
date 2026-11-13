#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "KartGameMode.generated.h"

class AKartCharacter;
class AKartGameState;
class UKartWeaponData;
class UDamageType;

/**
 * Free For All rules: lobby → countdown → timed match → results → play again / lobby.
 *
 * Authority: exists on the server only. Owns match flow, spawning, respawning, scoring and host actions.
 * Replicates: nothing itself; results go out through AKartGameState / AKartPlayerState / the karts.
 * Client prediction: none. Clients ask for host actions through AKartPlayerController Server RPCs,
 * which are checked here (only the listen-server host may start / restart / change the match).
 */
UCLASS()
class SMASHKARTS_API AKartGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AKartGameMode();

	// ---------------- Tuning (edit in BP_KartGameMode) ----------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "10", Units = "s"))
	float MatchDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", Units = "s"))
	float CountdownDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", Units = "s"))
	float RespawnDelay;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", Units = "s"))
	float SpawnProtectionDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", Units = "s"))
	float ResultsDuration;

	/** A fall within this many seconds of being hit credits the attacker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", Units = "s"))
	float FallCreditWindow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "2", ClampMax = "16"))
	int32 MaxKartPlayers;

	/** Match lengths the host can pick in the lobby (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	TArray<int32> MatchDurationChoices;

	/** After the results screen, go back to the lobby automatically if the host did nothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	bool bAutoReturnToLobby;

	/** Spawn points closer than this to another kart count as occupied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0"))
	float SpawnOccupiedRadius;

	/** Respawn picks randomly among this many of the safest spawn points. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1"))
	int32 SpawnRandomTopN;

	/** Every weapon in the game (debug SK.GiveWeapon, HUD). Loot boxes keep their own pools. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapons")
	TArray<TObjectPtr<UKartWeaponData>> WeaponCatalog;

	// ---------------- Host actions (validated) ----------------
	bool IsHost(const AController* Controller) const;
	void RequestStartMatch(AController* Requester);
	void RequestPlayAgain(AController* Requester);
	void RequestBackToLobby(AController* Requester);
	void RequestEndMatch(AController* Requester);
	void RequestSetMatchDuration(AController* Requester, int32 Seconds);

	// ---------------- Gameplay ----------------
	/** Server: a kart hit 0 health or fell out of the world. Killer may be null. */
	void KartEliminated(AKartCharacter* Victim, AController* Killer, TSubclassOf<UDamageType> DamageType, bool bFell);

	// ---------------- Debug / tests (no-ops in Shipping where it matters) ----------------
	void DebugSetMatchTimeRemaining(float Seconds);
	void DebugForceStartMatch();
	UKartWeaponData* FindWeaponByName(const FString& Name) const;
	void DumpStateToLog() const;

	// ---------------- AGameModeBase / AGameMode ----------------
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void Logout(AController* Exiting) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual bool ReadyToEndMatch_Implementation() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;

protected:
	virtual void HandleMatchIsWaitingToStart() override;
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual bool FindInactivePlayer(APlayerController* PC) override;
	virtual void AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC) override;

private:
	AKartGameState* GetKartGameState() const;
	double Now() const;

	void BeginCountdown();
	void OnCountdownFinished();
	void OnMatchTimeUp();
	void OnResultsFinished();
	void GoToLobby();

	void ScheduleRespawn(AController* Controller);
	void OnRespawnTimer(TWeakObjectPtr<AController> Controller);
	void RespawnNow(AController* Controller);
	void RespawnEveryone();
	void ClearRespawnTimers();

	/** Removes projectiles and traps, resets loot boxes (active only during a match). */
	void ResetWorldForRound(bool bLootActive);
	void RecheckLootBoxes();

	int32 PickFreeColorIndex(const APlayerState* Ignore) const;

	FTimerHandle CountdownTimer;
	FTimerHandle MatchTimer;
	FTimerHandle ResultsTimer;
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
	int32 NextJoinOrder = 0;
};
