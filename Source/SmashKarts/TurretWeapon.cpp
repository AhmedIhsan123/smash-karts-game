#include "TurretWeapon.h"
#include "KartWeaponData.h"
#include "KartCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATurretWeapon::ATurretWeapon()
{
	PrimaryActorTick.bCanEverTick = true; // turns the gun mesh toward the target on every machine
}

void ATurretWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATurretWeapon, CurrentTarget);
}

void ATurretWeapon::Fire(float ChargeAlpha)
{
	if (!HasAuthority() || bActive || !WeaponData)
	{
		return;
	}

	bActive = true;
	SetActiveFor(WeaponData->TurretDuration);
	const float Interval = 1.f / FMath::Max(1.f, WeaponData->ShotsPerSecond);
	GetWorldTimerManager().SetTimer(ShootTimer, this, &ATurretWeapon::ShootTick, Interval, true);
	GetWorldTimerManager().SetTimer(DurationTimer, this, &ATurretWeapon::EndTurret, WeaponData->TurretDuration, false);
}

void ATurretWeapon::ShootTick()
{
	AKartCharacter* Kart = GetOwnerKart();
	if (!Kart || !WeaponData)
	{
		return;
	}

	AActor* NewTarget = FindTarget();
	if (NewTarget != CurrentTarget)
	{
		CurrentTarget = NewTarget;
		OnRep_CurrentTarget(); // RepNotify doesn't run on the server
	}
	if (!CurrentTarget)
	{
		return; // only fires while locked
	}

	const float Distance = FVector::Dist(Kart->GetActorLocation(), CurrentTarget->GetActorLocation());
	const float Alpha = FMath::Clamp(Distance / WeaponData->LockRange, 0.f, 1.f);
	const float Damage = FMath::Lerp(WeaponData->DamageNear, WeaponData->DamageFar, Alpha);

	UGameplayStatics::ApplyDamage(CurrentTarget, Damage, GetInstigatorController(), this,
		WeaponData->GetDamageTypeOrDefault());
}

void ATurretWeapon::EndTurret()
{
	GetWorldTimerManager().ClearTimer(ShootTimer);
	GetWorldTimerManager().ClearTimer(DurationTimer);
	CurrentTarget = nullptr;
	OnRep_CurrentTarget();
	ConsumeUse(); // Uses = 1 → weapon finishes and destroys itself
}

AActor* ATurretWeapon::FindTarget() const
{
	const AKartCharacter* Kart = GetOwnerKart();
	if (!Kart)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDistSq = FMath::Square(WeaponData->LockRange);

	for (TActorIterator<AKartCharacter> It(GetWorld()); It; ++It)
	{
		AKartCharacter* Other = *It;
		if (Other == Kart || Other->IsEliminated())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Kart->GetActorLocation(), Other->GetActorLocation());
		if (DistSq < BestDistSq && HasLineOfSight(Other))
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	}
	return Best;
}

bool ATurretWeapon::HasLineOfSight(const AActor* Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TurretLOS), false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit,
		GetOwner()->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);

	return !bBlocked || Hit.GetActor() == Target;
}

void ATurretWeapon::OnRep_CurrentTarget()
{
	OnTargetChanged(CurrentTarget);
}

void ATurretWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FRotator Desired = GetActorRotation();
	if (CurrentTarget)
	{
		Desired = (CurrentTarget->GetActorLocation() - WeaponMesh->GetComponentLocation()).Rotation();
	}
	Desired.Pitch = 0.f;
	Desired.Roll = 0.f;

	WeaponMesh->SetWorldRotation(FMath::RInterpTo(WeaponMesh->GetComponentRotation(), Desired, DeltaSeconds, 15.f));
}
