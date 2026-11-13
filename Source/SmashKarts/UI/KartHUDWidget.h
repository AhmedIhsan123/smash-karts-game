#pragma once

#include "CoreMinimal.h"
#include "UI/KartWidgetBase.h"
#include "KartHUDWidget.generated.h"

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
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> KillFeedBox;
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
};
