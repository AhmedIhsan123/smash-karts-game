#include "Game/KartOnlineSubsystem.h"
#include "Game/KartGameInstance.h"
#include "KartRules.h"
#include "SmashKarts.h"
#include "Engine/Engine.h"
#include "GeneralProjectSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_KART_EOS
#include <atomic>
#include "EOSShared.h"
#include "IEOSSDKManager.h"
#include "SocketSubsystemEOS.h"
#include "SocketSubsystemEOSUtils.h"
#include "eos_sdk.h"
#include "eos_connect.h"
#include "eos_sessions.h"
#endif

const FString UKartOnlineSubsystem::EmptyString;

#if WITH_KART_EOS

namespace
{
	/** Credentials live in [EOSSDK.Platform.KartArena] (Config/Windows/WindowsEngine.ini, gitignored). */
	const TCHAR* const PlatformConfigName = TEXT("KartArena");
	const char* const RoomBucket = "KartArena:FFA";
	const char* const AttrRoomCode = "ROOMCODE";
	const char* const AttrGameVersion = "GAMEVER";
	const char* const AttrHostName = "HOSTNAME";
	constexpr uint32 RoomMaxPlayers = 8;
	constexpr int32 RoomSearchAttempts = 3;
	constexpr float RoomSearchRetrySeconds = 2.f;
	const TCHAR* const EOSNetDriverClass = TEXT("/Script/SocketSubsystemEOS.NetDriverEOS");
	const TCHAR* const IpNetDriverClass = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");

	FString GameVersion()
	{
		return GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	}

	FString ProductUserIdToString(EOS_ProductUserId Id)
	{
		char Buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
		int32_t Length = sizeof(Buffer);
		if (Id && EOS_ProductUserId_ToString(Id, Buffer, &Length) == EOS_EResult::EOS_Success)
		{
			return UTF8_TO_TCHAR(Buffer);
		}
		return FString();
	}

	/** Short reason for the menu. */
	FString DescribeResult(EOS_EResult Result)
	{
		switch (Result)
		{
		case EOS_EResult::EOS_NoConnection:
		case EOS_EResult::EOS_TimedOut:
		case EOS_EResult::EOS_ServiceFailure:
			return TEXT("can't reach Epic's servers");
		case EOS_EResult::EOS_InvalidCredentials:
		case EOS_EResult::EOS_InvalidAuth:
		case EOS_EResult::EOS_AccessDenied:
		case EOS_EResult::EOS_MissingPermissions:
		case EOS_EResult::EOS_ClientPolicyMissingAction:
		case EOS_EResult::EOS_NotConfigured:
			return FString::Printf(TEXT("the EOS credentials were rejected (%s), see EOS_SETUP.md"), *LexToString(Result));
		case EOS_EResult::EOS_TooManyRequests:
			return TEXT("too many attempts, waiting");
		default:
			return LexToString(Result);
		}
	}

	FString CopyStringAttribute(EOS_HSessionDetails Details, const char* Key)
	{
		EOS_SessionDetails_CopySessionAttributeByKeyOptions Options = {};
		Options.ApiVersion = EOS_SESSIONDETAILS_COPYSESSIONATTRIBUTEBYKEY_API_LATEST;
		Options.AttrKey = Key;
		EOS_SessionDetails_Attribute* Attribute = nullptr;
		FString Out;
		if (EOS_SessionDetails_CopySessionAttributeByKey(Details, &Options, &Attribute) == EOS_EResult::EOS_Success && Attribute)
		{
			if (Attribute->Data && Attribute->Data->ValueType == EOS_EAttributeType::EOS_AT_STRING && Attribute->Data->Value.AsUtf8)
			{
				Out = UTF8_TO_TCHAR(Attribute->Data->Value.AsUtf8);
			}
			EOS_SessionDetails_Attribute_Release(Attribute);
		}
		return Out;
	}

	bool AddStringAttribute(EOS_HSessionModification Modification, const char* Key, const FString& Value)
	{
		const FTCHARToUTF8 ValueUtf8(*Value.Left(60));
		EOS_Sessions_AttributeData Data = {};
		Data.ApiVersion = EOS_SESSIONS_ATTRIBUTEDATA_API_LATEST;
		Data.Key = Key;
		Data.Value.AsUtf8 = ValueUtf8.Get();
		Data.ValueType = EOS_EAttributeType::EOS_AT_STRING;

		EOS_SessionModification_AddAttributeOptions Options = {};
		Options.ApiVersion = EOS_SESSIONMODIFICATION_ADDATTRIBUTE_API_LATEST;
		Options.SessionAttribute = &Data;
		Options.AdvertisementType = EOS_ESessionAttributeAdvertisementType::EOS_SAAT_Advertise;
		const EOS_EResult Result = EOS_SessionModification_AddAttribute(Modification, &Options);
		UE_CLOG(Result != EOS_EResult::EOS_Success, LogSmashKarts, Warning, TEXT("[Online] Room attribute %hs: %s"), Key, *LexToString(Result));
		return Result == EOS_EResult::EOS_Success;
	}

	bool AddStringSearchParameter(EOS_HSessionSearch Search, const char* Key, const char* ValueUtf8)
	{
		EOS_Sessions_AttributeData Data = {};
		Data.ApiVersion = EOS_SESSIONS_ATTRIBUTEDATA_API_LATEST;
		Data.Key = Key;
		Data.Value.AsUtf8 = ValueUtf8;
		Data.ValueType = EOS_EAttributeType::EOS_AT_STRING;

		EOS_SessionSearch_SetParameterOptions Options = {};
		Options.ApiVersion = EOS_SESSIONSEARCH_SETPARAMETER_API_LATEST;
		Options.Parameter = &Data;
		Options.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;
		const EOS_EResult Result = EOS_SessionSearch_SetParameter(Search, &Options);
		UE_CLOG(Result != EOS_EResult::EOS_Success, LogSmashKarts, Warning, TEXT("[Online] Search parameter %hs: %s"), Key, *LexToString(Result));
		return Result == EOS_EResult::EOS_Success;
	}

	/**
	 * Heap context for one async EOS call. The SDK calls Callback on the game thread (inside EOS_Platform_Tick);
	 * Then runs only if the subsystem still exists. Some operations report EOS_OperationWillRetry first and
	 * call again later, so the context is freed only when the operation is complete.
	 */
	template <typename TInfo>
	struct TKartEOSCall
	{
		TWeakObjectPtr<UKartOnlineSubsystem> Owner;
		TFunction<void(UKartOnlineSubsystem&, const TInfo&)> Then;

		static void* Make(UKartOnlineSubsystem* InOwner, TFunction<void(UKartOnlineSubsystem&, const TInfo&)>&& InThen)
		{
			return new TKartEOSCall{ InOwner, MoveTemp(InThen) };
		}

		static void EOS_CALL Callback(const TInfo* Data)
		{
			if (!EOS_EResult_IsOperationComplete(Data->ResultCode))
			{
				return;
			}
			TKartEOSCall* Call = static_cast<TKartEOSCall*>(Data->ClientData);
			if (UKartOnlineSubsystem* Subsystem = Call->Owner.Get())
			{
				Call->Then(*Subsystem, *Data);
			}
			delete Call;
		}
	};

	/** The logged-in product user, shared with the socket utils (read from the net driver). */
	struct FKartEOSLocalUser
	{
		std::atomic<EOS_ProductUserId> Id{ nullptr };
	};

	/** What FSocketSubsystemEOS needs from "the online subsystem": who is logged in. */
	class FKartEOSSocketUtils : public ISocketSubsystemEOSUtils
	{
	public:
		explicit FKartEOSSocketUtils(const TSharedRef<FKartEOSLocalUser, ESPMode::ThreadSafe>& InUser) : User(InUser) {}

		virtual EOS_ProductUserId GetLocalUserId() override { return User->Id.load(); }
		virtual FString GetSessionId() override { return FString(); }
		/** NAME_None: GetWorldForOnline() then means "the game world", which is what a standalone game has. */
		virtual FName GetSubsystemInstanceName() override { return NAME_None; }
		virtual bool IsLoggedIn() override { return User->Id.load() != nullptr; }

	private:
		TSharedRef<FKartEOSLocalUser, ESPMode::ThreadSafe> User;
	};
}

struct FKartEOSState
{
	IEOSPlatformHandlePtr Platform;
	EOS_HConnect Connect = nullptr;
	EOS_HSessions Sessions = nullptr;
	TSharedRef<FKartEOSLocalUser, ESPMode::ThreadSafe> User = MakeShared<FKartEOSLocalUser, ESPMode::ThreadSafe>();
	TSharedPtr<FSocketSubsystemEOS, ESPMode::ThreadSafe> Sockets;
	EOS_NotificationId AuthExpiryNotify = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId LoginStatusNotify = EOS_INVALID_NOTIFICATIONID;
	FName SavedDriverClass;
	FName SavedDriverFallback;
	bool bUsingEOSDriver = false;
	bool bRefreshing = false;
	bool bReplacedDeviceId = false;
};

#else // !WITH_KART_EOS

struct FKartEOSState
{
};

#endif // WITH_KART_EOS

// =====================================================================
// Lifetime
// =====================================================================

void UKartOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EOS = MakeShared<FKartEOSState>();

	FString WhyNot;
	if (!StartPlatform(WhyNot))
	{
		SetState(EKartOnlineState::Off, WhyNot);
		UE_LOG(LogSmashKarts, Log, TEXT("[Online] Off: %s. Direct IP only."), *WhyNot);
		return;
	}
	BeginLogin();
}

void UKartOnlineSubsystem::Deinitialize()
{
	if (RetryHandle.IsValid())
	{
		FTSTicker::RemoveTicker(RetryHandle);
		RetryHandle.Reset();
	}
#if WITH_KART_EOS
	DestroyRoom();
	RemoveNotifications();
	UseEOSNetDriver(false);
	if (EOS->Sockets.IsValid())
	{
		// The world's net driver is already gone (engine exit and map changes shut it down first)
		EOS->Sockets->Shutdown();
		EOS->Sockets.Reset();
	}
	EOS->User->Id = nullptr;
	EOS->Platform.Reset(); // the SDK manager releases the platform
#endif
	EOS.Reset();
	Super::Deinitialize();
}

void UKartOnlineSubsystem::SetState(EKartOnlineState NewState, const FString& Detail)
{
	const bool bChanged = State != NewState || StatusDetail != Detail;
	State = NewState;
	StatusDetail = Detail;
	if (bChanged)
	{
		OnStateChanged.Broadcast();
	}
}

FString UKartOnlineSubsystem::GetStatusText() const
{
	switch (State)
	{
	case EKartOnlineState::Connecting:
		return TEXT("Online: connecting to Epic Online Services...");
	case EKartOnlineState::Ready:
		return TEXT("Online: ready. Host Online gives you a room code; friends type it next to Join Game.");
	case EKartOnlineState::Failed:
	{
		const int32 Seconds = FMath::Max(0, FMath::CeilToInt(RetryAtTime - FPlatformTime::Seconds()));
		return FString::Printf(TEXT("Online: unavailable (%s). Retrying in %d s. Direct IP still works."), *StatusDetail, Seconds);
	}
	case EKartOnlineState::Off:
	default:
		return FString::Printf(TEXT("Online play is off (%s). Direct IP on UDP port 7777, see HOSTING.md."), *StatusDetail);
	}
}

FString UKartOnlineSubsystem::GetLoginDisplayName() const
{
	const UKartGameInstance* GI = Cast<UKartGameInstance>(GetGameInstance());
	const FString Name = GI ? GI->GetPlayerName() : FString();
	return Name.IsEmpty() ? FString(TEXT("Player")) : Name.Left(32);
}

#if WITH_KART_EOS

// =====================================================================
// Platform + login (Device ID)
// =====================================================================

bool UKartOnlineSubsystem::StartPlatform(FString& OutWhyNot)
{
	if (GIsEditor)
	{
		OutWhyNot = TEXT("editor; play a standalone game for room codes");
		return false;
	}
	if (IsRunningDedicatedServer() || IsRunningCommandlet())
	{
		OutWhyNot = TEXT("server");
		return false;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NoEOS")))
	{
		OutWhyNot = TEXT("-NoEOS");
		return false;
	}
	IEOSSDKManager* Manager = IEOSSDKManager::Get();
	if (!Manager || !Manager->IsInitialized())
	{
		OutWhyNot = TEXT("EOS SDK not loaded");
		return false;
	}

	const FString Section = FString(TEXT("EOSSDK.Platform.")) + PlatformConfigName;
	const FEOSSDKPlatformConfig* Loaded = GConfig->DoesSectionExist(*Section, GEngineIni) ? Manager->GetPlatformConfig(PlatformConfigName, true) : nullptr;
	if (!Loaded)
	{
		OutWhyNot = TEXT("not set up in this build");
		return false;
	}
	FEOSSDKPlatformConfig Config = *Loaded;
	for (const FString* Value : { &Config.ProductId, &Config.SandboxId, &Config.DeploymentId, &Config.ClientId, &Config.ClientSecret })
	{
		if (Value->IsEmpty() || Value->Contains(TEXT("PASTE")))
		{
			OutWhyNot = TEXT("EOS credentials incomplete, see EOS_SETUP.md");
			return false;
		}
	}

	// A game client without Epic accounts: no overlay, no voice
	Config.bIsServer = false;
	Config.bLoadingInEditor = false;
	Config.bDisableOverlay = true;
	Config.bDisableSocialOverlay = true;
	Config.bEnableRTC = false;
	Manager->AddPlatformConfig(Config, true);

	EOS->Platform = Manager->CreatePlatform(PlatformConfigName);
	if (!EOS->Platform.IsValid())
	{
		OutWhyNot = TEXT("EOS didn't start, see the log");
		return false;
	}
	EOS->Connect = EOS_Platform_GetConnectInterface(*EOS->Platform);
	EOS->Sessions = EOS_Platform_GetSessionsInterface(*EOS->Platform);
	if (!EOS->Connect || !EOS->Sessions)
	{
		OutWhyNot = TEXT("EOS interfaces missing");
		EOS->Platform.Reset();
		return false;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Online] EOS platform started (SDK %s)"), *Manager->GetSDKVersion());
	return true;
}

void UKartOnlineSubsystem::BeginLogin()
{
	SetState(EKartOnlineState::Connecting);
	UE_LOG(LogSmashKarts, Log, TEXT("[Online] Logging in (device ID)"));

#if !UE_BUILD_SHIPPING
	// Testing two copies on one PC: the device ID belongs to the Windows user, so both copies would be the same
	// EOS user. -EOSNewDevice replaces it with a new one (new product user) for this copy. Dev builds only.
	if (!EOS->bReplacedDeviceId && FParse::Param(FCommandLine::Get(), TEXT("EOSNewDevice")))
	{
		EOS->bReplacedDeviceId = true;
		EOS_Connect_DeleteDeviceIdOptions DeleteOptions = {};
		DeleteOptions.ApiVersion = EOS_CONNECT_DELETEDEVICEID_API_LATEST;
		using FDeleteCall = TKartEOSCall<EOS_Connect_DeleteDeviceIdCallbackInfo>;
		EOS_Connect_DeleteDeviceId(EOS->Connect, &DeleteOptions, FDeleteCall::Make(this, [](UKartOnlineSubsystem& Self, const EOS_Connect_DeleteDeviceIdCallbackInfo& Info)
		{
			UE_LOG(LogSmashKarts, Log, TEXT("[Online] -EOSNewDevice: old device ID deleted (%s)"), *LexToString(Info.ResultCode));
			Self.CreateDeviceId();
		}), &FDeleteCall::Callback);
		return;
	}
#endif
	CreateDeviceId();
}

void UKartOnlineSubsystem::CreateDeviceId()
{
	// One-time per Windows user; "already exists" is the normal case after the first run
	EOS_Connect_CreateDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	Options.DeviceModel = "PC Windows";
	using FCall = TKartEOSCall<EOS_Connect_CreateDeviceIdCallbackInfo>;
	EOS_Connect_CreateDeviceId(EOS->Connect, &Options, FCall::Make(this, [](UKartOnlineSubsystem& Self, const EOS_Connect_CreateDeviceIdCallbackInfo& Info)
	{
		if (Info.ResultCode == EOS_EResult::EOS_Success || Info.ResultCode == EOS_EResult::EOS_DuplicateNotAllowed)
		{
			Self.ConnectLogin(false);
		}
		else
		{
			Self.HandleLoginFailed(TEXT("device ID"), static_cast<int32>(Info.ResultCode), false);
		}
	}), &FCall::Callback);
}

void UKartOnlineSubsystem::ConnectLogin(bool bRefresh)
{
	if (!EOS.IsValid() || !EOS->Connect)
	{
		return;
	}
	EOS->bRefreshing = bRefresh;

	EOS_Connect_Credentials Credentials = {};
	Credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
	Credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
	Credentials.Token = nullptr;

	// Required for Device ID logins; informational only
	const FTCHARToUTF8 NameUtf8(*GetLoginDisplayName());
	EOS_Connect_UserLoginInfo UserInfo = {};
	UserInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
	UserInfo.DisplayName = NameUtf8.Get();

	EOS_Connect_LoginOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
	Options.Credentials = &Credentials;
	Options.UserLoginInfo = &UserInfo;

	using FCall = TKartEOSCall<EOS_Connect_LoginCallbackInfo>;
	EOS_Connect_Login(EOS->Connect, &Options, FCall::Make(this, [bRefresh](UKartOnlineSubsystem& Self, const EOS_Connect_LoginCallbackInfo& Info)
	{
		if (Info.ResultCode == EOS_EResult::EOS_Success)
		{
			Self.EOS->User->Id = Info.LocalUserId;
			Self.HandleLoggedIn(bRefresh);
		}
		else if (Info.ResultCode == EOS_EResult::EOS_InvalidUser && Info.ContinuanceToken)
		{
			// First login with this device: create the product user
			EOS_Connect_CreateUserOptions CreateOptions = {};
			CreateOptions.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
			CreateOptions.ContinuanceToken = Info.ContinuanceToken;
			using FCreateCall = TKartEOSCall<EOS_Connect_CreateUserCallbackInfo>;
			EOS_Connect_CreateUser(Self.EOS->Connect, &CreateOptions, FCreateCall::Make(&Self, [bRefresh](UKartOnlineSubsystem& Inner, const EOS_Connect_CreateUserCallbackInfo& CreateInfo)
			{
				if (CreateInfo.ResultCode == EOS_EResult::EOS_Success)
				{
					Inner.EOS->User->Id = CreateInfo.LocalUserId;
					UE_LOG(LogSmashKarts, Log, TEXT("[Online] Created a new product user for this device"));
					Inner.HandleLoggedIn(bRefresh);
				}
				else
				{
					Inner.HandleLoginFailed(TEXT("create user"), static_cast<int32>(CreateInfo.ResultCode), bRefresh);
				}
			}), &FCreateCall::Callback);
		}
		else
		{
			Self.HandleLoginFailed(TEXT("login"), static_cast<int32>(Info.ResultCode), bRefresh);
		}
	}), &FCall::Callback);
}

void UKartOnlineSubsystem::HandleLoggedIn(bool bRefresh)
{
	FailedLogins = 0;
	EOS->bRefreshing = false;
	if (bRefresh)
	{
		UE_LOG(LogSmashKarts, Log, TEXT("[Online] Login refreshed"));
		return;
	}
	StartSockets();
	UseEOSNetDriver(EOS->Sockets.IsValid());
	AddNotifications();
	UE_LOG(LogSmashKarts, Log, TEXT("[Online] Logged in as %s. Room codes ready."), *GetLocalAddress());
	SetState(EKartOnlineState::Ready);
}

void UKartOnlineSubsystem::HandleLoginFailed(const TCHAR* Step, int32 ResultCode, bool bRefresh)
{
	const EOS_EResult Result = static_cast<EOS_EResult>(ResultCode);
	EOS->bRefreshing = false;
	UE_LOG(LogSmashKarts, Warning, TEXT("[Online] %s failed at %s: %s"), bRefresh ? TEXT("Login refresh") : TEXT("Login"), Step, *LexToString(Result));
	if (bRefresh)
	{
		return; // still logged in until the token runs out; the login-status notification handles that
	}
	++FailedLogins;
	SetState(EKartOnlineState::Failed, DescribeResult(Result));
	ScheduleRetry();
}

void UKartOnlineSubsystem::HandleSignedOut()
{
	UE_LOG(LogSmashKarts, Warning, TEXT("[Online] Signed out of EOS; back to direct IP until the login comes back"));
	EOS->User->Id = nullptr;
	UseEOSNetDriver(false);
	bRoomOpen = false;
	++FailedLogins;
	SetState(EKartOnlineState::Failed, TEXT("signed out"));
	ScheduleRetry();
}

void UKartOnlineSubsystem::ScheduleRetry()
{
	if (RetryHandle.IsValid())
	{
		FTSTicker::RemoveTicker(RetryHandle);
	}
	const float Delay = FailedLogins <= 1 ? 10.f : (FailedLogins == 2 ? 30.f : 60.f);
	RetryAtTime = FPlatformTime::Seconds() + Delay;
	RetryHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UKartOnlineSubsystem::HandleRetryTick), Delay);
}

bool UKartOnlineSubsystem::HandleRetryTick(float DeltaTime)
{
	RetryHandle.Reset();
	if (State == EKartOnlineState::Failed)
	{
		BeginLogin();
	}
	return false; // once
}

void UKartOnlineSubsystem::Retry()
{
	if (State != EKartOnlineState::Failed)
	{
		return;
	}
	if (RetryHandle.IsValid())
	{
		FTSTicker::RemoveTicker(RetryHandle);
		RetryHandle.Reset();
	}
	BeginLogin();
}

void UKartOnlineSubsystem::AddNotifications()
{
	if (EOS->AuthExpiryNotify == EOS_INVALID_NOTIFICATIONID)
	{
		// ~10 minutes before the login token expires: log in again with the same device ID
		EOS_Connect_AddNotifyAuthExpirationOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST;
		EOS->AuthExpiryNotify = EOS_Connect_AddNotifyAuthExpiration(EOS->Connect, &Options, this,
			[](const EOS_Connect_AuthExpirationCallbackInfo* Data)
			{
				if (UKartOnlineSubsystem* Self = static_cast<UKartOnlineSubsystem*>(Data->ClientData))
				{
					UE_LOG(LogSmashKarts, Log, TEXT("[Online] Login token expiring, refreshing"));
					Self->ConnectLogin(true);
				}
			});
	}
	if (EOS->LoginStatusNotify == EOS_INVALID_NOTIFICATIONID)
	{
		EOS_Connect_AddNotifyLoginStatusChangedOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST;
		EOS->LoginStatusNotify = EOS_Connect_AddNotifyLoginStatusChanged(EOS->Connect, &Options, this,
			[](const EOS_Connect_LoginStatusChangedCallbackInfo* Data)
			{
				UKartOnlineSubsystem* Self = static_cast<UKartOnlineSubsystem*>(Data->ClientData);
				if (Self && Data->CurrentStatus == EOS_ELoginStatus::EOS_LS_NotLoggedIn && Self->State == EKartOnlineState::Ready)
				{
					Self->HandleSignedOut();
				}
			});
	}
}

void UKartOnlineSubsystem::RemoveNotifications()
{
	if (!EOS.IsValid() || !EOS->Connect)
	{
		return;
	}
	if (EOS->AuthExpiryNotify != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_Connect_RemoveNotifyAuthExpiration(EOS->Connect, EOS->AuthExpiryNotify);
		EOS->AuthExpiryNotify = EOS_INVALID_NOTIFICATIONID;
	}
	if (EOS->LoginStatusNotify != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_Connect_RemoveNotifyLoginStatusChanged(EOS->Connect, EOS->LoginStatusNotify);
		EOS->LoginStatusNotify = EOS_INVALID_NOTIFICATIONID;
	}
}

// =====================================================================
// P2P transport
// =====================================================================

void UKartOnlineSubsystem::StartSockets()
{
	if (EOS->Sockets.IsValid())
	{
		return;
	}
	TSharedPtr<FSocketSubsystemEOS, ESPMode::ThreadSafe> Sockets = MakeShared<FSocketSubsystemEOS, ESPMode::ThreadSafe>(
		EOS->Platform, MakeShared<FKartEOSSocketUtils, ESPMode::ThreadSafe>(EOS->User));
	FString Error;
	if (!Sockets->Init(Error))
	{
		UE_LOG(LogSmashKarts, Warning, TEXT("[Online] EOS P2P sockets failed to start: %s"), *Error);
		return;
	}
	EOS->Sockets = Sockets;
}

void UKartOnlineSubsystem::UseEOSNetDriver(bool bUse)
{
	if (!GEngine || !EOS.IsValid() || EOS->bUsingEOSDriver == bUse)
	{
		return;
	}
	for (FNetDriverDefinition& Definition : GEngine->NetDriverDefinitions)
	{
		if (Definition.DefName != NAME_GameNetDriver)
		{
			continue;
		}
		if (bUse)
		{
			EOS->SavedDriverClass = Definition.DriverClassName;
			EOS->SavedDriverFallback = Definition.DriverClassNameFallback;
			Definition.DriverClassName = FName(EOSNetDriverClass);
			Definition.DriverClassNameFallback = FName(IpNetDriverClass);
		}
		else
		{
			Definition.DriverClassName = EOS->SavedDriverClass;
			Definition.DriverClassNameFallback = EOS->SavedDriverFallback;
		}
	}
	EOS->bUsingEOSDriver = bUse;
	UE_LOG(LogSmashKarts, Log, TEXT("[Online] Game net driver: %s"), bUse ? TEXT("EOS P2P (IP addresses pass through)") : TEXT("IP"));
}

FString UKartOnlineSubsystem::GetLocalAddress() const
{
	const FString Id = EOS.IsValid() ? ProductUserIdToString(EOS->User->Id.load()) : FString();
	return Id.IsEmpty() ? FString() : FString(TEXT("EOS:")) + Id;
}

// =====================================================================
// Rooms (EOS sessions with a room code attribute)
// =====================================================================

void UKartOnlineSubsystem::CreateRoom(const FString& HostName, FKartRoomCreated OnDone)
{
	if (!IsReady())
	{
		OnDone.ExecuteIfBound(false, TEXT("Online play isn't connected."));
		return;
	}
	DestroyRoom();

	// FGuid is random (CoCreateGuid); one number per character
	const FGuid Random = FGuid::NewGuid();
	RoomCode = KartRules::MakeRoomCode({ static_cast<int32>(Random.A), static_cast<int32>(Random.B), static_cast<int32>(Random.C),
		static_cast<int32>(Random.D), static_cast<int32>(Random.A ^ Random.D) });
	RoomSessionName = FString(TEXT("KartRoom_")) + RoomCode;
	RoomError.Reset();
	bCreatingRoom = true;
	bRoomOpen = false;

	const FTCHARToUTF8 SessionNameUtf8(*RoomSessionName);
	EOS_Sessions_CreateSessionModificationOptions CreateOptions = {};
	CreateOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST;
	CreateOptions.SessionName = SessionNameUtf8.Get();
	CreateOptions.BucketId = RoomBucket;
	CreateOptions.MaxPlayers = RoomMaxPlayers;
	CreateOptions.LocalUserId = EOS->User->Id.load();
	CreateOptions.bPresenceEnabled = EOS_FALSE;
	CreateOptions.SessionId = nullptr;
	CreateOptions.bSanctionsEnabled = EOS_FALSE;

	EOS_HSessionModification Modification = nullptr;
	const EOS_EResult CreateResult = EOS_Sessions_CreateSessionModification(EOS->Sessions, &CreateOptions, &Modification);
	if (CreateResult != EOS_EResult::EOS_Success || !Modification)
	{
		bCreatingRoom = false;
		RoomError = FString::Printf(TEXT("Couldn't create the online room (%s)."), *LexToString(CreateResult));
		RoomCode.Reset();
		RoomSessionName.Reset();
		OnDone.ExecuteIfBound(false, RoomError);
		return;
	}

	// Friends connect to "EOS:<our product user id>"
	const FTCHARToUTF8 AddressUtf8(*GetLocalAddress());
	EOS_SessionModification_SetHostAddressOptions HostOptions = {};
	HostOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETHOSTADDRESS_API_LATEST;
	HostOptions.HostAddress = AddressUtf8.Get();
	EOS_SessionModification_SetHostAddress(Modification, &HostOptions);

	EOS_SessionModification_SetPermissionLevelOptions PermissionOptions = {};
	PermissionOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETPERMISSIONLEVEL_API_LATEST;
	PermissionOptions.PermissionLevel = EOS_EOnlineSessionPermissionLevel::EOS_OSPF_PublicAdvertised;
	EOS_SessionModification_SetPermissionLevel(Modification, &PermissionOptions);

	EOS_SessionModification_SetJoinInProgressAllowedOptions JoinOptions = {};
	JoinOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST;
	JoinOptions.bAllowJoinInProgress = EOS_TRUE;
	EOS_SessionModification_SetJoinInProgressAllowed(Modification, &JoinOptions);

	AddStringAttribute(Modification, AttrRoomCode, RoomCode);
	AddStringAttribute(Modification, AttrGameVersion, GameVersion());
	AddStringAttribute(Modification, AttrHostName, HostName.IsEmpty() ? FString(TEXT("Host")) : HostName);

	EOS_Sessions_UpdateSessionOptions UpdateOptions = {};
	UpdateOptions.ApiVersion = EOS_SESSIONS_UPDATESESSION_API_LATEST;
	UpdateOptions.SessionModificationHandle = Modification;
	using FCall = TKartEOSCall<EOS_Sessions_UpdateSessionCallbackInfo>;
	EOS_Sessions_UpdateSession(EOS->Sessions, &UpdateOptions, FCall::Make(this, [Code = RoomCode, OnDone](UKartOnlineSubsystem& Self, const EOS_Sessions_UpdateSessionCallbackInfo& Info)
	{
		if (Code != Self.RoomCode)
		{
			// The host left (or re-hosted) while this room was being created: take it down again
			if (Info.ResultCode == EOS_EResult::EOS_Success)
			{
				Self.DestroySessionNamed(FString(TEXT("KartRoom_")) + Code);
			}
			return;
		}
		Self.bCreatingRoom = false;
		if (Info.ResultCode == EOS_EResult::EOS_Success)
		{
			Self.bRoomOpen = true;
			UE_LOG(LogSmashKarts, Log, TEXT("[Online] Room %s is open (session %hs)"), *Code, Info.SessionId ? Info.SessionId : "?");
#if !UE_BUILD_SHIPPING
			// For SK.JoinLastRoom (a second copy on this PC joins without anyone typing the code)
			FFileHelper::SaveStringToFile(Code, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LastRoomCode.txt")));
#endif
			OnDone.ExecuteIfBound(true, Code);
		}
		else
		{
			Self.RoomError = FString::Printf(TEXT("Couldn't create the online room (%s)."), *DescribeResult(Info.ResultCode));
			UE_LOG(LogSmashKarts, Warning, TEXT("[Online] Room %s failed: %s"), *Code, *LexToString(Info.ResultCode));
			Self.RoomCode.Reset();
			Self.RoomSessionName.Reset();
			OnDone.ExecuteIfBound(false, Self.RoomError);
		}
	}), &FCall::Callback);
	EOS_SessionModification_Release(Modification);
}

void UKartOnlineSubsystem::DestroyRoom()
{
	if (RoomSessionName.IsEmpty())
	{
		return;
	}
	// Still being created: the create callback sees the code changed and destroys it when it lands
	if (!bCreatingRoom)
	{
		DestroySessionNamed(RoomSessionName);
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Online] Closing room %s"), *RoomCode);
	RoomCode.Reset();
	RoomSessionName.Reset();
	bCreatingRoom = false;
	bRoomOpen = false;
}

void UKartOnlineSubsystem::DestroySessionNamed(const FString& SessionName)
{
	if (!EOS.IsValid() || !EOS->Sessions)
	{
		return;
	}
	const FTCHARToUTF8 NameUtf8(*SessionName);
	EOS_Sessions_DestroySessionOptions Options = {};
	Options.ApiVersion = EOS_SESSIONS_DESTROYSESSION_API_LATEST;
	Options.SessionName = NameUtf8.Get();
	using FCall = TKartEOSCall<EOS_Sessions_DestroySessionCallbackInfo>;
	EOS_Sessions_DestroySession(EOS->Sessions, &Options, FCall::Make(this, [SessionName](UKartOnlineSubsystem& Self, const EOS_Sessions_DestroySessionCallbackInfo& Info)
	{
		UE_LOG(LogSmashKarts, Log, TEXT("[Online] Session %s closed: %s"), *SessionName, *LexToString(Info.ResultCode));
	}), &FCall::Callback);
}

void UKartOnlineSubsystem::FindRoom(const FString& Code, FKartRoomFound OnDone)
{
	if (!IsReady())
	{
		OnDone.ExecuteIfBound(false, TEXT("Online play isn't connected."), FString());
		return;
	}
	bFindingRoom = true;
	StartRoomSearch(KartRules::NormalizeRoomCode(Code), 1, OnDone);
}

void UKartOnlineSubsystem::StartRoomSearch(const FString& Code, int32 Attempt, FKartRoomFound OnDone)
{
	auto Fail = [this, &OnDone](const FString& Message)
	{
		bFindingRoom = false;
		OnDone.ExecuteIfBound(false, Message, FString());
	};
	if (!IsReady())
	{
		Fail(TEXT("Online play disconnected."));
		return;
	}

	EOS_Sessions_CreateSessionSearchOptions SearchOptions = {};
	SearchOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONSEARCH_API_LATEST;
	SearchOptions.MaxSearchResults = 10;
	EOS_HSessionSearch Search = nullptr;
	const EOS_EResult CreateResult = EOS_Sessions_CreateSessionSearch(EOS->Sessions, &SearchOptions, &Search);
	if (CreateResult != EOS_EResult::EOS_Success || !Search)
	{
		Fail(FString::Printf(TEXT("Couldn't search for games (%s)."), *LexToString(CreateResult)));
		return;
	}
	const FTCHARToUTF8 CodeUtf8(*Code);
	AddStringSearchParameter(Search, EOS_SESSIONS_SEARCH_BUCKET_ID, RoomBucket);
	AddStringSearchParameter(Search, AttrRoomCode, CodeUtf8.Get());

	UE_LOG(LogSmashKarts, Log, TEXT("[Online] Looking for room %s (try %d)"), *Code, Attempt);
	EOS_SessionSearch_FindOptions FindOptions = {};
	FindOptions.ApiVersion = EOS_SESSIONSEARCH_FIND_API_LATEST;
	FindOptions.LocalUserId = EOS->User->Id.load();
	using FCall = TKartEOSCall<EOS_SessionSearch_FindCallbackInfo>;
	EOS_SessionSearch_Find(Search, &FindOptions, FCall::Make(this, [Search, Code, Attempt, OnDone](UKartOnlineSubsystem& Self, const EOS_SessionSearch_FindCallbackInfo& Info)
	{
		struct FFound
		{
			FString Address;
			FString Version;
			FString HostName;
		};
		TArray<FFound> Found;
		if (Info.ResultCode == EOS_EResult::EOS_Success)
		{
			EOS_SessionSearch_GetSearchResultCountOptions CountOptions = {};
			CountOptions.ApiVersion = EOS_SESSIONSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
			const uint32 Count = EOS_SessionSearch_GetSearchResultCount(Search, &CountOptions);
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				EOS_SessionSearch_CopySearchResultByIndexOptions IndexOptions = {};
				IndexOptions.ApiVersion = EOS_SESSIONSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
				IndexOptions.SessionIndex = Index;
				EOS_HSessionDetails Details = nullptr;
				if (EOS_SessionSearch_CopySearchResultByIndex(Search, &IndexOptions, &Details) != EOS_EResult::EOS_Success || !Details)
				{
					continue;
				}
				FFound Entry;
				EOS_SessionDetails_CopyInfoOptions InfoOptions = {};
				InfoOptions.ApiVersion = EOS_SESSIONDETAILS_COPYINFO_API_LATEST;
				EOS_SessionDetails_Info* SessionInfo = nullptr;
				if (EOS_SessionDetails_CopyInfo(Details, &InfoOptions, &SessionInfo) == EOS_EResult::EOS_Success && SessionInfo)
				{
					Entry.Address = SessionInfo->HostAddress ? FString(UTF8_TO_TCHAR(SessionInfo->HostAddress)) : FString();
					if (!Entry.Address.StartsWith(TEXT("EOS:")))
					{
						// Fall back to the owner's id (the host address should always be ours, but be safe)
						const FString Owner = ProductUserIdToString(SessionInfo->OwnerUserId);
						Entry.Address = Owner.IsEmpty() ? FString() : FString(TEXT("EOS:")) + Owner;
					}
					EOS_SessionDetails_Info_Release(SessionInfo);
				}
				Entry.Version = CopyStringAttribute(Details, AttrGameVersion);
				Entry.HostName = CopyStringAttribute(Details, AttrHostName);
				EOS_SessionDetails_Release(Details);
				if (!Entry.Address.IsEmpty())
				{
					Found.Add(Entry);
				}
			}
		}
		EOS_SessionSearch_Release(Search);

		const bool bSearchWorked = Info.ResultCode == EOS_EResult::EOS_Success || Info.ResultCode == EOS_EResult::EOS_NotFound;
		if (Found.IsEmpty() && bSearchWorked && Attempt < RoomSearchAttempts)
		{
			// A room created a moment ago may not be searchable yet
			UKartOnlineSubsystem* SelfPtr = &Self;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(SelfPtr, [SelfPtr, Code, Attempt, OnDone](float)
			{
				SelfPtr->StartRoomSearch(Code, Attempt + 1, OnDone);
				return false;
			}), RoomSearchRetrySeconds);
			return;
		}

		Self.bFindingRoom = false;
		if (Found.IsEmpty())
		{
			const FString Message = bSearchWorked
				? FString::Printf(TEXT("No game found with code %s. Check the code with the host (it's in their lobby)."), *Code)
				: FString::Printf(TEXT("Couldn't search for games (%s)."), *DescribeResult(Info.ResultCode));
			UE_LOG(LogSmashKarts, Log, TEXT("[Online] Room %s not found (%s)"), *Code, *LexToString(Info.ResultCode));
			OnDone.ExecuteIfBound(false, Message, FString());
			return;
		}

		const FString MyVersion = GameVersion();
		const FFound* Best = Found.FindByPredicate([&MyVersion](const FFound& Entry) { return Entry.Version == MyVersion; });
		if (!Best)
		{
			const FString Message = FString::Printf(TEXT("That game runs version %s and you have %s. Everyone needs the same build."),
				Found[0].Version.IsEmpty() ? TEXT("?") : *Found[0].Version, *MyVersion);
			OnDone.ExecuteIfBound(false, Message, FString());
			return;
		}
		if (Best->Address == Self.GetLocalAddress())
		{
			OnDone.ExecuteIfBound(false, TEXT("That's your own game."), FString());
			return;
		}
		UE_LOG(LogSmashKarts, Log, TEXT("[Online] Room %s found: %s hosts at %s"), *Code, *Best->HostName, *Best->Address);
		// Brackets keep FURL from reading "EOS:" as a protocol
		OnDone.ExecuteIfBound(true, FString::Printf(TEXT("[%s]"), *Best->Address), Best->HostName);
	}), &FCall::Callback);
}

#else // !WITH_KART_EOS

bool UKartOnlineSubsystem::StartPlatform(FString& OutWhyNot)
{
	OutWhyNot = TEXT("not in this build");
	return false;
}
void UKartOnlineSubsystem::BeginLogin() {}
void UKartOnlineSubsystem::CreateDeviceId() {}
void UKartOnlineSubsystem::ConnectLogin(bool) {}
void UKartOnlineSubsystem::HandleLoginFailed(const TCHAR*, int32, bool) {}
void UKartOnlineSubsystem::HandleLoggedIn(bool) {}
void UKartOnlineSubsystem::HandleSignedOut() {}
void UKartOnlineSubsystem::ScheduleRetry() {}
bool UKartOnlineSubsystem::HandleRetryTick(float) { return false; }
void UKartOnlineSubsystem::Retry() {}
void UKartOnlineSubsystem::AddNotifications() {}
void UKartOnlineSubsystem::RemoveNotifications() {}
void UKartOnlineSubsystem::StartSockets() {}
void UKartOnlineSubsystem::UseEOSNetDriver(bool) {}
FString UKartOnlineSubsystem::GetLocalAddress() const { return FString(); }
void UKartOnlineSubsystem::CreateRoom(const FString&, FKartRoomCreated OnDone) { OnDone.ExecuteIfBound(false, TEXT("Online play isn't in this build.")); }
void UKartOnlineSubsystem::DestroyRoom() {}
void UKartOnlineSubsystem::DestroySessionNamed(const FString&) {}
void UKartOnlineSubsystem::FindRoom(const FString&, FKartRoomFound OnDone) { OnDone.ExecuteIfBound(false, TEXT("Online play isn't in this build."), FString()); }
void UKartOnlineSubsystem::StartRoomSearch(const FString&, int32, FKartRoomFound) {}

#endif // WITH_KART_EOS
