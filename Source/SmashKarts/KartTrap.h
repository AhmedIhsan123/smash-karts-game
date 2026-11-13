#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KartTrap.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UKartWeaponData;
class UNiagaraSystem;
class APlayerState;

/**
 * Placed trap (Mine, later Spike Strip / Fake Loot Box). Server handles triggering + damage.
 * Make a Blueprint child per trap (BP_Mine) to set the mesh, FX and green/red color.
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

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<UStaticMeshComponent> Mesh;

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

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Detonate(FVector Location);

	// Runs on every machine. Default: spawns DetonateFX. Override in the Blueprint for more (sound etc.)
	UFUNCTION(BlueprintNativeEvent, Category = "Trap")
	void OnDetonateFX(FVector Location);

	UPROPERTY(EditDefaultsOnly, Category = "Trap|FX")
	TObjectPtr<UNiagaraSystem> DetonateFX;

	UPROPERTY(EditDefaultsOnly, Category = "Trap|FX", meta = (ClampMin = "0.05"))
	float DetonateFXScale = 1.f;

	// Runs on every machine. true = this trap belongs to the local player (show green), false = red
	UFUNCTION(BlueprintImplementableEvent, Category = "Trap")
	void OnOwnerColor(bool bIsMine);

	void UpdateOwnerColor();

private:
	bool bArmed = false;
	bool bDetonated = false;
	FTimerHandle ArmTimer;
};
