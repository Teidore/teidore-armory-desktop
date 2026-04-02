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

	// Store defaults so we can reset later
	DefaultRotation = FRotator::ZeroRotator;
	DefaultPanOffset = FVector::ZeroVector;

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
		}

		// Zoom: fires on each scroll tick
		if (ZoomAction)
		{
			EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AGunViewerPawn::OnZoom);
		}

		// Pan: fires every frame while right-click is held and mouse moves
		if (PanAction)
		{
			EIC->BindAction(PanAction, ETriggerEvent::Triggered, this, &AGunViewerPawn::OnPan);
		}

		// Reset: fires once on middle mouse click
		if (ResetAction)
		{
			EIC->BindAction(ResetAction, ETriggerEvent::Started, this, &AGunViewerPawn::OnReset);
		}
	}
}

// ─────────────────────────────────────────────────────────────────
// Tick — smoothly interpolate rotation, zoom, and pan each frame
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Smoothly interpolate rotation
	CurrentRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationInterpSpeed);
	GunPivot->SetRelativeRotation(CurrentRotation);

	// Smoothly interpolate zoom (camera distance along -X axis)
	CurrentZoomDistance = FMath::FInterpTo(CurrentZoomDistance, TargetZoomDistance, DeltaTime, ZoomInterpSpeed);
	ViewCamera->SetRelativeLocation(FVector(-CurrentZoomDistance, 0.0f, 0.0f));

	// Smoothly interpolate pan (moves the gun pivot)
	CurrentPanOffset = FMath::VInterpTo(CurrentPanOffset, TargetPanOffset, DeltaTime, PanInterpSpeed);
	GunPivot->SetRelativeLocation(CurrentPanOffset);
}

// ─────────────────────────────────────────────────────────────────
// OnRotate — left-click drag rotates the gun
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::OnRotate(const FInputActionValue& Value)
{
	// Value is a 2D axis (mouse delta X, mouse delta Y)
	const FVector2D Delta = Value.Get<FVector2D>();

	// Horizontal drag = yaw, Vertical drag = pitch
	TargetRotation.Yaw += Delta.X * RotationSpeed;
	TargetRotation.Pitch += Delta.Y * RotationSpeed;

	// Clamp pitch so the gun can't flip upside down
	TargetRotation.Pitch = FMath::Clamp(TargetRotation.Pitch, MinPitch, MaxPitch);
}

// ─────────────────────────────────────────────────────────────────
// OnZoom — scroll wheel adjusts camera distance
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::OnZoom(const FInputActionValue& Value)
{
	// Value is a 1D float: positive = scroll up (zoom in), negative = scroll down (zoom out)
	const float ScrollDelta = Value.Get<float>();

	// Subtract because scrolling up should decrease distance (move closer)
	TargetZoomDistance -= ScrollDelta * ZoomStep;

	// Clamp so the user can't zoom through the gun or lose it
	TargetZoomDistance = FMath::Clamp(TargetZoomDistance, MinZoomDistance, MaxZoomDistance);
}

// ─────────────────────────────────────────────────────────────────
// OnPan — right-click drag shifts the gun pivot
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::OnPan(const FInputActionValue& Value)
{
	const FVector2D Delta = Value.Get<FVector2D>();

	// Pan along the camera's local Y (horizontal) and Z (vertical) axes
	TargetPanOffset.Y += Delta.X * PanSpeed;
	TargetPanOffset.Z -= Delta.Y * PanSpeed; // Invert Y so dragging up moves gun up

	// Clamp so the gun can't be dragged off-screen
	TargetPanOffset.Y = FMath::Clamp(TargetPanOffset.Y, -MaxPanDistance, MaxPanDistance);
	TargetPanOffset.Z = FMath::Clamp(TargetPanOffset.Z, -MaxPanDistance, MaxPanDistance);
}

// ─────────────────────────────────────────────────────────────────
// OnReset — middle mouse click snaps everything back to defaults
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::OnReset(const FInputActionValue& Value)
{
	TargetRotation = DefaultRotation;
	TargetZoomDistance = DefaultZoomDistance;
	TargetPanOffset = DefaultPanOffset;
}
