#include "UI/KartNameTagWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UKartNameTagWidget::BuildTree()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Column;

	NameText = MakeText(TEXT(""), 16);
	Column->AddChildToVerticalBox(NameText)->SetHorizontalAlignment(HAlign_Center);

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
	HealthBar->SetPercent(1.f);
	Column->AddChildToVerticalBox(Sized(HealthBar, 90.f, 8.f))->SetHorizontalAlignment(HAlign_Center);
}

void UKartNameTagWidget::SetInfo(const FString& Name, float HealthFraction, const FLinearColor& Color)
{
	SetTextIfChanged(NameText, Name);
	SetTextColor(NameText, Color);
	if (HealthBar)
	{
		HealthBar->SetPercent(FMath::Clamp(HealthFraction, 0.f, 1.f));
		HealthBar->SetFillColorAndOpacity(HealthFraction > 0.5f ? FLinearColor(0.15f, 0.85f, 0.25f)
			: (HealthFraction > 0.25f ? FLinearColor(1.f, 0.75f, 0.1f) : FLinearColor(1.f, 0.2f, 0.15f)));
	}
}
