// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "KartCharacter.generated.h"

// Forward declarations: "these types exist, I'll include them in the .cpp".
// A header only needs the full type when it uses its size/members. For pointers, a name is enough.
// This keeps compile times down: every file that includes us doesn't drag these headers in too.
class USpringArmComponent;
class UCameraComponent;
class UKartHealthComponent;
class UKartStatusComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;
class UDamageType;
struct FInputActionValue;

class UKartWeaponComponent;

/**
 * The player's kart.
 *
 * Authority: server applies damage, eliminations and status; the owning client drives (CMC prediction).
 * Replicates: movement (CMC), bEliminated (RepNotify hides/disables the kart everywhere), death FX multicast;
 * components replicate health, status and the held weapon.
 * Client prediction: movement and steering (controller yaw) as before. Input is locked locally from
 * replicated state (eliminated / countdown / results); the movement component zeroes acceleration with the
 * same rule on both sides, so the lock is prediction-safe.
 */
UCLASS()
class SMASHKARTS_API AKartCharacter : public ACharacter
{
	GENERATED_BODY() // Always first in the class body

public:
	// Sets default values for this character's properties
	// Constructor, this is when components are built
	AKartCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Event tick
	virtual void Tick(float DeltaSeconds) override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	/** Server: KillZ / kill volume. Routed to the game mode as an elimination instead of destroying the kart. */
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	FORCEINLINE UKartWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }
	FORCEINLINE UKartHealthComponent* GetHealthComponent() const { return HealthComponent; }
	FORCEINLINE UKartStatusComponent* GetStatusComponent() const { return StatusComponent; }

	/** Server: hide, disable and stop the kart, drop its weapon and statuses, play the death FX everywhere. */
	void Eliminate(bool bBigExplosion);

	UFUNCTION(BlueprintPure, Category = "Kart")
	bool IsEliminated() const { return bEliminated; }

	/** True while the kart must not drive or fire: eliminated, countdown, results (later: frozen). Any machine. */
	bool IsInputLocked() const;

	/** Server: the last other player who damaged us, if it was within WindowSeconds. */
	AController* GetRecentAttacker(float WindowSeconds) const;
	TSubclassOf<UDamageType> GetRecentAttackType() const { return LastAttackType; }

protected:
	// "virtual ... override" = overriding a parent method
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void BeginPlay() override;

	// Input handlers, called by Enhanced Input when the actions fire
	void Throttle(const FInputActionValue& Value);
	void Steer(const FInputActionValue& Value);
	void SteerReleased();

	UFUNCTION()
	void OnRep_Eliminated();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_EliminatedFX(bool bBigExplosion);

	/** Runs on every machine when the kart is smashed. Default: spawns DeathFX / BigDeathFX. Override in BP for more. */
	UFUNCTION(BlueprintNativeEvent, Category = "Kart|FX")
	void OnEliminatedFX(bool bBigExplosion);

	// --- Input assets (assigned in BP_Kart, not hardcoded) ---
	UPROPERTY(EditDefaultsOnly, Category = "Kart|Input")
	TObjectPtr<UInputMappingContext> KartMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Kart|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditDefaultsOnly, Category = "Kart|Input")
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(EditDefaultsOnly, Category = "Kart|Input")
	TObjectPtr<UInputAction> FireAction;

	// --- FX (assigned in BP_Kart) ---
	UPROPERTY(EditDefaultsOnly, Category = "Kart|FX")
	TObjectPtr<UNiagaraSystem> DeathFX;

	/** Death explosion for Nuke / Sticky Bomb kills (falls back to DeathFX scaled up). */
	UPROPERTY(EditDefaultsOnly, Category = "Kart|FX")
	TObjectPtr<UNiagaraSystem> BigDeathFX;

	UPROPERTY(EditDefaultsOnly, Category = "Kart|FX", meta = (ClampMin = "0.1"))
	float BigDeathFXScale;

	/** Blink period while spawn protected (seconds per on/off cycle). */
	UPROPERTY(EditDefaultsOnly, Category = "Kart|FX", meta = (ClampMin = "0.05"))
	float ProtectionBlinkPeriod;

	// --- Tuning ---
	// Degrees per second at full speed. EditDefaultsOnly = editable in the BP's Class Defaults.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Steering", meta = (ClampMin = "0"))
	float TurnRate;

	// Forward speed (cm/s) at which steering reaches full strength.
	// Below this, steering ramps up from 0; above it, steering is full.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Steering", meta = (ClampMin = "1"))
	float SteerSpeed;

	// The rate at which steering ramps to full turn rate (higher = faster turn rate speed ramp up)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Steering", meta = (ClampMin = "0"))
	float SteerRampUpRate;

	// The rate at which the steering will return to forward when releasing steer keys (higher = faster snap)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Steering", meta = (ClampMin = "0"))
	float SteerReturnRate;


private:
	/** Logs which machine / net role this kart copy is on. Context = where it was called from. */
	void LogNetState(const TCHAR* Context) const;

	/** Kart visuals on/off: hidden when eliminated, blinking while spawn protected. Every machine. */
	void UpdateVisibility();

	// --- Components ---
	// VisibleAnywhere: you can see and edit the component's settings in BP, but can't swap the component itself.
	// AllowPrivateAccess: lets Blueprint read a private member (otherwise BlueprintReadOnly + private = error).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> KartMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UKartHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UKartWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, Category = "Kart")
	TObjectPtr<UKartStatusComponent> StatusComponent;


	/** Show the on-screen steering debug line (owning player only). */
	UPROPERTY(EditAnywhere, Category = "Kart|Debug")
	bool bDebugSteer;

	UPROPERTY(ReplicatedUsing = OnRep_Eliminated)
	bool bEliminated = false;

	// Private variables for steering
	float RawSteerInput = 0.f;
	float CurrentSteer = 0.f;

	// Server: last attacker (for fall credit)
	TWeakObjectPtr<AController> LastAttacker;
	double LastAttackTime = -1000.0;
	TSubclassOf<UDamageType> LastAttackType;

	void Fire();
};
