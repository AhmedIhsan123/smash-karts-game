// Fill out your copyright notice in the Description page of Project Settings.

// Class header include always comes first
#include "KartCharacter.h"

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

// Sets default values
AKartCharacter::AKartCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	// The kart doesn't need per-frame logic of its own: CMC ticks itself.
	PrimaryActorTick.bCanEverTick = false;

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
	KartMesh->SetupAttachment(GetCapsuleComponent());
	// Visual only: the capsule does the collision, so the mesh shouldn't snag on walls
	KartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// SPRING ARM
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetCapsuleComponent());
	SpringArm->TargetArmLength = 600.f;
	SpringArm->SetRelativeRotation(FRotator(-20.f, 0.f, 0.f));  // Pitch, Yaw, Roll: look down 20°
	SpringArm->bUsePawnControlRotation = false;  // Follow the kart's rotation, not the mouse
	SpringArm->bEnableCameraLag = true;          // Smooths out jitter
	SpringArm->CameraLagSpeed = 12.f;

	// CAMERA
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);  // Attach to the arm's end
	Camera->bUsePawnControlRotation = false;

	// --- Movement tuning (starting values; we'll tune by feel) ---
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;  // WE control rotation via steering, not CMC
	Move->MaxWalkSpeed = 1200.f;              // Top speed (cm/s)
	Move->MaxAcceleration = 1500.f;           // How fast you reach top speed
	Move->BrakingDecelerationWalking = 800.f; // How fast you coast to a stop with no input
	Move->GroundFriction = 2.f;               // Low = velocity lags behind facing = a bit of slide
}

void AKartCharacter::NotifyControllerChanged() 
{
	Super::NotifyControllerChanged();

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
	}
}

void AKartCharacter::Throttle(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>(); // -1 (S) to +1 (W)

	// Push only along the kart's forward direction: no strafing.
	// AddMovementInput doesn't move us directly; it feeds CMC, which predicts and networks the move.
	AddMovementInput(GetActorForwardVector(), Axis);
}

void AKartCharacter::Steer(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>(); // -1 (A) to +1 (D)
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds(); // The world delta seconds
	const float Dot = FVector::DotProduct(GetVelocity(), GetActorForwardVector());
	const float SpeedFactor = FMath::Clamp(FMath::Abs(Dot) / GetCharacterMovement()->MaxWalkSpeed, 0.f, 1.f);
	const float DirectionSign = (Dot < 0.f) ? -1.f : 1.f;

	// --- DEBUG: remove once the jitter is fixed ---
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			1,                  // Key: same line every frame (no flooding)
			0.f,                // Show for one frame; it's rewritten each frame
			FColor::Yellow,
			FString::Printf(TEXT("Sign: %.0f   Dot: %.1f   SpeedFactor: %.2f"), DirectionSign, Dot, SpeedFactor));
	}

	AddControllerYawInput(Axis * TurnRate * DeltaSeconds * SpeedFactor * DirectionSign);
}