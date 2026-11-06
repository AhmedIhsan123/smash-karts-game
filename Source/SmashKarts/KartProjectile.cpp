#include "KartProjectile.h"
#include "KartWeaponData.h"
#include "KartCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AKartProjectile::AKartProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(20.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);       // walls, floor, props → hit
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // karts → overlap
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore); // projectiles never hit each other (side-by-side rockets)
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 0.f;  // set from data in BeginPlay
	Movement->MaxSpeed = 0.f;      // 0 = no limit
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.f;
}

void AKartProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKartProjectile, WeaponData);
	DOREPLIFETIME(AKartProjectile, LaunchSpeed);
}

void AKartProjectile::InitProjectile(UKartWeaponData* InData, float InSpeed)
{
	WeaponData = InData;
	LaunchSpeed = InSpeed;
}

void AKartProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Runs on server AND clients (replicated values arrive before BeginPlay on clients)
	if (APawn* Shooter = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
	}
	if (WeaponData)
	{
		Movement->ProjectileGravityScale = WeaponData->ProjectileGravityScale;
	}
	Movement->Velocity = GetActorForwardVector() * LaunchSpeed;

	if (HasAuthority())
	{
		Collision->OnComponentHit.AddDynamic(this, &AKartProjectile::OnHit);
		Collision->OnComponentBeginOverlap.AddDynamic(this, &AKartProjectile::OnOverlap);
		SetLifeSpan(WeaponData ? WeaponData->ProjectileLifetime : 3.f);
	}
}

void AKartProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bExploded || !OtherActor || OtherActor == GetInstigator())
	{
		return;
	}
	if (Cast<AKartCharacter>(OtherActor))
	{
		Explode(OtherActor);
	}
}

void AKartProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bExploded || (OtherActor && OtherActor == GetInstigator()))
	{
		return;
	}
	Explode(Cast<AKartCharacter>(OtherActor) ? OtherActor : nullptr);
}

void AKartProjectile::Explode(AActor* DirectHitActor)
{
	bExploded = true;
	const FVector Location = GetActorLocation();

	if (WeaponData)
	{
		AController* ShooterController = GetInstigatorController();
		const TSubclassOf<UDamageType> DamageType = WeaponData->GetDamageTypeOrDefault();

		if (DirectHitActor && WeaponData->DirectDamage > 0.f)
		{
			UGameplayStatics::ApplyDamage(DirectHitActor, WeaponData->DirectDamage, ShooterController, this, DamageType);
		}

		if (WeaponData->SplashRadius > 0.f && WeaponData->SplashMaxDamage > 0.f)
		{
			TArray<AActor*> IgnoreActors;
			if (GetInstigator()) { IgnoreActors.Add(GetInstigator()); } // no self-damage
			if (DirectHitActor)  { IgnoreActors.Add(DirectHitActor); }  // already took direct damage

			UGameplayStatics::ApplyRadialDamageWithFalloff(this,
				WeaponData->SplashMaxDamage, WeaponData->SplashMinDamage, Location,
				0.f, WeaponData->SplashRadius, 1.f, DamageType, IgnoreActors,
				this, ShooterController, ECC_Visibility);
		}
	}

	Multicast_Explode(Location);

	// Keep the actor alive a moment so the multicast arrives, but stop and hide it
	Movement->StopMovementImmediately();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	SetLifeSpan(0.5f);
}

void AKartProjectile::Multicast_Explode_Implementation(FVector Location)
{
	SetActorHiddenInGame(true);
	OnExplodeFX(Location);
}
