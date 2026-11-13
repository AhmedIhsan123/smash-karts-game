#pragma once

#include "CoreMinimal.h"
#include "UI/KartWidgetBase.h"
#include "KartPauseWidget.generated.h"

class UButton;
class UKartSettingsWidget;

DECLARE_MULTICAST_DELEGATE(FOnKartPauseResume);

/**
 * In-game menu (Esc / P): Resume, Settings, Leave Match, and End Match for the host. The game keeps running
 * underneath (it's multiplayer); the kart ignores your keys while this is open. Local only.
 */
UCLASS()
class SMASHKARTS_API UKartPauseWidget : public UKartWidgetBase
{
	GENERATED_BODY()

public:
	FOnKartPauseResume OnResume;

	/** Show the host-only button or not (call when opening). */
	void SetIsHost(bool bHost);

protected:
	virtual void BuildTree() override;

private:
	UFUNCTION() void OnResumeClicked();
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnLeaveClicked();
	UFUNCTION() void OnEndMatchClicked();
	void OnSettingsClosed();

	UPROPERTY(Transient) TObjectPtr<UWidget> MenuColumn;
	UPROPERTY(Transient) TObjectPtr<UButton> EndMatchButton;
	UPROPERTY(Transient) TObjectPtr<UKartSettingsWidget> SettingsPanel;
};
