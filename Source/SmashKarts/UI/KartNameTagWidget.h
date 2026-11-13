#pragma once

#include "CoreMinimal.h"
#include "UI/KartWidgetBase.h"
#include "KartNameTagWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** Name + mini health bar shown above other players' karts (screen-space widget component). Local only. */
UCLASS()
class SMASHKARTS_API UKartNameTagWidget : public UKartWidgetBase
{
	GENERATED_BODY()

public:
	void SetInfo(const FString& Name, float HealthFraction, const FLinearColor& Color);

protected:
	virtual void BuildTree() override;

private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
};
