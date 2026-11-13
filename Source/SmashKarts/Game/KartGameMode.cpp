#include "Game/KartGameMode.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerState.h"
#include "Game/KartPlayerController.h"
#include "UI/KartHUD.h"
#include "KartCharacter.h"
#include "KartDamageType.h"
#include "KartHealthComponent.h"
#include "KartProjectile.h"
#include "KartRules.h"
#include "KartStatusComponent.h"
#include "KartTrap.h"
#include "KartWeaponData.h"
#include "LootBox.h"
#include "SmashKarts.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AKartGameMode::AKartGameMode()
{
	GameStateClass = AKartGameState::StaticClass();
	PlayerStateClass = AKartPlayerState::StaticClass();
	PlayerControllerClass = AKartPlayerController::StaticClass();
	HUDClass = AKartHUD::StaticClass();

	MatchDuration = 180.f;
	CountdownDuration = 3.f;
	RespawnDelay = 3.f;
	SpawnProtectionDuration = 2.f;
	ResultsDuration = 12.f;
	FallCreditWindow = 5.f;
	MaxKartPlayers = 8;
	MatchDurationChoices = { 120, 180, 300 };
	bAutoReturnToLobby = true;
	SpawnOccupiedRadius = 250.f;
	SpawnRandomTopN = 3;

	bDelayedStart = true; // the host starts the match from the lobby
}

AKartGameState* AKartGameMode::GetKartGameState() const
{
	return GetGameState<AKartGameState>();
}

double AKartGameMode::Now() const
{
	return KartTime::ServerNow(this);
}

// =====================================================================
// Players joining / leaving
// =====================================================================

void AKartGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (GameSession && !UGameplayStatics::HasOption(Options, TEXT("MaxPlayers")))
	{
		GameSession->MaxPlayers = MaxKartPlayers;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Game] InitGame %s | max players %d"), *MapName, GameSession ? GameSession->MaxPlayers : -1);
}

FString AKartGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!Error.IsEmpty() || !NewPlayerController)
	{
		return Error;
	}

	if (AKartPlayerState* PS = NewPlayerController->GetPlayerState<AKartPlayerState>())
	{
		PS->SetJoinOrder(NextJoinOrder++);
		PS->SetColorIndex(PickFreeColorIndex(PS));
		PS->SetIsHost(NewPlayerController->IsLocalController());
		PS->ResetMatchStats();

		// Our own option so the engine's machine-name default never shows up. Set by the main menu.
		FString Requested = KartRules::SanitizePlayerName(UGameplayStatics::ParseOption(Options, TEXT("SKName")));
		if (Requested.IsEmpty())
		{
			Requested = FString::Printf(TEXT("Player %d"), PS->GetJoinOrder() + 1);
		}
		ChangeName(NewPlayerController, Requested, false);

		UE_LOG(LogSmashKarts, Log, TEXT("[Game] Player joined: %s | join %d | colour %d | host %s"),
			*PS->GetPlayerName(), PS->GetJoinOrder(), PS->GetColorIndex(), PS->IsHost() ? TEXT("yes") : TEXT("no"));
	}
	return Error;
}

void AKartGameMode::Logout(AController* Exiting)
{
	if (Exiting)
	{
		if (FTimerHandle* Handle = RespawnTimers.Find(Exiting))
		{
			GetWorldTimerManager().ClearTimer(*Handle);
		}
		RespawnTimers.Remove(Exiting);
		UE_LOG(LogSmashKarts, Log, TEXT("[Game] Player left: %s"), Exiting->PlayerState ? *Exiting->PlayerState->GetPlayerName() : TEXT("?"));
	}
	Super::Logout(Exiting);
}

int32 AKartGameMode::PickFreeColorIndex(const APlayerState* Ignore) const
{
	const AKartGameState* GS = GetKartGameState();
	const int32 PaletteSize = (GS && GS->PlayerColors.Num() > 0) ? GS->PlayerColors.Num() : 8;

	TArray<bool> Used;
	Used.Init(false, PaletteSize);
	if (GS)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			const AKartPlayerState* KPS = Cast<AKartPlayerState>(PS);
			if (KPS && KPS != Ignore && Used.IsValidIndex(KPS->GetColorIndex()))
			{
				Used[KPS->GetColorIndex()] = true;
			}
		}
	}
	for (int32 i = 0; i < PaletteSize; ++i)
	{
		if (!Used[i])
		{
			return i;
		}
	}
	return NextJoinOrder % PaletteSize;
}

bool AKartGameMode::FindInactivePlayer(APlayerController* PC)
{
	return false; // rejoiners start fresh with 0 score
}

void AKartGameMode::AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC)
{
	// Don't keep stats of players who left (see FindInactivePlayer)
}

// =====================================================================
// Spawning
// =====================================================================

bool AKartGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	const FName State = GetMatchState();
	const bool bSpawnPhase = State == MatchState::WaitingToStart || State == KartMatchState::Countdown || State == MatchState::InProgress;
	// Skip AGameMode's "only while InProgress" rule; keep the base validity checks
	return bSpawnPhase && AGameModeBase::PlayerCanRestart_Implementation(Player);
}

void AKartGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Lobby, countdown and match: spawn straight away (late joiners start at 0). Results: wait.
	if (NewPlayer && !MustSpectate(NewPlayer) && PlayerCanRestart(NewPlayer))
	{
		RestartPlayer(NewPlayer);
	}
}

bool AKartGameMode::ReadyToStartMatch_Implementation()
{
	return false; // only the host starts (RequestStartMatch)
}

bool AKartGameMode::ReadyToEndMatch_Implementation()
{
	return false; // the match timer ends it
}

bool AKartGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	return false; // always pick a fresh start via ChoosePlayerStart
}

AActor* AKartGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Starts.Add(*It);
	}
	if (Starts.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const APawn* OwnPawn = Player ? Player->GetPawn() : nullptr;
	TArray<FVector> Enemies;
	TArray<FVector> AllKarts;
	for (TActorIterator<AKartCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == OwnPawn)
		{
			continue;
		}
		AllKarts.Add(It->GetActorLocation());
		if (!It->IsEliminated())
		{
			Enemies.Add(It->GetActorLocation());
		}
	}

	// Score = distance to the nearest living enemy (bigger is safer); occupied spots are pushed to the bottom
	TArray<float> Scores;
	Scores.Reserve(Starts.Num());
	for (const APlayerStart* Start : Starts)
	{
		const FVector Loc = Start->GetActorLocation();
		float Score = 100000.f;
		for (const FVector& Enemy : Enemies)
		{
			Score = FMath::Min(Score, static_cast<float>(FVector::Dist(Loc, Enemy)));
		}
		for (const FVector& Kart : AllKarts)
		{
			if (FVector::Dist(Loc, Kart) < SpawnOccupiedRadius)
			{
				Score -= 1000000.f;
				break;
			}
		}
		Scores.Add(Score + FMath::FRandRange(0.f, 1.f)); // jitter breaks ties (empty arena)
	}

	const int32 Index = KartRules::PickSpawnIndex(Scores, SpawnRandomTopN, FMath::FRand());
	return Starts.IsValidIndex(Index) ? Starts[Index] : Starts[0];
}

void AKartGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	Super::FinishRestartPlayer(NewPlayer, StartRotation);

	AKartCharacter* Kart = NewPlayer ? Cast<AKartCharacter>(NewPlayer->GetPawn()) : nullptr;
	if (!Kart)
	{
		return;
	}
	if (AKartPlayerState* PS = NewPlayer->GetPlayerState<AKartPlayerState>())
	{
		PS->SetRespawnServerTime(0.0);
	}
	if (IsMatchInProgress() && SpawnProtectionDuration > 0.f && Kart->GetStatusComponent())
	{
		Kart->GetStatusComponent()->ApplyStatus(EKartStatus::SpawnProtected, SpawnProtectionDuration);
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Game] Spawned %s at %s"), *GetNameSafe(NewPlayer->PlayerState), *Kart->GetActorLocation().ToCompactString());
}

void AKartGameMode::ScheduleRespawn(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	FTimerHandle& Handle = RespawnTimers.FindOrAdd(Controller);
	GetWorldTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateUObject(this, &AKartGameMode::OnRespawnTimer, TWeakObjectPtr<AController>(Controller)),
		FMath::Max(0.01f, RespawnDelay), false);

	if (AKartPlayerState* PS = Controller->GetPlayerState<AKartPlayerState>())
	{
		PS->SetRespawnServerTime(Now() + RespawnDelay);
	}
}

void AKartGameMode::OnRespawnTimer(TWeakObjectPtr<AController> Controller)
{
	if (Controller.IsValid())
	{
		RespawnTimers.Remove(Controller);
		RespawnNow(Controller.Get());
	}
}

void AKartGameMode::RespawnNow(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (!PlayerCanRestart(PC))
		{
			return;
		}
	}
	RestartPlayer(Controller);
}

void AKartGameMode::RespawnEveryone()
{
	ClearRespawnTimers();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (PC->PlayerState && !PC->PlayerState->IsOnlyASpectator())
			{
				RespawnNow(PC);
			}
		}
	}
}

void AKartGameMode::ClearRespawnTimers()
{
	for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& Pair : RespawnTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	RespawnTimers.Reset();
}

// =====================================================================
// Eliminations
// =====================================================================

void AKartGameMode::KartEliminated(AKartCharacter* Victim, AController* Killer, TSubclassOf<UDamageType> DamageType, bool bFell)
{
	if (!Victim || Victim->IsEliminated())
	{
		return;
	}

	AController* VictimController = Victim->GetController();

	// No attacker (fall, world, self): credit whoever hit us last, if it was recent
	if (!Killer || Killer == VictimController)
	{
		Killer = Victim->GetRecentAttacker(FallCreditWindow);
		if (Killer && !bFell)
		{
			DamageType = Victim->GetRecentAttackType();
		}
	}

	AKartPlayerState* VictimPS = Victim->GetPlayerState<AKartPlayerState>();
	AKartPlayerState* KillerPS = (Killer && Killer != VictimController) ? Killer->GetPlayerState<AKartPlayerState>() : nullptr;

	if (IsMatchInProgress())
	{
		if (KillerPS)
		{
			KillerPS->AddElimination();
		}
		if (VictimPS)
		{
			VictimPS->AddDeath();
		}
	}

	Victim->Eliminate(UKartDamageType::WantsBigExplosion(DamageType));

	UE_LOG(LogSmashKarts, Log, TEXT("[Game] %s eliminated %s%s | scores: killer %.0f"),
		KillerPS ? *KillerPS->GetPlayerName() : TEXT("nobody"),
		VictimPS ? *VictimPS->GetPlayerName() : *GetNameSafe(Victim),
		bFell ? TEXT(" (fell)") : TEXT(""),
		KillerPS ? KillerPS->GetScore() : 0.f);

	if (AKartGameState* GS = GetKartGameState())
	{
		GS->Multicast_Elimination(KillerPS, VictimPS, DamageType, bFell);
	}

	if (VictimController)
	{
		ScheduleRespawn(VictimController);
	}
	else
	{
		Victim->SetLifeSpan(RespawnDelay); // nobody to respawn (bot / left)
	}
}

// =====================================================================
// Match flow
// =====================================================================

bool AKartGameMode::IsHost(const AController* Controller) const
{
	const APlayerController* PC = Cast<APlayerController>(Controller);
	return PC && PC->IsLocalController();
}

void AKartGameMode::RequestStartMatch(AController* Requester)
{
	if (!IsHost(Requester))
	{
		UE_LOG(LogSmashKarts, Warning, TEXT("[Game] Start refused: %s is not the host"), *GetNameSafe(Requester));
		return;
	}
	if (GetMatchState() == MatchState::WaitingToStart)
	{
		BeginCountdown();
	}
	else if (GetMatchState() == MatchState::WaitingPostMatch)
	{
		BeginCountdown(); // same as Play Again
	}
}

void AKartGameMode::RequestPlayAgain(AController* Requester)
{
	if (IsHost(Requester) && GetMatchState() == MatchState::WaitingPostMatch)
	{
		BeginCountdown();
	}
}

void AKartGameMode::RequestBackToLobby(AController* Requester)
{
	if (IsHost(Requester) && (GetMatchState() == MatchState::WaitingPostMatch || GetMatchState() == KartMatchState::Countdown))
	{
		GoToLobby();
	}
}

void AKartGameMode::RequestEndMatch(AController* Requester)
{
	if (!IsHost(Requester))
	{
		return;
	}
	if (IsMatchInProgress())
	{
		EndMatch();
	}
	else if (GetMatchState() == KartMatchState::Countdown)
	{
		GoToLobby();
	}
}

void AKartGameMode::RequestSetMatchDuration(AController* Requester, int32 Seconds)
{
	const bool bCanChange = GetMatchState() == MatchState::WaitingToStart || GetMatchState() == MatchState::WaitingPostMatch;
	if (!IsHost(Requester) || !bCanChange)
	{
		return;
	}
	MatchDuration = static_cast<float>(FMath::Clamp(Seconds, 30, 900));
	if (AKartGameState* GS = GetKartGameState())
	{
		GS->MatchDurationSeconds = FMath::RoundToInt(MatchDuration);
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Game] Match length set to %.0f s"), MatchDuration);
}

void AKartGameMode::BeginCountdown()
{
	GetWorldTimerManager().ClearTimer(ResultsTimer);
	GetWorldTimerManager().ClearTimer(MatchTimer);

	if (AKartGameState* GS = GetKartGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (AKartPlayerState* KPS = Cast<AKartPlayerState>(PS))
			{
				KPS->ResetMatchStats();
			}
		}
		GS->MatchDurationSeconds = FMath::RoundToInt(MatchDuration);
		GS->CountdownEndServerTime = Now() + CountdownDuration;
	}

	ResetWorldForRound(true);
	SetMatchState(KartMatchState::Countdown);
	RespawnEveryone(); // fresh karts at fresh starts, frozen until GO

	UE_LOG(LogSmashKarts, Log, TEXT("[Game] Countdown %.1f s, match %.0f s"), CountdownDuration, MatchDuration);

	if (CountdownDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(CountdownTimer, this, &AKartGameMode::OnCountdownFinished, CountdownDuration, false);
	}
	else
	{
		OnCountdownFinished();
	}
}

void AKartGameMode::OnCountdownFinished()
{
	if (GetMatchState() == KartMatchState::Countdown)
	{
		SetMatchState(MatchState::InProgress); // StartMatch() would refuse: AGameMode treats Countdown as started
	}
}

void AKartGameMode::HandleMatchIsWaitingToStart()
{
	Super::HandleMatchIsWaitingToStart();

	GetWorldTimerManager().ClearTimer(CountdownTimer);
	GetWorldTimerManager().ClearTimer(MatchTimer);
	GetWorldTimerManager().ClearTimer(ResultsTimer);
	ResetWorldForRound(false); // lobby: no loot

	if (AKartGameState* GS = GetKartGameState())
	{
		GS->MatchDurationSeconds = FMath::RoundToInt(MatchDuration);
	}

	// Players who logged in while the map was still loading (the host!) have no kart yet
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->GetPawn() && PlayerCanRestart(PC))
		{
			RestartPlayer(PC);
		}
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Game] Lobby (waiting for the host)"));
}

void AKartGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted(); // spawns anyone still without a kart

	if (AKartGameState* GS = GetKartGameState())
	{
		GS->MatchEndServerTime = Now() + MatchDuration;
	}
	GetWorldTimerManager().SetTimer(MatchTimer, this, &AKartGameMode::OnMatchTimeUp, FMath::Max(1.f, MatchDuration), false);
	RecheckLootBoxes();
	UE_LOG(LogSmashKarts, Log, TEXT("[Game] Match started (%.0f s)"), MatchDuration);
}

void AKartGameMode::OnMatchTimeUp()
{
	EndMatch();
}

void AKartGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	GetWorldTimerManager().ClearTimer(MatchTimer);
	ClearRespawnTimers();

	if (AKartGameState* GS = GetKartGameState())
	{
		GS->ResultsEndServerTime = Now() + ResultsDuration;
	}
	if (ResultsDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(ResultsTimer, this, &AKartGameMode::OnResultsFinished, ResultsDuration, false);
	}
	DumpStateToLog();
}

void AKartGameMode::OnResultsFinished()
{
	if (bAutoReturnToLobby && GetMatchState() == MatchState::WaitingPostMatch)
	{
		GoToLobby();
	}
}

void AKartGameMode::GoToLobby()
{
	SetMatchState(MatchState::WaitingToStart);
	RespawnEveryone();
}

void AKartGameMode::ResetWorldForRound(bool bLootActive)
{
	UWorld* World = GetWorld();
	for (TActorIterator<AKartProjectile> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<AKartTrap> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<ALootBox> It(World); It; ++It)
	{
		It->ResetBox(bLootActive);
	}
}

void AKartGameMode::RecheckLootBoxes()
{
	for (TActorIterator<ALootBox> It(GetWorld()); It; ++It)
	{
		It->RecheckOverlaps();
	}
}

// =====================================================================
// Debug
// =====================================================================

void AKartGameMode::DebugSetMatchTimeRemaining(float Seconds)
{
	if (!IsMatchInProgress())
	{
		return;
	}
	Seconds = FMath::Max(0.5f, Seconds);
	GetWorldTimerManager().SetTimer(MatchTimer, this, &AKartGameMode::OnMatchTimeUp, Seconds, false);
	if (AKartGameState* GS = GetKartGameState())
	{
		GS->MatchEndServerTime = Now() + Seconds;
	}
}

void AKartGameMode::DebugForceStartMatch()
{
	if (GetMatchState() == MatchState::WaitingToStart || GetMatchState() == MatchState::WaitingPostMatch)
	{
		BeginCountdown();
	}
}

UKartWeaponData* AKartGameMode::FindWeaponByName(const FString& Name) const
{
	for (UKartWeaponData* Data : WeaponCatalog)
	{
		if (!Data)
		{
			continue;
		}
		const FString AssetName = Data->GetName();
		if (AssetName.Equals(Name, ESearchCase::IgnoreCase) || AssetName.Equals(TEXT("DA_") + Name, ESearchCase::IgnoreCase)
			|| Data->DisplayName.ToString().Replace(TEXT(" "), TEXT("")).Equals(Name.Replace(TEXT(" "), TEXT("")), ESearchCase::IgnoreCase))
		{
			return Data;
		}
	}
	return nullptr;
}

void AKartGameMode::DumpStateToLog() const
{
	const AKartGameState* GS = GetKartGameState();
	if (!GS)
	{
		return;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[State] Match %s | time left %.1f | players %d"),
		*GetMatchState().ToString(), GS->GetMatchTimeRemaining(), GS->PlayerArray.Num());
	int32 Rank = 1;
	for (const AKartPlayerState* PS : GS->GetSortedPlayers())
	{
		const AController* C = Cast<AController>(PS->GetOwner());
		const AKartCharacter* Kart = C ? Cast<AKartCharacter>(C->GetPawn()) : nullptr;
		UE_LOG(LogSmashKarts, Log, TEXT("[State]  #%d %s | score %.0f | elims %d | deaths %d | host %s | kart %s | hp %.0f"),
			Rank++, *PS->GetPlayerName(), PS->GetScore(), PS->GetEliminations(), PS->GetDeaths(),
			PS->IsHost() ? TEXT("y") : TEXT("n"),
			Kart ? (Kart->IsEliminated() ? TEXT("dead") : TEXT("alive")) : TEXT("none"),
			(Kart && Kart->GetHealthComponent()) ? Kart->GetHealthComponent()->GetHealth() : 0.f);
	}
}
