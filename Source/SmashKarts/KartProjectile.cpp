#include "KartProjectile.h"
#include "KartFeedback.h"
#include "KartWeaponData.h"
#include "KartCharacter.h"
#include "KartRules.h"
#include "KartStatusComponent.h"
#include "SmashKarts.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

AKartProjectile::AKartProjectile()
{
	PrimaryActorTick.bCanEverTick = true;           // only homing missiles tick (proximity fuse, server)
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(20.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_KartProjectile);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);       // walls, floor, props (static or dynamic) → hit
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // karts → overlap
	Collision->SetCollisionResponseToChannel(ECC_KartProjectile, ECR_Ignore); // projectiles never hit each other (side-by-side rockets)
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
	DOREPLIFETIME(AKartProjectile, InheritedVelocity);
	DOREPLIFETIME(AKartProjectile, HomingTarget);
	DOREPLIFETIME(AKartProjectile, bLanded);
	DOREPLIFETIME(AKartProjectile, LandedLocation);
}

void AKartProjectile::InitProjectile(UKartWeaponData* InData, float InSpeed, const FVector& InInheritedVelocity)
{
	WeaponData = InData;
	LaunchSpeed = InSpeed;
	InheritedVelocity = InInheritedVelocity;
	if (const APawn* Shooter = GetInstigator())
	{
		OwnerController = Shooter->GetController();
		OwnerPlayerState = Shooter->GetPlayerState();
	}
}

bool AKartProjectile::IsOwnerKart(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	if (Actor == GetInstigator())
	{
		return true;
	}
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && OwnerPlayerState.IsValid() && Pawn->GetPlayerState() == OwnerPlayerState.Get();
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
		if (WeaponData->bHoming)
		{
			Movement->MaxSpeed = LaunchSpeed; // homing turns the missile without speeding it up
		}
	}
	Movement->Velocity = GetActorForwardVector() * LaunchSpeed + FVector(InheritedVelocity);

	if (TintColor.A > 0.f && Mesh && Mesh->GetNumMaterials() > 0)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TintParameter, FLinearColor(TintColor.R, TintColor.G, TintColor.B, 1.f));
		}
	}

	if (HasAuthority())
	{
		SpawnLocation = GetActorLocation();
		UE_LOG(LogSmashKarts, Verbose, TEXT("[Projectile] %s launched at %s vel %s gravity %.2f"), *GetName(),
			*GetActorLocation().ToCompactString(), *Movement->Velocity.ToCompactString(), Movement->ProjectileGravityScale);
		Collision->OnComponentHit.AddDynamic(this, &AKartProjectile::OnHit);
		Collision->OnComponentBeginOverlap.AddDynamic(this, &AKartProjectile::OnOverlap);
		SetLifeSpan(WeaponData ? WeaponData->ProjectileLifetime : 3.f);

		if (WeaponData && WeaponData->bHoming)
		{
			GetWorldTimerManager().SetTimer(HomingTimer, this, &AKartProjectile::UpdateHoming, 0.1f, true, FMath::Max(0.01f, WeaponData->HomingDelay));
		}
	}
}

// =====================================================================
// Hits
// =====================================================================

void AKartProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bExploded || bLanded || !OtherActor || IsOwnerKart(OtherActor))
	{
		return;
	}
	const AKartCharacter* Kart = Cast<AKartCharacter>(OtherActor);
	if (!Kart || Kart->IsEliminated())
	{
		return;
	}
	if (WeaponData && WeaponData->bStickOnImpact)
	{
		Land(OtherActor, OtherComp);
	}
	else
	{
		Explode(OtherActor);
	}
}

void AKartProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Projectile] %s hit %s / %s"), *GetName(), *GetNameSafe(OtherActor), *GetNameSafe(OtherComp));
	if (bExploded || bLanded || IsOwnerKart(OtherActor))
	{
		return;
	}
	if (WeaponData && WeaponData->bStickOnImpact)
	{
		Land(OtherActor, OtherComp);
		return;
	}
	Explode(Cast<AKartCharacter>(OtherActor) ? OtherActor : nullptr);
}

void AKartProjectile::Explode(AActor* DirectHitActor)
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Projectile] %s exploded at %s, direct hit %s, travelled %.0f"), *GetName(),
		*GetActorLocation().ToCompactString(), *GetNameSafe(DirectHitActor), FVector::Dist(SpawnLocation, GetActorLocation()));
	GetWorldTimerManager().ClearTimer(HomingTimer);
	GetWorldTimerManager().ClearTimer(FuseTimer);
	GetWorldTimerManager().ClearTimer(ProximityTimer);
	GetWorldTimerManager().ClearTimer(AlarmTimer);
	const FVector Location = GetActorLocation();

	if (WeaponData)
	{
		AController* ShooterController = OwnerController.IsValid() ? OwnerController.Get() : GetInstigatorController();
		const TSubclassOf<UDamageType> DamageType = WeaponData->GetDamageTypeOrDefault();

		if (DirectHitActor && WeaponData->DirectDamage > 0.f)
		{
			// Bullets lose damage over distance travelled
			float Damage = WeaponData->DirectDamage;
			if (WeaponData->MaxRange > 0.f)
			{
				const float Far = WeaponData->DirectDamageFar >= 0.f ? WeaponData->DirectDamageFar : WeaponData->DirectDamage;
				Damage = KartRules::DamageByDistance(static_cast<float>(FVector::Dist(SpawnLocation, Location)), WeaponData->MaxRange, WeaponData->DirectDamage, Far);
			}
			UGameplayStatics::ApplyDamage(DirectHitActor, Damage, ShooterController, this, DamageType);

			// On-hit status (Snowball freeze) for karts that survived and aren't protected
			AKartCharacter* Kart = Cast<AKartCharacter>(DirectHitActor);
			if (WeaponData->bApplyHitStatus && Kart && !Kart->IsEliminated() && !Kart->HasStatus(EKartStatus::SpawnProtected)
				&& Kart->GetStatusComponent())
			{
				Kart->GetStatusComponent()->ApplyStatus(WeaponData->HitStatus, WeaponData->HitStatusDuration);
			}
		}

		if (WeaponData->SplashRadius > 0.f && WeaponData->SplashMaxDamage > 0.f)
		{
			TArray<AActor*> IgnoreActors;
			if (GetInstigator()) { IgnoreActors.Add(GetInstigator()); } // no self-damage
			if (ShooterController && ShooterController->GetPawn()) { IgnoreActors.AddUnique(ShooterController->GetPawn()); } // ...even after respawning
			if (DirectHitActor)  { IgnoreActors.Add(DirectHitActor); }  // already took direct damage

			UGameplayStatics::ApplyRadialDamageWithFalloff(this,
				WeaponData->SplashMaxDamage, WeaponData->SplashMinDamage, Location,
				0.f, WeaponData->SplashRadius, 1.f, DamageType, IgnoreActors,
				this, ShooterController, ECC_Visibility);
		}
	}

	Multicast_Explode(Location);

	// Keep the actor alive a moment so the multicast arrives, but stop and hide it
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Movement->StopMovementImmediately();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	SetLifeSpan(0.5f);
}

void AKartProjectile::Multicast_Explode_Implementation(FVector Location)
{
	SetActorHiddenInGame(true);
	OnExplodeFX(Location);
	KartFeedback::Explosion(this, Location, ExplosionFXScale);
}

void AKartProjectile::OnExplodeFX_Implementation(FVector Location)
{
	if (ExplosionFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionFX, Location, FRotator::ZeroRotator, FVector(ExplosionFXScale));
	}
}

// =====================================================================
// Homing (Seeker Missile)
// =====================================================================

AActor* AKartProjectile::FindHomingTarget() const
{
	if (!WeaponData)
	{
		return nullptr;
	}
	const FVector Location = GetActorLocation();
	const FVector Heading = Movement->Velocity.IsNearlyZero() ? GetActorForwardVector() : Movement->Velocity.GetSafeNormal();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(WeaponData->HomingConeDegrees * 0.5f));

	AActor* Best = nullptr;
	float BestDistSq = FMath::Square(WeaponData->HomingRange);
	for (TActorIterator<AKartCharacter> It(GetWorld()); It; ++It)
	{
		AKartCharacter* Kart = *It;
		if (Kart->IsEliminated() || IsOwnerKart(Kart))
		{
			continue;
		}
		const FVector ToKart = Kart->GetActorLocation() - Location;
		const float DistSq = static_cast<float>(ToKart.SizeSquared());
		if (DistSq < BestDistSq && FVector::DotProduct(Heading, ToKart.GetSafeNormal()) >= MinDot)
		{
			BestDistSq = DistSq;
			Best = Kart;
		}
	}
	return Best;
}

void AKartProjectile::UpdateHoming()
{
	if (bExploded || bLanded)
	{
		GetWorldTimerManager().ClearTimer(HomingTimer);
		return;
	}
	const AKartCharacter* Current = Cast<AKartCharacter>(HomingTarget);
	if (Current && !Current->IsEliminated())
	{
		return; // keep chasing
	}
	AActor* NewTarget = FindHomingTarget();
	if (NewTarget != HomingTarget)
	{
		UE_LOG(LogSmashKarts, Verbose, TEXT("[Projectile] %s homing -> %s"), *GetName(), *GetNameSafe(NewTarget));
		HomingTarget = NewTarget;
		OnRep_HomingTarget(); // RepNotify doesn't run on the server
	}
}

void AKartProjectile::OnRep_HomingTarget()
{
	ApplyHoming();
}

void AKartProjectile::ApplyHoming()
{
	if (HomingTarget && WeaponData)
	{
		Movement->HomingTargetComponent = HomingTarget->GetRootComponent();
		Movement->HomingAccelerationMagnitude = WeaponData->HomingAcceleration;
		Movement->bIsHomingProjectile = true;
	}
	else
	{
		Movement->HomingTargetComponent = nullptr;
		Movement->bIsHomingProjectile = false;
	}
	if (HasAuthority())
	{
		SetActorTickEnabled(HomingTarget != nullptr && !bExploded); // proximity fuse
	}
}

void AKartProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && !bExploded)
	{
		UE_LOG(LogSmashKarts, Verbose, TEXT("[Projectile] %s removed without exploding at %s (landed %d)"), *GetName(),
			*GetActorLocation().ToCompactString(), bLanded ? 1 : 0);
	}
	Super::EndPlay(EndPlayReason);
}

void AKartProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Server: proximity fuse for homing missiles (counts as a direct hit)
	if (!HasAuthority() || bExploded || !WeaponData || !HomingTarget)
	{
		return;
	}
	const AKartCharacter* Target = Cast<AKartCharacter>(HomingTarget);
	if (Target && !Target->IsEliminated()
		&& FVector::DistSquared(Target->GetActorLocation(), GetActorLocation()) <= FMath::Square(WeaponData->HomingProximityFuse))
	{
		Explode(HomingTarget);
	}
}

// =====================================================================
// Sticky bomb
// =====================================================================

void AKartProjectile::Land(AActor* HitActor, UPrimitiveComponent* HitComponent)
{
	if (bLanded || bExploded || !WeaponData)
	{
		return;
	}
	bLanded = true;
	LandedLocation = GetActorLocation();
	Movement->StopMovementImmediately();
	Movement->bIsHomingProjectile = false;
	Movement->SetComponentTickEnabled(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Landing on a kart: direct hit, then ride along on it
	if (AKartCharacter* Kart = Cast<AKartCharacter>(HitActor))
	{
		AController* ShooterController = OwnerController.IsValid() ? OwnerController.Get() : GetInstigatorController();
		if (WeaponData->DirectDamage > 0.f)
		{
			UGameplayStatics::ApplyDamage(Kart, WeaponData->DirectDamage, ShooterController, this, WeaponData->GetDamageTypeOrDefault());
		}
		if (!Kart->IsEliminated())
		{
			AttachToComponent(Kart->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	else if (HitComponent && HitComponent->GetMobility() == EComponentMobility::Movable)
	{
		AttachToComponent(HitComponent, FAttachmentTransformRules::KeepWorldTransform);
	}

	OnRep_Landed(); // RepNotify doesn't run on the server

	SetLifeSpan(WeaponData->StickyFuseTime + 3.f);
	GetWorldTimerManager().SetTimer(FuseTimer, FTimerDelegate::CreateUObject(this, &AKartProjectile::Explode, static_cast<AActor*>(nullptr)),
		FMath::Max(0.05f, WeaponData->StickyFuseTime), false);
	GetWorldTimerManager().SetTimer(ProximityTimer, this, &AKartProjectile::CheckProximity, 0.1f, true, 0.1f);
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Projectile] %s stuck to %s"), *GetName(), *GetNameSafe(HitActor));
}

void AKartProjectile::OnRep_Landed()
{
	if (!bLanded)
	{
		return;
	}
	Movement->StopMovementImmediately();
	Movement->bIsHomingProjectile = false;
	Movement->SetComponentTickEnabled(false);
	if (!HasAuthority() && !GetAttachParentActor())
	{
		SetActorLocation(LandedLocation);
	}
}

void AKartProjectile::CheckProximity()
{
	if (bExploded || bAlarmStarted || !WeaponData)
	{
		return;
	}
	const float RadiusSq = FMath::Square(WeaponData->StickyProximityRadius);
	for (TActorIterator<AKartCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsEliminated() && !IsOwnerKart(*It) && FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) <= RadiusSq)
		{
			StartAlarm();
			return;
		}
	}
}

void AKartProjectile::StartAlarm()
{
	bAlarmStarted = true;
	GetWorldTimerManager().ClearTimer(ProximityTimer);
	Multicast_Alarm();
	GetWorldTimerManager().SetTimer(AlarmTimer, FTimerDelegate::CreateUObject(this, &AKartProjectile::Explode, static_cast<AActor*>(nullptr)),
		FMath::Max(0.05f, WeaponData ? WeaponData->StickyAlarmTime : 0.5f), false);
}

void AKartProjectile::Multicast_Alarm_Implementation()
{
	OnAlarmFX();
}

void AKartProjectile::OnAlarmFX_Implementation()
{
	if (AlarmFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(AlarmFX, GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true);
	}
}
