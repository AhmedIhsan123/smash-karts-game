#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Layout/Margin.h"
#include "Widgets/Layout/Anchors.h"
#include "KartWidgetBase.generated.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UCanvasPanel;
class UCanvasPanelSlot;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * Base for UI whose widget tree is built in C++ (no widget Blueprints needed).
 * Subclasses override BuildTree() and create widgets with the helpers below.
 * Local only: widgets exist on the owning player's machine and never replicate.
 */
UCLASS(Abstract)
class SMASHKARTS_API UKartWidgetBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Build the widget tree here; set WidgetTree->RootWidget. */
	virtual void BuildTree() {}

	// ---------- Helpers ----------
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color = FLinearColor::White, bool bOutline = true);

	/** Button with a centred label. Bind with Button->OnClicked.AddDynamic(this, &UMyWidget::Handler). */
	UButton* MakeButton(const FString& Label, int32 FontSize = 22, const FLinearColor& Tint = FLinearColor(0.15f, 0.45f, 0.95f), float MinWidth = 220.f);

	/** Text field with a hint (wrap it in Sized() to set its width). */
	UEditableTextBox* MakeTextField(const FString& Text, const FString& Hint);

	/** Wraps a widget in a fixed-size box (width/height <= 0 = free). */
	UWidget* Sized(UWidget* Child, float Width, float Height);
	UBorder* MakePanel(const FLinearColor& Color, const FMargin& InPadding);
	UCanvasPanelSlot* AddToCanvas(UCanvasPanel* Canvas, UWidget* Child, const FAnchors& Anchors, const FVector2D& Alignment,
		const FVector2D& Position, bool bAutoSize = true, const FVector2D& Size = FVector2D(100.f, 30.f));

	static void SetTextIfChanged(UTextBlock* Block, const FString& NewText);
	static void SetTextColor(UTextBlock* Block, const FLinearColor& Color);

private:
	void EnsureTree();
};
