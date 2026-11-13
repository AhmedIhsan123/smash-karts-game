#include "TrapWeapon.h"
#include "KartWeaponData.h"
#include "KartTrap.h"
#include "KartCharacter.h"
#include "Components/CapsuleComponent.h"

void ATrapWeapon::FireOnce(float ChargeAlpha)
{
	if (!WeaponData || !WeaponData->TrapClass)
	{
		return;
	}

	AKartCharacter* Kart = GetOwnerKart();
	if (!Kart)
	{
		return;
	}

	// Point behind the kart, then find the ground below it
	const FVector Behind = Kart->GetActorLocation() - Kart->GetActorForwardVector() * WeaponData->DropDistance;
	const float HalfHeight = Kart->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector DropLocation = Behind - FVector(0.f, 0.f, HalfHeight);

	// Visibility hits floors of any object type (L_Test's floor is WorldDynamic) but not karts, triggers or loot boxes
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TrapDrop), false);
	Params.AddIgnoredActor(Kart);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Behind, Behind - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
	{
		DropLocation = Hit.ImpactPoint;
	}

	const FTransform SpawnTransform(FRotator(0.f, Kart->GetActorRotation().Yaw, 0.f), DropLocation);

	AKartTrap* Trap = GetWorld()->SpawnActorDeferred<AKartTrap>(
		WeaponData->TrapClass, SpawnTransform, Kart, Kart,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (Trap)
	{
		Trap->InitTrap(WeaponData);
		Trap->FinishSpawning(SpawnTransform);
	}
}
