#include "GunViewerPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

// ─────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────

AGunViewerPawn::AGunViewerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// GunPivot — attach gun meshes here. Rotation is applied to this component.
	GunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunPivot"));
	GunPivot->SetupAttachment(SceneRoot);

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SceneRoot);

	TargetCameraDistance = MinFramingDistance;
	CurrentCameraDistance = MinFramingDistance;
}

// ─────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::BeginPlay()
{
	Super::BeginPlay();

	DefaultRotation = FRotator::ZeroRotator;

	// Auto-frame the gun on startup
	RecalculateFraming();
	CurrentCameraDistance = TargetCameraDistance;
	ViewCamera->SetRelativeLocation(FVector(-CurrentCameraDistance, 0.0f, 0.0f));
	ViewCamera->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

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

		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}
}

// ─────────────────────────────────────────────────────────────────
// Input Binding
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (RotateAction)
		{
			EIC->BindAction(RotateAction, ETriggerEvent::Triggered, this, &AGunViewerPawn::OnRotate);
			EIC->BindAction(RotateAction, ETriggerEvent::Completed, this, &AGunViewerPawn::OnRotateReleased);
		}

		if (ToggleLockAction)
		{
			EIC->BindAction(ToggleLockAction, ETriggerEvent::Started, this, &AGunViewerPawn::OnToggleLock);
		}
	}
}

// ─────────────────────────────────────────────────────────────────
// Tick — interpolate rotation and camera distance each frame
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

			if (TargetRotation.Equals(DefaultRotation, 0.1f))
			{
				TargetRotation = DefaultRotation;
				bWaitingToReturnRotation = false;
			}
		}
	}

	// Smoothly interpolate rotation
	CurrentRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationInterpSpeed);
	GunPivot->SetRelativeRotation(CurrentRotation);

	// Smoothly interpolate camera distance (auto-framing)
	CurrentCameraDistance = FMath::FInterpTo(CurrentCameraDistance, TargetCameraDistance, DeltaTime, FramingInterpSpeed);
	ViewCamera->SetRelativeLocation(FVector(-CurrentCameraDistance, 0.0f, 0.0f));
}

// ─────────────────────────────────────────────────────────────────
// OnRotate — left-click drag
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
// Auto-Framing — calculates ideal camera distance from gun bounds
// ─────────────────────────────────────────────────────────────────

float AGunViewerPawn::CalculateIdealDistance() const
{
	if (!GunPivot || !ViewCamera)
	{
		return MinFramingDistance;
	}

	// Get the combined bounding box of all components under GunPivot
	FBox BoundingBox(ForceInit);
	bool bHasBounds = false;

	TArray<USceneComponent*> Children;
	GunPivot->GetChildrenComponents(true, Children);

	for (USceneComponent* Child : Children)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Child);
		if (Primitive && Primitive->IsVisible())
		{
			FBoxSphereBounds LocalBounds = Primitive->CalcLocalBounds();
			FBox LocalBox = LocalBounds.GetBox();

			if (LocalBox.IsValid)
			{
				// Transform to GunPivot-relative space
				FTransform RelativeTransform = Primitive->GetComponentTransform().GetRelativeTransform(GunPivot->GetComponentTransform());
				FBox TransformedBox = LocalBox.TransformBy(RelativeTransform);

				if (bHasBounds)
				{
					BoundingBox += TransformedBox;
				}
				else
				{
					BoundingBox = TransformedBox;
					bHasBounds = true;
				}
			}
		}
	}

	if (!bHasBounds)
	{
		return MinFramingDistance;
	}

	// Use the bounding sphere radius to determine the camera distance
	// that fits everything in the camera's field of view
	float BoundsRadius = BoundingBox.GetExtent().Size();
	float HalfFOVRad = FMath::DegreesToRadians(ViewCamera->FieldOfView * 0.5f);
	float IdealDistance = (BoundsRadius * FramingPadding) / FMath::Tan(HalfFOVRad);

	return FMath::Max(IdealDistance, MinFramingDistance);
}

void AGunViewerPawn::RecalculateFraming()
{
	TargetCameraDistance = CalculateIdealDistance();
}

// ─────────────────────────────────────────────────────────────────
// Rotation Lock
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
		bWaitingToReturnRotation = true;
		TimeSinceRotateRelease = 0.0f;
	}
	else
	{
		bWaitingToReturnRotation = false;
	}
}
