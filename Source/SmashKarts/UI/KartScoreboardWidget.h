#pragma once

#include "CoreMinimal.h"
#include "UI/KartWidgetBase.h"
#include "KartScoreboardWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * Scoreboard (hold Tab) and results screen (after the match). Local only; rebuilt from the
 * replicated PlayerStates a few times per second while visible.
 */
UCLASS()
class SMASHKARTS_API UKartScoreboardWidget : public UKartWidgetBase
{
	GENERATED_BODY()

public:
	/** Results mode shows the winner and the host's options. */
	void SetResultsMode(bool bInResults);

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Rebuild();
	UWidget* MakeRow(const TArray<FString>& Cells, const FLinearColor& Color, const FLinearColor* Swatch, int32 FontSize);

	UFUNCTION() void OnPlayAgainClicked();
	UFUNCTION() void OnLobbyClicked();
	UFUNCTION() void OnLeaveClicked();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FooterText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Rows;
	UPROPERTY(Transient) TObjectPtr<UWidget> ResultsButtons;
	UPROPERTY(Transient) TObjectPtr<UButton> PlayAgainButton;
	UPROPERTY(Transient) TObjectPtr<UButton> LobbyButton;

	bool bResults = false;
	float RefreshAccumulator = 1.f;
};
