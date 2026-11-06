#pragma once

#include "CoreMinimal.h"
#include "KartWeapon.h"
#include "TrapWeapon.generated.h"

/** Archetype C: Mines, Spike Strip, Fake Loot Box. Each shot drops one AKartTrap behind the kart. */
UCLASS()
class SMASHKARTS_API ATrapWeapon : public AKartWeapon
{
	GENERATED_BODY()

protected:
	virtual void FireOnce(float ChargeAlpha) override;
};
