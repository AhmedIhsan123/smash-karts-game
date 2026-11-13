#include "LootBox.h"
#include "KartWeaponData.h"
#include "KartWeaponComponent.h"
#include "KartRules.h"
#include "SmashKarts.h"
#include "KartCharacter.h"
#include "Game/KartGameState.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "KartQuestionMarksComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
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

	QuestionMarks = CreateDefaultSubobject<UKartQuestionMarksComponent>(TEXT("QuestionMarks"));
	QuestionMarks->SetupAttachment(Mesh); // spins and hides with the box
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
	OnRep_Active(); // apply the initial visibility (the game mode may have set bActive before BeginPlay)
}

void ALootBox::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryGive(OtherActor);
}

void ALootBox::TryGive(AActor* OtherActor)
{
	if (!bActive)
	{
		return;
	}

	// Weapons only during the match (lobby = drive around, countdown = frozen)
	const AKartGameState* GS = GetWorld()->GetGameState<AKartGameState>();
	if (GS && !GS->IsPlaying())
	{
		return;
	}

	AKartCharacter* Kart = Cast<AKartCharacter>(OtherActor);
	if (!Kart || Kart->IsEliminated())
	{
		return;
	}

	UKartWeaponComponent* WeaponComp = Kart->GetWeaponComponent();
	if (!WeaponComp || WeaponComp->HasWeapon()) // one weapon at a time: drive through with a weapon = no pickup
	{
		return;
	}

	UKartWeaponData* Data = RollWeapon();
	if (!Data)
	{
		UE_LOG(LogSmashKarts, Warning, TEXT("[LootBox] %s: pool empty or all weights 0"), *GetName());
		return;
	}

	if (WeaponComp->GiveWeapon(Data))
	{
		Multicast_PickedUp();
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
	RecheckOverlaps(); // a kart may already be sitting inside the box
}

void ALootBox::ResetBox(bool bInActive)
{
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	if (bActive != bInActive)
	{
		bActive = bInActive;
		OnRep_Active();
	}
}

void ALootBox::RecheckOverlaps()
{
	if (!HasAuthority() || !bActive)
	{
		return;
	}
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
	TArray<float> Weights;
	Weights.Reserve(WeaponPool.Num());
	for (const UKartWeaponData* Data : WeaponPool)
	{
		Weights.Add(Data ? Data->DropWeight : 0.f);
	}
	const int32 Index = KartRules::PickWeightedIndex(Weights, FMath::FRand());
	return WeaponPool.IsValidIndex(Index) ? WeaponPool[Index].Get() : nullptr;
}

void ALootBox::Multicast_PickedUp_Implementation()
{
	if (PickupFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupFX, GetActorLocation(), FRotator::ZeroRotator, FVector(PickupFXScale));
	}
}

void ALootBox::OnRep_Active()
{
	if (Mesh)
	{
		Mesh->SetVisibility(bActive, true);
	}
	OnActiveChanged(bActive);
}
