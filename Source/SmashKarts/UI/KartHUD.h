#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KartHUD.generated.h"

class UKartHUDWidget;
class UKartScoreboardWidget;
class UDamageType;

/**
 * Owns the C++-built widgets for the local player and turns match events into HUD messages.
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

	UKartHUDWidget* GetHUDWidget() const { return HUDWidget; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UKartHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UKartScoreboardWidget> ScoreboardWidgetClass;

private:
	void EnsureWidgets();

	UPROPERTY(Transient)
	TObjectPtr<UKartHUDWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UKartScoreboardWidget> ScoreboardWidget;
};
