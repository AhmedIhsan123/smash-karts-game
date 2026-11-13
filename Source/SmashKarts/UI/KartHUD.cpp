#include "UI/KartHUD.h"
#include "UI/KartHUDWidget.h"
#include "UI/KartScoreboardWidget.h"
#include "Game/KartGameState.h"
#include "Game/KartPlayerController.h"
#include "Game/KartPlayerState.h"
#include "KartCharacter.h"
#include "KartDamageType.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"

AKartHUD::AKartHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	HUDWidgetClass = UKartHUDWidget::StaticClass();
	ScoreboardWidgetClass = UKartScoreboardWidget::StaticClass();
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
	if (!ScoreboardWidget && ScoreboardWidgetClass)
	{
		ScoreboardWidget = CreateWidget<UKartScoreboardWidget>(PC, ScoreboardWidgetClass);
		if (ScoreboardWidget)
		{
			ScoreboardWidget->AddToViewport(10);
			ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void AKartHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EnsureWidgets();

	if (!ScoreboardWidget)
	{
		return;
	}
	const AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayerController());
	const AKartGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKartGameState>() : nullptr;
	const bool bResults = GS && GS->IsPostMatch();
	const bool bShow = bResults || (PC && PC->IsScoreboardHeld());

	ScoreboardWidget->SetResultsMode(bResults);
	const ESlateVisibility Wanted = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (ScoreboardWidget->GetVisibility() != Wanted)
	{
		ScoreboardWidget->SetVisibility(Wanted);
	}
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

void AKartHUD::DrawHUD()
{
	Super::DrawHUD();

	const AKartPlayerController* PC = Cast<AKartPlayerController>(GetOwningPlayerController());
	if (!PC || !PC->IsNetOverlayVisible() || !Canvas)
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
