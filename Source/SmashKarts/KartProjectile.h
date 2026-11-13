#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KartProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UKartWeaponData;
class UNiagaraSystem;
class AController;
class APlayerState;

/**
 * Replicated projectile (Rockets, Bullets, Seeker Missile, Cannon Balls, Sticky Bomb, Snowball, Nuke).
 *
 * Authority: the server detects hits, applies damage / hit status, picks homing targets and runs sticky-bomb
 * fuses. Replicates: weapon data, launch speed + inherited velocity (set before BeginPlay), movement, homing
 * target and the landed state (RepNotifies keep the client simulation in step). Explosion FX via multicast.
 * Client prediction: none (clients simulate the same flight from the replicated launch values).
 * Make a Blueprint child per weapon (BP_Rocket) to set the mesh, colour and FX.
 */
UCLASS()
class SMASHKARTS_API AKartProjectile : public AActor
{
	GENERATED_BODY()

public:
	AKartProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Server: call between SpawnActorDeferred and FinishSpawning
	void InitProjectile(UKartWeaponData* InData, float InSpeed, const FVector& InInheritedVelocity = FVector::ZeroVector);

	/** True for the shooter's kart, including a respawned one (no self-damage after dying). */
	bool IsOwnerKart(const AActor* Actor) const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(Replicated)
	TObjectPtr<UKartWeaponData> WeaponData;

	UPROPERTY(Replicated)
	float LaunchSpeed = 0.f;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 InheritedVelocity;

	UPROPERTY(ReplicatedUsing = OnRep_HomingTarget)
	TObjectPtr<AActor> HomingTarget;

	UPROPERTY(ReplicatedUsing = OnRep_Landed)
	bool bLanded = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize LandedLocation;

	UFUNCTION()
	void OnRep_HomingTarget();

	UFUNCTION()
	void OnRep_Landed();

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Server: damage + FX + clean up
	void Explode(AActor* DirectHitActor);

	// Server: sticky bomb hit something; it stops (riding along on karts) and arms its fuse / proximity trigger
	void Land(AActor* HitActor, UPrimitiveComponent* HitComponent);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Explode(FVector Location);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Alarm();

	// Runs on every machine. Default: spawns ExplosionFX. Override in the Blueprint for more (sound etc.)
	UFUNCTION(BlueprintNativeEvent, Category = "Projectile")
	void OnExplodeFX(FVector Location);

	// Runs on every machine when a sticky bomb starts its alarm. Default: spawns AlarmFX
	UFUNCTION(BlueprintNativeEvent, Category = "Projectile")
	void OnAlarmFX();

	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	TObjectPtr<UNiagaraSystem> ExplosionFX;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX", meta = (ClampMin = "0.05"))
	float ExplosionFXScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	TObjectPtr<UNiagaraSystem> AlarmFX;

	/** Tint for the projectile mesh (sets the material's colour parameter). Alpha 0 = leave the material alone. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	FLinearColor TintColor = FLinearColor(1.f, 1.f, 1.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	FName TintParameter = TEXT("Color");

private:
	void ApplyHoming();
	void UpdateHoming();
	AActor* FindHomingTarget() const;
	void CheckProximity();
	void StartAlarm();

	bool bExploded = false;
	bool bAlarmStarted = false;
	FVector SpawnLocation = FVector::ZeroVector;
	FTimerHandle HomingTimer;
	FTimerHandle FuseTimer;
	FTimerHandle ProximityTimer;
	FTimerHandle AlarmTimer;

	// Server: the shooter, kept by controller / PlayerState so kills still count after the shooter respawns
	TWeakObjectPtr<AController> OwnerController;
	TWeakObjectPtr<APlayerState> OwnerPlayerState;
};
