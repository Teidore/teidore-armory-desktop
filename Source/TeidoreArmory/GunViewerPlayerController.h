#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GunViewerPlayerController.generated.h"

/**
 * Custom PlayerController that disables all default movement/look input.
 * This is a product viewer — the camera is fixed, only the gun rotates.
 */
UCLASS()
class TEIDOREARMORY_API AGunViewerPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGunViewerPlayerController();

protected:
	virtual void BeginPlay() override;
};
