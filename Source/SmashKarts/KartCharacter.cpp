// Fill out your copyright notice in the Description page of Project Settings.

// Class header include always comes first
#include "KartCharacter.h"
#include "SmashKarts.h"

// The real includes for everything we forward-declared, plus what we call into
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "KartDamageType.h"
#include "KartHealthComponent.h"
#include "KartRules.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "KartWeaponComponent.h"
#include "Game/KartGameMode.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerState.h"
#include "Engine/DamageEvents.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "KartMovementComponent.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"


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

	// HEALTH COMPONENT
	HealthComponent = CreateDefaultSubobject<UKartHealthComponent>(TEXT("HealthComponent"));

	// WEAPON COMPONENT
	WeaponComponent = CreateDefaultSubobject<UKartWeaponComponent>(TEXT("WeaponComponent"));

	// STATUS COMPONENT (freeze, popped tires, buffs, spawn protection)
	StatusComponent = CreateDefaultSubobject<UKartStatusComponent>(TEXT("StatusComponent"));

	// Kart feel defaults
	TurnRate = 40.f;
	SteerSpeed = 250.f;
	SteerRampUpRate = 2.f;
	SteerReturnRate = 3.f;

	BigDeathFXScale = 3.f;
	ProtectionBlinkPeriod = 0.2f;

	BodyColorParameter = TEXT("Color");
	FrozenTint = FLinearColor(0.55f, 0.85f, 1.f);
	FrozenTintAmount = 0.75f;

	SpikyDamageTakenMultiplier = 0.9f;
	MinKnockbackScale = 0.3f;
	bPredictKnockbackOnOwner = true;

	bDebugSteer = false;
}

void AKartCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKartCharacter, bEliminated);
}

void AKartCharacter::LogNetState(const TCHAR* Context) const
{
	UE_LOG(LogSmashKarts, Verbose, TEXT("[%s] %s | Machine: %s | Role: %s | Local: %s"),
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

	if (KartMesh && KartMesh->GetNumMaterials() > 0)
	{
		BodyMaterial = KartMesh->CreateDynamicMaterialInstance(0);
	}
	if (StatusComponent)
	{
		StatusComponent->OnStatusChanged.AddUObject(this, &AKartCharacter::HandleStatusChanged);
	}
	UpdateBodyColor();
}

void AKartCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UpdateBodyColor(); // server: PlayerState is known now
}

void AKartCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	UpdateBodyColor(); // clients: colour arrives with the PlayerState
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
		EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AKartCharacter::Fire);
		EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &AKartCharacter::FireReleased); // charge throw release
	}
}

bool AKartCharacter::HasStatus(EKartStatus Status) const
{
	return StatusComponent && StatusComponent->HasStatus(Status);
}

bool AKartCharacter::IsInputLocked() const
{
	if (bEliminated || HasStatus(EKartStatus::Frozen))
	{
		return true;
	}
	if (const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr)
	{
		if (GS->IsCountdown() || GS->IsPostMatch())
		{
			return true;
		}
	}
	return false;
}

void AKartCharacter::Throttle(const FInputActionValue& Value)
{
	if (IsInputLocked())
	{
		return;
	}
	const float Axis = Value.Get<float>(); // -1 (S) to +1 (W)

	// Push only along the kart's forward direction: no strafing.
	// AddMovementInput doesn't move us directly; it feeds CMC, which predicts and networks the move.
	AddMovementInput(GetActorForwardVector(), Axis);
}

// Set the raw steer input to a value -1 to +1
void AKartCharacter::Steer(const FInputActionValue& Value) { RawSteerInput = Value.Get<float>(); } // -1 (A) to +1 (D)

// When steering is released, set the raw steer input to 0
void AKartCharacter::SteerReleased() { RawSteerInput = 0.f; }

void AKartCharacter::SetDebugAutoDrive(float Seconds, float Steer)
{
	DebugAutoDriveUntil = GetWorld()->GetTimeSeconds() + Seconds;
	DebugAutoDriveSteer = FMath::Clamp(Steer, -1.f, 1.f);
	UE_LOG(LogSmashKarts, Log, TEXT("[Debug] Auto-drive %.0f s, steer %.2f"), Seconds, DebugAutoDriveSteer);
}

void AKartCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Every copy: hidden when smashed, blinking while spawn protected
	UpdateVisibility();

	// Only the owning player has input; other copies of this kart skip steering.
	if (!IsLocallyControlled()) return;

	// Development: scripted driving for network tests (acts exactly like held keys)
	if (DebugAutoDriveUntil > 0.0)
	{
		if (GetWorld()->GetTimeSeconds() < DebugAutoDriveUntil)
		{
			RawSteerInput = DebugAutoDriveSteer;
			if (!IsInputLocked())
			{
				AddMovementInput(GetActorForwardVector(), 1.f);
			}
		}
		else
		{
			DebugAutoDriveUntil = 0.0;
			RawSteerInput = 0.f;
		}
	}

	// Locked (smashed / frozen / countdown / results): let the wheel return to centre, don't turn
	const float TargetSteer = IsInputLocked() ? 0.f : RawSteerInput;

	// Pick how fast CurrentSteer moves: ramp up while a key is held, ease back when released
	const float Rate = FMath::IsNearlyZero(TargetSteer) ? SteerReturnRate : SteerRampUpRate;

	// Move CurrentSteer toward the target at a steady speed of Rate units per second
	CurrentSteer = FMath::FInterpConstantTo(CurrentSteer, TargetSteer, DeltaSeconds, Rate);

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
	if (!GetCharacterMovement()->IsFalling() && !IsInputLocked())
	{
		AddControllerYawInput(CurrentSteer * TurnRate * DeltaSeconds * SpeedFactor * DirectionSign);
	}
}

void AKartCharacter::UpdateVisibility()
{
	if (!KartMesh)
	{
		return;
	}
	bool bVisible = !bEliminated;
	if (bVisible && HasStatus(EKartStatus::SpawnProtected))
	{
		const double Phase = FMath::Fmod(GetWorld()->GetTimeSeconds(), static_cast<double>(ProtectionBlinkPeriod));
		bVisible = Phase < ProtectionBlinkPeriod * 0.6;
	}
	if (KartMesh->IsVisible() != bVisible)
	{
		KartMesh->SetVisibility(bVisible, true);
	}
}

void AKartCharacter::UpdateBodyColor()
{
	if (!BodyMaterial)
	{
		return;
	}
	const AKartPlayerState* PS = GetPlayerState<AKartPlayerState>();
	FLinearColor Color = PS ? PS->GetPlayerColor() : FLinearColor(0.8f, 0.8f, 0.8f);
	if (HasStatus(EKartStatus::Frozen))
	{
		Color = FLinearColor::LerpUsingHSV(Color, FrozenTint, FrozenTintAmount);
	}
	BodyMaterial->SetVectorParameterValue(BodyColorParameter, Color);
}

void AKartCharacter::HandleStatusChanged(uint8 OldFlags, uint8 NewFlags)
{
	UpdateBodyColor();
	for (const EKartStatus Status : TEnumRange<EKartStatus>())
	{
		const uint8 Bit = UKartStatusComponent::ToBit(Status);
		if ((OldFlags & Bit) != (NewFlags & Bit))
		{
			OnStatusFX(Status, (NewFlags & Bit) != 0);
		}
	}
}

void AKartCharacter::OnStatusFX_Implementation(EKartStatus Status, bool bActive)
{
	// Visual hooks for Blueprint children; the C++ default only tints the body (UpdateBodyColor)
}

float AKartCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bEliminated)
	{
		return 0.f;
	}

	// Damage modifiers before health: spawn protection / invincibility block everything
	if (HasStatus(EKartStatus::SpawnProtected) || HasStatus(EKartStatus::Invincible))
	{
		return 0.f;
	}

	// Super handles radial falloff and returns the final amount
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (HasStatus(EKartStatus::Spiky) && !UKartDamageType::IgnoresDamageReduction(DamageEvent.DamageTypeClass))
	{
		ActualDamage *= SpikyDamageTakenMultiplier;
	}
	if (!HealthComponent || ActualDamage <= 0.f)
	{
		return 0.f;
	}

	// Remember who hit us (fall credit)
	if (EventInstigator && EventInstigator != GetController())
	{
		LastAttacker = EventInstigator;
		LastAttackTime = KartTime::ServerNow(this);
		LastAttackType = DamageEvent.DamageTypeClass;
	}

	HealthComponent->ApplyHealthDamage(ActualDamage);

	if (HealthComponent->GetHealth() <= 0.f)
	{
		if (AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>())
		{
			GM->KartEliminated(this, EventInstigator, DamageEvent.DamageTypeClass, false);
		}
		else
		{
			Eliminate(false);
		}
		return ActualDamage;
	}

	// Survived: knockback from the damage type, away from the blast (scaled by falloff, slightly up)
	float Strength = 0.f;
	float Upward = 0.f;
	UKartDamageType::GetKnockback(DamageEvent.DamageTypeClass, Strength, Upward);
	if (Strength > 0.f || Upward > 0.f)
	{
		FVector From = GetActorLocation() - GetActorForwardVector();
		float Scale = 1.f;
		if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
		{
			const FRadialDamageEvent& Radial = static_cast<const FRadialDamageEvent&>(DamageEvent);
			From = Radial.Origin;
			Scale = FMath::Max(MinKnockbackScale, Radial.Params.GetDamageScale(static_cast<float>(FVector::Dist(Radial.Origin, GetActorLocation()))));
		}
		else if (DamageCauser)
		{
			From = DamageCauser->GetActorLocation();
		}
		FVector Direction = (GetActorLocation() - From).GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = -GetActorForwardVector().GetSafeNormal2D();
		}
		ApplyKnockback((Direction * Strength + FVector::UpVector * Upward) * Scale);
	}
	return ActualDamage;
}

void AKartCharacter::ApplyKnockback(const FVector& LaunchVelocity)
{
	if (!HasAuthority() || bEliminated || LaunchVelocity.IsNearlyZero())
	{
		return;
	}
	LaunchCharacter(LaunchVelocity, false, false);
	if (bPredictKnockbackOnOwner && !IsLocallyControlled())
	{
		Client_Knockback(LaunchVelocity);
	}
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Kart] %s knocked back %s"), *GetNameSafe(GetPlayerState()), *LaunchVelocity.ToCompactString());
}

void AKartCharacter::Client_Knockback_Implementation(FVector_NetQuantize LaunchVelocity)
{
	if (!HasAuthority() && !bEliminated)
	{
		LaunchCharacter(LaunchVelocity, false, false); // predicted; the server's version wins on correction
	}
}

void AKartCharacter::MoveBlockedBy(const FHitResult& Impact)
{
	Super::MoveBlockedBy(Impact);

	// Server: ramming a frozen kart smashes it (velocity is still the pre-impact one here)
	if (!HasAuthority() || bEliminated)
	{
		return;
	}
	AKartCharacter* Other = Cast<AKartCharacter>(Impact.GetActor());
	if (!Other || Other->IsEliminated())
	{
		return;
	}
	const FVector ToOther = (Other->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const float ClosingSpeed = static_cast<float>(FVector::DotProduct(GetVelocity() - Other->GetVelocity(), ToOther));
	if (AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>())
	{
		GM->HandleKartRam(this, Other, ClosingSpeed);
	}
}

void AKartCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (!HasAuthority() || bEliminated)
	{
		return; // already parked until the respawn
	}
	if (AKartGameMode* GM = GetWorld()->GetAuthGameMode<AKartGameMode>())
	{
		GM->KartEliminated(this, nullptr, nullptr, true);
	}
	else
	{
		Super::FellOutOfWorld(DmgType);
	}
}

AController* AKartCharacter::GetRecentAttacker(float WindowSeconds) const
{
	if (LastAttacker.IsValid() && KartTime::ServerNow(this) - LastAttackTime <= WindowSeconds)
	{
		return LastAttacker.Get();
	}
	return nullptr;
}

void AKartCharacter::Eliminate(bool bBigExplosion)
{
	if (!HasAuthority() || bEliminated)
	{
		return;
	}
	bEliminated = true;

	if (WeaponComponent)
	{
		WeaponComponent->DestroyWeapon();
	}
	if (StatusComponent)
	{
		StatusComponent->ClearAll();
	}
	Multicast_EliminatedFX(bBigExplosion);
	OnRep_Eliminated(); // RepNotify doesn't run on the server
}

void AKartCharacter::OnRep_Eliminated()
{
	UE_LOG(LogSmashKarts, Log, TEXT("[Kart] %s eliminated=%s (%s)"), *GetNameSafe(GetPlayerState()), bEliminated ? TEXT("yes") : TEXT("no"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"));

	if (!bEliminated)
	{
		return;
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RawSteerInput = 0.f;
	CurrentSteer = 0.f;
	if (WeaponComponent)
	{
		WeaponComponent->CancelCharge();
	}
	UpdateVisibility();
}

void AKartCharacter::Multicast_EliminatedFX_Implementation(bool bBigExplosion)
{
	OnEliminatedFX(bBigExplosion);
}

void AKartCharacter::OnEliminatedFX_Implementation(bool bBigExplosion)
{
	UNiagaraSystem* System = (bBigExplosion && BigDeathFX) ? BigDeathFX.Get() : DeathFX.Get();
	if (!System)
	{
		return;
	}
	const float Scale = (bBigExplosion && !BigDeathFX) ? BigDeathFXScale : 1.f;
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, GetActorLocation(), FRotator::ZeroRotator, FVector(Scale));
}

void AKartCharacter::Fire()
{
	if (IsInputLocked() || !WeaponComponent)
	{
		return;
	}
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Fire] pressed, has weapon: %s"), WeaponComponent->HasWeapon() ? TEXT("yes") : TEXT("no"));
	WeaponComponent->FirePressed();
}

void AKartCharacter::FireReleased()
{
	if (WeaponComponent)
	{
		if (IsInputLocked())
		{
			WeaponComponent->CancelCharge();
		}
		else
		{
			WeaponComponent->FireReleased();
		}
	}
}
