#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "KartSaveGame.generated.h"

/** Local player preferences (name, volume). Display settings live in GameUserSettings. Never replicated. */
UCLASS()
class SMASHKARTS_API UKartSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString PlayerName;

	UPROPERTY()
	float MasterVolume = 1.f;

	UPROPERTY()
	FString LastJoinAddress;
};
