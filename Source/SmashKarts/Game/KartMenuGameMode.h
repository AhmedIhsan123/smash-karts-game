#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "KartMenuGameMode.generated.h"

class UKartMainMenuWidget;

/**
 * Game mode for L_MainMenu: no pawn, just the menu. Standalone only (never hosts).
 * Authority/replication: none needed.
 */
UCLASS()
class SMASHKARTS_API AKartMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKartMenuGameMode();
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};

/** Shows the main menu widget with a mouse cursor. Local only. */
UCLASS()
class SMASHKARTS_API AKartMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Transient)
	TObjectPtr<UKartMainMenuWidget> MenuWidget;
};
