// Fill out your copyright notice in the Description page of Project Settings.

// Class header include always comes first
#include "KartCharacter.h"
#include "SmashKarts.h"

// The real includes for everything we forward-declared, plus what we call into
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "KartMovementComponent.h"
#include "Engine/Engine.h"


// Sets default values
AKartCharacter::AKartCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UKartMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Tick drives steering smoothing
	PrimaryActorTick.bCanEverTick = true;

	// --- Collision shape ---
	// The capsule is the root and handles all collision. Half-height can't be smaller than the radius,
	// so equal values = a sphere. Fine for a small chunky kart.
	GetCapsuleComponent()->InitCapsuleSize(50.f, 50.f);

	// --- Rotation rules (the key kart setup) ---
	// The actor's yaw follows the controller's yaw. Steering changes controller yaw,
	// and CMC sends that to the server as part of its predicted moves. Rotation is networked for free.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// --- Components: the C++ version of the Components panel ---

	// STATIC MESH KART
	KartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KartMesh"));
	KartMesh->SetupAttachment(GetMesh());
	// Visual only: the capsule does the collision, so the mesh shouldn't snag on walls
	KartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// SPRING ARM
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetCapsuleComponent());
	SpringArm->TargetArmLength = 600.f;
	SpringArm->SetRelativeRotation(FRotator(-20.f, 0.f, 0.f));  // Pitch, Yaw, Roll: look down 20°
	SpringArm->bUsePawnControlRotation = false;  // Follow the kart's rotation, not the mouse
	SpringArm->bEnableCameraLag = true;          // Smooths out jitter
	SpringArm->CameraLagSpeed = 8.f;

	// CAMERA
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);  // Attach to the arm's end
	Camera->bUsePawnControlRotation = false;

	// Kart feel defaults
	TurnRate = 40.f;
	SteerSpeed = 250.f;
	SteerRampUpRate = 2.f;
	SteerReturnRate = 3.f;

	bDebugSteer = false;
}

void AKartCharacter::LogNetState(const TCHAR* Context) const
{
	UE_LOG(LogSmashKarts, Log, TEXT("[%s] %s | Machine: %s | Role: %s | Local: %s"),
		Context,                                              // const TCHAR* already -> no *
		*GetName(),                                           // FString -> needs *
		HasAuthority() ? TEXT("Server") : TEXT("Client"),     // ternary gives const TCHAR* -> no *
		*UEnum::GetValueAsString(GetLocalRole()),             // FString like "ROLE_Authority" -> needs *
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));
}

void AKartCharacter::BeginPlay()
{
	Super::BeginPlay();
	LogNetState(TEXT("BeginPlay"));
}

void AKartCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	LogNetState(TEXT("ControllerChanged"));
	// C++ version of the BP "Add Mapping Context" node.
	// "if (Type* X = Cast<Type>(Y))" = cast, null-check, and scope the variable in one line. Very common idiom.
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (KartMappingContext) // Don't crash if you forgot to assign it in the BP
			{
				Subsystem->AddMappingContext(KartMappingContext, 0); // 0 = priority
			}
		}
	}
}

// Called to bind functionality to input
void AKartCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// &AKartCharacter::Throttle = a member-function pointer
		// Triggered fires every frame while the key is held
		EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AKartCharacter::Throttle);
		EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &AKartCharacter::Steer);
		EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &AKartCharacter::SteerReleased);
	}
}

void AKartCharacter::Throttle(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>(); // -1 (S) to +1 (W)

	// Push only along the kart's forward direction: no strafing.
	// AddMovementInput doesn't move us directly; it feeds CMC, which predicts and networks the move.
	AddMovementInput(GetActorForwardVector(), Axis);
}

// Set the raw steer input to a value -1 to +1
void AKartCharacter::Steer(const FInputActionValue& Value) { RawSteerInput = Value.Get<float>(); } // -1 (A) to +1 (D)

// When steering is released, set the raw steer input to 0
void AKartCharacter::SteerReleased() { RawSteerInput = 0.f; }

void AKartCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Only the owning player has input; other copies of this kart skip steering.
	if (!IsLocallyControlled()) return;

	// Pick how fast CurrentSteer moves: ramp up while a key is held, ease back when released
	const float Rate = FMath::IsNearlyZero(RawSteerInput) ? SteerReturnRate : SteerRampUpRate;

	// Move CurrentSteer toward RawSteerInput at a steady speed of Rate units per second
	CurrentSteer = FMath::FInterpConstantTo(CurrentSteer, RawSteerInput, DeltaSeconds, Rate);

	// Speed along the kart's nose (cm/s). Positive = driving forward, negative = reversing.
	// Sideways sliding doesn't count, so it drops toward 0 in a hard slide.
	const float ForwardSpeed = FVector::DotProduct(GetVelocity(), GetActorForwardVector());

	// Flip steering when reversing, like a real car
	const float DirectionSign = (ForwardSpeed < 0.f) ? -1.f : 1.f;

	// Steering strength from 0 to 1: none when stopped, full at SteerSpeed and above.
	// Because it fades to 0 as ForwardSpeed nears 0, the DirectionSign flip near 0 can't cause jitter.
	const float SpeedFactor = FMath::Clamp(FMath::Abs(ForwardSpeed) / SteerSpeed, 0.f, 1.f);

	if (bDebugSteer && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			1,                // Key: same key replaces the old line instead of stacking (-1 = always new line)
			0.f,              // Duration in seconds (0 = one frame, good for Tick)
			FColor::Yellow,
			FString::Printf(TEXT("Fwd: %.1f  Steer: %.2f  Factor: %.2f  Sign: %.0f"),
				ForwardSpeed, CurrentSteer, SpeedFactor, DirectionSign));
	}

	// Only steer on the ground. In the air, we don't want to change facing direction.
	// No zero-speed check needed: SpeedFactor is already 0 when stopped.
	if (!GetCharacterMovement()->IsFalling())
	{
		AddControllerYawInput(CurrentSteer * TurnRate * DeltaSeconds * SpeedFactor * DirectionSign);
	}
}