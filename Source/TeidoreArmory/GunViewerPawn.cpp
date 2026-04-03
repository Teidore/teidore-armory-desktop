#include "GunViewerPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

// ─────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────

AGunViewerPawn::AGunViewerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SceneRoot);
	ViewCamera->bAutoActivate = true;

	// Orthographic projection — matches the web builder
	ViewCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
	ViewCamera->OrthoWidth = 200.0f; // Will be updated based on viewport width

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

// ─────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::BeginPlay()
{
	Super::BeginPlay();

	// Find all StaticMeshActors — these are the weapon parts
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStaticMeshActor::StaticClass(), FoundActors);

	// Calculate combined bounding box for centering
	FBox CombinedBounds(ForceInit);
	bool bHasBounds = false;

	for (AActor* Actor : FoundActors)
	{
		// Set all components to Movable FIRST
		TArray<UActorComponent*> AllComponents;
		Actor->GetComponents(AllComponents);
		for (UActorComponent* Comp : AllComponents)
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
			{
				SceneComp->SetMobility(EComponentMobility::Movable);
			}
		}

		FVector Origin, Extent;
		Actor->GetActorBounds(false, Origin, Extent);
		FBox ActorBox(Origin - Extent, Origin + Extent);

		if (bHasBounds)
			CombinedBounds += ActorBox;
		else
		{
			CombinedBounds = ActorBox;
			bHasBounds = true;
		}
	}

	WeaponCenter = bHasBounds ? CombinedBounds.GetCenter() : FVector::ZeroVector;

	// Store actors and their offsets from center
	for (AActor* Actor : FoundActors)
	{
		WeaponActors.Add(Actor);
		ActorOffsets.Add(Actor->GetActorLocation() - WeaponCenter);
		ActorBaseRotations.Add(Actor->GetActorRotation());
	}

	UE_LOG(LogTemp, Warning, TEXT("GunViewerPawn: Found %d weapon parts, center = %s"),
		WeaponActors.Num(), *WeaponCenter.ToString());

	// Position the pawn at weapon center
	SetActorLocation(WeaponCenter);

	// Set camera position — looking at the weapon from the front
	// Web uses (0, 0.1, 10) — in UE5 that's offset along X (forward)
	ViewCamera->SetRelativeLocation(FVector(1000.0f, 0.0f, 10.0f));
	ViewCamera->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	// Set initial rotation to weapon's default
	FRotator DefaultRot = GetDefaultRotation();
	RotationAngle = FVector2D(DefaultRot.Yaw, DefaultRot.Pitch);
	TargetAngle = RotationAngle;

	// Update orthographic zoom for current viewport
	UpdateOrthoZoom();

	// Set up input — clear defaults, add only ours
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
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
		if (ToggleLockAction)
		{
			EIC->BindAction(ToggleLockAction, ETriggerEvent::Started, this, &AGunViewerPawn::OnToggleLock);
		}
	}
}

// ─────────────────────────────────────────────────────────────────
// Tick — spring physics rotation + mouse input
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// ── Update ortho zoom if viewport resized ──
	UpdateOrthoZoom();

	// ── Mouse input ──
	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);
	FVector2D CurrentMousePos(MouseX, MouseY);

	bool bLeftMouseDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);

	if (bLeftMouseDown)
	{
		if (!bIsDragging)
		{
			// Just started dragging
			bIsDragging = true;
			bLastMouseValid = false; // Don't use stale position
		}

		if (bLastMouseValid)
		{
			FVector2D Delta = CurrentMousePos - LastMousePos;

			// Apply rotation directly while dragging
			// Divide by viewport size to normalize, then multiply by speed
			RotationAngle.X += Delta.X * RotationSpeed * 0.5f;
			RotationAngle.Y += -Delta.Y * RotationSpeed * 0.5f;

			// Track velocity for momentum on release
			RotationVelocity.X = Delta.X * RotationSpeed * 0.5f / FMath::Max(DeltaTime, 0.001f);
			RotationVelocity.Y = -Delta.Y * RotationSpeed * 0.5f / FMath::Max(DeltaTime, 0.001f);
		}

		LastMousePos = CurrentMousePos;
		bLastMouseValid = true;
	}
	else
	{
		if (bIsDragging)
		{
			// Just released
			bIsDragging = false;
			bLastMouseValid = false;

			if (!bRotationLocked)
			{
				// Set target to default orientation — spring will pull it back
				FRotator DefaultRot = GetDefaultRotation();
				TargetAngle = FVector2D(DefaultRot.Yaw, DefaultRot.Pitch);
			}
		}

		// ── Spring physics when not dragging ──
		// Spring force: F = -tension * (position - target) - friction * velocity
		// This gives the smooth deceleration + snap-back from the web version

		FVector2D Displacement = RotationAngle - TargetAngle;
		FVector2D SpringForce = -SpringTension * Displacement - SpringFriction * RotationVelocity;

		// Apply force (F = ma, mass = 1, so acceleration = force)
		RotationVelocity += SpringForce * DeltaTime;
		RotationAngle += RotationVelocity * DeltaTime;

		// If rotation locked, target tracks current position (no spring pull)
		if (bRotationLocked)
		{
			TargetAngle = RotationAngle;
			// Just apply friction for smooth stop
			RotationVelocity *= FMath::Max(0.0f, 1.0f - SpringFriction * DeltaTime * 0.1f);
			RotationAngle += RotationVelocity * DeltaTime;
		}
	}

	// ── Apply rotation to all weapon parts ──
	FRotator CurrentRot(RotationAngle.Y, RotationAngle.X, 0.0f);

	for (int32 i = 0; i < WeaponActors.Num(); i++)
	{
		if (WeaponActors[i])
		{
			FVector RotatedOffset = CurrentRot.RotateVector(ActorOffsets[i]);
			WeaponActors[i]->SetActorLocation(WeaponCenter + RotatedOffset);
			WeaponActors[i]->SetActorRotation(CurrentRot + ActorBaseRotations[i]);
		}
	}
}

// ─────────────────────────────────────────────────────────────────
// Orthographic zoom — maps viewport width to ortho width
// Matches the web builder's responsive zoom
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::UpdateOrthoZoom()
{
	if (!ViewCamera) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	int32 ViewportX, ViewportY;
	PC->GetViewportSize(ViewportX, ViewportY);

	if (ViewportX == LastViewportWidth || ViewportX == 0) return;
	LastViewportWidth = ViewportX;

	// Map viewport width to ortho width
	// Web values: 100 at <768, up to 300 at 1800+
	// UE5 OrthoWidth is in unreal units, so we scale up from the web values
	float OrthoWidth;
	if (ViewportX < 768)
		OrthoWidth = 100.0f;
	else if (ViewportX < 900)
		OrthoWidth = 180.0f;
	else if (ViewportX < 1000)
		OrthoWidth = 200.0f;
	else if (ViewportX < 1200)
		OrthoWidth = 210.0f;
	else if (ViewportX < 1400)
		OrthoWidth = 225.0f;
	else if (ViewportX < 1800)
		OrthoWidth = 250.0f;
	else
		OrthoWidth = 300.0f;

	ViewCamera->OrthoWidth = OrthoWidth;

	UE_LOG(LogTemp, Log, TEXT("OrthoZoom: viewport=%d, width=%f"), ViewportX, OrthoWidth);
}

// ─────────────────────────────────────────────────────────────────
// Default rotation per weapon type
// ─────────────────────────────────────────────────────────────────

FRotator AGunViewerPawn::GetDefaultRotation() const
{
	// Web values are in radians: (pitch, yaw, roll)
	// Convert to degrees for UE5
	switch (CurrentWeaponType)
	{
	case EWeaponType::Pistol:
		return FRotator(FMath::RadiansToDegrees(0.05f), 0.0f, 0.0f);
	case EWeaponType::SemiAutoRifle:
		return FRotator(FMath::RadiansToDegrees(0.05f), 0.0f, 0.0f);
	case EWeaponType::BoltActionRifle:
		return FRotator(FMath::RadiansToDegrees(0.05f), FMath::RadiansToDegrees(4.7f), 0.0f);
	case EWeaponType::Shotgun:
		return FRotator(FMath::RadiansToDegrees(0.05f), FMath::RadiansToDegrees(4.73f), 0.0f);
	default:
		return FRotator::ZeroRotator;
	}
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
		// Unlocking — spring will pull back to default
		FRotator DefaultRot = GetDefaultRotation();
		TargetAngle = FVector2D(DefaultRot.Yaw, DefaultRot.Pitch);
	}
}

// ─────────────────────────────────────────────────────────────────
// RecalculateFraming — for when parts change
// ─────────────────────────────────────────────────────────────────

void AGunViewerPawn::RecalculateFraming()
{
	// Recalculate center and offsets when parts change
	FBox CombinedBounds(ForceInit);
	bool bHasBounds = false;

	for (AActor* Actor : WeaponActors)
	{
		if (Actor)
		{
			FVector Origin, Extent;
			Actor->GetActorBounds(false, Origin, Extent);
			FBox ActorBox(Origin - Extent, Origin + Extent);

			if (bHasBounds)
				CombinedBounds += ActorBox;
			else
			{
				CombinedBounds = ActorBox;
				bHasBounds = true;
			}
		}
	}

	if (bHasBounds)
	{
		WeaponCenter = CombinedBounds.GetCenter();
		SetActorLocation(WeaponCenter);

		// Recalculate offsets
		ActorOffsets.Empty();
		ActorBaseRotations.Empty();
		for (AActor* Actor : WeaponActors)
		{
			if (Actor)
			{
				ActorOffsets.Add(Actor->GetActorLocation() - WeaponCenter);
				ActorBaseRotations.Add(Actor->GetActorRotation());
			}
		}
	}

	// Force ortho zoom update
	LastViewportWidth = 0;
	UpdateOrthoZoom();
}
