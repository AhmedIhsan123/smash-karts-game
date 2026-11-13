// SK.* console commands (development builds only). Each one finds the local KartPlayerController in the
// world it was typed in and routes through AKartPlayerController::RunDebugCommand (server RPC when needed).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Game/KartPlayerController.h"
#include "SmashKarts.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	AKartPlayerController* FindLocalKartPC(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController())
			{
				return Cast<AKartPlayerController>(PC);
			}
		}
		return nullptr;
	}

	void Route(const TCHAR* Name, const TArray<FString>& Args, UWorld* World)
	{
		AKartPlayerController* PC = FindLocalKartPC(World);
		if (!PC)
		{
			UE_LOG(LogSmashKarts, Warning, TEXT("[Debug] SK.%s: no local KartPlayerController in this world"), Name);
			return;
		}
		FString Command = Name;
		for (const FString& Arg : Args)
		{
			Command += TEXT(" ") + Arg;
		}
		PC->RunDebugCommand(Command);
	}

#define SK_DEBUG_COMMAND(Name, Help) \
	static FAutoConsoleCommandWithWorldAndArgs GSKCmd_##Name(TEXT("SK." #Name), TEXT(Help), \
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { Route(TEXT(#Name), Args, World); }));

	SK_DEBUG_COMMAND(GiveWeapon, "SK.GiveWeapon <Name>  e.g. Rockets, Mines, MachineGun (asset or display name)")
	SK_DEBUG_COMMAND(Damage, "SK.Damage <amount>  damage your own kart (no credit)")
	SK_DEBUG_COMMAND(Kill, "SK.Kill  eliminate your own kart")
	SK_DEBUG_COMMAND(SetStatus, "SK.SetStatus <Frozen|TiresPopped|Spiky|Invincible|Tesla|SpawnProtected> <seconds; 0 clears, -1 until respawn>")
	SK_DEBUG_COMMAND(EndMatch, "SK.EndMatch  end the running match now")
	SK_DEBUG_COMMAND(MatchTime, "SK.MatchTime <seconds>  set the time left in the running match")
	SK_DEBUG_COMMAND(StartMatch, "SK.StartMatch  start the countdown (same as the host pressing Start)")
	SK_DEBUG_COMMAND(Lobby, "SK.Lobby  back to the lobby (host)")
	SK_DEBUG_COMMAND(State, "SK.State  log match state and scores (server log)")
	SK_DEBUG_COMMAND(ShowNet, "SK.ShowNet  toggle the net role / ping overlay (local)")
	SK_DEBUG_COMMAND(AutoTest, "SK.AutoTest <players> [quit]  scripted full-match test on the host; results in the log")

#undef SK_DEBUG_COMMAND
}

#endif // !UE_BUILD_SHIPPING
