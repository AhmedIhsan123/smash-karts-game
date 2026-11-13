#include "Game/KartPlayerState.h"
#include "Game/KartGameState.h"
#include "SmashKarts.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AKartPlayerState::AKartPlayerState()
{
	SetNetUpdateFrequency(10.f);
}

void AKartPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKartPlayerState, Eliminations);
	DOREPLIFETIME(AKartPlayerState, Deaths);
	DOREPLIFETIME(AKartPlayerState, JoinOrder);
	DOREPLIFETIME(AKartPlayerState, ColorIndex);
	DOREPLIFETIME(AKartPlayerState, bIsHost);
	DOREPLIFETIME(AKartPlayerState, RespawnServerTime);
}

void AKartPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (AKartPlayerState* Target = Cast<AKartPlayerState>(PlayerState))
	{
		Target->JoinOrder = JoinOrder;
		Target->ColorIndex = ColorIndex;
		Target->bIsHost = bIsHost;
	}
}

void AKartPlayerState::ResetMatchStats()
{
	SetScore(0.f);
	Eliminations = 0;
	Deaths = 0;
	RespawnServerTime = 0.0;
	OnRep_Stats();
}

void AKartPlayerState::AddElimination()
{
	SetScore(GetScore() + 1.f);
	++Eliminations;
	OnRep_Stats();
}

void AKartPlayerState::AddDeath()
{
	++Deaths;
	OnRep_Stats();
}

void AKartPlayerState::OnRep_Score()
{
	Super::OnRep_Score();
	UE_LOG(LogSmashKarts, Log, TEXT("[Score] %s = %.0f (%s)"), *GetPlayerName(), GetScore(),
		HasAuthority() ? TEXT("Server") : TEXT("Client"));
}

void AKartPlayerState::OnRep_Stats()
{
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Stats] %s elims %d deaths %d (%s)"), *GetPlayerName(), Eliminations, Deaths,
		HasAuthority() ? TEXT("Server") : TEXT("Client"));
}

FLinearColor AKartPlayerState::GetPlayerColor() const
{
	const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr;
	return GS ? GS->GetColorForIndex(ColorIndex) : FLinearColor::White;
}
