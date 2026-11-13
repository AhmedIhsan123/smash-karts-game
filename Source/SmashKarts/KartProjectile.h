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
 * Replicated projectile. Server detects hits and applies damage;
 * clients just simulate the flight and play the explosion FX.
 * Make a Blueprint child per weapon (BP_Rocket) to set the mesh and FX.
 */
UCLASS()
class SMASHKARTS_API AKartProjectile : public AActor
{
	GENERATED_BODY()

public:
	AKartProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Server: call between SpawnActorDeferred and FinishSpawning
	void InitProjectile(UKartWeaponData* InData, float InSpeed);

protected:
	virtual void BeginPlay() override;

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

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Server: damage + FX + clean up
	void Explode(AActor* DirectHitActor);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Explode(FVector Location);

	// Runs on every machine. Default: spawns ExplosionFX. Override in the Blueprint for more (sound etc.)
	UFUNCTION(BlueprintNativeEvent, Category = "Projectile")
	void OnExplodeFX(FVector Location);

	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	TObjectPtr<UNiagaraSystem> ExplosionFX;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX", meta = (ClampMin = "0.05"))
	float ExplosionFXScale = 1.f;

	/** True for the shooter's kart, including a respawned one (no self-damage after dying). */
	bool IsOwnerKart(const AActor* Actor) const;

private:
	bool bExploded = false;

	// Server: the shooter, kept by controller / PlayerState so kills still count after the shooter respawns
	TWeakObjectPtr<AController> OwnerController;
	TWeakObjectPtr<APlayerState> OwnerPlayerState;
};
