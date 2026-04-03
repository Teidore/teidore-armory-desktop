#include "TeidoreArmoryGameMode.h"
#include "GunViewerPawn.h"
#include "GunViewerPlayerController.h"

ATeidoreArmoryGameMode::ATeidoreArmoryGameMode()
{
	// Don't auto-spawn a pawn — we place BP_GunViewerPawn in the level with AutoPossess
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AGunViewerPlayerController::StaticClass();
}
