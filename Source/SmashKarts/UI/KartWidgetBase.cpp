#include "UI/KartWidgetBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
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
