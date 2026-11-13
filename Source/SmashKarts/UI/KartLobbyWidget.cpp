#include "UI/KartLobbyWidget.h"
#include "Game/KartGameInstance.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerController.h"
#include "Game/KartPlayerState.h"
#include "KartRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"

void UKartLobbyWidget::BuildTree()
{
	UBorder* Panel = MakePanel(FLinearColor(0.f, 0.f, 0.f, 0.65f), FMargin(20.f, 16.f));
	WidgetTree->RootWidget = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);

	Column->AddChildToVerticalBox(MakeText(TEXT("LOBBY"), 30, FLinearColor(1.f, 0.82f, 0.2f)));
	Column->AddChildToVerticalBox(MakeText(TEXT("Drive around while everyone joins"), 16, FLinearColor(0.8f, 0.8f, 0.8f), false));

	PlayerList = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(PlayerList)->SetPadding(FMargin(0.f, 12.f));

	// Host controls
	UVerticalBox* Host = WidgetTree->ConstructWidget<UVerticalBox>();
	HostControls = Host;
	LengthText = MakeText(TEXT("Match length"), 18);
	Host->AddChildToVerticalBox(LengthText);
	UHorizontalBox* Lengths = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* L2 = MakeButton(TEXT("2 min"), 18, FLinearColor(0.25f, 0.3f, 0.45f), 80.f);
	UButton* L3 = MakeButton(TEXT("3 min"), 18, FLinearColor(0.25f, 0.3f, 0.45f), 80.f);
	UButton* L5 = MakeButton(TEXT("5 min"), 18, FLinearColor(0.25f, 0.3f, 0.45f), 80.f);
	L2->OnClicked.AddDynamic(this, &UKartLobbyWidget::OnLength2Clicked);
	L3->OnClicked.AddDynamic(this, &UKartLobbyWidget::OnLength3Clicked);
	L5->OnClicked.AddDynamic(this, &UKartLobbyWidget::OnLength5Clicked);
	Lengths->AddChildToHorizontalBox(L2);
	Lengths->AddChildToHorizontalBox(L3)->SetPadding(FMargin(6.f, 0.f));
	Lengths->AddChildToHorizontalBox(L5);
	Host->AddChildToVerticalBox(Lengths)->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));
	UButton* Start = MakeButton(TEXT("Start Match  (Enter)"), 24, FLinearColor(0.1f, 0.6f, 0.25f), 300.f);
	Start->OnClicked.AddDynamic(this, &UKartLobbyWidget::OnStartClicked);
	Host->AddChildToVerticalBox(Start);
	ShareText = MakeText(TEXT(""), 16, FLinearColor(0.7f, 0.9f, 1.f), false);
	ShareText->SetAutoWrapText(true);
	Host->AddChildToVerticalBox(Sized(ShareText, 320.f, 0.f))->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	Column->AddChildToVerticalBox(Host);

	WaitingText = MakeText(TEXT("Waiting for the host to start..."), 18, FLinearColor(0.8f, 0.8f, 0.8f), false);
	Column->AddChildToVerticalBox(WaitingText);
}

FString UKartLobbyWidget::GetLocalAddresses()
{
	FString Out;
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TArray<TSharedPtr<FInternetAddr>> Addresses;
	if (Sockets && Sockets->GetLocalAdapterAddresses(Addresses))
	{
		for (const TSharedPtr<FInternetAddr>& Addr : Addresses)
		{
			if (!Addr.IsValid() || Addr->GetProtocolType() != FNetworkProtocolTypes::IPv4)
			{
				continue;
			}
			const FString Ip = Addr->ToString(false);
			if (Ip.StartsWith(TEXT("127.")) || Ip.StartsWith(TEXT("169.254.")) || Ip == TEXT("0.0.0.0"))
			{
				continue;
			}
			FString Hint;
			if (Ip.StartsWith(TEXT("26."))) { Hint = TEXT("  (Radmin VPN)"); }
			else if (Ip.StartsWith(TEXT("192.168.")) || Ip.StartsWith(TEXT("10."))) { Hint = TEXT("  (same network / ZeroTier)"); }
			Out += Ip + Hint + TEXT("\n");
		}
	}
	return Out.TrimEnd();
}

void UKartLobbyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.5f)
	{
		RefreshAccumulator = 0.f;
		Rebuild();
	}
}

void UKartLobbyWidget::Rebuild()
{
	const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr;
	const AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayer());
	if (!GS || !PlayerList)
	{
		return;
	}

	PlayerList->ClearChildren();
	const TArray<AKartPlayerState*> Players = GS->GetSortedPlayers();
	for (const AKartPlayerState* PS : Players)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->AddChildToHorizontalBox(Sized(MakePanel(PS->GetPlayerColor(), FMargin(0.f)), 16.f, 16.f))->SetVerticalAlignment(VAlign_Center);
		FString Label = PS->GetPlayerName();
		if (PS->IsHost())
		{
			Label += TEXT("   HOST");
		}
		if (PC && PS == PC->PlayerState)
		{
			Label += TEXT("   (you)");
		}
		Row->AddChildToHorizontalBox(MakeText(Label, 20, FLinearColor::White, false))->SetPadding(FMargin(8.f, 0.f));
		PlayerList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
	}

	const bool bHost = PC && PC->IsLocalHost();
	HostControls->SetVisibility(bHost ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	WaitingText->SetVisibility(bHost ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	SetTextIfChanged(LengthText, FString::Printf(TEXT("Match length: %s"), *KartRules::FormatMatchTime(static_cast<float>(GS->MatchDurationSeconds))));

	if (bHost)
	{
		if (CachedAddresses.IsEmpty())
		{
			CachedAddresses = GetLocalAddresses();
		}
		const UKartGameInstance* GI = GetGameInstance<UKartGameInstance>();
		const int32 Port = GI ? GI->GamePort : 7777;
		SetTextIfChanged(ShareText, FString::Printf(TEXT("Friends join with one of these addresses (port %d):\n%s\nOver the internet without a VPN: your public IP + port forwarding (HOSTING.md)."),
			Port, CachedAddresses.IsEmpty() ? TEXT("(no network adapter found)") : *CachedAddresses));
	}
	else if (const AKartPlayerState* HostPS = GS->GetHostPlayerState())
	{
		SetTextIfChanged(WaitingText, FString::Printf(TEXT("Waiting for %s to start..."), *HostPS->GetPlayerName()));
	}
}

void UKartLobbyWidget::OnStartClicked()
{
	if (AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayer()))
	{
		PC->Server_RequestStartMatch();
	}
}

void UKartLobbyWidget::SetLength(int32 Seconds)
{
	if (AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayer()))
	{
		PC->Server_SetMatchDuration(Seconds);
	}
}

void UKartLobbyWidget::OnLength2Clicked() { SetLength(120); }
void UKartLobbyWidget::OnLength3Clicked() { SetLength(180); }
void UKartLobbyWidget::OnLength5Clicked() { SetLength(300); }
