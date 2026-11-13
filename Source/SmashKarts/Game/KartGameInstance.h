#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "KartGameInstance.generated.h"

class UKartSaveGame;

/**
 * Lives for the whole session (survives map changes): host / join / leave, connection error messages
 * for the main menu, and the player's saved name and volume.
 *
 * Authority: local only. Hosting = OpenLevel(arena, "listen"); joining = ClientTravel to "<ip>[:port]".
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

	// ---------- Sessions (direct IP) ----------
	/** Start a listen server on the arena map. */
	UFUNCTION(BlueprintCallable, Category = "Kart")
	void HostGame();

	/** Join "<ip>" or "<ip>:<port>" (default port 7777). False if the address is empty. */
	UFUNCTION(BlueprintCallable, Category = "Kart")
	bool JoinGame(const FString& Address);

	/** Back to the main menu. The host closing its game disconnects everyone (they see "The host left"). */
	UFUNCTION(BlueprintCallable, Category = "Kart")
	void LeaveToMainMenu();

	/** Message for the main menu (connection errors, "host left"). Empty if none. Clears it. */
	FString ConsumeMenuMessage();
	void SetMenuMessage(const FString& Message);

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
	FString GetArenaMap() const;
	FString GetMainMenuMap() const;
	FString BuildPlayerOptions() const;
	void LoadSettings();
	void SaveSettings();
	void ApplyVolume() const;

	UPROPERTY(Transient)
	TObjectPtr<UKartSaveGame> Settings;

	FString MenuMessage;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
