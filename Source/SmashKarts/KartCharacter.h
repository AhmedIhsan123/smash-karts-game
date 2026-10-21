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
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class SMASHKARTS_API AKartCharacter : public ACharacter
{
	GENERATED_BODY() // Always first in the class body

public:
	// Sets default values for this character's properties
	// Constructor, this is when components are built
	AKartCharacter(const FObjectInitializer& ObjectInitializer);

	// Event tick
	virtual void Tick(float DeltaSeconds) override;

protected:
	// "virtual ... override" = overriding a parent method
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	// Input handlers, called by Enhanced Input when the actions fire
	void Throttle(const FInputActionValue& Value);
	void Steer(const FInputActionValue& Value);
	void SteerReleased();

	// --- Input assets (assigned in BP_Kart, not hardcoded) ---
	UPROPERTY(EditDefaultsOnly, Category = "Kart|Input")
	TObjectPtr<UInputMappingContext> KartMappingContext;
	
	UPROPERTY(EditDefaultsOnly, Category = "Kart|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditDefaultsOnly, Category = "Kart|Input")
	TObjectPtr<UInputAction> SteerAction;


	// --- Tuning ---
	// Degrees per second at full speed. EditDefaultsOnly = editable in the BP's Class Defaults.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Steering", meta = (ClampMin = "0"))
	float TurnRate = 40.f;

	// Forward speed (cm/s) at which steering reaches full strength.
	// Below this, steering ramps up from 0; above it, steering is full.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Steering", meta = (ClampMin = "1"))
	float SteerSpeed = 350.f;

	// The rate at which steering ramps to full turn rate (higher = faster turn rate speed ramp up)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Steering", meta = (ClampMin = "0"))
	float SteerRampUpRate = 3.f;

	// The rate at which the steering will return to forward when releasing steer keys (higher = faster snap)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kart|Steering", meta = (ClampMin = "0"))
	float SteerReturnRate = 5.f;


private:	
	// --- Components ---
	// VisibleAnywhere: you can see and edit the component's settings in BP, but can't swap the component itself.
	// AllowPrivateAccess: lets Blueprint read a private member (otherwise BlueprintReadOnly + private = error).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> KartMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	// Private variables for steering
	float RawSteerInput = 0.f;
	float CurrentSteer = 0.f;
};
