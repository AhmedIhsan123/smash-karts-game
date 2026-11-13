#include "UI/KartHUDWidget.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerController.h"
#include "Game/KartPlayerState.h"
#include "KartCharacter.h"
#include "KartHealthComponent.h"
#include "KartRules.h"
#include "KartStatusComponent.h"
#include "KartWeapon.h"
#include "KartWeaponComponent.h"
#include "KartWeaponData.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameMode.h"

namespace
{
	const FLinearColor HudGold(1.f, 0.82f, 0.2f);
	const FLinearColor SoftWhite(0.92f, 0.92f, 0.92f);
	constexpr int32 KillFeedMax = 5;
	constexpr double KillFeedLifetime = 7.0;
}

void UKartHUDWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Top centre: timer + phase
	TimerText = MakeText(TEXT("3:00"), 40);
	AddToCanvas(Root, TimerText, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 16.f));
	PhaseText = MakeText(TEXT(""), 18, SoftWhite);
	AddToCanvas(Root, PhaseText, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 66.f));

	// Top left: score + rank
	ScoreText = MakeText(TEXT(""), 24);
	AddToCanvas(Root, ScoreText, FAnchors(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(24.f, 20.f));

	// Top right: kill feed
	KillFeedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("KillFeed"));
	for (int32 i = 0; i < KillFeedMax; ++i)
	{
		UTextBlock* Line = MakeText(TEXT(""), 18);
		UVerticalBoxSlot* LineSlot = KillFeedBox->AddChildToVerticalBox(Line);
		LineSlot->SetHorizontalAlignment(HAlign_Right);
		KillFeedLines.Add(Line);
	}
	AddToCanvas(Root, KillFeedBox, FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-24.f, 20.f));

	// Centre: big message + sub line
	CenterText = MakeText(TEXT(""), 64, HudGold);
	AddToCanvas(Root, CenterText, FAnchors(0.5f, 0.35f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	SubText = MakeText(TEXT(""), 26);
	AddToCanvas(Root, SubText, FAnchors(0.5f, 0.35f), FVector2D(0.5f, 0.f), FVector2D(0.f, 48.f));

	// Bottom left: health
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetPercent(1.f);
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.85f, 0.25f));
	AddToCanvas(Root, HealthBar, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(24.f, -24.f), false, FVector2D(320.f, 28.f));
	HealthText = MakeText(TEXT("100"), 22);
	AddToCanvas(Root, HealthText, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(24.f, -56.f));

	// Bottom left, above health: active statuses
	StatusText = MakeText(TEXT(""), 20, FLinearColor(0.6f, 0.9f, 1.f));
	AddToCanvas(Root, StatusText, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(24.f, -88.f));

	// Bottom right: weapon icon (generated square with initials), name, uses, charge meter
	WeaponIcon = MakePanel(FLinearColor(0.2f, 0.2f, 0.2f, 0.6f), FMargin(0.f));
	WeaponIconText = MakeText(TEXT(""), 26);
	WeaponIcon->SetContent(WeaponIconText);
	WeaponIcon->SetHorizontalAlignment(HAlign_Center);
	WeaponIcon->SetVerticalAlignment(VAlign_Center);
	AddToCanvas(Root, WeaponIcon, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-24.f, -24.f), false, FVector2D(64.f, 64.f));

	WeaponText = MakeText(TEXT(""), 28);
	AddToCanvas(Root, WeaponText, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-100.f, -56.f));
	UsesText = MakeText(TEXT(""), 20, SoftWhite);
	AddToCanvas(Root, UsesText, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-100.f, -24.f));

	ChargeBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ChargeBar"));
	ChargeBar->SetFillColorAndOpacity(FLinearColor(1.f, 0.75f, 0.1f));
	ChargeBar->SetPercent(0.f);
	ChargeBar->SetVisibility(ESlateVisibility::Collapsed);
	AddToCanvas(Root, ChargeBar, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-100.f, -96.f), false, FVector2D(240.f, 16.f));

	// Bottom centre: controls / lobby hint
	HintText = MakeText(TEXT(""), 16, SoftWhite);
	AddToCanvas(Root, HintText, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -18.f));
}

void UKartHUDWidget::AddKillFeedLine(const FString& Text, const FLinearColor& Color)
{
	FFeedEntry Entry;
	Entry.Text = Text;
	Entry.Color = Color;
	Entry.Time = FPlatformTime::Seconds();
	FeedEntries.Insert(Entry, 0);
	if (FeedEntries.Num() > KillFeedMax)
	{
		FeedEntries.SetNum(KillFeedMax);
	}
}

void UKartHUDWidget::ShowBanner(const FString& Text, const FLinearColor& Color, float Seconds)
{
	BannerText = Text;
	BannerColor = Color;
	BannerUntil = FPlatformTime::Seconds() + Seconds;
}

void UKartHUDWidget::RefreshKillFeed(double LocalNow)
{
	for (int32 i = 0; i < KillFeedLines.Num(); ++i)
	{
		UTextBlock* Line = KillFeedLines[i];
		if (FeedEntries.IsValidIndex(i) && LocalNow - FeedEntries[i].Time < KillFeedLifetime)
		{
			SetTextIfChanged(Line, FeedEntries[i].Text);
			SetTextColor(Line, FeedEntries[i].Color);
		}
		else
		{
			SetTextIfChanged(Line, FString());
		}
	}
}

void UKartHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const double LocalNow = FPlatformTime::Seconds();
	RefreshKillFeed(LocalNow);

	AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayer());
	const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr;
	if (!PC || !GS)
	{
		return;
	}
	const AKartPlayerState* PS = PC->GetPlayerState<AKartPlayerState>();
	const AKartCharacter* Kart = Cast<AKartCharacter>(PC->GetPawn());
	const double ServerNow = GS->GetServerWorldTimeSeconds();
	const bool bHost = PC->IsLocalHost();

	// Detect GO!
	const FName State = GS->GetMatchState();
	if (State != LastMatchState)
	{
		if (State == MatchState::InProgress && LastMatchState == KartMatchState::Countdown)
		{
			GoUntil = LocalNow + 1.0;
		}
		LastMatchState = State;
	}

	// ---- Timer + phase ----
	FString Timer;
	FString Phase;
	FString Center;
	FLinearColor CenterColor = HudGold;
	FString Hint = TEXT("W/S drive   A/D steer   Space fire   Tab scores");
	const int32 NumPlayers = GS->PlayerArray.Num();

	if (GS->IsLobby())
	{
		Timer = TEXT("LOBBY");
		Phase = FString::Printf(TEXT("%d player%s  |  match %s"), NumPlayers, NumPlayers == 1 ? TEXT("") : TEXT("s"),
			*KartRules::FormatMatchTime(static_cast<float>(GS->MatchDurationSeconds)));
		Hint = bHost ? TEXT("You are the host: press ENTER to start the match   |   Tab: players")
		             : TEXT("Waiting for the host to start   |   Tab: players");
	}
	else if (GS->IsCountdown())
	{
		Timer = KartRules::FormatMatchTime(static_cast<float>(GS->MatchDurationSeconds));
		Phase = TEXT("GET READY");
		Center = FString::FromInt(FMath::Max(1, FMath::CeilToInt(GS->GetCountdownRemaining())));
	}
	else if (GS->IsPlaying())
	{
		const float Remaining = GS->GetMatchTimeRemaining();
		Timer = KartRules::FormatMatchTime(Remaining);
		SetTextColor(TimerText, Remaining <= 10.f ? FLinearColor(1.f, 0.3f, 0.2f) : FLinearColor::White);
		if (LocalNow < GoUntil)
		{
			Center = TEXT("GO!");
		}
	}
	else if (GS->IsPostMatch())
	{
		Timer = TEXT("0:00");
		Phase = TEXT("MATCH OVER");
		Hint = bHost ? TEXT("ENTER: play again   BACKSPACE: back to lobby") : TEXT("Waiting for the host");
	}
	SetTextIfChanged(TimerText, Timer);
	SetTextIfChanged(PhaseText, Phase);
	SetTextIfChanged(HintText, Hint);

	// ---- Score + rank ----
	if (PS)
	{
		const int32 Rank = GS->GetRankOf(PS);
		SetTextIfChanged(ScoreText, FString::Printf(TEXT("Score %d   #%d of %d"), FMath::RoundToInt(PS->GetScore()), Rank, NumPlayers));
		SetTextColor(ScoreText, PS->GetPlayerColor());
	}

	// ---- Health ----
	const UKartHealthComponent* Health = Kart ? Kart->GetHealthComponent() : nullptr;
	const bool bAlive = Kart && !Kart->IsEliminated();
	if (Health && bAlive)
	{
		const float Max = FMath::Max(1.f, Health->GetMaxHealth());
		const float Pct = FMath::Clamp(Health->GetHealth() / Max, 0.f, 1.f);
		HealthBar->SetPercent(Pct);
		HealthBar->SetFillColorAndOpacity(Pct > 0.5f ? FLinearColor(0.15f, 0.85f, 0.25f)
			: (Pct > 0.25f ? FLinearColor(1.f, 0.75f, 0.1f) : FLinearColor(1.f, 0.2f, 0.15f)));
		SetTextIfChanged(HealthText, FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Health->GetHealth()), FMath::RoundToInt(Max)));
	}
	else
	{
		HealthBar->SetPercent(0.f);
		SetTextIfChanged(HealthText, TEXT("SMASHED"));
	}

	// ---- Weapon ----
	const UKartWeaponComponent* WeaponComp = Kart ? Kart->GetWeaponComponent() : nullptr;
	const AKartWeapon* Weapon = WeaponComp ? WeaponComp->GetCurrentWeapon() : nullptr;
	const UKartWeaponData* Data = Weapon ? Weapon->GetWeaponData() : nullptr;
	if (Data)
	{
		const FString Name = Data->DisplayName.IsEmpty() ? Data->GetName() : Data->DisplayName.ToString();
		SetTextIfChanged(WeaponText, Name);
		SetTextIfChanged(UsesText, Data->bChargeThrow
			? FString::Printf(TEXT("Uses left: %d   (hold Space, release to throw)"), Weapon->GetUsesLeft())
			: FString::Printf(TEXT("Uses left: %d   (Space)"), Weapon->GetUsesLeft()));

		// Generated icon: weapon colour + initials ("Machine Gun" -> "MG", "Rockets" -> "RO")
		TArray<FString> Words;
		Name.ParseIntoArrayWS(Words);
		FString Initials;
		if (Words.Num() >= 2)
		{
			Initials = Words[0].Left(1) + Words[1].Left(1);
		}
		else
		{
			Initials = Name.Left(2);
		}
		SetTextIfChanged(WeaponIconText, Initials.ToUpper());
		WeaponIcon->SetBrushColor(Data->HudColor);
		WeaponIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		SetTextIfChanged(WeaponText, TEXT("No weapon"));
		SetTextIfChanged(UsesText, GS->IsPlaying() ? TEXT("Drive through a ? box") : TEXT(""));
		WeaponIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ---- Charge meter (charge-throw weapons) ----
	if (WeaponComp && WeaponComp->IsCharging())
	{
		ChargeBar->SetPercent(WeaponComp->GetChargeAlpha());
		ChargeBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else if (ChargeBar->GetVisibility() != ESlateVisibility::Collapsed)
	{
		ChargeBar->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ---- Statuses ----
	FString Statuses;
	if (bAlive && Kart->GetStatusComponent())
	{
		static const TCHAR* Names[] = { TEXT("FROZEN"), TEXT("TIRES POPPED"), TEXT("SPIKY"), TEXT("INVINCIBLE"), TEXT("TESLA") };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Names); ++i)
		{
			const EKartStatus Status = static_cast<EKartStatus>(i);
			if (Kart->HasStatus(Status))
			{
				const float Left = Kart->GetStatusComponent()->GetTimeRemaining(Status);
				Statuses += Left > 0.f ? FString::Printf(TEXT("%s %.1fs   "), Names[i], Left) : FString::Printf(TEXT("%s   "), Names[i]);
			}
		}
	}
	SetTextIfChanged(StatusText, Statuses.TrimEnd());

	// ---- Centre messages: countdown / GO > banner ----
	FString Sub;
	if (Center.IsEmpty() && LocalNow < BannerUntil)
	{
		Center = BannerText;
		CenterColor = BannerColor;
	}
	if (PS && PS->GetRespawnServerTime() > ServerNow && !GS->IsPostMatch())
	{
		Sub = FString::Printf(TEXT("Respawning in %d"), FMath::Max(1, FMath::CeilToInt(PS->GetRespawnServerTime() - ServerNow)));
	}
	else if (bAlive && Kart->GetStatusComponent() && Kart->GetStatusComponent()->HasStatus(EKartStatus::SpawnProtected))
	{
		Sub = TEXT("Spawn protection (ends when you fire)");
	}
	SetTextIfChanged(CenterText, Center);
	SetTextColor(CenterText, CenterColor);
	SetTextIfChanged(SubText, Sub);
}
