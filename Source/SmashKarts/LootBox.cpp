#include "LootBox.h"
#include "KartWeaponData.h"
#include "KartWeaponComponent.h"
#include "SmashKarts.h"
#include "KartCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ALootBox::ALootBox()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false); // spin is cosmetic, each machine spins its own copy

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(100.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Rotator = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Rotator"));
	Rotator->RotationRate = FRotator(0.f, 90.f, 0.f);
}

void ALootBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALootBox, bActive);
}

void ALootBox::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ALootBox::OnOverlap);
	}
}

void ALootBox::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogSmashKarts, Warning, TEXT("[LootBox] %s overlapped by %s (comp %s)"),
		*GetName(), *GetNameSafe(OtherActor), *GetNameSafe(OtherComp));
	TryGive(OtherActor);
}

void ALootBox::TryGive(AActor* OtherActor)
{
	if (!bActive) { UE_LOG(LogSmashKarts, Warning, TEXT("[LootBox] inactive")); return; }

	AKartCharacter* Kart = Cast<AKartCharacter>(OtherActor);
	if (!Kart) { UE_LOG(LogSmashKarts, Warning, TEXT("[LootBox] not a kart")); return; }

	UKartWeaponComponent* WeaponComp = Kart->GetWeaponComponent();
	if (!WeaponComp) { UE_LOG(LogSmashKarts, Warning, TEXT("[LootBox] kart has NO WeaponComponent")); return; }
	if (WeaponComp->HasWeapon()) { UE_LOG(LogSmashKarts, Warning, TEXT("[LootBox] kart already has a weapon")); return; }

	UKartWeaponData* Data = RollWeapon();
	if (!Data) { UE_LOG(LogSmashKarts, Warning, TEXT("[LootBox] roll returned nothing - pool empty or weights 0")); return; }

	if (WeaponComp->GiveWeapon(Data))
	{
		bActive = false;
		OnRep_Active();
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &ALootBox::Respawn, RespawnTime, false);
	}
	else
	{
		UE_LOG(LogSmashKarts, Warning, TEXT("[LootBox] GiveWeapon failed - check Weapon Class in %s"), *GetNameSafe(Data));
	}
}

void ALootBox::Respawn()
{
	bActive = true;
	OnRep_Active();

	// A kart may already be sitting inside the box
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, AKartCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		TryGive(Actor);
		if (!bActive)
		{
			break;
		}
	}
}

UKartWeaponData* ALootBox::RollWeapon() const
{
	float TotalWeight = 0.f;
	for (UKartWeaponData* Data : WeaponPool)
	{
		if (Data)
		{
			TotalWeight += FMath::Max(0.f, Data->DropWeight);
		}
	}
	if (TotalWeight <= 0.f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	UKartWeaponData* LastValid = nullptr;
	for (UKartWeaponData* Data : WeaponPool)
	{
		if (!Data || Data->DropWeight <= 0.f)
		{
			continue;
		}
		LastValid = Data;
		Roll -= Data->DropWeight;
		if (Roll <= 0.f)
		{
			return Data;
		}
	}
	return LastValid; // float rounding safety
}

void ALootBox::OnRep_Active()
{
	Mesh->SetVisibility(bActive, true);
	OnActiveChanged(bActive);
}
