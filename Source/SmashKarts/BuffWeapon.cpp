#include "BuffWeapon.h"
#include "KartWeaponData.h"
#include "KartCharacter.h"
#include "KartRules.h"
#include "KartStatusComponent.h"
#include "KartStatusFXComponent.h"
#include "SmashKarts.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	FString KartName(const AKartCharacter* Kart)
	{
		const APlayerState* PS = Kart ? Kart->GetPlayerState() : nullptr;
		return PS ? PS->GetPlayerName() : GetNameSafe(Kart);
	}
}

void ABuffWeapon::Fire(float ChargeAlpha)
{
	AKartCharacter* Kart = GetOwnerKart();
	if (!HasAuthority() || bActive || !WeaponData || !Kart || !Kart->GetStatusComponent() || !CanFireNow())
	{
		return;
	}
	bActive = true;
	MarkFired();
	Kart->GetStatusComponent()->ApplyStatus(WeaponData->BuffStatus, WeaponData->BuffDuration);
	SetActiveFor(WeaponData->BuffDuration);
	UE_LOG(LogSmashKarts, Log, TEXT("[Weapon] %s activated %s for %.1f s"), *KartName(Kart), *GetNameSafe(WeaponData), WeaponData->BuffDuration);

	// The status component times the buff; the weapon checks it and leaves when the status is gone
	GetWorldTimerManager().SetTimer(CheckTimer, this, &ABuffWeapon::CheckBuff, 0.1f, true);

	if (WeaponData->AuraInterval > 0.f && WeaponData->AuraRadius > 0.f)
	{
		// Zaps at 0, 1, 2... intervals while the buff lasts (Tesla Coil: 5 s / 0.5 s = 10 zaps)
		ZapsLeft = KartRules::AuraZapCount(WeaponData->BuffDuration, WeaponData->AuraInterval);
		GetWorldTimerManager().SetTimer(AuraTimer, this, &ABuffWeapon::AuraTick, WeaponData->AuraInterval, true, 0.f);
	}
}

void ABuffWeapon::CheckBuff()
{
	const AKartCharacter* Kart = GetOwnerKart();
	const bool bStatusOn = Kart && !Kart->IsEliminated() && WeaponData && Kart->HasStatus(WeaponData->BuffStatus);
	const bool bTimeUp = KartTime::ServerNow(this) >= ActiveEndServerTime + 0.25; // fallback if the status was extended elsewhere
	if (bStatusOn && !bTimeUp)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(CheckTimer);
	GetWorldTimerManager().ClearTimer(AuraTimer);
	bActive = false;
	ActiveEndServerTime = 0.0;
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Weapon] %s buff ended"), *GetNameSafe(WeaponData));
	ConsumeUse(); // Uses = 1: the weapon finishes and destroys itself. Nothing after this line
}

void ABuffWeapon::AuraTick()
{
	AKartCharacter* Kart = GetOwnerKart();
	if (!Kart || Kart->IsEliminated() || !WeaponData || ZapsLeft <= 0 || !Kart->HasStatus(WeaponData->BuffStatus))
	{
		GetWorldTimerManager().ClearTimer(AuraTimer);
		return;
	}
	--ZapsLeft;

	const FVector Center = Kart->GetActorLocation();
	const double RadiusSq = FMath::Square(static_cast<double>(WeaponData->AuraRadius));
	TArray<AKartCharacter*> Victims;
	for (TActorIterator<AKartCharacter> It(GetWorld()); It; ++It)
	{
		AKartCharacter* Other = *It;
		if (Other == Kart || Other->IsEliminated() || Other->HasStatus(EKartStatus::SpawnProtected))
		{
			continue;
		}
		if (FVector::DistSquared(Center, Other->GetActorLocation()) > RadiusSq)
		{
			continue;
		}
		if (WeaponData->bAuraNeedsLineOfSight && !CanSee(Kart, Other))
		{
			continue;
		}
		Victims.Add(Other);
	}
	if (Victims.Num() == 0)
	{
		return;
	}

	// Beams first, so a killing zap still shows
	TArray<AActor*> Targets(Victims);
	Multicast_Zap(Targets);

	AController* Attacker = Kart->GetController();
	for (AKartCharacter* Victim : Victims)
	{
		UGameplayStatics::ApplyDamage(Victim, WeaponData->AuraDamage, Attacker, this, WeaponData->GetDamageTypeOrDefault());
	}
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Weapon] %s zapped %d kart(s), %d zaps left"), *KartName(Kart), Victims.Num(), ZapsLeft);
}

bool ABuffWeapon::CanSee(const AKartCharacter* Kart, const AKartCharacter* Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BuffAuraLOS), false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Kart);
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Kart->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
}

void ABuffWeapon::Multicast_Zap_Implementation(const TArray<AActor*>& Targets)
{
	if (AKartCharacter* Kart = GetOwnerKart())
	{
		if (UKartStatusFXComponent* FX = Kart->GetStatusFX())
		{
			FX->PlayZapBeams(Targets);
		}
	}
}

void ABuffWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Taken away early (debug, round reset): the buff goes with it. Natural ends already cleared bActive
	if (EndPlayReason == EEndPlayReason::Destroyed && HasAuthority() && bActive && WeaponData)
	{
		if (AKartCharacter* Kart = GetOwnerKart())
		{
			if (UKartStatusComponent* Status = Kart->GetStatusComponent())
			{
				Status->ClearStatus(WeaponData->BuffStatus);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}
