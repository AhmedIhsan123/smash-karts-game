#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "KartPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Player controller: host actions, UI keys and debug commands.
 *
 * Authority: clients only send requests (Server_* RPCs). AKartGameMode checks that the sender is the host.
 * Replicates: nothing extra.
 * Client prediction: none. UI keys use a mapping context built at runtime (no input assets needed):
 * Tab = scoreboard (hold), Enter = start match / play again (host), Backspace = back to lobby (host).
 * Note: the engine's legacy yaw input scale is left at its default on purpose (kart steering is tuned with it).
 */
UCLASS()
class SMASHKARTS_API AKartPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKartPlayerController();

	// ---------- Host actions (sent by any client, honoured only for the host) ----------
	UFUNCTION(Server, Reliable)
	void Server_RequestStartMatch();

	UFUNCTION(Server, Reliable)
	void Server_RequestPlayAgain();

	UFUNCTION(Server, Reliable)
	void Server_RequestBackToLobby();

	UFUNCTION(Server, Reliable)
	void Server_RequestEndMatch();

	UFUNCTION(Server, Reliable)
	void Server_SetMatchDuration(int32 Seconds);

	// ---------- Debug (development builds; ignored in Shipping) ----------
	/** Local entry point used by the SK.* console commands. Routes to the server when needed. */
	void RunDebugCommand(const FString& Command);

	UFUNCTION(Server, Reliable)
	void Server_DebugCommand(const FString& Command);

	// ---------- Local UI state ----------
	bool IsScoreboardHeld() const { return bScoreboardHeld; }
	bool IsNetOverlayVisible() const { return bShowNetOverlay; }

	/** True on the machine of the listen-server host (works on clients through the PlayerState flag). */
	bool IsLocalHost() const;

protected:
	virtual void SetupInputComponent() override;

private:
	void BuildUIInput();
	void OnScoreboardPressed();
	void OnScoreboardReleased();
	void OnConfirmPressed();
	void OnBackPressed();

	/** Server side of RunDebugCommand. */
	void HandleDebugCommand(const FString& Command);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> UIMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ScoreboardAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BackAction;

	bool bScoreboardHeld = false;
	bool bShowNetOverlay = false;
};
