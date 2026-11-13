#include "Game/KartPlayerController.h"
#include "Game/KartGameMode.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerState.h"
#include "UI/KartHUD.h"
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
	PauseAction = NewObject<UInputAction>(this, TEXT("IA_UI_Pause"));

	UIMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_UI_Runtime"));
	UIMappingContext->MapKey(ScoreboardAction, EKeys::Tab);
	UIMappingContext->MapKey(ConfirmAction, EKeys::Enter);
	UIMappingContext->MapKey(BackAction, EKeys::BackSpace);
	UIMappingContext->MapKey(PauseAction, EKeys::Escape);
	UIMappingContext->MapKey(PauseAction, EKeys::P); // Esc stops PIE in the editor; P works everywhere
}

void AKartPlayerController::OnPausePressed()
{
	TogglePauseMenu();
}

void AKartPlayerController::TogglePauseMenu()
{
	if (AKartHUD* KartHUD = GetHUD<AKartHUD>())
	{
		KartHUD->SetPauseMenuOpen(!KartHUD->IsPauseMenuOpen());
		UpdateInputMode();
	}
}

bool AKartPlayerController::IsGameInputBlocked() const
{
	const AKartHUD* KartHUD = GetHUD<AKartHUD>();
	return KartHUD && KartHUD->IsPauseMenuOpen();
}

void AKartPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (IsLocalController())
	{
		UpdateInputMode();
	}
}

void AKartPlayerController::UpdateInputMode()
{
	const AKartHUD* KartHUD = GetHUD<AKartHUD>();
	const int8 Wanted = (KartHUD && KartHUD->WantsCursor()) ? 1 : 0;
	if (Wanted == CursorState)
	{
		return;
	}
	CursorState = Wanted;
	if (Wanted)
	{
		// Keyboard keeps driving (lobby), the mouse can click the overlay buttons
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

void AKartPlayerController::Client_ShowDamageNumber_Implementation(FVector_NetQuantize Location, float Amount, bool bKilled)
{
	if (AKartHUD* KartHUD = GetHUD<AKartHUD>())
	{
		KartHUD->AddDamageNumber(Location, Amount, bKilled);
	}
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
		EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &AKartPlayerController::OnPausePressed);
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

void AKartPlayerController::Server_SetBotCount_Implementation(int32 Count)
{
	if (AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>())
	{
		GM->RequestSetBotCount(this, FMath::Clamp(Count, 0, 16)); // never trust client values
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
	if (Name.StartsWith(TEXT("AutoDrive"), ESearchCase::IgnoreCase))
	{
		// Local: the kart drives itself exactly like held keys (predicted moves), for network tests
		TArray<FString> Args;
		Name.ParseIntoArrayWS(Args);
		AutoDriveEndTime = GetWorld()->GetTimeSeconds() + (Args.Num() >= 2 ? FCString::Atof(*Args[1]) : 30.f);
		AutoDriveSteer = Args.Num() >= 3 ? FCString::Atof(*Args[2]) : 0.6f;
		ReapplyAutoDrive();
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
	else if (Cmd.Equals(TEXT("SpawnBots"), ESearchCase::IgnoreCase) && GM)
	{
		GM->SetBotCount(Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 3); // dev builds: anyone may add bots
	}
	else if (Cmd.Equals(TEXT("AutoTest"), ESearchCase::IgnoreCase))
	{
		const int32 Players = Args.Num() >= 2 ? FMath::Max(1, FCString::Atoi(*Args[1])) : 2;
		bool bQuit = false;
		FString Scenario = TEXT("loop");
		for (int32 i = 2; i < Args.Num(); ++i)
		{
			if (Args[i].Equals(TEXT("quit"), ESearchCase::IgnoreCase))
			{
				bQuit = true;
			}
			else
			{
				Scenario = Args[i];
			}
		}
		FKartAutoTest::Start(GetWorld(), Players, bQuit, Scenario);
	}
	else
	{
		UE_LOG(LogSmashKarts, Warning, TEXT("[Debug] Unknown or incomplete command: %s"), *Command);
	}
#endif
}

void AKartPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (IsLocalController())
	{
		ReapplyAutoDrive(); // host: new kart after a respawn
	}
}

void AKartPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	ReapplyAutoDrive(); // client: new kart after a respawn
}

void AKartPlayerController::ReapplyAutoDrive()
{
#if !UE_BUILD_SHIPPING
	const double Remaining = AutoDriveEndTime - GetWorld()->GetTimeSeconds();
	AKartCharacter* Kart = Cast<AKartCharacter>(GetPawn());
	if (Kart && Remaining > 0.0)
	{
		Kart->SetDebugAutoDrive(static_cast<float>(Remaining), AutoDriveSteer);
	}
#endif
}

void AKartPlayerController::Client_DebugExec_Implementation(const FString& Command)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogSmashKarts, Log, TEXT("[Debug] Host asked this client to run: %s"), *Command);
	ConsoleCommand(Command.Left(256));
#endif
}
