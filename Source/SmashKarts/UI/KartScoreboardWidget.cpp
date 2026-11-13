#include "UI/KartScoreboardWidget.h"
#include "Game/KartGameInstance.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerController.h"
#include "Components/Button.h"
#include "Game/KartPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	// Column widths: rank, colour, name, score, elims, deaths, ping
	const float ColumnWidths[] = { 50.f, 26.f, 260.f, 90.f, 90.f, 90.f, 80.f };
	const FLinearColor BoardGold(1.f, 0.82f, 0.2f);
}

void UKartScoreboardWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = MakePanel(FLinearColor(0.f, 0.f, 0.f, 0.72f), FMargin(28.f, 20.f));
	AddToCanvas(Root, Panel, FAnchors(0.5f, 0.45f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);

	TitleText = MakeText(TEXT("SCOREBOARD"), 34, BoardGold);
	Column->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Center);

	Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	UVerticalBoxSlot* RowsSlot = Column->AddChildToVerticalBox(Rows);
	RowsSlot->SetPadding(FMargin(0.f, 14.f));

	FooterText = MakeText(TEXT(""), 18, FLinearColor(0.85f, 0.85f, 0.85f));
	Column->AddChildToVerticalBox(FooterText)->SetHorizontalAlignment(HAlign_Center);

	// Results buttons
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	ResultsButtons = Buttons;
	PlayAgainButton = MakeButton(TEXT("Play Again"), 22, FLinearColor(0.1f, 0.6f, 0.25f), 200.f);
	LobbyButton = MakeButton(TEXT("Back to Lobby"), 22, FLinearColor(0.25f, 0.3f, 0.45f), 200.f);
	UButton* LeaveButton = MakeButton(TEXT("Leave"), 22, FLinearColor(0.55f, 0.15f, 0.15f), 140.f);
	PlayAgainButton->OnClicked.AddDynamic(this, &UKartScoreboardWidget::OnPlayAgainClicked);
	LobbyButton->OnClicked.AddDynamic(this, &UKartScoreboardWidget::OnLobbyClicked);
	LeaveButton->OnClicked.AddDynamic(this, &UKartScoreboardWidget::OnLeaveClicked);
	Buttons->AddChildToHorizontalBox(PlayAgainButton);
	Buttons->AddChildToHorizontalBox(LobbyButton)->SetPadding(FMargin(8.f, 0.f));
	Buttons->AddChildToHorizontalBox(LeaveButton);
	Column->AddChildToVerticalBox(Buttons)->SetHorizontalAlignment(HAlign_Center);
	ResultsButtons->SetVisibility(ESlateVisibility::Collapsed);
}

void UKartScoreboardWidget::SetResultsMode(bool bInResults)
{
	if (bResults != bInResults)
	{
		bResults = bInResults;
		RefreshAccumulator = 1.f; // rebuild now
		if (ResultsButtons)
		{
			ResultsButtons->SetVisibility(bResults ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	const AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayer());
	const bool bHost = PC && PC->IsLocalHost();
	if (PlayAgainButton)
	{
		PlayAgainButton->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		LobbyButton->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UKartScoreboardWidget::OnPlayAgainClicked()
{
	if (AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayer()))
	{
		PC->Server_RequestPlayAgain();
	}
}

void UKartScoreboardWidget::OnLobbyClicked()
{
	if (AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayer()))
	{
		PC->Server_RequestBackToLobby();
	}
}

void UKartScoreboardWidget::OnLeaveClicked()
{
	if (UKartGameInstance* GI = GetGameInstance<UKartGameInstance>())
	{
		GI->LeaveToMainMenu();
	}
}

void UKartScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.25f)
	{
		RefreshAccumulator = 0.f;
		Rebuild();
	}
}

UWidget* UKartScoreboardWidget::MakeRow(const TArray<FString>& Cells, const FLinearColor& Color, const FLinearColor* Swatch, int32 FontSize)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 i = 0; i < Cells.Num() && i < UE_ARRAY_COUNT(ColumnWidths); ++i)
	{
		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>();
		Cell->SetWidthOverride(ColumnWidths[i]);
		if (i == 1 && Swatch)
		{
			UBorder* Chip = MakePanel(*Swatch, FMargin(0.f));
			USizeBox* ChipSize = WidgetTree->ConstructWidget<USizeBox>();
			ChipSize->SetWidthOverride(16.f);
			ChipSize->SetHeightOverride(16.f);
			ChipSize->SetContent(Chip);
			if (USizeBoxSlot* ChipSlot = Cast<USizeBoxSlot>(Cell->SetContent(ChipSize)))
			{
				ChipSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		else
		{
			Cell->SetContent(MakeText(Cells[i], FontSize, Color, false));
		}
		Row->AddChildToHorizontalBox(Cell)->SetVerticalAlignment(VAlign_Center);
	}
	return Row;
}

void UKartScoreboardWidget::Rebuild()
{
	const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr;
	AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayer());
	if (!GS || !Rows)
	{
		return;
	}

	const TArray<AKartPlayerState*> Sorted = GS->GetSortedPlayers();
	const APlayerState* LocalPS = PC ? PC->PlayerState.Get() : nullptr;

	if (bResults && Sorted.Num() > 0)
	{
		SetTextIfChanged(TitleText, FString::Printf(TEXT("RESULTS  -  %s WINS!"), *Sorted[0]->GetPlayerName().ToUpper()));
	}
	else
	{
		SetTextIfChanged(TitleText, GS->IsLobby() ? TEXT("PLAYERS") : TEXT("SCOREBOARD"));
	}

	Rows->ClearChildren();
	Rows->AddChildToVerticalBox(MakeRow({ TEXT("#"), TEXT(""), TEXT("Name"), TEXT("Score"), TEXT("Elims"), TEXT("Deaths"), TEXT("Ping") },
		FLinearColor(0.7f, 0.7f, 0.7f), nullptr, 18));

	for (int32 i = 0; i < Sorted.Num(); ++i)
	{
		const AKartPlayerState* PS = Sorted[i];
		const FLinearColor Swatch = PS->GetPlayerColor();
		FLinearColor TextColor = FLinearColor::White;
		if (bResults && i == 0)
		{
			TextColor = BoardGold;
		}
		else if (PS == LocalPS)
		{
			TextColor = FLinearColor(0.6f, 0.9f, 1.f);
		}
		FString Name = PS->GetPlayerName();
		if (PS->IsHost())
		{
			Name += TEXT("  (host)");
		}
		if (PS == LocalPS)
		{
			Name += TEXT("  < you");
		}
		const TArray<FString> Cells = {
			FString::FromInt(i + 1), TEXT(""), Name,
			FString::FromInt(FMath::RoundToInt(PS->GetScore())),
			FString::FromInt(PS->GetEliminations()),
			FString::FromInt(PS->GetDeaths()),
			PS->IsABot() ? FString(TEXT("bot")) : FString::FromInt(FMath::RoundToInt(PS->GetPingInMilliseconds()))
		};
		UWidget* Row = MakeRow(Cells, TextColor, &Swatch, (bResults && i == 0) ? 24 : 20);
		Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 3.f));
	}

	FString Footer;
	if (bResults)
	{
		const int32 Secs = FMath::CeilToInt(GS->GetResultsRemaining());
		Footer = (PC && PC->IsLocalHost())
			? FString::Printf(TEXT("ENTER: play again    BACKSPACE: lobby    (lobby in %d s)"), Secs)
			: FString::Printf(TEXT("Waiting for the host...  (lobby in %d s)"), Secs);
	}
	else if (GS->IsLobby())
	{
		Footer = (PC && PC->IsLocalHost()) ? TEXT("Press ENTER to start") : TEXT("Waiting for the host to start");
	}
	SetTextIfChanged(FooterText, Footer);
}
