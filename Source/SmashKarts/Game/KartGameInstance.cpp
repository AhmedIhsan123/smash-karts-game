#include "Game/KartGameInstance.h"
#include "Game/KartGameState.h"
#include "Game/KartOnlineSubsystem.h"
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
	if (UKartOnlineSubsystem* Online = GetOnline())
	{
		OnlineStateHandle = Online->OnStateChanged.AddUObject(this, &UKartGameInstance::HandleOnlineStateChanged);
	}
}

void UKartGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	if (UKartOnlineSubsystem* Online = GetOnline())
	{
		Online->OnStateChanged.Remove(OnlineStateHandle);
	}
	Super::Shutdown();
}

// =====================================================================
// Settings
// =====================================================================

void UKartGameInstance::LoadSettings()
{
	Settings = UGameplayStatics::DoesSaveGameExist(SettingsSlot, 0)
		? Cast<UKartSaveGame>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0))
		: nullptr;
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

UKartOnlineSubsystem* UKartGameInstance::GetOnline() const
{
	return GetSubsystem<UKartOnlineSubsystem>();
}

FString UKartGameInstance::GetHostedRoomCode() const
{
	const UKartOnlineSubsystem* Online = GetOnline();
	return (bHostingOnline && Online) ? Online->GetRoomCode() : FString();
}

void UKartGameInstance::StopHostingOnline()
{
	if (bHostingOnline)
	{
		bHostingOnline = false;
		if (UKartOnlineSubsystem* Online = GetOnline())
		{
			Online->DestroyRoom();
		}
	}
}

FString UKartGameInstance::OnlineNotReadyMessage(bool bHosting) const
{
	UKartOnlineSubsystem* Online = GetOnline();
	const EKartOnlineState State = Online ? Online->GetState() : EKartOnlineState::Off;
	switch (State)
	{
	case EKartOnlineState::Ready:
		return FString();
	case EKartOnlineState::Connecting:
		return TEXT("Still connecting to online play. Try again in a few seconds.");
	case EKartOnlineState::Failed:
		return TEXT("Online play is unavailable right now (see the line at the bottom). Direct IP still works.");
	case EKartOnlineState::Off:
	default:
		return bHosting
			? FString(TEXT("Online play isn't set up in this build. Use Host Game and share your IP (HOSTING.md)."))
			: FString(TEXT("Room codes need online play, which isn't set up in this build. Join with the host's IP address instead (HOSTING.md)."));
	}
}

bool UKartGameInstance::QueueUntilOnline(EKartPendingOnline Action, const FString& Code)
{
	UKartOnlineSubsystem* Online = GetOnline();
	if (!Online)
	{
		return false;
	}
	if (Online->GetState() == EKartOnlineState::Failed)
	{
		Online->Retry(); // a click is a good moment to try again
	}
	if (Online->GetState() != EKartOnlineState::Connecting)
	{
		return false;
	}
	PendingOnline = Action;
	PendingRoomCode = Code;
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Waiting for online play to %s"), Action == EKartPendingOnline::Host ? TEXT("host") : *FString::Printf(TEXT("join %s"), *Code));
	SetMenuMessage(Action == EKartPendingOnline::Host
		? FString(TEXT("Connecting to online play... your game starts as soon as it's ready."))
		: FString::Printf(TEXT("Connecting to online play... then joining room %s."), *Code), false);
	return true;
}

void UKartGameInstance::HandleOnlineStateChanged()
{
	const UKartOnlineSubsystem* Online = GetOnline();
	const EKartOnlineState State = Online ? Online->GetState() : EKartOnlineState::Off;
	if (PendingOnline == EKartPendingOnline::None || State == EKartOnlineState::Connecting)
	{
		return;
	}
	const EKartPendingOnline Action = PendingOnline;
	PendingOnline = EKartPendingOnline::None;
	if (State != EKartOnlineState::Ready)
	{
		SetMenuMessage(TEXT("Couldn't connect to online play (see the line at the bottom). Direct IP still works."));
		return;
	}
	if (Action == EKartPendingOnline::Host)
	{
		HostOnlineGame();
	}
	else
	{
		JoinRoom(PendingRoomCode);
	}
}

void UKartGameInstance::HostGame()
{
	const FString Map = GetArenaMap();
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Hosting on %s (direct IP, port %d)"), *Map, GamePort);
	MenuMessage.Reset();
	PendingOnline = EKartPendingOnline::None;
	StopHostingOnline();
	// bUseIPSockets: plain UDP even when EOS P2P is the game net driver (it's ignored otherwise)
	UGameplayStatics::OpenLevel(this, FName(*Map), true, FString(TEXT("listen?bUseIPSockets")) + BuildPlayerOptions());
}

bool UKartGameInstance::HostOnlineGame()
{
	if (QueueUntilOnline(EKartPendingOnline::Host, FString()))
	{
		return true;
	}
	const FString NotReady = OnlineNotReadyMessage(true);
	if (!NotReady.IsEmpty())
	{
		SetMenuMessage(NotReady);
		return false;
	}
	const FString Map = GetArenaMap();
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Hosting online on %s"), *Map);
	MenuMessage.Reset();
	StopHostingOnline();
	bHostingOnline = true;
	// The room is advertised while the map loads; the lobby shows the code once EOS confirms it
	GetOnline()->CreateRoom(GetPlayerName(), FKartRoomCreated::CreateUObject(this, &UKartGameInstance::HandleRoomCreated));
	UGameplayStatics::OpenLevel(this, FName(*Map), true, FString(TEXT("listen")) + BuildPlayerOptions());
	return true;
}

void UKartGameInstance::HandleRoomCreated(bool bSuccess, const FString& CodeOrError)
{
	if (!bHostingOnline)
	{
		return;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Online room: %s"), *CodeOrError);
	// Already in the arena: put the code on the game state for everyone's lobby (the game mode does it on map load otherwise)
	UWorld* World = GetWorld();
	AKartGameState* GS = World ? World->GetGameState<AKartGameState>() : nullptr;
	if (GS && GS->HasAuthority())
	{
		GS->RoomCode = GetHostedRoomCode();
	}
}

bool UKartGameInstance::JoinGame(const FString& CodeOrAddress)
{
	FString Target = CodeOrAddress.TrimStartAndEnd();
	if (Target.IsEmpty())
	{
		SetMenuMessage(TEXT("Type a room code or the host's IP address first."));
		return false;
	}
	if (Settings)
	{
		Settings->LastJoinAddress = Target;
		SaveSettings();
	}
	if (KartRules::IsRoomCode(Target))
	{
		return JoinRoom(KartRules::NormalizeRoomCode(Target));
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
	bJoiningOnline = false;
	PendingOnline = EKartPendingOnline::None;
	PC->ClientTravel(Target + BuildPlayerOptions(), TRAVEL_Absolute);
	return true;
}

bool UKartGameInstance::JoinRoom(const FString& Code)
{
	if (QueueUntilOnline(EKartPendingOnline::Join, Code))
	{
		return true;
	}
	const FString NotReady = OnlineNotReadyMessage(false);
	if (!NotReady.IsEmpty())
	{
		SetMenuMessage(NotReady);
		return false;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Looking for room %s"), *Code);
	SetMenuMessage(FString::Printf(TEXT("Looking for room %s..."), *Code), false);
	bJoiningOnline = true;
	GetOnline()->FindRoom(Code, FKartRoomFound::CreateUObject(this, &UKartGameInstance::HandleRoomFound));
	return true;
}

void UKartGameInstance::HandleRoomFound(bool bSuccess, const FString& UrlOrError, const FString& HostName)
{
	if (!bSuccess)
	{
		SetMenuMessage(UrlOrError);
		return;
	}
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Joining %s's room at %s"), *HostName, *UrlOrError);
	SetMenuMessage(FString::Printf(TEXT("Joining %s's game..."), HostName.IsEmpty() ? TEXT("the host") : *HostName), false);
	PC->ClientTravel(UrlOrError + BuildPlayerOptions(), TRAVEL_Absolute);
}

void UKartGameInstance::LeaveToMainMenu()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Leaving to the main menu (%s)"), World->GetNetMode() == NM_ListenServer ? TEXT("host") : TEXT("player"));
	PendingOnline = EKartPendingOnline::None;
	StopHostingOnline();
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

void UKartGameInstance::SetMenuMessage(const FString& Message, bool bIsError)
{
	MenuMessage = Message;
	bMenuMessageIsError = bIsError;
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Message: %s"), *Message);
}

FString UKartGameInstance::ConsumeMenuMessage(bool* bOutIsError)
{
	FString Out = MenuMessage;
	if (bOutIsError)
	{
		*bOutIsError = bMenuMessageIsError;
	}
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
	const FString ConnectProblem = bJoiningOnline
		? FString(TEXT("Couldn't connect to the host's online game. They may have left; ask them to host again for a new code."))
		: FString(TEXT("Couldn't connect. Check the IP address and that the host is in a game (see HOSTING.md)."));

	FString Message;
	if (ErrorString.Contains(TEXT("full"), ESearchCase::IgnoreCase))
	{
		Message = TEXT("That game is full.");
	}
	else if (bJoining && FailureType != ENetworkFailure::OutdatedClient && FailureType != ENetworkFailure::OutdatedServer)
	{
		Message = ConnectProblem;
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
			Message = ConnectProblem;
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
		? FString(bJoiningOnline ? TEXT("Couldn't connect to the host's online game. Ask them to host again for a new code.")
			: TEXT("Couldn't connect. Check the IP address and that the host's game is running."))
		: FString::Printf(TEXT("Couldn't load the game: %s"), *ErrorString));
}
