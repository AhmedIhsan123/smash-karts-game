#include "UI/KartMainMenuWidget.h"
#include "UI/KartSettingsWidget.h"
#include "Game/KartGameInstance.h"
#include "SmashKarts.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"

void UKartMainMenuWidget::BuildTree()
{
	const UKartGameInstance* GI = GetGameInstance<UKartGameInstance>();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Full-screen background
	UBorder* Background = MakePanel(FLinearColor(0.03f, 0.05f, 0.12f, 1.f), FMargin(0.f));
	UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background);
	BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BackgroundSlot->SetOffsets(FMargin(0.f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	MainColumn = Column;
	AddToCanvas(Root, Column, FAnchors(0.5f, 0.45f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	TitleText = MakeText(GI ? GI->GameTitle.ToUpper() : FString(TEXT("KART ARENA")), 64, FLinearColor(1.f, 0.82f, 0.2f));
	Column->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* Subtitle = MakeText(TEXT("free-for-all kart battles with friends"), 20, FLinearColor(0.8f, 0.85f, 1.f), false);
	Column->AddChildToVerticalBox(Subtitle)->SetHorizontalAlignment(HAlign_Center);

	// Name
	UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	NameRow->AddChildToHorizontalBox(Sized(MakeText(TEXT("Your name"), 22), 160.f, 0.f))->SetVerticalAlignment(VAlign_Center);
	NameField = MakeTextField(GI ? GI->GetPlayerName() : FString(), TEXT("type a name (max 16)"));
	NameRow->AddChildToHorizontalBox(Sized(NameField, 320.f, 40.f))->SetVerticalAlignment(VAlign_Center);
	Column->AddChildToVerticalBox(NameRow)->SetPadding(FMargin(0.f, 28.f, 0.f, 12.f));

	// Host
	UButton* HostButton = MakeButton(TEXT("Host Game"), 26, FLinearColor(0.1f, 0.6f, 0.25f), 480.f);
	HostButton->OnClicked.AddDynamic(this, &UKartMainMenuWidget::OnHostClicked);
	Column->AddChildToVerticalBox(HostButton)->SetPadding(FMargin(0.f, 8.f));

	// Join
	UHorizontalBox* JoinRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddressField = MakeTextField(GI ? GI->GetLastJoinAddress() : FString(TEXT("127.0.0.1")), TEXT("host IP, e.g. 192.168.1.20"));
	JoinRow->AddChildToHorizontalBox(Sized(AddressField, 320.f, 44.f))->SetVerticalAlignment(VAlign_Center);
	UButton* JoinButton = MakeButton(TEXT("Join Game"), 24, FLinearColor(0.15f, 0.45f, 0.95f), 150.f);
	JoinButton->OnClicked.AddDynamic(this, &UKartMainMenuWidget::OnJoinClicked);
	JoinRow->AddChildToHorizontalBox(JoinButton)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	Column->AddChildToVerticalBox(JoinRow)->SetPadding(FMargin(0.f, 8.f));

	// Settings / Quit
	UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* SettingsButton = MakeButton(TEXT("Settings"), 22, FLinearColor(0.25f, 0.3f, 0.45f), 235.f);
	SettingsButton->OnClicked.AddDynamic(this, &UKartMainMenuWidget::OnSettingsClicked);
	BottomRow->AddChildToHorizontalBox(SettingsButton);
	UButton* QuitButton = MakeButton(TEXT("Quit"), 22, FLinearColor(0.55f, 0.15f, 0.15f), 235.f);
	QuitButton->OnClicked.AddDynamic(this, &UKartMainMenuWidget::OnQuitClicked);
	BottomRow->AddChildToHorizontalBox(QuitButton)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	Column->AddChildToVerticalBox(BottomRow)->SetPadding(FMargin(0.f, 8.f));

	MessageText = MakeText(TEXT(""), 20, FLinearColor(1.f, 0.75f, 0.3f));
	MessageText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(Sized(MessageText, 480.f, 0.f))->SetPadding(FMargin(0.f, 16.f));

	UTextBlock* Footer = MakeText(FString::Printf(TEXT("Direct IP on UDP port %d. Playing over the internet: see HOSTING.md."), GI ? GI->GamePort : 7777),
		16, FLinearColor(0.6f, 0.65f, 0.75f), false);
	AddToCanvas(Root, Footer, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -16.f));

	// Settings overlay
	SettingsPanel = CreateWidget<UKartSettingsWidget>(GetOwningPlayer(), UKartSettingsWidget::StaticClass());
	if (SettingsPanel)
	{
		AddToCanvas(Root, SettingsPanel, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
		SettingsPanel->OnClosed.AddUObject(this, &UKartMainMenuWidget::OnSettingsClosed);
	}
}

void UKartMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UKartGameInstance* GI = GetGameInstance<UKartGameInstance>())
	{
		const FString Message = GI->ConsumeMenuMessage();
		if (!Message.IsEmpty())
		{
			ShowMessage(Message, true);
		}
	}
}

void UKartMainMenuWidget::ShowMessage(const FString& Message, bool bError)
{
	SetTextIfChanged(MessageText, Message);
	SetTextColor(MessageText, bError ? FLinearColor(1.f, 0.55f, 0.35f) : FLinearColor(0.6f, 1.f, 0.6f));
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Showing: %s"), *Message);
}

void UKartMainMenuWidget::SaveName()
{
	if (UKartGameInstance* GI = GetGameInstance<UKartGameInstance>())
	{
		GI->SetPlayerName(NameField->GetText().ToString());
	}
}

void UKartMainMenuWidget::OnHostClicked()
{
	SaveName();
	ShowMessage(TEXT("Starting your game..."), false);
	if (UKartGameInstance* GI = GetGameInstance<UKartGameInstance>())
	{
		GI->HostGame();
	}
}

void UKartMainMenuWidget::OnJoinClicked()
{
	SaveName();
	UKartGameInstance* GI = GetGameInstance<UKartGameInstance>();
	const FString Address = AddressField->GetText().ToString();
	if (GI && GI->JoinGame(Address))
	{
		ShowMessage(FString::Printf(TEXT("Connecting to %s..."), *Address), false);
	}
	else if (GI)
	{
		ShowMessage(GI->ConsumeMenuMessage(), true);
	}
}

void UKartMainMenuWidget::OnSettingsClicked()
{
	if (SettingsPanel)
	{
		SettingsPanel->Refresh();
		SettingsPanel->SetVisibility(ESlateVisibility::Visible);
		MainColumn->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UKartMainMenuWidget::OnSettingsClosed()
{
	SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
	MainColumn->SetVisibility(ESlateVisibility::Visible);
}

void UKartMainMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
