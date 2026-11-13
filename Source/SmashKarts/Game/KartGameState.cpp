#include "Game/KartGameState.h"
#include "Game/KartPlayerState.h"
#include "UI/KartHUD.h"
#include "KartDamageType.h"
#include "KartRules.h"
#include "SmashKarts.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

const FName KartMatchState::Countdown = FName(TEXT("Countdown"));

AKartGameState::AKartGameState()
{
	// Readable on any background: red, blue, green, yellow, purple, orange, cyan, pink
	PlayerColors = {
		FLinearColor(0.90f, 0.12f, 0.12f),
		FLinearColor(0.10f, 0.35f, 0.95f),
		FLinearColor(0.12f, 0.75f, 0.20f),
		FLinearColor(0.98f, 0.85f, 0.10f),
		FLinearColor(0.60f, 0.20f, 0.85f),
		FLinearColor(1.00f, 0.50f, 0.05f),
		FLinearColor(0.10f, 0.85f, 0.90f),
		FLinearColor(1.00f, 0.40f, 0.70f)
	};
}

void AKartGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKartGameState, CountdownEndServerTime);
	DOREPLIFETIME(AKartGameState, MatchEndServerTime);
	DOREPLIFETIME(AKartGameState, ResultsEndServerTime);
	DOREPLIFETIME(AKartGameState, MatchDurationSeconds);
}

void AKartGameState::OnRep_MatchState()
{
	Super::OnRep_MatchState();
	UE_LOG(LogSmashKarts, Log, TEXT("[Match] State -> %s (%s)"), *GetMatchState().ToString(),
		HasAuthority() ? TEXT("Server") : TEXT("Client"));
	OnMatchStateChanged.Broadcast(GetMatchState());
}

void AKartGameState::Multicast_Elimination_Implementation(APlayerState* Killer, APlayerState* Victim,
	TSubclassOf<UDamageType> DamageType, bool bFell)
{
	UE_LOG(LogSmashKarts, Log, TEXT("[Elim] %s -> %s [%s]%s (%s)"),
		Killer ? *Killer->GetPlayerName() : TEXT("-"),
		Victim ? *Victim->GetPlayerName() : TEXT("?"),
		*UKartDamageType::GetKillFeedName(DamageType).ToString(),
		bFell ? TEXT(" fell") : TEXT(""),
		HasAuthority() ? TEXT("Server") : TEXT("Client"));

	OnElimination.Broadcast(Killer, Victim, DamageType, bFell);

	// Every local player's HUD gets the kill feed entry / banner directly (no binding-order issues)
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController())
			{
				if (AKartHUD* HUD = PC->GetHUD<AKartHUD>())
				{
					HUD->HandleElimination(Killer, Victim, DamageType, bFell);
				}
			}
		}
	}
}

bool AKartGameState::IsLobby() const { return GetMatchState() == MatchState::WaitingToStart; }
bool AKartGameState::IsCountdown() const { return GetMatchState() == KartMatchState::Countdown; }
bool AKartGameState::IsPlaying() const { return GetMatchState() == MatchState::InProgress; }
bool AKartGameState::IsPostMatch() const { return GetMatchState() == MatchState::WaitingPostMatch; }

float AKartGameState::GetCountdownRemaining() const
{
	return IsCountdown() ? static_cast<float>(FMath::Max(0.0, CountdownEndServerTime - GetServerWorldTimeSeconds())) : 0.f;
}

float AKartGameState::GetMatchTimeRemaining() const
{
	if (IsPlaying())
	{
		return static_cast<float>(FMath::Max(0.0, MatchEndServerTime - GetServerWorldTimeSeconds()));
	}
	return IsPostMatch() ? 0.f : static_cast<float>(MatchDurationSeconds);
}

float AKartGameState::GetResultsRemaining() const
{
	return IsPostMatch() ? static_cast<float>(FMath::Max(0.0, ResultsEndServerTime - GetServerWorldTimeSeconds())) : 0.f;
}

TArray<AKartPlayerState*> AKartGameState::GetSortedPlayers() const
{
	TArray<AKartPlayerState*> Players;
	TArray<KartRules::FStandingEntry> Entries;
	for (APlayerState* PS : PlayerArray)
	{
		AKartPlayerState* KPS = Cast<AKartPlayerState>(PS);
		if (!KPS || KPS->IsOnlyASpectator())
		{
			continue;
		}
		KartRules::FStandingEntry Entry;
		Entry.Score = KPS->GetScore();
		Entry.Deaths = KPS->GetDeaths();
		Entry.JoinOrder = KPS->GetJoinOrder();
		Entry.Id = Players.Add(KPS);
		Entries.Add(Entry);
	}
	KartRules::SortStandings(Entries);

	TArray<AKartPlayerState*> Sorted;
	Sorted.Reserve(Entries.Num());
	for (const KartRules::FStandingEntry& Entry : Entries)
	{
		Sorted.Add(Players[Entry.Id]);
	}
	return Sorted;
}

int32 AKartGameState::GetRankOf(const APlayerState* PlayerState) const
{
	const TArray<AKartPlayerState*> Sorted = GetSortedPlayers();
	for (int32 i = 0; i < Sorted.Num(); ++i)
	{
		if (Sorted[i] == PlayerState)
		{
			return i + 1;
		}
	}
	return 0;
}

FLinearColor AKartGameState::GetColorForIndex(int32 ColorIndex) const
{
	if (PlayerColors.Num() == 0)
	{
		return FLinearColor::White;
	}
	return PlayerColors[FMath::Abs(ColorIndex) % PlayerColors.Num()];
}

AKartPlayerState* AKartGameState::GetHostPlayerState() const
{
	for (APlayerState* PS : PlayerArray)
	{
		AKartPlayerState* KPS = Cast<AKartPlayerState>(PS);
		if (KPS && KPS->IsHost())
		{
			return KPS;
		}
	}
	return nullptr;
}
