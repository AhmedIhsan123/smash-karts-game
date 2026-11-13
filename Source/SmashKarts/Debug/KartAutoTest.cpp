#include "Debug/KartAutoTest.h"

#if !UE_BUILD_SHIPPING

#include "Game/KartGameMode.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerState.h"
#include "KartCharacter.h"
#include "KartHealthComponent.h"
#include "KartStatusComponent.h"
#include "SmashKarts.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	class FAutoTestRunner : public TSharedFromThis<FAutoTestRunner>
	{
	public:
		FAutoTestRunner(UWorld* InWorld, int32 InExpected, bool bInQuit)
			: World(InWorld), Expected(FMath::Max(2, InExpected)), bQuit(bInQuit)
		{
		}

		void Begin()
		{
			StepStart = FPlatformTime::Seconds();
			UE_LOG(LogSmashKarts, Log, TEXT("[AutoTest] Started, waiting for %d players"), Expected);
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FAutoTestRunner::Tick), 0.1f);
		}

		void Stop()
		{
			if (TickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
				TickerHandle.Reset();
			}
		}

	private:
		TWeakObjectPtr<UWorld> World;
		int32 Expected = 2;
		bool bQuit = false;
		int32 Step = 0;
		double StepStart = 0.0;
		bool bFinished = false;
		FTSTicker::FDelegateHandle TickerHandle;

		TWeakObjectPtr<APlayerController> HostPC;
		TWeakObjectPtr<APlayerController> VictimPC;
		TWeakObjectPtr<AKartCharacter> OldVictimKart;

		AKartGameMode* GM() const { return World.IsValid() ? World->GetAuthGameMode<AKartGameMode>() : nullptr; }
		AKartGameState* GS() const { return World.IsValid() ? World->GetGameState<AKartGameState>() : nullptr; }
		AKartCharacter* KartOf(const TWeakObjectPtr<APlayerController>& PC) const { return PC.IsValid() ? Cast<AKartCharacter>(PC->GetPawn()) : nullptr; }
		AKartPlayerState* PSOf(const TWeakObjectPtr<APlayerController>& PC) const { return PC.IsValid() ? PC->GetPlayerState<AKartPlayerState>() : nullptr; }
		float Elapsed() const { return static_cast<float>(FPlatformTime::Seconds() - StepStart); }

		void Next(const TCHAR* What)
		{
			UE_LOG(LogSmashKarts, Log, TEXT("[AutoTest] Step %d OK: %s"), Step, What);
			++Step;
			StepStart = FPlatformTime::Seconds();
		}

		bool Fail(const FString& Reason)
		{
			UE_LOG(LogSmashKarts, Error, TEXT("[AutoTest] FAIL at step %d: %s"), Step, *Reason);
			if (GM())
			{
				GM()->DumpStateToLog();
			}
			return Finish();
		}

		bool Finish()
		{
			bFinished = true;
			if (bQuit && GEngine && World.IsValid())
			{
				GEngine->Exec(World.Get(), TEXT("quit"));
			}
			return false; // stop ticking
		}

		bool Timeout(float Seconds, const TCHAR* What)
		{
			return Elapsed() > Seconds ? Fail(FString::Printf(TEXT("timed out waiting for %s"), What)) : true;
		}

		bool AllPlayersHaveLiveKarts() const
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const AKartCharacter* Kart = It->Get() ? Cast<AKartCharacter>(It->Get()->GetPawn()) : nullptr;
				if (!Kart || Kart->IsEliminated())
				{
					return false;
				}
			}
			return true;
		}

		bool AllScoresZero() const
		{
			for (APlayerState* PS : GS()->PlayerArray)
			{
				const AKartPlayerState* KPS = Cast<AKartPlayerState>(PS);
				if (KPS && (KPS->GetScore() != 0.f || KPS->GetDeaths() != 0 || KPS->GetEliminations() != 0))
				{
					return false;
				}
			}
			return true;
		}

		void DropBelowKillZ(AKartCharacter* Kart) const
		{
			const float KillZ = World->GetWorldSettings()->KillZ;
			const FVector Loc = Kart->GetActorLocation();
			Kart->SetActorLocation(FVector(Loc.X, Loc.Y, FMath::Max(KillZ - 300.f, -1.0e6f)), false, nullptr, ETeleportType::TeleportPhysics);
		}

		bool Tick(float DeltaTime)
		{
			if (bFinished)
			{
				return false;
			}
			if (!World.IsValid() || !GM() || !GS())
			{
				return Fail(TEXT("world / game mode / game state missing (run on the host)"));
			}
			AKartGameMode* Mode = GM();
			AKartGameState* State = GS();

			switch (Step)
			{
			case 0: // wait for players
			{
				if (State->PlayerArray.Num() >= Expected && AllPlayersHaveLiveKarts())
				{
					for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
					{
						APlayerController* PC = It->Get();
						if (PC && PC->IsLocalController()) { HostPC = PC; }
						else if (PC && !VictimPC.IsValid()) { VictimPC = PC; }
					}
					if (!HostPC.IsValid() || !VictimPC.IsValid())
					{
						return Fail(TEXT("need a local host and at least one remote client"));
					}
					if (!State->IsLobby())
					{
						return Fail(TEXT("server is not in the lobby"));
					}
					if (World->GetWorldSettings()->KillZ < -1.0e6f)
					{
						return Fail(TEXT("KillZ is not set on this map (World Settings)"));
					}
					Next(TEXT("players joined, lobby, karts alive"));
					return true;
				}
				return Timeout(120.f, TEXT("players"));
			}
			case 1:
				Mode->RequestStartMatch(HostPC.Get());
				if (!State->IsCountdown())
				{
					return Fail(TEXT("host start did not enter Countdown"));
				}
				Next(TEXT("countdown started"));
				return true;
			case 2:
				if (State->IsPlaying())
				{
					if (!AllScoresZero())
					{
						return Fail(TEXT("scores not reset at match start"));
					}
					Next(TEXT("match in progress, scores 0"));
					return true;
				}
				return Timeout(Mode->CountdownDuration + 3.f, TEXT("InProgress"));
			case 3:
			{
				AKartCharacter* Victim = KartOf(VictimPC);
				AKartCharacter* Shooter = KartOf(HostPC);
				if (!Victim || !Shooter)
				{
					return Fail(TEXT("missing karts at match start"));
				}
				OldVictimKart = Victim;
				for (int32 i = 0; i < 3; ++i)
				{
					UGameplayStatics::ApplyDamage(Victim, 40.f, HostPC.Get(), Shooter, UDamageType::StaticClass());
				}
				if (!Victim->IsEliminated())
				{
					return Fail(TEXT("victim not eliminated after 120 damage"));
				}
				if (PSOf(HostPC)->GetScore() != 1.f || PSOf(VictimPC)->GetDeaths() != 1)
				{
					return Fail(FString::Printf(TEXT("expected host score 1 / victim deaths 1, got %.0f / %d"), PSOf(HostPC)->GetScore(), PSOf(VictimPC)->GetDeaths()));
				}
				Next(TEXT("damage elimination credited (+1)"));
				return true;
			}
			case 4:
			{
				AKartCharacter* Victim = KartOf(VictimPC);
				if (Victim && Victim != OldVictimKart.Get() && !Victim->IsEliminated())
				{
					const UKartHealthComponent* Health = Victim->GetHealthComponent();
					if (!Health || Health->GetHealth() != Health->GetMaxHealth())
					{
						return Fail(TEXT("respawned without full health"));
					}
					if (!Victim->GetStatusComponent() || !Victim->GetStatusComponent()->HasStatus(EKartStatus::SpawnProtected))
					{
						return Fail(TEXT("respawned without spawn protection"));
					}
					Next(TEXT("respawned with full health + spawn protection"));
					return true;
				}
				return Timeout(Mode->RespawnDelay + 3.f, TEXT("respawn"));
			}
			case 5:
			{
				AKartCharacter* Victim = KartOf(VictimPC);
				UGameplayStatics::ApplyDamage(Victim, 50.f, HostPC.Get(), KartOf(HostPC), UDamageType::StaticClass());
				if (Victim->GetHealthComponent()->GetHealth() != Victim->GetHealthComponent()->GetMaxHealth())
				{
					return Fail(TEXT("spawn protection did not block damage"));
				}
				Next(TEXT("spawn protection blocks damage"));
				return true;
			}
			case 6:
			{
				AKartCharacter* Victim = KartOf(VictimPC);
				if (Victim && !Victim->GetStatusComponent()->HasStatus(EKartStatus::SpawnProtected))
				{
					UGameplayStatics::ApplyDamage(Victim, 10.f, HostPC.Get(), KartOf(HostPC), UDamageType::StaticClass());
					OldVictimKart = Victim;
					DropBelowKillZ(Victim);
					Next(TEXT("protection ended; hit then dropped below KillZ"));
					return true;
				}
				return Timeout(Mode->SpawnProtectionDuration + 3.f, TEXT("spawn protection to end"));
			}
			case 7:
				if (OldVictimKart.IsValid() && OldVictimKart->IsEliminated())
				{
					if (PSOf(HostPC)->GetScore() != 2.f)
					{
						return Fail(FString::Printf(TEXT("fall after a hit should credit the attacker, host score %.0f"), PSOf(HostPC)->GetScore()));
					}
					Next(TEXT("fall within 5 s of a hit credited (+1)"));
					return true;
				}
				return Timeout(3.f, TEXT("fall elimination"));
			case 8:
			{
				AKartCharacter* Victim = KartOf(VictimPC);
				if (Victim && Victim != OldVictimKart.Get() && !Victim->IsEliminated())
				{
					OldVictimKart = Victim;
					DropBelowKillZ(Victim);
					Next(TEXT("respawned; dropped below KillZ with no attacker"));
					return true;
				}
				return Timeout(Mode->RespawnDelay + 3.f, TEXT("second respawn"));
			}
			case 9:
				if (OldVictimKart.IsValid() && OldVictimKart->IsEliminated())
				{
					if (PSOf(HostPC)->GetScore() != 2.f || PSOf(VictimPC)->GetDeaths() != 3)
					{
						return Fail(FString::Printf(TEXT("uncredited fall: expected host 2 / victim deaths 3, got %.0f / %d"), PSOf(HostPC)->GetScore(), PSOf(VictimPC)->GetDeaths()));
					}
					Mode->DebugSetMatchTimeRemaining(2.f);
					Next(TEXT("uncredited fall counted as a death only; match time set to 2 s"));
					return true;
				}
				return Timeout(3.f, TEXT("uncredited fall elimination"));
			case 10:
				if (State->IsPostMatch())
				{
					const TArray<AKartPlayerState*> Sorted = State->GetSortedPlayers();
					if (Sorted.Num() == 0 || Sorted[0] != PSOf(HostPC))
					{
						return Fail(TEXT("host should be first in the results"));
					}
					Mode->RequestPlayAgain(HostPC.Get());
					Next(TEXT("timer ended the match, host ranked #1; play again requested"));
					return true;
				}
				return Timeout(6.f, TEXT("post-match"));
			case 11:
				if (State->IsCountdown() || State->IsPlaying())
				{
					if (!AllScoresZero())
					{
						return Fail(TEXT("play again did not reset scores"));
					}
					Next(TEXT("play again: countdown, scores reset"));
					return true;
				}
				return Timeout(2.f, TEXT("countdown after play again"));
			case 12:
				if (State->IsPlaying())
				{
					Mode->RequestEndMatch(HostPC.Get());
					if (!State->IsPostMatch())
					{
						return Fail(TEXT("host end match did not reach post-match"));
					}
					Mode->RequestBackToLobby(HostPC.Get());
					Next(TEXT("match ended by host, back to lobby requested"));
					return true;
				}
				return Timeout(Mode->CountdownDuration + 3.f, TEXT("second match start"));
			case 13:
				if (State->IsLobby() && AllPlayersHaveLiveKarts())
				{
					Next(TEXT("lobby with everyone respawned"));
					UE_LOG(LogSmashKarts, Log, TEXT("[AutoTest] PASS"));
					return Finish();
				}
				return Timeout(3.f, TEXT("lobby"));
			default:
				return Finish();
			}
		}
	};

	TSharedPtr<FAutoTestRunner> GRunner;
}

void FKartAutoTest::Start(UWorld* World, int32 ExpectedPlayers, bool bQuitWhenDone)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogSmashKarts, Warning, TEXT("[AutoTest] Run it on the host"));
		return;
	}
	if (GRunner.IsValid())
	{
		GRunner->Stop();
	}
	GRunner = MakeShared<FAutoTestRunner>(World, ExpectedPlayers, bQuitWhenDone);
	GRunner->Begin();
}

#else

void FKartAutoTest::Start(UWorld* World, int32 ExpectedPlayers, bool bQuitWhenDone)
{
}

#endif // !UE_BUILD_SHIPPING
