#include "GunViewerPlayerController.h"
#include "EnhancedInputSubsystems.h"

AGunViewerPlayerController::AGunViewerPlayerController()
{
	// Show cursor by default — this is a product viewer
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AGunViewerPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Set input mode to Game and UI so mouse clicks work but cursor stays visible
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	// Remove any default input mapping contexts that the engine may have added
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();
	}
}
