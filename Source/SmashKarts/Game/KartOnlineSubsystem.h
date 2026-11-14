#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KartOnlineSubsystem.generated.h"

struct FKartEOSState;

UENUM()
enum class EKartOnlineState : uint8
{
	/** Online play is off: no EOS credentials in this build, the editor (PIE), -NoEOS or a dedicated server. Direct IP only. */
	Off,
	/** Logging in to Epic Online Services. */
	Connecting,
	/** Logged in: room codes and peer-to-peer connections work. */
	Ready,
	/** Login failed (no internet, wrong credentials, EOS down). Retries on its own; direct IP still works. */
	Failed
};

/** CreateRoom result: (true, room code) or (false, a message for the player). */
DECLARE_DELEGATE_TwoParams(FKartRoomCreated, bool /*bSuccess*/, const FString& /*CodeOrError*/);

/** FindRoom result: (true, travel URL "[EOS:<host id>]", host name) or (false, a message for the player, ""). */
DECLARE_DELEGATE_ThreeParams(FKartRoomFound, bool /*bSuccess*/, const FString& /*UrlOrError*/, const FString& /*HostName*/);

/**
 * Online play through Epic Online Services (EOS), without Epic accounts or login windows.
 * - Login: EOS Connect with a Device ID (created once per Windows user, then reused).
 * - Rooms: the host advertises an EOS session carrying a 5-character room code; friends search for the code.
 * - Transport: EOS P2P (NAT traversal, relays when needed) through the engine's NetDriverEOS. Once logged in,
 *   it replaces IpNetDriver as the GameNetDriver. It passes IP addresses through as plain UDP, so direct IP
 *   keeps working (an IP host adds "?bUseIPSockets" to its listen URL).
 * Off when there are no credentials (Config/Windows/WindowsEngine.ini, see Docs/EOS_SETUP.md), in the editor,
 * with -NoEOS or on a dedicated server: the game then behaves exactly like the direct-IP version.
 *
 * Authority: local (every machine logs itself in). Replicates: nothing; the host's room code reaches the other
 * players through AKartGameState::RoomCode. Client prediction: n/a.
 */
UCLASS()
class SMASHKARTS_API UKartOnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	EKartOnlineState GetState() const { return State; }
	bool IsReady() const { return State == EKartOnlineState::Ready; }

	/** False when online play is off for this build / run (direct IP only). */
	bool IsAvailable() const { return State != EKartOnlineState::Off; }

	/** One line for the main menu ("Online: ready...", "Online: connecting...", ...). */
	FString GetStatusText() const;

	/** Log in again right away (after a failure). Does nothing while connecting / ready / off. */
	void Retry();

	// ---------- Hosting ----------

	/** Advertise a room with a new code for this host. Needs IsReady(). Replaces a room this machine already hosts. */
	void CreateRoom(const FString& HostName, FKartRoomCreated OnDone);

	/** Stop advertising the room (the host left). Safe to call any time. */
	void DestroyRoom();

	/** Code of the room this machine hosts; empty while it's being created and when there is none. */
	const FString& GetRoomCode() const { return bRoomOpen ? RoomCode : EmptyString; }
	bool IsCreatingRoom() const { return bCreatingRoom; }

	/** Why the last CreateRoom failed (empty if it didn't). */
	const FString& GetRoomError() const { return RoomError; }

	// ---------- Joining ----------

	/** Look up a room code. Needs IsReady(). Tries three times, 2 s apart (a new room takes a moment to show up). */
	void FindRoom(const FString& Code, FKartRoomFound OnDone);
	bool IsFindingRoom() const { return bFindingRoom; }

	/** "EOS:<product user id>" of this machine, empty when not logged in. */
	FString GetLocalAddress() const;

	/** Fired on the game thread whenever GetState() changes. */
	FSimpleMulticastDelegate OnStateChanged;

private:
	void SetState(EKartOnlineState NewState, const FString& Detail = FString());
	bool StartPlatform(FString& OutWhyNot);
	void BeginLogin();
	void CreateDeviceId();
	void ConnectLogin(bool bRefresh);
	void HandleLoginFailed(const TCHAR* Step, int32 ResultCode, bool bRefresh);
	void HandleLoggedIn(bool bRefresh);
	void HandleSignedOut();
	void ScheduleRetry();
	bool HandleRetryTick(float DeltaTime);
	void StartSockets();
	void UseEOSNetDriver(bool bUse);
	void AddNotifications();
	void RemoveNotifications();
	void StartRoomSearch(const FString& Code, int32 Attempt, FKartRoomFound OnDone);
	void DestroySessionNamed(const FString& SessionName);
	FString GetLoginDisplayName() const;

	EKartOnlineState State = EKartOnlineState::Off;
	FString StatusDetail;

	FString RoomCode;
	FString RoomSessionName;
	FString RoomError;
	bool bCreatingRoom = false;
	bool bRoomOpen = false;
	bool bFindingRoom = false;

	int32 FailedLogins = 0;
	double RetryAtTime = 0.0;
	FTSTicker::FDelegateHandle RetryHandle;

	/** EOS handles (platform, P2P socket subsystem, notifications). Defined in the .cpp so this header needs no EOS SDK. */
	TSharedPtr<FKartEOSState> EOS;

	static const FString EmptyString;
};
