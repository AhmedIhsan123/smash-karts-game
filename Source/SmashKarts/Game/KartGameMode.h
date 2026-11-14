#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "KartGameMode.generated.h"

class AKartCharacter;
class AKartGameState;
class AKartBotController;
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

	// ---------------- Contact smash (Invincible > Spiky > normal; ramming a frozen kart) ----------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contact", meta = (ClampMin = "0"))
	float ContactSmashDamage;

	/** Centre-to-centre distance at which a Spiky kart's maces hit another kart. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contact", meta = (ClampMin = "0"))
	float SpikyReach;

	/** Extra gap on top of both capsule radii that still counts as touching. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contact", meta = (ClampMin = "0"))
	float ContactTouchSlack;

	/** Closing speed (cm/s) needed for a hit on a frozen kart to count as a ram. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contact", meta = (ClampMin = "0"))
	float RamMinSpeed;

	/** Seconds before the same attacker can smash the same victim again. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contact", meta = (ClampMin = "0"))
	float ContactCooldown;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contact")
	TSubclassOf<UDamageType> SpikyContactDamageType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contact")
	TSubclassOf<UDamageType> InvincibleContactDamageType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Contact")
	TSubclassOf<UDamageType> RamDamageType;

	// ---------------- Host actions (validated) ----------------
	bool IsHost(const AController* Controller) const;
	void RequestStartMatch(AController* Requester);
	void RequestPlayAgain(AController* Requester);
	void RequestBackToLobby(AController* Requester);
	void RequestEndMatch(AController* Requester);
	void RequestSetMatchDuration(AController* Requester, int32 Seconds);

	// ---------------- Bots ----------------
	/** Server: add or remove practice bots so there are Count of them (humans + bots never exceed MaxKartPlayers). Returns the new count. */
	int32 SetBotCount(int32 Count);
	void RequestSetBotCount(AController* Requester, int32 Count);
	int32 GetBotCount() const { return Bots.Num(); }

	// ---------------- Gameplay ----------------
	/** Server: a kart hit 0 health or fell out of the world. Killer may be null. */
	void KartEliminated(AKartCharacter* Victim, AController* Killer, TSubclassOf<UDamageType> DamageType, bool bFell);

	/** Server: Rammer drove into Victim (blocking hit). Smashes frozen victims. */
	void HandleKartRam(AKartCharacter* Rammer, AKartCharacter* Victim, float ClosingSpeed);

	virtual void Tick(float DeltaSeconds) override;

	// ---------------- Debug / tests (no-ops in Shipping where it matters) ----------------
	void DebugSetMatchTimeRemaining(float Seconds);
	void DebugForceStartMatch();
	UKartWeaponData* FindWeaponByName(const FString& Name) const;
	void DumpStateToLog() const;

	// ---------------- AGameModeBase / AGameMode ----------------
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void Logout(AController* Exiting) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
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

	/** Spiky / Invincible reach checks between every pair of living karts. */
	void ResolveKartContacts();
	void ApplyContactSmash(AKartCharacter* Attacker, AKartCharacter* Victim, TSubclassOf<UDamageType> DamageType);

	TMap<uint64, double> ContactCooldowns;

	FTimerHandle CountdownTimer;
	FTimerHandle MatchTimer;
	FTimerHandle ResultsTimer;
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
	int32 NextJoinOrder = 0;

	/** True while karts may (re)spawn: lobby, countdown, match. */
	bool IsSpawnPhase() const;
	void AddBot();
	void RemoveBot();
	void TrimBotsToFit();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AKartBotController>> Bots;
	int32 BotsCreated = 0;
};
