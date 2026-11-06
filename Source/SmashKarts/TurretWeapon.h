#pragma once

#include "CoreMinimal.h"
#include "KartWeapon.h"
#include "TurretWeapon.generated.h"

/**
 * Archetype B: Machine Gun. Press Fire once to activate; for TurretDuration seconds it
 * auto-targets the nearest visible enemy within LockRange and fires hitscan shots.
 */
UCLASS()
class SMASHKARTS_API ATurretWeapon : public AKartWeapon
{
	GENERATED_BODY()

public:
	ATurretWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Fire(float ChargeAlpha) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentTarget)
	TObjectPtr<AActor> CurrentTarget;

	UFUNCTION()
	void OnRep_CurrentTarget();

	// Runs on every machine when the target changes (nullptr = stopped shooting).
	// Start/stop a muzzle flash + gun sound loop here in a Blueprint child if you make one
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnTargetChanged(AActor* NewTarget);

	void ShootTick();
	void EndTurret();
	AActor* FindTarget() const;
	bool HasLineOfSight(const AActor* Target) const;

private:
	bool bActive = false;
	FTimerHandle ShootTimer;
	FTimerHandle DurationTimer;
};
