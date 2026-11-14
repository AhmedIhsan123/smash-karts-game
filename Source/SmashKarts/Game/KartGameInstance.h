#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "KartGameInstance.generated.h"

class UKartSaveGame;
class UKartOnlineSubsystem;

/** Host Online / Join by code clicked while online play was still connecting: runs once it's ready. */
enum class EKartPendingOnline : uint8
{
	None,
	Host,
	Join
};

/**
 * Lives for the whole session (survives map changes): host / join / leave, connection error messages
 * for the main menu, and the player's saved name and volume.
 *
 * Authority: local only. Hosting = OpenLevel(arena, "listen"); joining = ClientTravel to "<ip>[:port]", or for a
 * room code to the host's EOS address found by UKartOnlineSubsystem ("[EOS:<id>]").
 * The player name travels to the server as our own "?SKName=" URL option (read in AKartGameMode::InitNewPlayer).
 * Replicates: nothing. Client prediction: n/a.
 */
UCLASS(Config = Game)
class SMASHKARTS_API UKartGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// ---------- Player preferences ----------
	UFUNCTION(BlueprintPure, Category = "Kart")
	FString GetPlayerName() const;

	UFUNCTION(BlueprintCallable, Category = "Kart")
	void SetPlayerName(const FString& NewName);

	float GetMasterVolume() const;
	void SetMasterVolume(float Volume);

	FString GetLastJoinAddress() const;

	// ---------- Sessions ----------
	/** Start a listen server on the arena map that friends join by IP address (LAN, VPN, port forwarding). */
	UFUNCTION(BlueprintCallable, Category = "Kart")
	void HostGame();

	/** Host online: listen over EOS peer-to-peer and advertise a room code. False (+ menu message) if online play isn't ready. */
	UFUNCTION(BlueprintCallable, Category = "Kart")
	bool HostOnlineGame();

	/** Join by room code (5 characters) or by "<ip>" / "<ip>:<port>" (default port 7777). False (+ menu message) if it couldn't start. */
	UFUNCTION(BlueprintCallable, Category = "Kart")
	bool JoinGame(const FString& CodeOrAddress);

	/** Back to the main menu. The host closing its game disconnects everyone (they see "The host left"). */
	UFUNCTION(BlueprintCallable, Category = "Kart")
	void LeaveToMainMenu();

	/** Online play (EOS). Valid after Init. */
	UKartOnlineSubsystem* GetOnline() const;

	/** True while this machine hosts an online (room code) game. */
	bool IsHostingOnline() const { return bHostingOnline; }

	/** The room code this machine hosts ("" for IP games and while the room is being created). */
	FString GetHostedRoomCode() const;

	/** Message for the main menu (connection errors, "host left", progress like "Looking for room..."). Empty if none. Clears it. */
	FString ConsumeMenuMessage(bool* bOutIsError = nullptr);
	void SetMenuMessage(const FString& Message, bool bIsError = true);

	/** Arena map players host on (Project Settings > Maps & Modes > Server Default Map unless overridden here). */
	UPROPERTY(Config)
	FString ArenaMapOverride;

	/** The port shown to players / used when an address has none. */
	UPROPERTY(Config)
	int32 GamePort = 7777;

	/** Short game title for menus and the window. */
	UPROPERTY(Config)
	FString GameTitle = TEXT("Kart Arena");

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandleRoomCreated(bool bSuccess, const FString& CodeOrError);
	void HandleRoomFound(bool bSuccess, const FString& UrlOrError, const FString& HostName);
	bool JoinRoom(const FString& Code);
	void StopHostingOnline();
	void HandleOnlineStateChanged();
	/** Online play still connecting (or retrying after a failure): remember the click and say so. True if queued. */
	bool QueueUntilOnline(EKartPendingOnline Action, const FString& Code);
	/** Why room codes can't be used right now ("" if they can). Kicks off a login retry after a failure. */
	FString OnlineNotReadyMessage(bool bHosting) const;
	FString GetArenaMap() const;
	FString GetMainMenuMap() const;
	FString BuildPlayerOptions() const;
	void LoadSettings();
	void SaveSettings();
	void ApplyVolume() const;

	UPROPERTY(Transient)
	TObjectPtr<UKartSaveGame> Settings;

	FString MenuMessage;
	bool bMenuMessageIsError = true;
	bool bHostingOnline = false;
	/** The last join attempt used a room code (for clearer connection error messages). */
	bool bJoiningOnline = false;
	EKartPendingOnline PendingOnline = EKartPendingOnline::None;
	FString PendingRoomCode;
	FDelegateHandle OnlineStateHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
