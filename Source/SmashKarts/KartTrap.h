#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KartTrap.generated.h"

class USphereComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UKartWeaponData;
class UNiagaraSystem;
class APlayerState;

/**
 * Placed trap (Mine, Spike Strip, later Fake Loot Box). Make a Blueprint child per trap (BP_Mine, BP_SpikeStrip)
 * for the mesh and FX; numbers come from the weapon data asset.
 * Mines detonate on the first enemy kart. Strips (bStaysAfterTrigger) catch each enemy kart once and stay.
 *
 * Authority: server only (arming, triggering, damage, status, lifetime).
 * Replicates: WeaponData and the owner's PlayerState (green for the owner, red for everyone else), a multicast for
 * the detonate / catch FX. Client prediction: none (traps don't move).
 */
UCLASS()
class SMASHKARTS_API AKartTrap : public AActor
{
	GENERATED_BODY()

public:
	AKartTrap();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Server: call between SpawnActorDeferred and FinishSpawning
	void InitTrap(UKartWeaponData* InData);

	// Instigator (the kart that dropped it) arrived on a client → update color
	virtual void OnRep_Instigator() override;

	// The player who placed it. Survives the owner's respawn (credit + owner-safe + colour use this, not the pawn)
	APlayerState* GetOwnerPlayerState() const { return OwnerPlayerState; }

	UKartWeaponData* GetWeaponData() const { return WeaponData; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Strip-shaped trigger, used instead of the sphere when the data asset sets TriggerBoxExtent. */
	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Repeated decoration across the trap's width (Spike Strip spikes). */
	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<UInstancedStaticMeshComponent> Decor;

	UPROPERTY(Replicated)
	TObjectPtr<UKartWeaponData> WeaponData;

	UPROPERTY(ReplicatedUsing = OnRep_OwnerPlayerState)
	TObjectPtr<APlayerState> OwnerPlayerState;

	UFUNCTION()
	void OnRep_OwnerPlayerState();

	// Server: credited for kills even after the owner's kart was destroyed and respawned
	TWeakObjectPtr<AController> OwnerController;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Arm();
	void TryTrigger(AActor* OtherActor);
	void Detonate(AActor* Victim);

	/** Server: a staying trap (Spike Strip) catches this kart: damage + status, the trap stays. */
	void Catch(AActor* Victim);

	/** Server: damage and status for the victim, credited to the owner. */
	void HurtVictim(AActor* Victim);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Detonate(FVector Location);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Catch(FVector Location);

	// Runs on every machine. Default: spawns DetonateFX. Override in the Blueprint for more (sound etc.)
	UFUNCTION(BlueprintNativeEvent, Category = "Trap")
	void OnDetonateFX(FVector Location);

	UPROPERTY(EditDefaultsOnly, Category = "Trap|FX")
	TObjectPtr<UNiagaraSystem> DetonateFX;

	UPROPERTY(EditDefaultsOnly, Category = "Trap|FX", meta = (ClampMin = "0.05"))
	float DetonateFXScale = 1.f;

	// Runs on every machine. true = this trap belongs to the local player (green), false = red.
	// Default: tints the Mesh (vector parameter ColorParameter) with OwnColor / EnemyColor
	UFUNCTION(BlueprintNativeEvent, Category = "Trap")
	void OnOwnerColor(bool bIsMine);

	void UpdateOwnerColor();

	/** Tint Mesh with OwnColor / EnemyColor (turn off if the mesh has its own look). */
	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look")
	bool bTintMeshWithOwnerColor = true;

	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look")
	FName ColorParameter = TEXT("Color");

	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look")
	FLinearColor OwnColor = FLinearColor(0.1f, 0.85f, 0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look")
	FLinearColor EnemyColor = FLinearColor(0.9f, 0.1f, 0.08f);

	/** Mesh repeated DecorCount times across the trap's width (Spike Strip: cones). Empty = none. */
	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look")
	TObjectPtr<UStaticMesh> DecorMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look")
	TObjectPtr<UMaterialInterface> DecorMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look")
	FLinearColor DecorColor = FLinearColor(0.75f, 0.75f, 0.8f);

	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look", meta = (ClampMin = "0", ClampMax = "64"))
	int32 DecorCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look")
	FVector DecorScale = FVector(0.15f, 0.15f, 0.2f);

	/** Height of the decoration above the trap's origin (the ground). */
	UPROPERTY(EditDefaultsOnly, Category = "Trap|Look")
	float DecorHeight = 12.f;

private:
	void BuildDecor();

	bool bArmed = false;
	bool bDetonated = false;
	FTimerHandle ArmTimer;

	/** Staying traps: karts already caught (each kart once). */
	TSet<TWeakObjectPtr<AActor>> Caught;
};
