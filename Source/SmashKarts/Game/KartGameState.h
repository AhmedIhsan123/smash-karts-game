#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "KartGameState.generated.h"

class AKartPlayerState;
class UDamageType;

/** Custom AGameMode match state between WaitingToStart (lobby) and InProgress. */
namespace KartMatchState
{
	extern SMASHKARTS_API const FName Countdown;
}

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKartMatchStateChanged, FName /*NewState*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnKartElimination, APlayerState* /*Killer*/, APlayerState* /*Victim*/, TSubclassOf<UDamageType> /*DamageType*/, bool /*bFell*/);

/**
 * Match-wide state for the FFA mode.
 *
 * Authority: only AKartGameMode (server) writes these values.
 * Replicates: MatchState (engine, RepNotify), the server-time timestamps when the countdown / match /
 * results end, and the chosen match length. Timers are never ticked: clients compute the remaining
 * time from GetServerWorldTimeSeconds(). Eliminations go out as one reliable multicast (kill feed + banners).
 * Client prediction: none.
 */
UCLASS()
class SMASHKARTS_API AKartGameState : public AGameState
{
	GENERATED_BODY()

public:
	AKartGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_MatchState() override;

	// ---------- Written by the server (AKartGameMode) ----------
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	double CountdownEndServerTime = 0.0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	double MatchEndServerTime = 0.0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	double ResultsEndServerTime = 0.0;

	/** Match length the host picked (seconds), shown in the lobby. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	int32 MatchDurationSeconds = 180;

	/** Player colour palette, indexed by AKartPlayerState::ColorIndex. Not replicated: same defaults everywhere. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Players")
	TArray<FLinearColor> PlayerColors;

	/** Server → everyone: someone was eliminated. Killer is null for uncredited falls / world damage. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Elimination(APlayerState* Killer, APlayerState* Victim, TSubclassOf<UDamageType> DamageType, bool bFell);

	// ---------- Queries (any machine) ----------
	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsLobby() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsCountdown() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsPlaying() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsPostMatch() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	float GetCountdownRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	float GetMatchTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	float GetResultsRemaining() const;

	/** Best first (score, then fewer deaths, then join order). */
	TArray<AKartPlayerState*> GetSortedPlayers() const;

	/** 1-based rank of a player in GetSortedPlayers(), 0 if not found. */
	int32 GetRankOf(const APlayerState* PlayerState) const;

	FLinearColor GetColorForIndex(int32 ColorIndex) const;

	/** The host's PlayerState (listen server), or null. */
	AKartPlayerState* GetHostPlayerState() const;

	FOnKartMatchStateChanged OnMatchStateChanged;
	FOnKartElimination OnElimination;
};
