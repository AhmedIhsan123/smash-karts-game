#include "KartTrap.h"
#include "KartWeaponData.h"
#include "KartCharacter.h"
#include "KartStatusComponent.h"
#include "SmashKarts.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

namespace
{
	void SetupTriggerShape(UPrimitiveComponent* Shape)
	{
		Shape->SetCollisionObjectType(ECC_WorldDynamic);
		Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
		Shape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // only karts
		Shape->SetGenerateOverlapEvents(true);
	}
}

AKartTrap::AKartTrap()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false); // traps don't move

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(100.f);
	SetupTriggerShape(Trigger);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetRootComponent(Trigger);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(Trigger);
	TriggerBox->InitBoxExtent(FVector(40.f, 150.f, 40.f));
	SetupTriggerShape(TriggerBox);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // switched on by data (TriggerBoxExtent)

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Decor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Decor"));
	Decor->SetupAttachment(Trigger);
	Decor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Decor->SetGenerateOverlapEvents(false);
}

void AKartTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKartTrap, WeaponData);
	DOREPLIFETIME(AKartTrap, OwnerPlayerState);
}

void AKartTrap::InitTrap(UKartWeaponData* InData)
{
	WeaponData = InData;
	if (const APawn* Dropper = GetInstigator())
	{
		OwnerController = Dropper->GetController();
		OwnerPlayerState = Dropper->GetPlayerState();
	}
}

void AKartTrap::OnRep_OwnerPlayerState()
{
	UpdateOwnerColor();
}

void AKartTrap::BeginPlay()
{
	Super::BeginPlay();

	// Strip-shaped trap: use the box instead of the sphere
	UPrimitiveComponent* ActiveTrigger = Trigger;
	if (WeaponData && !WeaponData->TriggerBoxExtent.IsNearlyZero())
	{
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TriggerBox->SetBoxExtent(WeaponData->TriggerBoxExtent);
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ActiveTrigger = TriggerBox;
	}
	else if (WeaponData)
	{
		Trigger->SetSphereRadius(WeaponData->TriggerRadius);
	}
	BuildDecor();
	UpdateOwnerColor();

	if (HasAuthority())
	{
		ActiveTrigger->OnComponentBeginOverlap.AddDynamic(this, &AKartTrap::OnOverlap);
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

void AKartTrap::BuildDecor()
{
	if (!Decor || !DecorMesh || DecorCount <= 0)
	{
		return;
	}
	Decor->SetStaticMesh(DecorMesh);
	if (DecorMaterial)
	{
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(DecorMaterial, this))
		{
			MID->SetVectorParameterValue(ColorParameter, DecorColor);
			Decor->SetMaterial(0, MID);
		}
	}

	// Spread across the trap's width (local Y = across the kart that dropped it)
	float Width = 200.f;
	if (WeaponData)
	{
		Width = WeaponData->TriggerBoxExtent.IsNearlyZero() ? WeaponData->TriggerRadius * 2.f : static_cast<float>(WeaponData->TriggerBoxExtent.Y) * 2.f;
	}
	for (int32 i = 0; i < DecorCount; ++i)
	{
		const float Alpha = DecorCount > 1 ? static_cast<float>(i) / static_cast<float>(DecorCount - 1) : 0.5f;
		const float Y = FMath::Lerp(-Width * 0.45f, Width * 0.45f, Alpha);
		Decor->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.f, Y, DecorHeight), DecorScale));
	}
}

void AKartTrap::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	UpdateOwnerColor();
}

void AKartTrap::UpdateOwnerColor()
{
	bool bIsMine = false;
	if (OwnerPlayerState)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController() && PC->PlayerState == OwnerPlayerState)
			{
				bIsMine = true;
				break;
			}
		}
	}
	else if (const APawn* Dropper = GetInstigator())
	{
		bIsMine = Dropper->IsLocallyControlled();
	}
	OnOwnerColor(bIsMine);
}

void AKartTrap::OnOwnerColor_Implementation(bool bIsMine)
{
	if (!bTintMeshWithOwnerColor || !Mesh || Mesh->GetNumMaterials() == 0)
	{
		return;
	}
	if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(ColorParameter, bIsMine ? OwnColor : EnemyColor);
	}
}

void AKartTrap::Arm()
{
	bArmed = true;

	// Catch karts that were already on it while it armed
	UPrimitiveComponent* ActiveTrigger = Trigger.Get();
	if (TriggerBox && TriggerBox->IsCollisionEnabled())
	{
		ActiveTrigger = TriggerBox.Get();
	}
	TArray<AActor*> Overlapping;
	ActiveTrigger->GetOverlappingActors(Overlapping, AKartCharacter::StaticClass());
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
	const AKartCharacter* Kart = Cast<AKartCharacter>(OtherActor);
	if (!Kart || Kart->IsEliminated() || (OwnerPlayerState && Kart->GetPlayerState() == OwnerPlayerState)) // still safe after respawning
	{
		return;
	}
	if (WeaponData && WeaponData->bStaysAfterTrigger)
	{
		Catch(OtherActor);
		return;
	}
	Detonate(OtherActor);
}

void AKartTrap::HurtVictim(AActor* Victim)
{
	if (!WeaponData || !Victim)
	{
		return;
	}
	if (WeaponData->TrapDamage > 0.f)
	{
		UGameplayStatics::ApplyDamage(Victim, WeaponData->TrapDamage, OwnerController.IsValid() ? OwnerController.Get() : GetInstigatorController(), this,
			WeaponData->GetDamageTypeOrDefault());
	}
	AKartCharacter* Kart = Cast<AKartCharacter>(Victim);
	if (WeaponData->bApplyTrapStatus && Kart && !Kart->IsEliminated() && Kart->GetStatusComponent()
		&& !Kart->HasStatus(EKartStatus::SpawnProtected) && !Kart->HasStatus(EKartStatus::Invincible))
	{
		Kart->GetStatusComponent()->ApplyStatus(WeaponData->TrapStatus, WeaponData->TrapStatusDuration);
	}
}

void AKartTrap::Detonate(AActor* Victim)
{
	bDetonated = true;
	HurtVictim(Victim);
	Multicast_Detonate(GetActorLocation());

	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	SetLifeSpan(0.5f);
}

void AKartTrap::Catch(AActor* Victim)
{
	const AKartCharacter* Kart = Cast<AKartCharacter>(Victim);
	if (!Kart || Caught.Contains(Victim))
	{
		return;
	}
	// Protected karts roll over it unharmed (not marked: driving over it again later still counts)
	if (Kart->HasStatus(EKartStatus::SpawnProtected) || Kart->HasStatus(EKartStatus::Invincible))
	{
		return;
	}
	Caught.Add(Victim);
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Trap] %s caught %s"), *GetName(), Kart->GetPlayerState() ? *Kart->GetPlayerState()->GetPlayerName() : *GetNameSafe(Kart));
	const FVector Location = Kart->GetActorLocation();
	HurtVictim(Victim);
	Multicast_Catch(Location);
}

void AKartTrap::Multicast_Detonate_Implementation(FVector Location)
{
	SetActorHiddenInGame(true);
	OnDetonateFX(Location);
}

void AKartTrap::Multicast_Catch_Implementation(FVector Location)
{
	OnDetonateFX(Location); // the trap stays visible
}

void AKartTrap::OnDetonateFX_Implementation(FVector Location)
{
	if (DetonateFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DetonateFX, Location, FRotator::ZeroRotator, FVector(DetonateFXScale));
	}
}
