#include "UI/KartSettingsWidget.h"
#include "Game/KartGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FIntPoint Resolutions[] = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440) };
	const TCHAR* QualityNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };

	FString WindowModeName(EWindowMode::Type Mode)
	{
		switch (Mode)
		{
		case EWindowMode::Fullscreen: return TEXT("Fullscreen");
		case EWindowMode::WindowedFullscreen: return TEXT("Borderless");
		default: return TEXT("Windowed");
		}
	}
}

void UKartSettingsWidget::BuildTree()
{
	UBorder* Panel = MakePanel(FLinearColor(0.02f, 0.03f, 0.08f, 0.95f), FMargin(32.f, 24.f));
	WidgetTree->RootWidget = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);

	auto AddRow = [this, Column](const FString& Label, UWidget* Control)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->AddChildToHorizontalBox(Sized(MakeText(Label, 22), 200.f, 0.f))->SetVerticalAlignment(VAlign_Center);
		Row->AddChildToHorizontalBox(Control)->SetVerticalAlignment(VAlign_Center);
		Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 6.f));
	};

	UTextBlock* Title = MakeText(TEXT("SETTINGS"), 32, FLinearColor(1.f, 0.82f, 0.2f));
	Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	// Volume: slider + percentage
	UHorizontalBox* VolumeRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	VolumeSlider = WidgetTree->ConstructWidget<USlider>();
	VolumeSlider->SetMinValue(0.f);
	VolumeSlider->SetMaxValue(1.f);
	VolumeSlider->OnValueChanged.AddDynamic(this, &UKartSettingsWidget::OnVolumeChanged);
	VolumeRow->AddChildToHorizontalBox(Sized(VolumeSlider, 260.f, 32.f))->SetVerticalAlignment(VAlign_Center);
	VolumeText = MakeText(TEXT("100%"), 20);
	VolumeRow->AddChildToHorizontalBox(VolumeText)->SetPadding(FMargin(12.f, 0.f));
	AddRow(TEXT("Master volume"), VolumeRow);

	auto MakeCycleButton = [this](UButton*& OutButton, UTextBlock*& OutText)
	{
		OutButton = MakeButton(TEXT("-"), 20, FLinearColor(0.2f, 0.25f, 0.4f), 260.f);
		OutText = Cast<UTextBlock>(Cast<UPanelWidget>(OutButton->GetContent())->GetChildAt(0));
		return OutButton;
	};
	UButton* Res = nullptr; UTextBlock* ResText = nullptr;
	UButton* Win = nullptr; UTextBlock* WinText = nullptr;
	UButton* Qual = nullptr; UTextBlock* QualText = nullptr;
	AddRow(TEXT("Resolution"), MakeCycleButton(Res, ResText));
	AddRow(TEXT("Window"), MakeCycleButton(Win, WinText));
	AddRow(TEXT("Quality"), MakeCycleButton(Qual, QualText));
	ResolutionButton = Res; ResolutionText = ResText;
	WindowModeButton = Win; WindowModeText = WinText;
	QualityButton = Qual; QualityText = QualText;
	ResolutionButton->OnClicked.AddDynamic(this, &UKartSettingsWidget::OnResolutionClicked);
	WindowModeButton->OnClicked.AddDynamic(this, &UKartSettingsWidget::OnWindowModeClicked);
	QualityButton->OnClicked.AddDynamic(this, &UKartSettingsWidget::OnQualityClicked);

	UTextBlock* Controls = MakeText(
		TEXT("CONTROLS\nW / S   drive / reverse\nA / D   steer\nSpace   fire (hold to charge throws, release to throw)\nTab   scoreboard\nEsc or P   menu"),
		18, FLinearColor(0.85f, 0.85f, 0.85f), false);
	Column->AddChildToVerticalBox(Controls)->SetPadding(FMargin(0.f, 16.f));

	BackButton = MakeButton(TEXT("Back"), 22);
	BackButton->OnClicked.AddDynamic(this, &UKartSettingsWidget::OnBackClicked);
	Column->AddChildToVerticalBox(BackButton)->SetHorizontalAlignment(HAlign_Center);

	Refresh();
}

void UKartSettingsWidget::Refresh()
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const UKartGameInstance* GI = GetGameInstance<UKartGameInstance>();
	if (VolumeSlider)
	{
		const float Volume = GI ? GI->GetMasterVolume() : 1.f;
		VolumeSlider->SetValue(Volume);
		SetTextIfChanged(VolumeText, FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Volume * 100.f)));
	}
	if (Settings && ResolutionText)
	{
		const FIntPoint Res = Settings->GetScreenResolution();
		SetTextIfChanged(ResolutionText, FString::Printf(TEXT("%d x %d"), Res.X, Res.Y));
		SetTextIfChanged(WindowModeText, WindowModeName(Settings->GetFullscreenMode()));
		const int32 Quality = Settings->GetOverallScalabilityLevel();
		SetTextIfChanged(QualityText, (Quality >= 0 && Quality < UE_ARRAY_COUNT(QualityNames)) ? QualityNames[Quality] : TEXT("Custom"));
	}
}

void UKartSettingsWidget::OnVolumeChanged(float Value)
{
	if (UKartGameInstance* GI = GetGameInstance<UKartGameInstance>())
	{
		GI->SetMasterVolume(Value);
	}
	SetTextIfChanged(VolumeText, FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f)));
}

void UKartSettingsWidget::OnResolutionClicked()
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	const FIntPoint Current = Settings->GetScreenResolution();
	int32 Next = 0;
	for (int32 i = 0; i < UE_ARRAY_COUNT(Resolutions); ++i)
	{
		if (Resolutions[i] == Current)
		{
			Next = (i + 1) % UE_ARRAY_COUNT(Resolutions);
			break;
		}
	}
	Settings->SetScreenResolution(Resolutions[Next]);
	ApplyDisplaySettings();
}

void UKartSettingsWidget::OnWindowModeClicked()
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	const EWindowMode::Type Mode = Settings->GetFullscreenMode();
	const EWindowMode::Type NextMode = Mode == EWindowMode::Windowed ? EWindowMode::WindowedFullscreen
		: (Mode == EWindowMode::WindowedFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);
	Settings->SetFullscreenMode(NextMode);
	ApplyDisplaySettings();
}

void UKartSettingsWidget::OnQualityClicked()
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	const int32 Current = Settings->GetOverallScalabilityLevel();
	Settings->SetOverallScalabilityLevel(Current < 0 ? 2 : (Current + 1) % 4);
	ApplyDisplaySettings();
}

void UKartSettingsWidget::ApplyDisplaySettings()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
	Refresh();
}

void UKartSettingsWidget::OnBackClicked()
{
	OnClosed.Broadcast();
}
