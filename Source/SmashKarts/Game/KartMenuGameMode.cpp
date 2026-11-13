#include "Game/KartMenuGameMode.h"
#include "UI/KartMainMenuWidget.h"
#include "SmashKarts.h"
#include "GameFramework/HUD.h"

AKartMenuGameMode::AKartMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AKartMenuPlayerController::StaticClass();
	HUDClass = AHUD::StaticClass();
}

void AKartMenuGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// The menu has no pawn on purpose
}

void AKartMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	MenuWidget = CreateWidget<UKartMainMenuWidget>(this, UKartMainMenuWidget::StaticClass());
	if (MenuWidget)
	{
		MenuWidget->SetIsFocusable(true); // UIOnly input mode focuses it (typing in the name / IP fields)
		MenuWidget->AddToViewport(0);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		bShowMouseCursor = true;
	}
	UE_LOG(LogSmashKarts, Log, TEXT("[Menu] Main menu shown"));
}
