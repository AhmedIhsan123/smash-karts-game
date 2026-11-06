#include "KartWeaponData.h"
#include "GameFramework/DamageType.h"

TSubclassOf<UDamageType> UKartWeaponData::GetDamageTypeOrDefault() const
{
	return DamageType ? DamageType : TSubclassOf<UDamageType>(UDamageType::StaticClass());
}
