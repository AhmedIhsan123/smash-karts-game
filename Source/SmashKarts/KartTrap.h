#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KartTrap.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UKartWeaponData;

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

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Replicated)
	TObjectPtr<UKartWeaponData> WeaponData;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Arm();
	void TryTrigger(AActor* OtherActor);
	void Detonate(AActor* Victim);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Detonate(FVector Location);

	// Runs on every machine. Explosion particles / sound in the Blueprint
	UFUNCTION(BlueprintImplementableEvent, Category = "Trap")
	void OnDetonateFX(FVector Location);

	// Runs on every machine. true = this trap belongs to the local player (show green), false = red
	UFUNCTION(BlueprintImplementableEvent, Category = "Trap")
	void OnOwnerColor(bool bIsMine);

	void UpdateOwnerColor();

private:
	bool bArmed = false;
	bool bDetonated = false;
	FTimerHandle ArmTimer;
};
