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

void UKartDamageType::GetKnockback(TSubclassOf<UDamageType> DamageType, float& OutStrength, float& OutUpward)
{
	const UKartDamageType* KartType = DamageType ? Cast<UKartDamageType>(DamageType->GetDefaultObject()) : nullptr;
	OutStrength = KartType ? KartType->KnockbackStrength : 0.f;
	OutUpward = KartType ? KartType->KnockbackUpward : 0.f;
}

bool UKartDamageType::IgnoresDamageReduction(TSubclassOf<UDamageType> DamageType)
{
	const UKartDamageType* KartType = DamageType ? Cast<UKartDamageType>(DamageType->GetDefaultObject()) : nullptr;
	return KartType && KartType->bIgnoreDamageReduction;
}
