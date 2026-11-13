#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "KartStatusFXComponent.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UNiagaraSystem;
class UKartStatusComponent;

/**
 * Cosmetic status visuals on a kart: ice block (Frozen), orbiting maces (Spiky / Mace Orbit), electric aura and
 * range ring (Tesla Coil), shield bubble (Invincible), sparks (TiresPopped) and the Tesla zap beams.
 *
 * Authority: none, cosmetic only (never touches gameplay).
 * Replicates: nothing itself. It follows the replicated status flags of the kart's UKartStatusComponent, so every
 * machine shows the same thing; zap beams are started by ABuffWeapon's multicast.
 * Client prediction: not needed (visuals follow replicated state).
 *
 * Meshes, materials and numbers are set on BP_Kart's StatusFX component (no asset paths in C++). Anything left
 * empty is skipped. Pieces are built once in BeginPlay and shown / hidden as statuses change.
 * The component sits at the capsule centre (kart heading), but rides on the smoothed mesh so it never jitters.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SMASHKARTS_API UKartStatusFXComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UKartStatusFXComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Every machine: flash jagged electric beams from this kart to each target (one Tesla Coil zap). */
	void PlayZapBeams(const TArray<AActor*>& Targets);

protected:
	virtual void BeginPlay() override;

	/** Vector parameter that takes each piece's colour (alpha = opacity on the translucent materials). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX")
	FName ColorParameter;

	// ---------------- Frozen: ice block ----------------
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Frozen")
	TObjectPtr<UStaticMesh> IceMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Frozen")
	TObjectPtr<UMaterialInterface> IceMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Frozen")
	FLinearColor IceColor;

	/** Scale of IceMesh (engine cube = 100 cm), centred on the kart. */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Frozen")
	FVector IceScale;

	// ---------------- Spiky (Mace Orbit): maces ----------------
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces")
	TObjectPtr<UStaticMesh> MaceMesh;

	/** Optional spikes around each mace head (six per mace). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces")
	TObjectPtr<UStaticMesh> MaceSpikeMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces")
	TObjectPtr<UMaterialInterface> MaceMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces")
	FLinearColor MaceColor;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces")
	FLinearColor MaceSpikeColor;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces", meta = (ClampMin = "0", ClampMax = "16"))
	int32 MaceCount;

	/** Keep in line with the game mode's SpikyReach (200 = this radius + the kart's 50 cm radius). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces", meta = (ClampMin = "0"))
	float MaceOrbitRadius;

	/** Degrees per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces")
	float MaceOrbitSpeed;

	/** Uniform scale of the mace head (engine sphere = 100 cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces", meta = (ClampMin = "0.01"))
	float MaceScale;

	/** Height of the maces above the capsule centre (negative = lower). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Maces")
	float MaceHeight;

	// ---------------- Tesla Coil: aura, range ring, beams ----------------
	/** Sphere used for the Tesla aura and the Invincible bubble. */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla")
	TObjectPtr<UStaticMesh> AuraMesh;

	/** Translucent material (Color alpha = opacity). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla")
	TObjectPtr<UMaterialInterface> AuraMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla")
	FLinearColor TeslaColor;

	/** Uniform scale of the aura sphere. */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla", meta = (ClampMin = "0.01"))
	float TeslaAuraScale;

	/** Flat disc on the ground showing the zap range (engine cylinder). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla")
	TObjectPtr<UStaticMesh> RangeRingMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla")
	FLinearColor TeslaRangeColor;

	/** Ring radius in cm. Keep in line with DA_TeslaCoil's AuraRadius. */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla", meta = (ClampMin = "0"))
	float TeslaRange;

	/** Beam segment mesh (engine cylinder: 100 cm tall along Z). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla")
	TObjectPtr<UStaticMesh> BeamMesh;

	/** Glowing material for the beams (additive looks best). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla")
	TObjectPtr<UMaterialInterface> BeamMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla")
	FLinearColor BeamColor;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla", meta = (ClampMin = "1", ClampMax = "12"))
	int32 BeamSegments;

	/** Max sideways kink of the beam's middle points (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla", meta = (ClampMin = "0"))
	float BeamJitter;

	/** Beam thickness (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla", meta = (ClampMin = "0.5"))
	float BeamThickness;

	/** Seconds a beam stays visible. */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla", meta = (ClampMin = "0.02"))
	float BeamDuration;

	/** Spark burst where a beam lands (optional). */
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla")
	TObjectPtr<UNiagaraSystem> ZapHitFX;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Tesla", meta = (ClampMin = "0.01"))
	float ZapHitFXScale;

	// ---------------- Invincible: shield bubble (uses AuraMesh / AuraMaterial) ----------------
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Invincible")
	FLinearColor InvincibleColor;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Invincible", meta = (ClampMin = "0.01"))
	float InvincibleScale;

	// ---------------- Popped tires: sparks under the kart ----------------
	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Popped Tires")
	TObjectPtr<UNiagaraSystem> PoppedTiresFX;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Popped Tires", meta = (ClampMin = "0.05"))
	float PoppedTiresFXInterval;

	UPROPERTY(EditDefaultsOnly, Category = "Status FX|Popped Tires", meta = (ClampMin = "0.01"))
	float PoppedTiresFXScale;

private:
	void BuildPieces();
	void HandleStatusChanged(uint8 OldFlags, uint8 NewFlags);
	void Refresh();
	UStaticMeshComponent* MakePiece(UStaticMesh* Mesh, USceneComponent* Parent, UMaterialInterface* Material, const FLinearColor& Color, bool bTranslucent);
	UMaterialInstanceDynamic* MakeMID(UMaterialInterface* Material, const FLinearColor& Color);
	void SetPieceVisible(UStaticMeshComponent* Piece, bool bShow);
	FVector GetZapPoint(const AActor* Actor) const;

	TWeakObjectPtr<UKartStatusComponent> Status;
	bool bBuilt = false;
	float HalfHeight = 50.f;
	float MaceAngle = 0.f;
	double BeamsHideTime = 0.0;
	double NextSparkTime = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> IceBlock;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> MaceRing;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TeslaAura;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TeslaRing;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ShieldBubble;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Beams;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TeslaAuraMID;
};
