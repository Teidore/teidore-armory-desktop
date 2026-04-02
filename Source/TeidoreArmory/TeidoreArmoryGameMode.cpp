#include "TeidoreArmoryGameMode.h"
#include "GunViewerPawn.h"

ATeidoreArmoryGameMode::ATeidoreArmoryGameMode()
{
	// Automatically use GunViewerPawn when a player joins
	DefaultPawnClass = AGunViewerPawn::StaticClass();
}
