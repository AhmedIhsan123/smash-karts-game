#pragma once

#include "CoreMinimal.h"
#include "UI/KartWidgetBase.h"
#include "KartMainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UKartSettingsWidget;

/**
 * Main menu: player name, Host Online Game (room code) and Host LAN / IP Game, Join Game (room code or IP),
 * Settings, Quit, the last connection message ("The host left the game", "No game found with code...") and an
 * online status line. Without online play (no EOS credentials) it shows a single Host Game button like before.
 * Local only; all actions go through UKartGameInstance.
 */
UCLASS()
class SMASHKARTS_API UKartMainMenuWidget : public UKartWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION() void OnHostOnlineClicked();
	UFUNCTION() void OnHostClicked();
	UFUNCTION() void OnJoinClicked();
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnQuitClicked();
	void OnSettingsClosed();
	void SaveName();
	void ShowMessage(const FString& Message, bool bError);
	/** Online status line + which host buttons show. Polled twice a second. */
	void RefreshOnline();

	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> NameField;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> AddressField;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OnlineText;
	UPROPERTY(Transient) TObjectPtr<UButton> HostOnlineButton;
	UPROPERTY(Transient) TObjectPtr<UButton> HostButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HostButtonLabel;
	UPROPERTY(Transient) TObjectPtr<UWidget> MainColumn;
	UPROPERTY(Transient) TObjectPtr<UKartSettingsWidget> SettingsPanel;

	float RefreshAccumulator = 1.f;
	/** -1 = not laid out yet, 0 = direct IP only, 1 = online buttons shown. */
	int8 ShownOnlineMode = -1;
};
