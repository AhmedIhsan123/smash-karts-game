#pragma once

#include "CoreMinimal.h"
#include "UI/KartWidgetBase.h"
#include "KartSettingsWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnKartSettingsClosed);

/**
 * Settings panel (main menu and pause menu): master volume, resolution, window mode, quality preset, screen shake,
 * controls list. Applies and saves immediately (GameUserSettings + the player's save file). Local only.
 */
UCLASS()
class SMASHKARTS_API UKartSettingsWidget : public UKartWidgetBase
{
	GENERATED_BODY()

public:
	FOnKartSettingsClosed OnClosed;

	/** Re-read current values into the controls (call when showing the panel). */
	void Refresh();

protected:
	virtual void BuildTree() override;

private:
	UFUNCTION() void OnVolumeChanged(float Value);
	UFUNCTION() void OnResolutionClicked();
	UFUNCTION() void OnWindowModeClicked();
	UFUNCTION() void OnQualityClicked();
	UFUNCTION() void OnShakeClicked();
	UFUNCTION() void OnBackClicked();

	void ApplyDisplaySettings();

	UPROPERTY(Transient) TObjectPtr<USlider> VolumeSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> VolumeText;
	UPROPERTY(Transient) TObjectPtr<UButton> ResolutionButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResolutionText;
	UPROPERTY(Transient) TObjectPtr<UButton> WindowModeButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WindowModeText;
	UPROPERTY(Transient) TObjectPtr<UButton> QualityButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> QualityText;
	UPROPERTY(Transient) TObjectPtr<UButton> ShakeButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ShakeText;
	UPROPERTY(Transient) TObjectPtr<UButton> BackButton;
};
