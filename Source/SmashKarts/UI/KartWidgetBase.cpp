#include "UI/KartWidgetBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Fonts/SlateFontInfo.h"

void UKartWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureTree();
}

TSharedRef<SWidget> UKartWidgetBase::RebuildWidget()
{
	EnsureTree();
	return Super::RebuildWidget();
}

void UKartWidgetBase::EnsureTree()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
}

UTextBlock* UKartWidgetBase::MakeText(const FString& Text, int32 Size, const FLinearColor& Color, bool bOutline)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(FText::FromString(Text));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	if (bOutline)
	{
		Font.OutlineSettings.OutlineSize = FMath::Max(1, Size / 12);
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
	}
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

UButton* UKartWidgetBase::MakeButton(const FString& Label, int32 FontSize, const FLinearColor& Tint, float MinWidth)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetBackgroundColor(Tint);
	UTextBlock* Text = MakeText(Label, FontSize, FLinearColor::White, false);
	Text->SetJustification(ETextJustify::Center);
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetMinDesiredWidth(MinWidth);
	Box->SetContent(Text);
	Button->SetContent(Box);
	return Button;
}

UEditableTextBox* UKartWidgetBase::MakeTextField(const FString& Text, const FString& Hint)
{
	UEditableTextBox* Field = WidgetTree->ConstructWidget<UEditableTextBox>();
	Field->SetText(FText::FromString(Text));
	Field->SetHintText(FText::FromString(Hint));
	return Field;
}

UWidget* UKartWidgetBase::Sized(UWidget* Child, float Width, float Height)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	if (Width > 0.f)
	{
		Box->SetWidthOverride(Width);
	}
	if (Height > 0.f)
	{
		Box->SetHeightOverride(Height);
	}
	Box->SetContent(Child);
	return Box;
}

UBorder* UKartWidgetBase::MakePanel(const FLinearColor& Color, const FMargin& InPadding)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
	Border->SetBrushColor(Color);
	Border->SetPadding(InPadding);
	return Border;
}

UCanvasPanelSlot* UKartWidgetBase::AddToCanvas(UCanvasPanel* Canvas, UWidget* Child, const FAnchors& Anchors,
	const FVector2D& Alignment, const FVector2D& Position, bool bAutoSize, const FVector2D& Size)
{
	UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
	CanvasSlot->SetAnchors(Anchors);
	CanvasSlot->SetAlignment(Alignment);
	CanvasSlot->SetPosition(Position);
	CanvasSlot->SetAutoSize(bAutoSize);
	if (!bAutoSize)
	{
		CanvasSlot->SetSize(Size);
	}
	return CanvasSlot;
}

void UKartWidgetBase::SetTextIfChanged(UTextBlock* Block, const FString& NewText)
{
	if (Block && !Block->GetText().ToString().Equals(NewText, ESearchCase::CaseSensitive))
	{
		Block->SetText(FText::FromString(NewText));
	}
}

void UKartWidgetBase::SetTextColor(UTextBlock* Block, const FLinearColor& Color)
{
	if (Block)
	{
		Block->SetColorAndOpacity(FSlateColor(Color));
	}
}
