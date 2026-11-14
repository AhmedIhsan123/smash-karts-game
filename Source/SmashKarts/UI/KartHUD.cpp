#include "UI/KartHUD.h"
#include "UI/KartHUDWidget.h"
#include "UI/KartLobbyWidget.h"
#include "UI/KartPauseWidget.h"
#include "UI/KartScoreboardWidget.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerController.h"
#include "Game/KartPlayerState.h"
#include "KartCharacter.h"
#include "KartDamageType.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

AKartHUD::AKartHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	HUDWidgetClass = UKartHUDWidget::StaticClass();
	ScoreboardWidgetClass = UKartScoreboardWidget::StaticClass();
	DamageNumberLifetime = 0.9f;
	DamageNumberRise = 60.f;
}

void AKartHUD::BeginPlay()
{
	Super::BeginPlay();
	EnsureWidgets();
}

void AKartHUD::EnsureWidgets()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (!HUDWidget && HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UKartHUDWidget>(PC, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport(0);
		}
	}
	if (!LobbyWidget)
	{
		LobbyWidget = CreateWidget<UKartLobbyWidget>(PC, UKartLobbyWidget::StaticClass());
		if (LobbyWidget)
		{
			LobbyWidget->AddToViewport(5);
			LobbyWidget->SetAlignmentInViewport(FVector2D(1.f, 0.f));
			LobbyWidget->SetAnchorsInViewport(FAnchors(1.f, 0.12f));
			LobbyWidget->SetPositionInViewport(FVector2D(-24.f, 0.f), false);
			LobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (!ScoreboardWidget && ScoreboardWidgetClass)
	{
		ScoreboardWidget = CreateWidget<UKartScoreboardWidget>(PC, ScoreboardWidgetClass);
		if (ScoreboardWidget)
		{
			ScoreboardWidget->AddToViewport(10);
			ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (!PauseWidget)
	{
		PauseWidget = CreateWidget<UKartPauseWidget>(PC, UKartPauseWidget::StaticClass());
		if (PauseWidget)
		{
			PauseWidget->AddToViewport(20);
			PauseWidget->SetVisibility(ESlateVisibility::Collapsed);
			PauseWidget->OnResume.AddUObject(this, &AKartHUD::OnPauseResume);
		}
	}
}

void AKartHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EnsureWidgets();

	const AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayerController());
	const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr;
	const bool bResults = GS && GS->IsPostMatch();

	if (ScoreboardWidget)
	{
		const bool bShow = bResults || (PC && PC->IsScoreboardHeld());
		ScoreboardWidget->SetResultsMode(bResults);
		// Results: clickable (buttons). Tab: look only.
		const ESlateVisibility Wanted = !bShow ? ESlateVisibility::Collapsed
			: (bResults ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::HitTestInvisible);
		if (ScoreboardWidget->GetVisibility() != Wanted)
		{
			ScoreboardWidget->SetVisibility(Wanted);
		}
	}
	if (LobbyWidget)
	{
		const ESlateVisibility Wanted = (GS && GS->IsLobby() && !bPauseOpen) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
		if (LobbyWidget->GetVisibility() != Wanted)
		{
			LobbyWidget->SetVisibility(Wanted);
		}
	}

	// Drop old damage numbers
	const double Now = FPlatformTime::Seconds();
	DamageNumbers.RemoveAll([this, Now](const FDamageNumber& Number) { return Now - Number.StartTime > DamageNumberLifetime; });
}

bool AKartHUD::WantsCursor() const
{
	const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr;
	return bPauseOpen || (GS && (GS->IsLobby() || GS->IsPostMatch()));
}

void AKartHUD::SetPauseMenuOpen(bool bOpen)
{
	EnsureWidgets();
	bPauseOpen = bOpen;
	if (PauseWidget)
	{
		const AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayerController());
		PauseWidget->SetIsHost(PC && PC->IsLocalHost());
		PauseWidget->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void AKartHUD::OnPauseResume()
{
	SetPauseMenuOpen(false);
}

void AKartHUD::HandleElimination(APlayerState* Killer, APlayerState* Victim, TSubclassOf<UDamageType> DamageType, bool bFell)
{
	EnsureWidgets();
	if (!HUDWidget)
	{
		return;
	}

	const FString KillerName = Killer ? Killer->GetPlayerName() : FString();
	const FString VictimName = Victim ? Victim->GetPlayerName() : FString(TEXT("?"));
	const FString Weapon = bFell ? FString(TEXT("knocked off")) : UKartDamageType::GetKillFeedName(DamageType).ToString();

	FString Line;
	if (Killer)
	{
		Line = FString::Printf(TEXT("%s  [%s]  %s"), *KillerName, *Weapon, *VictimName);
	}
	else
	{
		Line = bFell ? FString::Printf(TEXT("%s fell off"), *VictimName) : FString::Printf(TEXT("%s was smashed"), *VictimName);
	}
	const AKartPlayerState* KillerKPS = Cast<AKartPlayerState>(Killer);
	HUDWidget->AddKillFeedLine(Line, KillerKPS ? KillerKPS->GetPlayerColor() : FLinearColor::White);

	const APlayerState* LocalPS = GetOwningPlayerController() ? GetOwningPlayerController()->PlayerState.Get() : nullptr;
	if (LocalPS && Killer == LocalPS && Victim != LocalPS)
	{
		HUDWidget->ShowBanner(FString::Printf(TEXT("You smashed %s!"), *VictimName), FLinearColor(0.3f, 1.f, 0.4f), 2.5f);
	}
	else if (LocalPS && Victim == LocalPS)
	{
		const FString Text = Killer ? FString::Printf(TEXT("Smashed by %s"), *KillerName) : FString(bFell ? TEXT("You fell off!") : TEXT("Smashed!"));
		HUDWidget->ShowBanner(Text, FLinearColor(1.f, 0.35f, 0.3f), 2.5f);
	}
}

void AKartHUD::FlashDamage(float Amount)
{
	EnsureWidgets();
	if (HUDWidget)
	{
		HUDWidget->FlashDamage(Amount);
	}
}

void AKartHUD::AddDamageNumber(const FVector& WorldLocation, float Amount, bool bKilled)
{
	const double Now = FPlatformTime::Seconds();
	// Merge rapid hits on the same spot (machine gun) into one growing number
	for (FDamageNumber& Number : DamageNumbers)
	{
		if (!Number.bKilled && Now - Number.StartTime < 0.35 && FVector::DistSquared(Number.Location, WorldLocation) < FMath::Square(150.f))
		{
			Number.Amount += Amount;
			Number.Location = WorldLocation;
			Number.bKilled = bKilled;
			Number.StartTime = Now;
			return;
		}
	}
	FDamageNumber Number;
	Number.Location = WorldLocation;
	Number.Amount = Amount;
	Number.StartTime = Now;
	Number.bKilled = bKilled;
	DamageNumbers.Add(Number);
}

void AKartHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	// Floating damage numbers
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	const double Now = FPlatformTime::Seconds();
	for (const FDamageNumber& Number : DamageNumbers)
	{
		const float Age = static_cast<float>(Now - Number.StartTime);
		const float Alpha = FMath::Clamp(1.f - Age / DamageNumberLifetime, 0.f, 1.f);
		const FVector Screen = Canvas->Project(Number.Location + FVector(0.f, 0.f, 80.f));
		if (Screen.Z <= 0.f)
		{
			continue; // behind the camera
		}
		const FString Text = Number.bKilled ? FString::Printf(TEXT("%d  SMASH!"), FMath::RoundToInt(Number.Amount)) : FString::FromInt(FMath::Max(1, FMath::RoundToInt(Number.Amount)));
		const FLinearColor Color = Number.bKilled ? FLinearColor(1.f, 0.85f, 0.2f, Alpha) : FLinearColor(1.f, 1.f, 1.f, Alpha);
		DrawText(Text, Color, Screen.X - 12.f, Screen.Y - Age / DamageNumberLifetime * DamageNumberRise, Font, Number.bKilled ? 1.6f : 1.2f);
	}

	// Development: net overlay
	const AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayerController());
	if (!PC || !PC->IsNetOverlayVisible())
	{
		return;
	}
	const UWorld* World = GetWorld();
	const TCHAR* Mode = TEXT("Standalone");
	switch (World->GetNetMode())
	{
	case NM_ListenServer: Mode = TEXT("ListenServer"); break;
	case NM_Client: Mode = TEXT("Client"); break;
	case NM_DedicatedServer: Mode = TEXT("DedicatedServer"); break;
	default: break;
	}
	const APawn* Pawn = PC->GetPawn();
	const float Ping = PC->PlayerState ? PC->PlayerState->GetPingInMilliseconds() : 0.f;
	const AKartGameState* GS = World->GetGameState<AKartGameState>();
	const FString StateName = GS ? GS->GetMatchState().ToString() : FString(TEXT("-"));
	const FString RoleName = Pawn ? UEnum::GetValueAsString(Pawn->GetLocalRole()) : FString(TEXT("-"));
	const FString Line = FString::Printf(TEXT("Net: %s | Pawn role: %s | Ping: %.0f ms | %s"), Mode, *RoleName, Ping, *StateName);
	DrawText(Line, FLinearColor::Yellow, 20.f, Canvas->ClipY * 0.5f, GEngine ? GEngine->GetMediumFont() : nullptr, 1.f);
}
