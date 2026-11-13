#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootBox.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class URotatingMovementComponent;
class UKartWeaponData;

/**
 * Weapon pickup. Server-side overlap → weighted roll → give weapon → hide → respawn.
 * Authority: server only (overlap, roll, give). Replicates: bActive (RepNotify shows/hides the box).
 * Client prediction: none. Only gives weapons while the match is InProgress; hidden in the lobby.
 */
UCLASS()
class SMASHKARTS_API ALootBox : public AActor
{
	GENERATED_BODY()

public:
	ALootBox();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Server: cancel any respawn timer and force the box on/off (match start / lobby)
	void ResetBox(bool bInActive);

	// Server: give to karts already sitting inside the box (match start, respawn)
	void RecheckOverlaps();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Loot Box")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Loot Box")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Loot Box")
	TObjectPtr<URotatingMovementComponent> Rotator;

	// Weapons this box can give (weighted by each asset's DropWeight)
	UPROPERTY(EditAnywhere, Category = "Loot Box")
	TArray<TObjectPtr<UKartWeaponData>> WeaponPool;

	UPROPERTY(EditAnywhere, Category = "Loot Box", meta = (ClampMin = "0"))
	float RespawnTime = 5.f;

	UPROPERTY(ReplicatedUsing = OnRep_Active)
	bool bActive = true;

	UFUNCTION()
	void OnRep_Active();

	// Runs on every machine. Pickup / respawn FX + sound in the Blueprint
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot Box")
	void OnActiveChanged(bool bNowActive);

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryGive(AActor* OtherActor);
	void Respawn();
	UKartWeaponData* RollWeapon() const;

private:
	FTimerHandle RespawnTimer;
};
