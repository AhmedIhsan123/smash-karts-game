#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "KartPlayerState.generated.h"

/**
 * Per-player match data.
 *
 * Authority: only the server changes these (AKartGameMode).
 * Replicates: Score (engine), Eliminations, Deaths, JoinOrder (tie-break), ColorIndex (unique 0-7),
 * bIsHost, RespawnServerTime (>0 while waiting to respawn, server world time).
 * Client prediction: none.
 */
UCLASS()
class SMASHKARTS_API AKartPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AKartPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OnRep_Score() override;

	// ---------- Server ----------
	void ResetMatchStats();
	void AddElimination();
	void AddDeath();
	void SetJoinOrder(int32 InJoinOrder) { JoinOrder = InJoinOrder; }
	void SetColorIndex(int32 InColorIndex) { ColorIndex = InColorIndex; }
	void SetIsHost(bool bInIsHost) { bIsHost = bInIsHost; }
	void SetRespawnServerTime(double InTime) { RespawnServerTime = InTime; }

	// ---------- Any machine ----------
	UFUNCTION(BlueprintPure, Category = "Kart")
	int32 GetEliminations() const { return Eliminations; }

	UFUNCTION(BlueprintPure, Category = "Kart")
	int32 GetDeaths() const { return Deaths; }

	UFUNCTION(BlueprintPure, Category = "Kart")
	int32 GetJoinOrder() const { return JoinOrder; }

	UFUNCTION(BlueprintPure, Category = "Kart")
	int32 GetColorIndex() const { return ColorIndex; }

	UFUNCTION(BlueprintPure, Category = "Kart")
	bool IsHost() const { return bIsHost; }

	double GetRespawnServerTime() const { return RespawnServerTime; }

	/** Palette colour from the GameState (white if unknown). */
	UFUNCTION(BlueprintPure, Category = "Kart")
	FLinearColor GetPlayerColor() const;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Eliminations = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Deaths = 0;

	UPROPERTY(Replicated)
	int32 JoinOrder = 0;

	UPROPERTY(Replicated)
	int32 ColorIndex = 0;

	UPROPERTY(Replicated)
	bool bIsHost = false;

	UPROPERTY(Replicated)
	double RespawnServerTime = 0.0;

	UFUNCTION()
	void OnRep_Stats();
};
