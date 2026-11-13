#include "Game/KartPlayerController.h"
#include "Game/KartGameMode.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerState.h"
#include "Debug/KartAutoTest.h"
#include "KartCharacter.h"
#include "KartHealthComponent.h"
#include "KartStatusComponent.h"
#include "KartWeaponComponent.h"
#include "KartWeaponData.h"
#include "SmashKarts.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/DamageType.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"

AKartPlayerController::AKartPlayerController()
{
}

bool AKartPlayerController::IsLocalHost() const
{
	if (HasAuthority() && IsLocalController())
	{
		return true;
	}
	const AKartPlayerState* PS = GetPlayerState<AKartPlayerState>();
	return PS && PS->IsHost();
}

// =====================================================================
// UI input (runtime mapping context, priority above the kart's)
// =====================================================================

void AKartPlayerController::BuildUIInput()
{
	if (UIMappingContext)
	{
		return;
	}
	ScoreboardAction = NewObject<UInputAction>(this, TEXT("IA_UI_Scoreboard"));
	ConfirmAction = NewObject<UInputAction>(this, TEXT("IA_UI_Confirm"));
	BackAction = NewObject<UInputAction>(this, TEXT("IA_UI_Back"));

	UIMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_UI_Runtime"));
	UIMappingContext->MapKey(ScoreboardAction, EKeys::Tab);
	UIMappingContext->MapKey(ConfirmAction, EKeys::Enter);
	UIMappingContext->MapKey(BackAction, EKeys::BackSpace);
}

void AKartPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildUIInput();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(ScoreboardAction, ETriggerEvent::Started, this, &AKartPlayerController::OnScoreboardPressed);
		EIC->BindAction(ScoreboardAction, ETriggerEvent::Completed, this, &AKartPlayerController::OnScoreboardReleased);
		EIC->BindAction(ConfirmAction, ETriggerEvent::Started, this, &AKartPlayerController::OnConfirmPressed);
		EIC->BindAction(BackAction, ETriggerEvent::Started, this, &AKartPlayerController::OnBackPressed);
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(UIMappingContext, 1);
	}
}

void AKartPlayerController::OnScoreboardPressed() { bScoreboardHeld = true; }
void AKartPlayerController::OnScoreboardReleased() { bScoreboardHeld = false; }

void AKartPlayerController::OnConfirmPressed()
{
	const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr;
	if (!GS || !IsLocalHost())
	{
		return;
	}
	if (GS->IsLobby())
	{
		Server_RequestStartMatch();
	}
	else if (GS->IsPostMatch())
	{
		Server_RequestPlayAgain();
	}
}

void AKartPlayerController::OnBackPressed()
{
	const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr;
	if (GS && IsLocalHost() && GS->IsPostMatch())
	{
		Server_RequestBackToLobby();
	}
}

// =====================================================================
// Host action RPCs
// =====================================================================

void AKartPlayerController::Server_RequestStartMatch_Implementation()
{
	if (AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>())
	{
		GM->RequestStartMatch(this);
	}
}

void AKartPlayerController::Server_RequestPlayAgain_Implementation()
{
	if (AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>())
	{
		GM->RequestPlayAgain(this);
	}
}

void AKartPlayerController::Server_RequestBackToLobby_Implementation()
{
	if (AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>())
	{
		GM->RequestBackToLobby(this);
	}
}

void AKartPlayerController::Server_RequestEndMatch_Implementation()
{
	if (AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>())
	{
		GM->RequestEndMatch(this);
	}
}

void AKartPlayerController::Server_SetMatchDuration_Implementation(int32 Seconds)
{
	if (AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>())
	{
		GM->RequestSetMatchDuration(this, Seconds);
	}
}

// =====================================================================
// Debug commands (SK.*)
// =====================================================================

void AKartPlayerController::RunDebugCommand(const FString& Command)
{
#if !UE_BUILD_SHIPPING
	const FString Name = Command.TrimStartAndEnd();
	if (Name.StartsWith(TEXT("ShowNet"), ESearchCase::IgnoreCase))
	{
		bShowNetOverlay = !bShowNetOverlay; // local only
		return;
	}
	if (HasAuthority())
	{
		HandleDebugCommand(Name);
	}
	else
	{
		Server_DebugCommand(Name);
	}
#endif
}

void AKartPlayerController::Server_DebugCommand_Implementation(const FString& Command)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogSmashKarts, Warning, TEXT("[Debug] Ignored in Shipping: %s"), *Command);
#else
	HandleDebugCommand(Command.Left(256));
#endif
}

void AKartPlayerController::HandleDebugCommand(const FString& Command)
{
#if !UE_BUILD_SHIPPING
	TArray<FString> Args;
	Command.ParseIntoArrayWS(Args);
	if (Args.Num() == 0)
	{
		return;
	}
	const FString Cmd = Args[0];
	AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>();
	AKartCharacter* Kart = Cast<AKartCharacter>(GetPawn());
	UE_LOG(LogSmashKarts, Log, TEXT("[Debug] %s: %s"), *GetNameSafe(PlayerState), *Command);

	if (Cmd.Equals(TEXT("GiveWeapon"), ESearchCase::IgnoreCase) && Args.Num() >= 2 && GM && Kart)
	{
		UKartWeaponData* Data = GM->FindWeaponByName(Args[1]);
		if (!Data)
		{
			UE_LOG(LogSmashKarts, Warning, TEXT("[Debug] No weapon '%s' in BP_KartGameMode WeaponCatalog"), *Args[1]);
			return;
		}
		if (UKartWeaponComponent* WC = Kart->GetWeaponComponent())
		{
			WC->DestroyWeapon();
			WC->GiveWeapon(Data);
		}
	}
	else if (Cmd.Equals(TEXT("Damage"), ESearchCase::IgnoreCase) && Args.Num() >= 2 && Kart)
	{
		UGameplayStatics::ApplyDamage(Kart, FCString::Atof(*Args[1]), nullptr, nullptr, UDamageType::StaticClass());
	}
	else if (Cmd.Equals(TEXT("Kill"), ESearchCase::IgnoreCase) && Kart && GM)
	{
		GM->KartEliminated(Kart, nullptr, nullptr, false);
	}
	else if (Cmd.Equals(TEXT("SetStatus"), ESearchCase::IgnoreCase) && Args.Num() >= 2 && Kart && Kart->GetStatusComponent())
	{
		const int64 Value = StaticEnum<EKartStatus>()->GetValueByNameString(Args[1], EGetByNameFlags::None);
		if (Value == INDEX_NONE || Value >= static_cast<int64>(EKartStatus::Count))
		{
			UE_LOG(LogSmashKarts, Warning, TEXT("[Debug] Unknown status '%s' (Frozen, TiresPopped, Spiky, Invincible, Tesla, SpawnProtected)"), *Args[1]);
			return;
		}
		const float Seconds = Args.Num() >= 3 ? FCString::Atof(*Args[2]) : 5.f;
		if (Seconds == 0.f)
		{
			Kart->GetStatusComponent()->ClearStatus(static_cast<EKartStatus>(Value));
		}
		else
		{
			Kart->GetStatusComponent()->ApplyStatus(static_cast<EKartStatus>(Value), Seconds);
		}
	}
	else if (Cmd.Equals(TEXT("EndMatch"), ESearchCase::IgnoreCase) && GM)
	{
		GM->RequestEndMatch(GM->IsHost(this) ? this : nullptr);
		if (!GM->IsHost(this) && GM->IsMatchInProgress())
		{
			GM->EndMatch(); // debug: any player may end it in dev builds
		}
	}
	else if (Cmd.Equals(TEXT("MatchTime"), ESearchCase::IgnoreCase) && Args.Num() >= 2 && GM)
	{
		GM->DebugSetMatchTimeRemaining(FCString::Atof(*Args[1]));
	}
	else if (Cmd.Equals(TEXT("StartMatch"), ESearchCase::IgnoreCase) && GM)
	{
		GM->DebugForceStartMatch();
	}
	else if (Cmd.Equals(TEXT("Lobby"), ESearchCase::IgnoreCase) && GM)
	{
		GM->RequestBackToLobby(GM->IsHost(this) ? this : nullptr);
	}
	else if (Cmd.Equals(TEXT("State"), ESearchCase::IgnoreCase) && GM)
	{
		GM->DumpStateToLog();
	}
	else if (Cmd.Equals(TEXT("AutoTest"), ESearchCase::IgnoreCase))
	{
		const int32 Players = Args.Num() >= 2 ? FMath::Max(1, FCString::Atoi(*Args[1])) : 2;
		const bool bQuit = Args.Num() >= 3 && Args[2].Equals(TEXT("quit"), ESearchCase::IgnoreCase);
		FKartAutoTest::Start(GetWorld(), Players, bQuit);
	}
	else
	{
		UE_LOG(LogSmashKarts, Warning, TEXT("[Debug] Unknown or incomplete command: %s"), *Command);
	}
#endif
}
