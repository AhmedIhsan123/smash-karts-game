// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "KartStatusComponent.h"
#include "KartCharacter.generated.h"

// Forward declarations: "these types exist, I'll include them in the .cpp".
// A header only needs the full type when it uses its size/members. For pointers, a name is enough.
// This keeps compile times down: every file that includes us doesn't drag these headers in too.
class USpringArmComponent;
class UCameraComponent;
class UKartHealthComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UDamageType;
struct FInputActionValue;

class UKartWeaponComponent;

/**
 * The player's kart.
 *
 * Authority: server applies damage, knockback, eliminations and status; the owning client drives (CMC prediction).
 * Replicates: movement (CMC), bEliminated (RepNotify hides/disables the kart everywhere), death FX multicast;
 * components replicate health, status and the held weapon. Body colour comes from the replicated PlayerState.
 * Client prediction: movement and steering (controller yaw). Input is locked locally from replicated state
 * (eliminated / frozen / countdown / results) and the movement component zeroes acceleration with the same rule
 * on both sides. Knockback: the server launches the kart and tells the owning client to launch itself too,
 * so the owner sees it immediately; the CMC reconciles any difference with one small correction.
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

	/** Called by the movement component on blocking hits (server and owning client). Used to detect rams. */
	virtual void MoveBlockedBy(const FHitResult& Impact) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	FORCEINLINE UKartWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }
	FORCEINLINE UKartHealthComponent* GetHealthComponent() const { return HealthComponent; }
	FORCEINLINE UKartStatusComponent* GetStatusComponent() const { return StatusComponent; }

	/** Server: hide, disable and stop the kart, drop its weapon and statuses, play the death FX everywhere. */
	void Eliminate(bool bBigExplosion);

	UFUNCTION(BlueprintPure, Category = "Kart")
	bool IsEliminated() const { return bEliminated; }

	/** True if the status component has this status (any machine). */
	bool HasStatus(EKartStatus Status) const;

	/** True while the kart must not drive or fire: eliminated, frozen, countdown, results. Any machine. */
	bool IsInputLocked() const;

	/** Server: push the kart (adds to its velocity). The owning client predicts the same push. */
	void ApplyKnockback(const FVector& LaunchVelocity);

	/** Server: the last other player who damaged us, if it was within WindowSeconds. */
	AController* GetRecentAttacker(float WindowSeconds) const;
	TSubclassOf<UDamageType> GetRecentAttackType() const { return LastAttackType; }

	/** Development: drive automatically for a while (network tests). Local only. */
	void SetDebugAutoDrive(float Seconds, float Steer);

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

	UFUNCTION(Client, Unreliable)
	void Client_Knockback(FVector_NetQuantize LaunchVelocity);

	/** Runs on every machine when the kart is smashed. Default: spawns DeathFX / BigDeathFX. Override in BP for more. */
	UFUNCTION(BlueprintNativeEvent, Category = "Kart|FX")
	void OnEliminatedFX(bool bBigExplosion);

	/** Runs on every machine when a status turns on or off (VFX hook; the body tint is handled in C++). */
	UFUNCTION(BlueprintNativeEvent, Category = "Kart|FX")
	void OnStatusFX(EKartStatus Status, bool bActive);

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

	// --- Look ---
	/** Vector parameter on the kart body material that takes the player colour. */
	UPROPERTY(EditDefaultsOnly, Category = "Kart|Look")
	FName BodyColorParameter;

	UPROPERTY(EditDefaultsOnly, Category = "Kart|Look")
	FLinearColor FrozenTint;

	UPROPERTY(EditDefaultsOnly, Category = "Kart|Look", meta = (ClampMin = "0", ClampMax = "1"))
	float FrozenTintAmount;

	// --- Combat tuning ---
	/** Spiky (Mace Orbit) karts take this fraction of incoming damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Kart|Combat", meta = (ClampMin = "0"))
	float SpikyDamageTakenMultiplier;

	/** Knockback from an explosion's edge still pushes at least this fraction of full strength. */
	UPROPERTY(EditDefaultsOnly, Category = "Kart|Combat", meta = (ClampMin = "0", ClampMax = "1"))
	float MinKnockbackScale;

	/** The owning client applies knockback immediately instead of waiting for the server correction. */
	UPROPERTY(EditDefaultsOnly, Category = "Kart|Combat")
	bool bPredictKnockbackOnOwner;

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

	/** Player colour (+ frozen tint) on the body material. Every machine. */
	void UpdateBodyColor();

	void HandleStatusChanged(uint8 OldFlags, uint8 NewFlags);

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

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;


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

	// Development auto-drive (local)
	double DebugAutoDriveUntil = 0.0;
	float DebugAutoDriveSteer = 0.f;

	void Fire();
	void FireReleased();
};
