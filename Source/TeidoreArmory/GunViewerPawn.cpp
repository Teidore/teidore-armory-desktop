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

	// Perspective projection — better depth/realism than the web version
	ViewCamera->ProjectionMode = ECameraProjectionMode::Perspective;
	ViewCamera->FieldOfView = 45.0f;

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

	// Shift pivot up toward the slide — makes rotation feel centered on the heavy part
	WeaponCenter.Z += PivotVerticalOffset;

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

	// Position camera to look at the weapon from a good viewing angle
	// Calculate distance based on weapon bounds
	float CameraDistance = 50.0f; // default fallback
	if (bHasBounds)
	{
		float BoundsRadius = CombinedBounds.GetExtent().Size();
		float HalfFOVRad = FMath::DegreesToRadians(ViewCamera->FieldOfView * 0.5f);
		CameraDistance = FMath::Max((BoundsRadius * 1.5f) / FMath::Tan(HalfFOVRad), 30.0f);
	}

	// Camera sits along -X looking at the gun
	ViewCamera->SetRelativeLocation(FVector(-CameraDistance, 0.0f, 0.0f));
	ViewCamera->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	UE_LOG(LogTemp, Warning, TEXT("GunViewerPawn: Camera distance = %f"), CameraDistance);

	// Set initial rotation to weapon's default
	FRotator DefaultRot = GetDefaultRotation();
	RotationAngle = FVector2D(DefaultRot.Yaw, DefaultRot.Pitch);
	TargetAngle = RotationAngle;

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

	// ── Mouse input using GetInputMouseDelta ──
	float DeltaX = 0.0f, DeltaY = 0.0f;
	PC->GetInputMouseDelta(DeltaX, DeltaY);

	bool bLeftMouseDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);

	if (bLeftMouseDown)
	{
		if (!bIsDragging)
		{
			bIsDragging = true;
			RotationVelocity = FVector2D::ZeroVector;
		}

		if (FMath::Abs(DeltaX) > 0.01f || FMath::Abs(DeltaY) > 0.01f)
		{
			RotationAngle.X += -DeltaX * RotationSpeed;
			RotationAngle.Y += DeltaY * RotationSpeed;

			FVector2D InstantVel;
			InstantVel.X = -DeltaX * RotationSpeed / FMath::Max(DeltaTime, 0.001f);
			InstantVel.Y = DeltaY * RotationSpeed / FMath::Max(DeltaTime, 0.001f);

			RotationVelocity = FMath::Lerp(RotationVelocity, InstantVel, 0.25f);
		}
		else
		{
			RotationVelocity *= 0.85f;
		}
	}
	else
	{
		if (bIsDragging)
		{
			const float MaxReleaseVel = 600.0f;
			RotationVelocity.X = FMath::Clamp(RotationVelocity.X, -MaxReleaseVel, MaxReleaseVel);
			RotationVelocity.Y = FMath::Clamp(RotationVelocity.Y, -MaxReleaseVel, MaxReleaseVel);

			bIsDragging = false;

			if (!bRotationLocked)
			{
				FRotator DefaultRot = GetDefaultRotation();
				TargetAngle = FVector2D(DefaultRot.Yaw, DefaultRot.Pitch);
			}
		}

		// ── Spring physics when not dragging ──
		FVector2D Displacement = RotationAngle - TargetAngle;
		FVector2D SpringForce = -SpringTension * Displacement - SpringFriction * RotationVelocity;

		RotationVelocity += SpringForce * DeltaTime;
		RotationAngle += RotationVelocity * DeltaTime;

		// Clean snap when close — no micro-wobble
		if (!bRotationLocked && Displacement.Size() < 0.05f && RotationVelocity.Size() < 1.0f)
		{
			RotationAngle = TargetAngle;
			RotationVelocity = FVector2D::ZeroVector;
		}

		if (bRotationLocked)
		{
			TargetAngle = RotationAngle;
			// Smooth friction deceleration — glide to stop
			float FrictionFactor = FMath::Exp(-SpringFriction * DeltaTime * 0.08f);
			RotationVelocity *= FrictionFactor;
			RotationAngle += RotationVelocity * DeltaTime;

			if (RotationVelocity.Size() < 0.5f)
			{
				RotationVelocity = FVector2D::ZeroVector;
			}
		}
	}

	// ── Apply rotation to all weapon parts ──
	// Orbit rotation — simulates camera orbiting around the gun:
	// Yaw: rotate around Z (world up) — drag left/right spins the gun
	// Pitch: rotate around the yaw-rotated X axis — drag down shows top of gun
	// This matches how the web version's orbit controls work
	FQuat YawQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(RotationAngle.X));
	FVector PitchAxis = YawQuat.RotateVector(FVector::ForwardVector);
	FQuat PitchQuat = FQuat(PitchAxis, FMath::DegreesToRadians(RotationAngle.Y));
	FQuat CombinedQuat = PitchQuat * YawQuat;
	FRotator CombinedRotation = CombinedQuat.Rotator();

	for (int32 i = 0; i < WeaponActors.Num(); i++)
	{
		if (WeaponActors[i])
		{
			FVector RotatedOffset = CombinedQuat.RotateVector(ActorOffsets[i]);
			WeaponActors[i]->SetActorLocation(WeaponCenter + RotatedOffset);
			WeaponActors[i]->SetActorRotation(CombinedQuat * ActorBaseRotations[i].Quaternion());
		}
	}
}

// ─────────────────────────────────────────────────────────────────
// Default rotation per weapon type
// ─────────────────────────────────────────────────────────────────

FRotator AGunViewerPawn::GetDefaultRotation() const
{
	// Web values are in radians: (pitch, yaw, roll)
	// Convert to degrees for UE5
	// Default orientations per weapon type
	// Pistol/rifle show level, bolt-action/shotgun show right side (~270°)
	switch (CurrentWeaponType)
	{
	case EWeaponType::Pistol:
		return FRotator(0.0f, 0.0f, 0.0f);
	case EWeaponType::SemiAutoRifle:
		return FRotator(0.0f, 0.0f, 0.0f);
	case EWeaponType::BoltActionRifle:
		return FRotator(0.0f, 270.0f, 0.0f);
	case EWeaponType::Shotgun:
		return FRotator(0.0f, 270.0f, 0.0f);
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

}
