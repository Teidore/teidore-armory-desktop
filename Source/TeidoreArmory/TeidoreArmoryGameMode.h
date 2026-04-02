#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TeidoreArmoryGameMode.generated.h"

/**
 * Custom GameMode that sets GunViewerPawn as the default pawn.
 * This ensures the viewer pawn is automatically spawned when you hit Play
 * or when the packaged .exe launches.
 */
UCLASS()
class TEIDOREARMORY_API ATeidoreArmoryGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATeidoreArmoryGameMode();
};
