// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
DECLARE_LOG_CATEGORY_EXTERN(LogSmashKarts, Log, All);


/** Object channel for projectiles (Config/DefaultEngine.ini, "KartProjectile"): they ignore each other, hit everything else. */
#define ECC_KartProjectile ECC_GameTraceChannel1
