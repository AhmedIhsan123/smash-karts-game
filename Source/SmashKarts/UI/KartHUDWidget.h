#pragma once

#include "CoreMinimal.h"
#include "UI/KartWidgetBase.h"
#include "KartHUDWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

/**
 * In-match HUD: timer, phase, score/rank, health, weapon, kill feed, banners, respawn and spawn-protection text.
 * Local only. Polls replicated state every frame (GameState / PlayerState / own kart); events (kill feed,
 * banners) are pushed by AKartHUD.
 */
UCLASS()
class SMASHKARTS_API UKartHUDWidget : public UKartWidgetBase
{
	GENERATED_BODY()

public:
	void AddKillFeedLine(const FString& Text, const FLinearColor& Color);
	void ShowBanner(const FString& Text, const FLinearColor& Color, float Seconds);

	/** Red flash over the screen that fades in about a quarter second (your kart took Amount damage). */
	void FlashDamage(float Amount);

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshKillFeed(double LocalNow);

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimerText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PhaseText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ScoreText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CenterText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WeaponText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UsesText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PickupText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> ChargeBar;
	UPROPERTY(Transient) TObjectPtr<UBorder> WeaponIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WeaponIconText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> KillFeedBox;
	UPROPERTY(Transient) TObjectPtr<UBorder> DamageFlash;
	float DamageFlashAlpha = 0.f;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> KillFeedLines;

	struct FFeedEntry
	{
		FString Text;
		FLinearColor Color;
		double Time = 0.0;
	};
	TArray<FFeedEntry> FeedEntries;

	FString BannerText;
	FLinearColor BannerColor = FLinearColor::White;
	double BannerUntil = 0.0;

	FName LastMatchState;
	double GoUntil = 0.0;

	// "Picked up: Rockets" when the held weapon changes to a new one
	TWeakObjectPtr<const class AKartWeapon> LastWeapon;
	double PickupUntil = 0.0;
};
