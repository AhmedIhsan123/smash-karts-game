#pragma once

#include "CoreMinimal.h"
#include "UI/KartWidgetBase.h"
#include "KartLobbyWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * Lobby overlay (match state WaitingToStart): player list with colours and host badge, the online room code
 * (everyone sees it), Start Match, match-length and practice-bot buttons for the host, and for direct-IP games the
 * addresses friends should join. Local only; host actions go through AKartPlayerController Server RPCs (checked by
 * the game mode).
 */
UCLASS()
class SMASHKARTS_API UKartLobbyWidget : public UKartWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION() void OnStartClicked();
	UFUNCTION() void OnLength2Clicked();
	UFUNCTION() void OnLength3Clicked();
	UFUNCTION() void OnLength5Clicked();
	UFUNCTION() void OnAddBotClicked();
	UFUNCTION() void OnRemoveBotClicked();
	int32 CountBots() const;
	void SetLength(int32 Seconds);
	void Rebuild();
	static FString GetLocalAddresses();

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> PlayerList;
	UPROPERTY(Transient) TObjectPtr<UWidget> HostControls;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LengthText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BotsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ShareText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RoomCodeText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WaitingText;

	float RefreshAccumulator = 1.f;
	FString CachedAddresses;
};
