#include "Game/KartGameInstance.h"
#include "Game/KartSaveGame.h"
#include "KartRules.h"
#include "SmashKarts.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* SettingsSlot = TEXT("KartPlayerSettings");
}

void UKartGameInstance::Init()
{
	Super::Init();
	LoadSettings();
	ApplyVolume();
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UKartGameInstance::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UKartGameInstance::HandleTravelFailure);
	}
}

void UKartGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	Super::Shutdown();
}

// =====================================================================
// Settings
// =====================================================================

void UKartGameInstance::LoadSettings()
{
	Settings = Cast<UKartSaveGame>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0));
	if (!Settings)
	{
		Settings = Cast<UKartSaveGame>(UGameplayStatics::CreateSaveGameObject(UKartSaveGame::StaticClass()));
	}
}

void UKartGameInstance::SaveSettings()
{
	if (Settings)
	{
		UGameplayStatics::SaveGameToSlot(Settings, SettingsSlot, 0);
	}
}

FString UKartGameInstance::GetPlayerName() const
{
	return Settings ? Settings->PlayerName : FString();
}

void UKartGameInstance::SetPlayerName(const FString& NewName)
{
	if (Settings)
	{
		Settings->PlayerName = KartRules::SanitizePlayerName(NewName);
		SaveSettings();
	}
}

float UKartGameInstance::GetMasterVolume() const
{
	return Settings ? Settings->MasterVolume : 1.f;
}

void UKartGameInstance::SetMasterVolume(float Volume)
{
	if (Settings)
	{
		Settings->MasterVolume = FMath::Clamp(Volume, 0.f, 1.f);
		SaveSettings();
	}
	ApplyVolume();
}

void UKartGameInstance::ApplyVolume() const
{
	if (GEngine)
	{
		if (FAudioDevice* Device = GEngine->GetMainAudioDeviceRaw())
		{
			Device->SetTransientPrimaryVolume(GetMasterVolume());
		}
	}
}

FString UKartGameInstance::GetLastJoinAddress() const
{
	return (Settings && !Settings->LastJoinAddress.IsEmpty()) ? Settings->LastJoinAddress : FString(TEXT("127.0.0.1"));
}

// =====================================================================
// Host / join / leave
// =====================================================================

FString UKartGameInstance::GetArenaMap() const
{
	if (!ArenaMapOverride.IsEmpty())
	{
		return ArenaMapOverride;
	}
	return UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Server);
}

FString UKartGameInstance::GetMainMenuMap() const
{
	return UGameMapsSettings::GetGameDefaultMap();
}

FString UKartGameInstance::BuildPlayerOptions() const
{
	const FString Name = GetPlayerName();
	return Name.IsEmpty() ? FString() : FString::Printf(TEXT("?SKName=%s"), *Name.Replace(TEXT(" "), TEXT("%20")));
}

void UKartGameInstance::HostGame()
{
	const FString Map = GetArenaMap();
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Hosting on %s (port %d)"), *Map, GamePort);
	MenuMessage.Reset();
	UGameplayStatics::OpenLevel(this, FName(*Map), true, FString(TEXT("listen")) + BuildPlayerOptions());
}

bool UKartGameInstance::JoinGame(const FString& Address)
{
	FString Target = Address.TrimStartAndEnd();
	if (Target.IsEmpty())
	{
		SetMenuMessage(TEXT("Type the host's IP address first."));
		return false;
	}
	if (Settings)
	{
		Settings->LastJoinAddress = Target;
		SaveSettings();
	}
	if (!Target.Contains(TEXT(":")))
	{
		Target += FString::Printf(TEXT(":%d"), GamePort);
	}
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		return false;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Joining %s"), *Target);
	MenuMessage.Reset();
	PC->ClientTravel(Target + BuildPlayerOptions(), TRAVEL_Absolute);
	return true;
}

void UKartGameInstance::LeaveToMainMenu()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Leaving to the main menu (%s)"), World->GetNetMode() == NM_ListenServer ? TEXT("host") : TEXT("player"));
	if (World->GetNetMode() == NM_Client)
	{
		if (APlayerController* PC = GetFirstLocalPlayerController())
		{
			PC->ClientTravel(GetMainMenuMap(), TRAVEL_Absolute);
			return;
		}
	}
	// Host (or standalone): closing the map ends the listen server; clients get "host left"
	UGameplayStatics::OpenLevel(this, FName(*GetMainMenuMap()), true);
}

// =====================================================================
// Errors -> main menu message
// =====================================================================

void UKartGameInstance::SetMenuMessage(const FString& Message)
{
	MenuMessage = Message;
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Message: %s"), *Message);
}

FString UKartGameInstance::ConsumeMenuMessage()
{
	FString Out = MenuMessage;
	MenuMessage.Reset();
	return Out;
}

void UKartGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Network failure %s: %s"), ENetworkFailure::ToString(FailureType), *ErrorString);
	if (World && World->GetNetMode() == NM_ListenServer)
	{
		return; // a client dropping is not the host's problem
	}
	if (!MenuMessage.IsEmpty())
	{
		return; // one disconnect often fires several failures; the first one is the most specific
	}

	// Still joining (never got in): it's a connect problem, not a disconnect
	const bool bJoining = NetDriver && NetDriver->NetDriverName == NAME_PendingNetDriver;

	FString Message;
	if (ErrorString.Contains(TEXT("full"), ESearchCase::IgnoreCase))
	{
		Message = TEXT("That game is full.");
	}
	else if (bJoining && FailureType != ENetworkFailure::OutdatedClient && FailureType != ENetworkFailure::OutdatedServer)
	{
		Message = TEXT("Couldn't connect. Check the IP address and that the host is in a game (see HOSTING.md).");
	}
	else
	{
		switch (FailureType)
		{
		case ENetworkFailure::ConnectionLost:
		case ENetworkFailure::FailureReceived:
			Message = ErrorString.Contains(TEXT("closed"), ESearchCase::IgnoreCase) || ErrorString.Contains(TEXT("lost"), ESearchCase::IgnoreCase)
				|| ErrorString.IsEmpty()
				? FString(TEXT("The host left the game."))
				: FString::Printf(TEXT("Disconnected: %s"), *ErrorString);
			break;
		case ENetworkFailure::ConnectionTimeout:
			Message = TEXT("Lost connection to the host (timed out).");
			break;
		case ENetworkFailure::PendingConnectionFailure:
			Message = TEXT("Couldn't connect. Check the IP address and that the host's game is running (see HOSTING.md).");
			break;
		case ENetworkFailure::OutdatedClient:
		case ENetworkFailure::OutdatedServer:
			Message = TEXT("Your game version doesn't match the host's. Everyone needs the same build.");
			break;
		default:
			Message = FString::Printf(TEXT("Network error: %s"), *ErrorString);
			break;
		}
	}
	SetMenuMessage(Message);
}

void UKartGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Travel failure %s: %s"), ETravelFailure::ToString(FailureType), *ErrorString);
	SetMenuMessage(FailureType == ETravelFailure::PendingNetGameCreateFailure
		? FString(TEXT("Couldn't connect. Check the IP address and that the host's game is running."))
		: FString::Printf(TEXT("Couldn't load the game: %s"), *ErrorString));
}
