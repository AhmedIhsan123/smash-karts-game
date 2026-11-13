#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KartWeaponComponent.generated.h"

class AKartWeapon;
class UKartWeaponData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponChanged, AKartWeapon*, NewWeapon);

/** Holds the kart's current weapon and routes Fire to the server. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SMASHKARTS_API UKartWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKartWeaponComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Local (owning client or host): Fire key pressed. Charge weapons start charging instead of firing
	void FirePressed();

	// Local: Fire key released. Charge weapons fire now with the measured charge (0..1, clamped again on the server)
	void FireReleased();

	// Local: drop a charge in progress without firing (smashed, weapon gone)
	void CancelCharge() { bCharging = false; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsCharging() const { return bCharging; }

	// 0..1 while charging (local)
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetChargeAlpha() const;

	// Server: give a weapon (loot boxes call this). False if we already hold one
	bool GiveWeapon(UKartWeaponData* Data);

	// Server: the weapon calls this when it's used up
	void ClearWeapon(AKartWeapon* Weapon);

	// Server: destroy the held weapon right away (elimination, debug)
	void DestroyWeapon();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool HasWeapon() const { return CurrentWeapon != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AKartWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	// Fires on every machine when the held weapon changes (HUD hook later)
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponChanged OnWeaponChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AKartWeapon> CurrentWeapon;

	UFUNCTION()
	void OnRep_CurrentWeapon();

	UFUNCTION(Server, Reliable)
	void Server_Fire(float ChargeAlpha);

private:
	// Local charge state (owning machine only, never replicated)
	bool bCharging = false;
	double ChargeStartTime = 0.0;
};
