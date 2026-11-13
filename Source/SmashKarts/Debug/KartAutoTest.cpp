#include "Debug/KartAutoTest.h"

#if !UE_BUILD_SHIPPING

#include "Game/KartGameMode.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerState.h"
#include "Game/KartPlayerController.h"
#include "KartWeaponData.h"
#include "KartWeapon.h"
#include "KartWeaponComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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
		FAutoTestRunner(UWorld* InWorld, int32 InExpected, bool bInQuit, const FString& InScenario)
			: World(InWorld), Expected(FMath::Max(2, InExpected)), bQuit(bInQuit), bStatusScenario(InScenario.Equals(TEXT("status"), ESearchCase::IgnoreCase)), bWeaponsScenario(InScenario.Equals(TEXT("weapons"), ESearchCase::IgnoreCase))
		{
		}

		void Begin()
		{
			StepStart = FPlatformTime::Seconds();
			UE_LOG(LogSmashKarts, Log, TEXT("[AutoTest] Started (%s), waiting for %d players"), bStatusScenario ? TEXT("status") : (bWeaponsScenario ? TEXT("weapons") : TEXT("loop")), Expected);
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
		TWeakObjectPtr<AKartCharacter> OldHostKart;
		bool bStatusScenario = false;
		bool bWeaponsScenario = false;
		float RecordedHealth = 0.f;
		FVector RecordedLocation = FVector::ZeroVector;

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
					if (bWeaponsScenario)
					{
						Step = 200;
					}
					if (bStatusScenario)
					{
						Step = 100;
						if (AKartPlayerController* KPC = Cast<AKartPlayerController>(VictimPC.Get()))
						{
							KPC->Client_DebugExec(TEXT("SK.AutoDrive 120 0.6")); // the client drives like a player
						}
						GEngine->Exec(World.Get(), TEXT("NetEmulation.PktLag 60")); // server -> client lag
					}
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

			// ================= Status scenario (phase 2) =================
			case 100:
			{
				if (Elapsed() < 2.f)
				{
					return true; // let the client's auto-drive get up to speed first
				}
				AKartCharacter* Victim = KartOf(VictimPC);
				if (!Victim || Victim->IsEliminated())
				{
					return Fail(TEXT("victim kart missing"));
				}
				Victim->GetStatusComponent()->ApplyStatus(EKartStatus::Frozen, 1.5f);
				Next(TEXT("victim frozen (1.5 s) while driving"));
				return true;
			}
			case 101:
				if (Elapsed() >= 0.8f)
				{
					AKartCharacter* Victim = KartOf(VictimPC);
					const float Speed = Victim ? static_cast<float>(Victim->GetVelocity().Size2D()) : -1.f;
					if (!Victim || Speed > 60.f || !Victim->IsInputLocked())
					{
						return Fail(FString::Printf(TEXT("frozen kart should stop and lock input (speed %.0f)"), Speed));
					}
					Next(TEXT("frozen kart stopped and input locked"));
				}
				return true;
			case 102:
			{
				AKartCharacter* Victim = KartOf(VictimPC);
				if (Victim && !Victim->HasStatus(EKartStatus::Frozen))
				{
					Victim->GetStatusComponent()->ApplyStatus(EKartStatus::Invincible, 2.f);
					Victim->GetStatusComponent()->ApplyStatus(EKartStatus::Frozen, 2.f);
					const bool bFrozen = Victim->HasStatus(EKartStatus::Frozen);
					Victim->GetStatusComponent()->ClearStatus(EKartStatus::Invincible);
					if (bFrozen)
					{
						return Fail(TEXT("an invincible kart was frozen"));
					}
					Next(TEXT("freeze expired; invincible kart can't be frozen"));
					return true;
				}
				return Timeout(2.5f, TEXT("freeze to expire"));
			}
			case 103:
			{
				AKartCharacter* Victim = KartOf(VictimPC);
				const UKartWeaponData* Rockets = Mode->FindWeaponByName(TEXT("Rockets"));
				if (!Victim || !Rockets || !Rockets->DamageType)
				{
					return Fail(TEXT("need the victim and DA_Rockets with a damage type"));
				}
				const FVector Origin = Victim->GetActorLocation() - Victim->GetActorForwardVector() * 150.f;
				UGameplayStatics::ApplyRadialDamageWithFalloff(World.Get(), 20.f, 5.f, Origin, 0.f, 300.f, 1.f, Rockets->DamageType,
					TArray<AActor*>(), nullptr, HostPC.Get(), ECC_Visibility);
				Next(TEXT("explosion 150 cm behind the victim"));
				return true;
			}
			case 104:
			{
				AKartCharacter* Victim = KartOf(VictimPC);
				if (Victim && (Victim->GetCharacterMovement()->IsFalling() || Victim->GetVelocity().Z > 50.f))
				{
					Next(TEXT("explosion knocked the victim into the air"));
					return true;
				}
				return Timeout(1.5f, TEXT("knockback"));
			}
			case 105:
			{
				if (Elapsed() < 1.5f)
				{
					return true; // land first
				}
				AKartCharacter* Host = KartOf(HostPC);
				AKartCharacter* Victim = KartOf(VictimPC);
				if (!Host || !Victim)
				{
					return Fail(TEXT("karts missing before the spiky test"));
				}
				OldVictimKart = Victim;
				Host->GetStatusComponent()->ApplyStatus(EKartStatus::Spiky, 3.f);
				Victim->SetActorLocation(Host->GetActorLocation() + Host->GetActorRightVector() * 150.f, false, nullptr, ETeleportType::TeleportPhysics);
				Next(TEXT("host spiky, victim placed inside mace reach"));
				return true;
			}
			case 106:
				if (OldVictimKart.IsValid() && OldVictimKart->IsEliminated())
				{
					if (PSOf(HostPC)->GetScore() < 1.f)
					{
						return Fail(TEXT("spiky smash should credit the spiky kart"));
					}
					if (AKartCharacter* Host = KartOf(HostPC))
					{
						Host->GetStatusComponent()->ClearStatus(EKartStatus::Spiky);
					}
					Next(TEXT("spiky contact smashed the victim (+1)"));
					return true;
				}
				return Timeout(1.5f, TEXT("spiky contact smash"));
			case 107:
			case 110:
			{
				AKartCharacter* Host = KartOf(HostPC);
				AKartCharacter* Victim = KartOf(VictimPC);
				const bool bReady = Host && Victim && !Host->IsEliminated() && !Victim->IsEliminated()
					&& !Host->HasStatus(EKartStatus::SpawnProtected) && !Victim->HasStatus(EKartStatus::SpawnProtected);
				if (bReady)
				{
					Next(TEXT("both karts alive and unprotected"));
					return true;
				}
				return Timeout(Mode->RespawnDelay + Mode->SpawnProtectionDuration + 4.f, TEXT("respawns"));
			}
			case 108:
			{
				AKartCharacter* Host = KartOf(HostPC);
				AKartCharacter* Victim = KartOf(VictimPC);
				OldVictimKart = Victim;
				OldHostKart = Host;
				Host->GetStatusComponent()->ApplyStatus(EKartStatus::Spiky, 4.f);
				Victim->GetStatusComponent()->ApplyStatus(EKartStatus::Spiky, 4.f);
				Victim->SetActorLocation(Host->GetActorLocation() + Host->GetActorRightVector() * 150.f, false, nullptr, ETeleportType::TeleportPhysics);
				Next(TEXT("both spiky, placed in reach of each other"));
				return true;
			}
			case 109:
				if (OldVictimKart.IsValid() && OldHostKart.IsValid() && OldVictimKart->IsEliminated() && OldHostKart->IsEliminated())
				{
					Next(TEXT("two spiky karts smashed each other"));
					return true;
				}
				return Timeout(2.5f, TEXT("mutual spiky smash"));
			case 111:
			{
				AKartCharacter* Host = KartOf(HostPC);
				AKartCharacter* Victim = KartOf(VictimPC);
				OldVictimKart = Victim;
				Victim->GetStatusComponent()->ApplyStatus(EKartStatus::Frozen, 3.f);
				const FVector Back = Victim->GetActorForwardVector().GetSafeNormal2D();
				Host->SetActorLocation(Victim->GetActorLocation() - Back * 220.f, false, nullptr, ETeleportType::TeleportPhysics);
				Host->GetCharacterMovement()->Velocity = Back * 900.f;
				Next(TEXT("victim frozen, host driving into it at 900"));
				return true;
			}
			case 112:
				if (OldVictimKart.IsValid() && OldVictimKart->IsEliminated())
				{
					Next(TEXT("ramming a frozen kart smashed it"));
					return true;
				}
				return Timeout(1.5f, TEXT("frozen ram smash"));
			case 113:
				GEngine->Exec(World.Get(), TEXT("NetEmulation.Off"));
				Mode->RequestEndMatch(HostPC.Get());
				Mode->RequestBackToLobby(HostPC.Get());
				Next(TEXT("status scenario done, back to the lobby"));
				Step = 13;
				return true;

			// ================= Weapons scenario (phase 3) =================
			case 200:
				// Clear lane at y = -2000 (away from the L_Test ramp and loot boxes): host at x -2000 facing +X
				if (!PlaceForShot(0.f, 1200.f))
				{
					return Fail(TEXT("could not place karts"));
				}
				if (!GiveHost(TEXT("Bullets")))
				{
					return Fail(TEXT("could not give Bullets"));
				}
				Next(TEXT("karts placed 12 m apart, host has Bullets"));
				return true;
			case 201:
				if (Elapsed() >= 0.3f)
				{
					RecordedHealth = HealthOf(VictimPC);
					FireHost(0.f);
					Next(TEXT("bullets burst fired"));
				}
				return true;
			case 202:
				if (HealthOf(VictimPC) < RecordedHealth)
				{
					Next(*FString::Printf(TEXT("bullets hit: %.0f -> %.0f hp"), RecordedHealth, HealthOf(VictimPC)));
					return true;
				}
				return Timeout(1.5f, TEXT("bullet hits"));
			case 203:
				// Seeker: aim 20 degrees off a victim 25 m away; only homing can hit
				if (!PlaceForShot(20.f, 2500.f) || !GiveHost(TEXT("SeekerMissile")))
				{
					return Fail(TEXT("seeker setup failed"));
				}
				Next(TEXT("host aims 20 deg off with a Seeker Missile"));
				return true;
			case 204:
				if (Elapsed() >= 0.3f)
				{
					RecordedHealth = HealthOf(VictimPC);
					OldVictimKart = KartOf(VictimPC);
					FireHost(0.f);
					Next(TEXT("seeker fired"));
				}
				return true;
			case 205:
				if ((OldVictimKart.IsValid() && OldVictimKart->IsEliminated()) || HealthOf(VictimPC) <= RecordedHealth - 25.f)
				{
					Next(TEXT("seeker homed in and hit"));
					return true;
				}
				return Timeout(4.f, TEXT("seeker hit"));
			case 206:
			case 210:
			{
				AKartCharacter* Victim = KartOf(VictimPC);
				if (Victim && !Victim->IsEliminated() && !Victim->HasStatus(EKartStatus::SpawnProtected))
				{
					Next(TEXT("victim alive and unprotected"));
					return true;
				}
				return Timeout(Mode->RespawnDelay + Mode->SpawnProtectionDuration + 4.f, TEXT("victim respawn"));
			}
			case 207:
				// Sticky Bomb: charge throw (alpha 0.45 lands ~25 m out), proximity alarm then explosion
				if (!PlaceForShot(0.f, 2500.f) || !GiveHost(TEXT("StickyBomb")))
				{
					return Fail(TEXT("sticky setup failed"));
				}
				Next(TEXT("host has a Sticky Bomb, victim 25 m ahead"));
				return true;
			case 208:
				if (Elapsed() >= 0.3f)
				{
					RecordedHealth = HealthOf(VictimPC);
					OldVictimKart = KartOf(VictimPC);
					FireHost(0.45f);
					Next(TEXT("sticky bomb thrown"));
				}
				return true;
			case 209:
				if ((OldVictimKart.IsValid() && OldVictimKart->IsEliminated()) || HealthOf(VictimPC) < RecordedHealth)
				{
					Next(TEXT("sticky bomb landed and blew up near the victim"));
					return true;
				}
				return Timeout(6.f, TEXT("sticky bomb explosion"));
			case 211:
				if (!PlaceForShot(0.f, 2500.f) || !GiveHost(TEXT("CannonBalls")))
				{
					return Fail(TEXT("cannon setup failed"));
				}
				Next(TEXT("host has Cannon Balls, victim 25 m ahead"));
				return true;
			case 212:
				if (Elapsed() >= 0.3f)
				{
					RecordedHealth = HealthOf(VictimPC);
					OldVictimKart = KartOf(VictimPC);
					FireHost(0.45f);
					Next(TEXT("cannon volley thrown"));
				}
				return true;
			case 213:
				if ((OldVictimKart.IsValid() && OldVictimKart->IsEliminated()) || HealthOf(VictimPC) < RecordedHealth)
				{
					Next(TEXT("cannon balls hit the victim"));
					Mode->RequestEndMatch(HostPC.Get());
					Mode->RequestBackToLobby(HostPC.Get());
					Step = 13;
					return true;
				}
				return Timeout(5.f, TEXT("cannon ball hits"));

			default:
				return Finish();
			}
		}

		// ---------- weapons scenario helpers ----------
		float HealthOf(const TWeakObjectPtr<APlayerController>& PC) const
		{
			const AKartCharacter* Kart = KartOf(PC);
			return (Kart && Kart->GetHealthComponent() && !Kart->IsEliminated()) ? Kart->GetHealthComponent()->GetHealth() : 0.f;
		}

		/** Host at (-2000, -2000) facing +X rotated by YawOffset; victim straight down +X at Distance. */
		bool PlaceForShot(float YawOffset, float Distance)
		{
			AKartCharacter* Host = KartOf(HostPC);
			AKartCharacter* Victim = KartOf(VictimPC);
			if (!Host || !Victim)
			{
				return false;
			}
			const FVector HostLoc(-2000.f, -2000.f, 60.f);
			const FRotator Aim(0.f, YawOffset, 0.f);
			HostPC->SetControlRotation(Aim);
			Host->SetActorLocationAndRotation(HostLoc, Aim, false, nullptr, ETeleportType::TeleportPhysics);
			Host->GetCharacterMovement()->StopMovementImmediately();
			Victim->SetActorLocation(HostLoc + FVector(Distance, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
			Victim->GetCharacterMovement()->StopMovementImmediately();
			return true;
		}

		bool GiveHost(const TCHAR* WeaponName)
		{
			AKartCharacter* Host = KartOf(HostPC);
			UKartWeaponData* Data = GM() ? GM()->FindWeaponByName(WeaponName) : nullptr;
			if (!Host || !Data || !Host->GetWeaponComponent())
			{
				return false;
			}
			Host->GetWeaponComponent()->DestroyWeapon();
			return Host->GetWeaponComponent()->GiveWeapon(Data);
		}

		void FireHost(float ChargeAlpha)
		{
			AKartCharacter* Host = KartOf(HostPC);
			AKartWeapon* Weapon = (Host && Host->GetWeaponComponent()) ? Host->GetWeaponComponent()->GetCurrentWeapon() : nullptr;
			if (Weapon)
			{
				Weapon->Fire(ChargeAlpha);
			}
		}
	};

	TSharedPtr<FAutoTestRunner> GRunner;
}

void FKartAutoTest::Start(UWorld* World, int32 ExpectedPlayers, bool bQuitWhenDone, const FString& Scenario)
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
	GRunner = MakeShared<FAutoTestRunner>(World, ExpectedPlayers, bQuitWhenDone, Scenario);
	GRunner->Begin();
}

#else

void FKartAutoTest::Start(UWorld* World, int32 ExpectedPlayers, bool bQuitWhenDone, const FString& Scenario)
{
}

#endif // !UE_BUILD_SHIPPING
