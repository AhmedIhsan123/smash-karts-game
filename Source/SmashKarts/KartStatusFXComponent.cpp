#include "KartStatusFXComponent.h"
#include "KartStatusComponent.h"
#include "SmashKarts.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UKartStatusFXComponent::UKartStatusFXComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while something animates
	SetIsReplicatedByDefault(false);

	ColorParameter = TEXT("Color");

	IceColor = FLinearColor(0.55f, 0.85f, 1.f, 0.45f);
	IceScale = FVector(1.5f, 1.3f, 1.1f);

	MaceColor = FLinearColor(0.12f, 0.12f, 0.14f);
	MaceSpikeColor = FLinearColor(0.8f, 0.8f, 0.85f);
	MaceCount = 5;
	MaceOrbitRadius = 150.f;
	MaceOrbitSpeed = 300.f;
	MaceScale = 0.35f;
	MaceHeight = -10.f;

	TeslaColor = FLinearColor(0.35f, 0.85f, 1.f, 0.3f);
	TeslaAuraScale = 2.f;
	TeslaRangeColor = FLinearColor(0.35f, 0.85f, 1.f, 0.08f);
	TeslaRange = 500.f;
	BeamColor = FLinearColor(0.6f, 1.6f, 3.f, 1.f);
	BeamSegments = 4;
	BeamJitter = 30.f;
	BeamThickness = 6.f;
	BeamDuration = 0.12f;
	ZapHitFXScale = 0.4f;

	InvincibleColor = FLinearColor(1.f, 0.8f, 0.25f, 0.3f);
	InvincibleScale = 2.4f;

	PoppedTiresFXInterval = 0.5f;
	PoppedTiresFXScale = 0.3f;
}

void UKartStatusFXComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || GetNetMode() == NM_DedicatedServer)
	{
		return; // nothing to look at
	}

	// We ride on the kart's mesh (network smoothing) but want to sit at the capsule centre with the kart's heading:
	// undo the mesh's own offset from the capsule
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (Character->GetCapsuleComponent())
		{
			HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		if (Character->GetMesh() && GetAttachParent() == Character->GetMesh())
		{
			const FTransform MeshBase(Character->GetBaseRotationOffset(), Character->GetBaseTranslationOffset(), Character->GetMesh()->GetRelativeScale3D());
			SetRelativeTransform(MeshBase.Inverse());
		}
	}

	Status = Owner->FindComponentByClass<UKartStatusComponent>();
	if (Status.IsValid())
	{
		Status->OnStatusChanged.AddUObject(this, &UKartStatusFXComponent::HandleStatusChanged);
	}
	BuildPieces();
	Refresh(); // a late joiner may already see buffs on this kart
}

void UKartStatusFXComponent::BuildPieces()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	if (IceMesh)
	{
		IceBlock = MakePiece(IceMesh, this, IceMaterial, IceColor, true);
		IceBlock->SetRelativeScale3D(IceScale);
	}

	if (MaceMesh && MaceCount > 0)
	{
		MaceRing = NewObject<USceneComponent>(GetOwner(), NAME_None, RF_Transient);
		MaceRing->SetupAttachment(this);
		MaceRing->RegisterComponent();
		MaceRing->SetRelativeLocation(FVector(0.f, 0.f, MaceHeight));
		MaceRing->SetVisibility(false, true);

		UMaterialInstanceDynamic* HeadMID = MakeMID(MaceMaterial, MaceColor);
		UMaterialInstanceDynamic* SpikeMID = MakeMID(MaceMaterial, MaceSpikeColor);
		static const FVector SpikeDirections[] = { FVector::ForwardVector, -FVector::ForwardVector, FVector::RightVector,
			-FVector::RightVector, FVector::UpVector, -FVector::UpVector };

		for (int32 i = 0; i < MaceCount; ++i)
		{
			const float Angle = 2.f * PI * static_cast<float>(i) / static_cast<float>(MaceCount);
			UStaticMeshComponent* Head = MakePiece(MaceMesh, MaceRing, nullptr, FLinearColor::White, false);
			if (HeadMID)
			{
				Head->SetMaterial(0, HeadMID);
			}
			Head->SetRelativeLocation(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * MaceOrbitRadius);
			Head->SetRelativeScale3D(FVector(MaceScale));

			if (MaceSpikeMesh)
			{
				// Spikes in the head's local space (engine sphere radius 50, cone 100 tall, pivot in the middle)
				for (const FVector& Direction : SpikeDirections)
				{
					UStaticMeshComponent* Spike = MakePiece(MaceSpikeMesh, Head, nullptr, FLinearColor::White, false);
					if (SpikeMID)
					{
						Spike->SetMaterial(0, SpikeMID);
					}
					Spike->SetRelativeLocation(Direction * 60.f);
					Spike->SetRelativeRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
					Spike->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.5f));
				}
			}
		}
	}

	if (AuraMesh)
	{
		TeslaAura = MakePiece(AuraMesh, this, AuraMaterial, TeslaColor, true);
		TeslaAura->SetRelativeScale3D(FVector(TeslaAuraScale));
		TeslaAuraMID = Cast<UMaterialInstanceDynamic>(TeslaAura->GetMaterial(0));

		ShieldBubble = MakePiece(AuraMesh, this, AuraMaterial, InvincibleColor, true);
		ShieldBubble->SetRelativeScale3D(FVector(InvincibleScale));
	}

	if (RangeRingMesh && TeslaRange > 0.f)
	{
		// Flat disc just above the ground (engine cylinder: 100 cm wide, 100 tall)
		TeslaRing = MakePiece(RangeRingMesh, this, AuraMaterial, TeslaRangeColor, true);
		TeslaRing->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight + 2.f));
		TeslaRing->SetRelativeScale3D(FVector(TeslaRange / 50.f, TeslaRange / 50.f, 0.02f));
	}

	if (BeamMesh && BeamMaterial)
	{
		BeamMID = MakeMID(BeamMaterial, BeamColor);
	}
}

UStaticMeshComponent* UKartStatusFXComponent::MakePiece(UStaticMesh* Mesh, USceneComponent* Parent, UMaterialInterface* Material,
	const FLinearColor& Color, bool bTranslucent)
{
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
	Piece->SetStaticMesh(Mesh);
	Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Piece->SetGenerateOverlapEvents(false);
	Piece->SetCanEverAffectNavigation(false);
	Piece->SetCastShadow(!bTranslucent);
	Piece->bReceivesDecals = false;
	Piece->SetupAttachment(Parent);
	Piece->SetVisibility(false);
	Piece->RegisterComponent();
	if (Material)
	{
		Piece->SetMaterial(0, MakeMID(Material, Color));
	}
	return Piece;
}

UMaterialInstanceDynamic* UKartStatusFXComponent::MakeMID(UMaterialInterface* Material, const FLinearColor& Color)
{
	if (!Material)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
	if (MID)
	{
		MID->SetVectorParameterValue(ColorParameter, Color);
	}
	return MID;
}

void UKartStatusFXComponent::SetPieceVisible(UStaticMeshComponent* Piece, bool bShow)
{
	if (Piece && Piece->IsVisible() != bShow)
	{
		Piece->SetVisibility(bShow);
	}
}

void UKartStatusFXComponent::HandleStatusChanged(uint8 OldFlags, uint8 NewFlags)
{
	Refresh();
}

void UKartStatusFXComponent::Refresh()
{
	const UKartStatusComponent* StatusComp = Status.Get();
	auto Has = [StatusComp](EKartStatus Flag) { return StatusComp && StatusComp->HasStatus(Flag); };

	SetPieceVisible(IceBlock, Has(EKartStatus::Frozen));
	if (MaceRing && MaceRing->IsVisible() != Has(EKartStatus::Spiky))
	{
		MaceRing->SetVisibility(Has(EKartStatus::Spiky), true);
	}
	SetPieceVisible(TeslaAura, Has(EKartStatus::Tesla));
	SetPieceVisible(TeslaRing, Has(EKartStatus::Tesla));
	SetPieceVisible(ShieldBubble, Has(EKartStatus::Invincible));

	const bool bNeedsTick = Has(EKartStatus::Spiky) || Has(EKartStatus::Tesla) || (Has(EKartStatus::TiresPopped) && PoppedTiresFX)
		|| BeamsHideTime > 0.0;
	SetComponentTickEnabled(bNeedsTick);
}

void UKartStatusFXComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double Now = GetWorld()->GetTimeSeconds();

	if (MaceRing && MaceRing->IsVisible())
	{
		MaceAngle = FMath::Fmod(MaceAngle + MaceOrbitSpeed * DeltaTime, 360.f);
		MaceRing->SetRelativeRotation(FRotator(0.f, MaceAngle, 0.f));
	}

	if (TeslaAura && TeslaAura->IsVisible() && TeslaAuraMID)
	{
		// Crackle: the aura flickers in opacity and size
		FLinearColor Flicker = TeslaColor;
		Flicker.A *= FMath::FRandRange(0.45f, 1.f);
		TeslaAuraMID->SetVectorParameterValue(ColorParameter, Flicker);
		TeslaAura->SetRelativeScale3D(FVector(TeslaAuraScale * FMath::FRandRange(0.94f, 1.04f)));
	}

	const UKartStatusComponent* StatusComp = Status.Get();
	if (PoppedTiresFX && StatusComp && StatusComp->HasStatus(EKartStatus::TiresPopped) && Now >= NextSparkTime)
	{
		NextSparkTime = Now + PoppedTiresFXInterval;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PoppedTiresFX, GetComponentLocation() - FVector(0.f, 0.f, HalfHeight * 0.8f),
			FRotator::ZeroRotator, FVector(PoppedTiresFXScale));
	}

	if (BeamsHideTime > 0.0 && Now >= BeamsHideTime)
	{
		BeamsHideTime = 0.0;
		for (UStaticMeshComponent* Beam : Beams)
		{
			SetPieceVisible(Beam, false);
		}
		Refresh(); // may switch the tick off again
	}
}

FVector UKartStatusFXComponent::GetZapPoint(const AActor* Actor) const
{
	// The target's own FX component sits at its smoothed centre, which matches what this machine shows
	if (const UKartStatusFXComponent* TargetFX = Actor->FindComponentByClass<UKartStatusFXComponent>())
	{
		return TargetFX->GetComponentLocation();
	}
	return Actor->GetActorLocation();
}

void UKartStatusFXComponent::PlayZapBeams(const TArray<AActor*>& Targets)
{
	if (!BeamMesh || !BeamMID || GetNetMode() == NM_DedicatedServer || !GetWorld())
	{
		return;
	}

	const FVector Start = GetComponentLocation();
	int32 Used = 0;
	for (const AActor* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}
		const FVector End = GetZapPoint(Target);
		const FVector Span = End - Start;
		const float Length = static_cast<float>(Span.Size());
		if (Length < 1.f)
		{
			continue;
		}

		// Kinks go sideways and a little up/down, never along the beam
		const FVector Axis = Span / Length;
		FVector Side = FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			Side = FVector::RightVector;
		}
		const FVector Lift = FVector::CrossProduct(Side, Axis);

		FVector Prev = Start;
		for (int32 Segment = 1; Segment <= BeamSegments; ++Segment)
		{
			FVector Point = Start + Span * (static_cast<float>(Segment) / static_cast<float>(BeamSegments));
			if (Segment < BeamSegments)
			{
				Point += Side * FMath::FRandRange(-BeamJitter, BeamJitter) + Lift * FMath::FRandRange(-BeamJitter, BeamJitter) * 0.6f;
			}

			if (!Beams.IsValidIndex(Used))
			{
				UStaticMeshComponent* NewBeam = MakePiece(BeamMesh, this, nullptr, FLinearColor::White, true);
				NewBeam->SetMaterial(0, BeamMID);
				NewBeam->SetUsingAbsoluteLocation(true); // the zap stays where it happened while the kart moves on
				NewBeam->SetUsingAbsoluteRotation(true);
				NewBeam->SetUsingAbsoluteScale(true);
				Beams.Add(NewBeam);
			}
			UStaticMeshComponent* Beam = Beams[Used++];
			const FVector Piece = Point - Prev;
			const float PieceLength = FMath::Max(1.f, static_cast<float>(Piece.Size()));
			Beam->SetWorldLocationAndRotation((Prev + Point) * 0.5f, FRotationMatrix::MakeFromZ(Piece / PieceLength).Rotator());
			Beam->SetWorldScale3D(FVector(BeamThickness / 100.f, BeamThickness / 100.f, PieceLength / 100.f));
			SetPieceVisible(Beam, true);
			Prev = Point;
		}

		if (ZapHitFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ZapHitFX, End, FRotator::ZeroRotator, FVector(ZapHitFXScale));
		}
	}

	for (int32 i = Used; i < Beams.Num(); ++i)
	{
		SetPieceVisible(Beams[i], false);
	}
	UE_LOG(LogSmashKarts, Verbose, TEXT("[FX] %s zap: %d target(s), %d beam pieces (%s)"), *GetNameSafe(GetOwner()), Targets.Num(), Used,
		(GetOwner() && GetOwner()->HasAuthority()) ? TEXT("Server") : TEXT("Client"));
	if (Used > 0)
	{
		BeamsHideTime = GetWorld()->GetTimeSeconds() + BeamDuration;
		SetComponentTickEnabled(true);
	}
}
