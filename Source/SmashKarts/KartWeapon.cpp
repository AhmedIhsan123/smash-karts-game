#include "KartWeapon.h"
#include "KartWeaponData.h"
#include "KartCharacter.h"
#include "KartWeaponComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AKartWeapon::AKartWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false); // attachment replication keeps it on the kart

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Root);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AKartWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKartWeapon, WeaponData);
	DOREPLIFETIME(AKartWeapon, UsesLeft);
}

void AKartWeapon::InitWeapon(UKartWeaponData* InData)
{
	WeaponData = InData;
	UsesLeft = InData ? FMath::Max(1, InData->Uses) : 1;
	ApplyWeaponVisuals(); // RepNotify doesn't run on the server
}

void AKartWeapon::OnRep_WeaponData()
{
	ApplyWeaponVisuals();
}

void AKartWeapon::ApplyWeaponVisuals()
{
	if (!WeaponData)
	{
		return;
	}
	if (WeaponData->HeldMesh)
	{
		WeaponMesh->SetStaticMesh(WeaponData->HeldMesh);
	}
	WeaponMesh->SetRelativeLocation(WeaponData->HeldMeshOffset);
}

void AKartWeapon::Fire(float ChargeAlpha)
{
	// Ignore presses while a burst is still going or during the cooldown
	if (!HasAuthority() || !WeaponData || BurstShotsLeft > 0 || !CanFireNow())
	{
		return;
	}

	MarkFired();
	BurstCharge = ChargeAlpha;
	BurstShotsLeft = FMath::Max(1, WeaponData->BurstCount);
	BurstTick();
}

void AKartWeapon::BurstTick()
{
	// Fire one shot; with BurstInterval 0, fire the whole burst right now
	do
	{
		FireOnce(BurstCharge);
		--BurstShotsLeft;
	}
	while (BurstShotsLeft > 0 && WeaponData->BurstInterval <= 0.f);

	if (BurstShotsLeft > 0)
	{
		GetWorldTimerManager().SetTimer(BurstTimer, this, &AKartWeapon::BurstTick, WeaponData->BurstInterval, false);
	}
	else
	{
		ConsumeUse(); // may destroy this weapon — do nothing after this line
	}
}

bool AKartWeapon::CanFireNow() const
{
	const float Interval = WeaponData ? WeaponData->FireInterval : 0.f;
	return GetWorld()->GetTimeSeconds() - LastFireTime >= Interval;
}

void AKartWeapon::MarkFired()
{
	LastFireTime = GetWorld()->GetTimeSeconds();
}

void AKartWeapon::ConsumeUse()
{
	--UsesLeft;
	if (UsesLeft <= 0)
	{
		FinishWeapon();
	}
}

void AKartWeapon::FinishWeapon()
{
	if (AKartCharacter* Kart = GetOwnerKart())
	{
		if (UKartWeaponComponent* WeaponComp = Kart->GetWeaponComponent())
		{
			WeaponComp->ClearWeapon(this);
		}
	}
	Destroy();
}

AKartCharacter* AKartWeapon::GetOwnerKart() const
{
	return Cast<AKartCharacter>(GetOwner());
}
