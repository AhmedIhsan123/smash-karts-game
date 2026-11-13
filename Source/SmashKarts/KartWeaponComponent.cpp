#include "KartWeaponComponent.h"
#include "KartWeapon.h"
#include "KartWeaponData.h"
#include "KartCharacter.h"
#include "KartStatusComponent.h"
#include "SmashKarts.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

UKartWeaponComponent::UKartWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKartWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKartWeaponComponent, CurrentWeapon);
}

void UKartWeaponComponent::FirePressed()
{
	if (!CurrentWeapon) // skip the RPC if we have nothing to fire
	{
		return;
	}
	const UKartWeaponData* Data = CurrentWeapon->GetWeaponData();
	if (Data && Data->bChargeThrow)
	{
		bCharging = true;
		ChargeStartTime = GetWorld()->GetTimeSeconds();
		return;
	}
	Server_Fire(0.f);
}

void UKartWeaponComponent::FireReleased()
{
	if (!bCharging)
	{
		return;
	}
	const float Alpha = GetChargeAlpha();
	bCharging = false;
	if (CurrentWeapon)
	{
		Server_Fire(Alpha);
	}
}

float UKartWeaponComponent::GetChargeAlpha() const
{
	if (!bCharging || !CurrentWeapon)
	{
		return 0.f;
	}
	const UKartWeaponData* Data = CurrentWeapon->GetWeaponData();
	const float FullTime = FMath::Max(0.05f, Data ? Data->ChargeTime : 1.f);
	return FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - ChargeStartTime) / FullTime, 0.f, 1.f);
}

void UKartWeaponComponent::Server_Fire_Implementation(float ChargeAlpha)
{
	FireFromServer(ChargeAlpha);
}

void UKartWeaponComponent::FireFromServer(float ChargeAlpha)
{
	AKartCharacter* Kart = Cast<AKartCharacter>(GetOwner());
	if (!CurrentWeapon || !Kart || !Kart->HasAuthority() || Kart->IsInputLocked())
	{
		return;
	}
	// Firing ends spawn protection early
	if (UKartStatusComponent* Status = Kart->GetStatusComponent())
	{
		Status->ClearStatus(EKartStatus::SpawnProtected);
	}
	CurrentWeapon->Fire(FMath::Clamp(ChargeAlpha, 0.f, 1.f)); // never trust client values
}

void UKartWeaponComponent::DestroyWeapon()
{
	if (!CurrentWeapon || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	AKartWeapon* Weapon = CurrentWeapon;
	CurrentWeapon = nullptr;
	OnRep_CurrentWeapon();
	Weapon->Destroy();
}

bool UKartWeaponComponent::GiveWeapon(UKartWeaponData* Data)
{
	AKartCharacter* Kart = Cast<AKartCharacter>(GetOwner());
	if (!Kart || !Kart->HasAuthority() || CurrentWeapon || !Data || !Data->WeaponClass)
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.Owner = Kart;
	Params.Instigator = Kart;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AKartWeapon* Weapon = GetWorld()->SpawnActor<AKartWeapon>(Data->WeaponClass, Kart->GetActorTransform(), Params);
	if (!Weapon)
	{
		return false;
	}

	Weapon->InitWeapon(Data);
	// Attach to the Mesh (not the capsule) so it gets the same network smoothing as the kart visuals
	Weapon->AttachToComponent(Kart->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	CurrentWeapon = Weapon;
	OnRep_CurrentWeapon();

	UE_LOG(LogSmashKarts, Log, TEXT("[Weapon] %s picked up %s"), *GetNameSafe(Kart), *GetNameSafe(Data));
	return true;
}

void UKartWeaponComponent::ClearWeapon(AKartWeapon* Weapon)
{
	if (CurrentWeapon == Weapon)
	{
		UE_LOG(LogSmashKarts, Log, TEXT("[Weapon] %s used up %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Weapon));
		CurrentWeapon = nullptr;
		OnRep_CurrentWeapon();
	}
}

void UKartWeaponComponent::OnRep_CurrentWeapon()
{
	bCharging = false; // a charge never carries over to another weapon
	OnWeaponChanged.Broadcast(CurrentWeapon);
}

void UKartWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CurrentWeapon && GetOwner() && GetOwner()->HasAuthority())
	{
		CurrentWeapon->Destroy();
		CurrentWeapon = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
