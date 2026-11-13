#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KartHUD.generated.h"

class UKartHUDWidget;
class UKartLobbyWidget;
class UKartPauseWidget;
class UKartScoreboardWidget;
class UDamageType;

/**
 * Owns the C++-built widgets for the local player (HUD, scoreboard/results, lobby, pause menu), turns match
 * events into HUD messages, and draws floating damage numbers.
 * Local only (HUDs exist on the owning machine). Never replicates.
 */
UCLASS()
class SMASHKARTS_API AKartHUD : public AHUD
{
	GENERATED_BODY()

public:
	AKartHUD();

	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;

	/** Called by AKartGameState::Multicast_Elimination on every machine. */
	void HandleElimination(APlayerState* Killer, APlayerState* Victim, TSubclassOf<UDamageType> DamageType, bool bFell);

	/** Floating number at a world location (damage you dealt). */
	void AddDamageNumber(const FVector& WorldLocation, float Amount, bool bKilled);

	void SetPauseMenuOpen(bool bOpen);
	bool IsPauseMenuOpen() const { return bPauseOpen; }

	/** True when the mouse cursor should be visible (lobby, results, menu). */
	bool WantsCursor() const;

	UKartHUDWidget* GetHUDWidget() const { return HUDWidget; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UKartHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UKartScoreboardWidget> ScoreboardWidgetClass;

	/** How long a damage number floats (seconds) and how far it rises (pixels). */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float DamageNumberLifetime;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float DamageNumberRise;

private:
	void EnsureWidgets();
	void OnPauseResume();

	UPROPERTY(Transient) TObjectPtr<UKartHUDWidget> HUDWidget;
	UPROPERTY(Transient) TObjectPtr<UKartScoreboardWidget> ScoreboardWidget;
	UPROPERTY(Transient) TObjectPtr<UKartLobbyWidget> LobbyWidget;
	UPROPERTY(Transient) TObjectPtr<UKartPauseWidget> PauseWidget;

	struct FDamageNumber
	{
		FVector Location;
		float Amount = 0.f;
		double StartTime = 0.0;
		bool bKilled = false;
	};
	TArray<FDamageNumber> DamageNumbers;
	bool bPauseOpen = false;
};
