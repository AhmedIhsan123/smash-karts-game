#include "KartDamageType.h"

FText UKartDamageType::GetKillFeedName(TSubclassOf<UDamageType> DamageType)
{
	if (const UKartDamageType* KartType = DamageType ? Cast<UKartDamageType>(DamageType->GetDefaultObject()) : nullptr)
	{
		if (!KartType->KillFeedName.IsEmpty())
		{
			return KartType->KillFeedName;
		}
	}
	return NSLOCTEXT("SmashKarts", "UnknownWeapon", "smash");
}

bool UKartDamageType::WantsBigExplosion(TSubclassOf<UDamageType> DamageType)
{
	const UKartDamageType* KartType = DamageType ? Cast<UKartDamageType>(DamageType->GetDefaultObject()) : nullptr;
	return KartType && KartType->bBigDeathExplosion;
}
