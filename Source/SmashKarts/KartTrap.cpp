#include "KartTrap.h"
#include "KartWeaponData.h"
#include "KartCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AKartTrap::AKartTrap()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false); // traps don't move

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(100.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // only karts
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AKartTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKartTrap, WeaponData);
}

void AKartTrap::InitTrap(UKartWeaponData* InData)
{
	WeaponData = InData;
}

void AKartTrap::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponData)
	{
		Trigger->SetSphereRadius(WeaponData->TriggerRadius);
	}
	UpdateOwnerColor();

	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AKartTrap::OnOverlap);
		SetLifeSpan(WeaponData ? WeaponData->TrapLifetime : 30.f);

		const float Delay = WeaponData ? WeaponData->ArmingDelay : 0.5f;
		if (Delay > 0.f)
		{
			GetWorldTimerManager().SetTimer(ArmTimer, this, &AKartTrap::Arm, Delay, false);
		}
		else
		{
			Arm();
		}
	}
}

void AKartTrap::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	UpdateOwnerColor();
}

void AKartTrap::UpdateOwnerColor()
{
	const APawn* Dropper = GetInstigator();
	OnOwnerColor(Dropper && Dropper->IsLocallyControlled());
}

void AKartTrap::Arm()
{
	bArmed = true;

	// Catch karts that were already standing on it while it armed
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, AKartCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		TryTrigger(Actor);
		if (bDetonated)
		{
			break;
		}
	}
}

void AKartTrap::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryTrigger(OtherActor);
}

void AKartTrap::TryTrigger(AActor* OtherActor)
{
	if (!bArmed || bDetonated || !OtherActor || OtherActor == GetInstigator()) // own traps are safe
	{
		return;
	}
	if (Cast<AKartCharacter>(OtherActor))
	{
		Detonate(OtherActor);
	}
}

void AKartTrap::Detonate(AActor* Victim)
{
	bDetonated = true;

	if (WeaponData && WeaponData->TrapDamage > 0.f)
	{
		UGameplayStatics::ApplyDamage(Victim, WeaponData->TrapDamage, GetInstigatorController(), this,
			WeaponData->GetDamageTypeOrDefault());
	}

	Multicast_Detonate(GetActorLocation());

	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	SetLifeSpan(0.5f);
}

void AKartTrap::Multicast_Detonate_Implementation(FVector Location)
{
	SetActorHiddenInGame(true);
	OnDetonateFX(Location);
}
