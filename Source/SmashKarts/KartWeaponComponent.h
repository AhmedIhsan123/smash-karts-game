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

	// Local (owning client or host): Fire key pressed
	void FirePressed();

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
};
