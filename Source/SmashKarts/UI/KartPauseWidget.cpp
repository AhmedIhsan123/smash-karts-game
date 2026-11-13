#include "UI/KartPauseWidget.h"
#include "UI/KartSettingsWidget.h"
#include "Game/KartGameInstance.h"
#include "Game/KartPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UKartPauseWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Dim the game behind the menu
	UBorder* Dim = MakePanel(FLinearColor(0.f, 0.f, 0.f, 0.45f), FMargin(0.f));
	UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim);
	DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	DimSlot->SetOffsets(FMargin(0.f));

	UBorder* Panel = MakePanel(FLinearColor(0.02f, 0.03f, 0.08f, 0.95f), FMargin(32.f, 24.f));
	MenuColumn = Panel;
	AddToCanvas(Root, Panel, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);
	Column->AddChildToVerticalBox(MakeText(TEXT("MENU"), 32, FLinearColor(1.f, 0.82f, 0.2f)))->SetHorizontalAlignment(HAlign_Center);

	auto AddButton = [this, Column](const FString& Label, const FLinearColor& Tint) -> UButton*
	{
		UButton* Button = MakeButton(Label, 22, Tint, 300.f);
		Column->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 6.f));
		return Button;
	};
	AddButton(TEXT("Resume"), FLinearColor(0.1f, 0.6f, 0.25f))->OnClicked.AddDynamic(this, &UKartPauseWidget::OnResumeClicked);
	AddButton(TEXT("Settings"), FLinearColor(0.25f, 0.3f, 0.45f))->OnClicked.AddDynamic(this, &UKartPauseWidget::OnSettingsClicked);
	EndMatchButton = AddButton(TEXT("End Match (host)"), FLinearColor(0.6f, 0.4f, 0.1f));
	EndMatchButton->OnClicked.AddDynamic(this, &UKartPauseWidget::OnEndMatchClicked);
	AddButton(TEXT("Leave Match"), FLinearColor(0.55f, 0.15f, 0.15f))->OnClicked.AddDynamic(this, &UKartPauseWidget::OnLeaveClicked);

	SettingsPanel = CreateWidget<UKartSettingsWidget>(GetOwningPlayer(), UKartSettingsWidget::StaticClass());
	if (SettingsPanel)
	{
		AddToCanvas(Root, SettingsPanel, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
		SettingsPanel->OnClosed.AddUObject(this, &UKartPauseWidget::OnSettingsClosed);
	}
}

void UKartPauseWidget::SetIsHost(bool bHost)
{
	if (EndMatchButton)
	{
		EndMatchButton->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SettingsPanel)
	{
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MenuColumn)
	{
		MenuColumn->SetVisibility(ESlateVisibility::Visible);
	}
}

void UKartPauseWidget::OnResumeClicked()
{
	OnResume.Broadcast();
}

void UKartPauseWidget::OnSettingsClicked()
{
	SettingsPanel->Refresh();
	SettingsPanel->SetVisibility(ESlateVisibility::Visible);
	MenuColumn->SetVisibility(ESlateVisibility::Collapsed);
}

void UKartPauseWidget::OnSettingsClosed()
{
	SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
	MenuColumn->SetVisibility(ESlateVisibility::Visible);
}

void UKartPauseWidget::OnLeaveClicked()
{
	if (UKartGameInstance* GI = GetGameInstance<UKartGameInstance>())
	{
		GI->LeaveToMainMenu();
	}
}

void UKartPauseWidget::OnEndMatchClicked()
{
	if (AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayer()))
	{
		PC->Server_RequestEndMatch();
	}
	OnResume.Broadcast();
}
