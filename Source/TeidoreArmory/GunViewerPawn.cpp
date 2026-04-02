#include "GunViewerPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

// ─────────────────────────────────────────────────────────────────
// Constructor — sets up the component hierarchy
// ─────────────────────────────────────────────────────────────────

AGunViewerPawn::AGunViewerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// GunPivot sits at the origin — attach your gun meshes to this in the editor.
	// All rotation is applied to this component, so the gun spins in place.
	GunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunPivot"));
	GunPivot->SetupAttachment(SceneRoot);

	// Camera is offset along -X so it looks at the gun from the front.
	// We'll move it along its local X axis based on zoom distance.
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SceneRoot);

	// Initialize zoom
	TargetZoomDistance = DefaultZoomDistance;
	CurrentZoomDistance = DefaultZoomDistance;
}

// ─────────────────────────────────────────────────────────────────
// BeginPlay — register the input mapping context & store defaults
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::BeginPlay()
{
	Super::BeginPlay();

	// Reinitialize zoom distances from the (potentially editor-tweaked) default
	TargetZoomDistance = DefaultZoomDistance;
	CurrentZoomDistance = DefaultZoomDistance;

	// Place camera at the default zoom distance, looking at the origin
	ViewCamera->SetRelativeLocation(FVector(-DefaultZoomDistance, 0.0f, 0.0f));
	ViewCamera->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	// Store defaults so auto-return knows where to go
	DefaultRotation = FRotator::ZeroRotator;

	// Register the input mapping context with the Enhanced Input subsystem
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}

		// Show the mouse cursor — this is a product viewer, not an FPS
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}
}

// ─────────────────────────────────────────────────────────────────
// Input Binding — connect Enhanced Input actions to our callbacks
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Rotate: fires every frame while left-click is held and mouse moves
		if (RotateAction)
		{
			EIC->BindAction(RotateAction, ETriggerEvent::Triggered, this, &AGunViewerPawn::OnRotate);
			EIC->BindAction(RotateAction, ETriggerEvent::Completed, this, &AGunViewerPawn::OnRotateReleased);
		}

		// Zoom: fires on each scroll tick
		if (ZoomAction)
		{
			EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AGunViewerPawn::OnZoom);
		}

		// Toggle rotation lock
		if (ToggleLockAction)
		{
			EIC->BindAction(ToggleLockAction, ETriggerEvent::Started, this, &AGunViewerPawn::OnToggleLock);
		}
	}
}

// ─────────────────────────────────────────────────────────────────
// Tick — smoothly interpolate rotation, zoom, and pan each frame.
// When not dragging (and rotation lock is off), auto-return to defaults.
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Auto-return rotation after release (unless locked)
	if (!bRotationLocked && !bIsDraggingRotation && bWaitingToReturnRotation)
	{
		TimeSinceRotateRelease += DeltaTime;
		if (TimeSinceRotateRelease >= AutoReturnDelay)
		{
			TargetRotation = FMath::RInterpTo(TargetRotation, DefaultRotation, DeltaTime, AutoReturnInterpSpeed);

			// Stop auto-returning once we're close enough
			if (TargetRotation.Equals(DefaultRotation, 0.1f))
			{
				TargetRotation = DefaultRotation;
				bWaitingToReturnRotation = false;
			}
		}
	}

	// Smoothly interpolate rotation toward target
	CurrentRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationInterpSpeed);
	GunPivot->SetRelativeRotation(CurrentRotation);

	// Smoothly interpolate zoom (camera distance along -X axis)
	CurrentZoomDistance = FMath::FInterpTo(CurrentZoomDistance, TargetZoomDistance, DeltaTime, ZoomInterpSpeed);
	ViewCamera->SetRelativeLocation(FVector(-CurrentZoomDistance, 0.0f, 0.0f));

}

// ─────────────────────────────────────────────────────────────────
// OnRotate — left-click drag rotates the gun
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::OnRotate(const FInputActionValue& Value)
{
	bIsDraggingRotation = true;
	bWaitingToReturnRotation = false;

	const FVector2D Delta = Value.Get<FVector2D>();

	TargetRotation.Yaw += Delta.X * RotationSpeed;
	TargetRotation.Pitch += Delta.Y * RotationSpeed;
	TargetRotation.Pitch = FMath::Clamp(TargetRotation.Pitch, MinPitch, MaxPitch);
}

// Called when the user releases left-click — starts the auto-return timer
void AGunViewerPawn::OnRotateReleased(const FInputActionValue& Value)
{
	bIsDraggingRotation = false;

	if (!bRotationLocked)
	{
		bWaitingToReturnRotation = true;
		TimeSinceRotateRelease = 0.0f;
	}
}

// ─────────────────────────────────────────────────────────────────
// OnZoom — scroll wheel adjusts camera distance
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::OnZoom(const FInputActionValue& Value)
{
	const float ScrollDelta = Value.Get<float>();
	TargetZoomDistance -= ScrollDelta * ZoomStep;
	TargetZoomDistance = FMath::Clamp(TargetZoomDistance, MinZoomDistance, MaxZoomDistance);
}

// ─────────────────────────────────────────────────────────────────
// Rotation Lock — toggle via input or call from UI/Blueprint
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::OnToggleLock(const FInputActionValue& Value)
{
	SetRotationLocked(!bRotationLocked);
}

void AGunViewerPawn::SetRotationLocked(bool bLocked)
{
	bRotationLocked = bLocked;

	if (!bLocked)
	{
		// Unlocking — start auto-returning to defaults
		bWaitingToReturnRotation = true;
		TimeSinceRotateRelease = 0.0f;
	}
	else
	{
		// Locking — cancel any pending auto-return
		bWaitingToReturnRotation = false;
	}
}
