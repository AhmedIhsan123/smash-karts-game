#pragma once

#include "CoreMinimal.h"
#include "UI/KartWidgetBase.h"
#include "KartMainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UKartSettingsWidget;

/**
 * Main menu: player name, Host Game, Join Game (IP), Settings, Quit, and the last connection message
 * ("The host left the game", "Couldn't connect..."). Local only; all actions go through UKartGameInstance.
 */
UCLASS()
class SMASHKARTS_API UKartMainMenuWidget : public UKartWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION() void OnHostClicked();
	UFUNCTION() void OnJoinClicked();
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnQuitClicked();
	void OnSettingsClosed();
	void SaveName();
	void ShowMessage(const FString& Message, bool bError);

	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> NameField;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> AddressField;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UWidget> MainColumn;
	UPROPERTY(Transient) TObjectPtr<UKartSettingsWidget> SettingsPanel;
};
