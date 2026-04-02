#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "GunViewerPawn.generated.h"

// Forward declarations
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class USceneComponent;
class USpringArmComponent;

/**
 * AGunViewerPawn
 *
 * A product-viewer style pawn for inspecting gun models.
 * Works like a 3D product viewer on an e-commerce site:
 *   - Left-click drag to rotate the gun
 *   - Scroll wheel to zoom in/out
 *   - Right-click drag to pan
 *   - Middle mouse to reset view
 *
 * Attach your gun mesh actors as children of the GunPivot component in the editor.
 */
UCLASS()
class TEIDOREARMORY_API AGunViewerPawn : public APawn
{
	GENERATED_BODY()

public:
	AGunViewerPawn();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	// ─── Components ──────────────────────────────────────────────

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/**
	 * GunPivot — attach all your gun mesh actors as children of this component.
	 * All rotation happens on this pivot so the gun spins in place.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> GunPivot;

	/** The camera that views the gun. Stays fixed; the gun rotates in front of it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ViewCamera;

	// ─── Enhanced Input Assets ───────────────────────────────────
	// Assign these in the Blueprint or Details panel after creating Input Action assets.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Left-click drag — rotate the gun */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RotateAction;

	/** Scroll wheel — zoom in/out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	/** Right-click drag — pan the view */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PanAction;

	/** Middle mouse button — reset view to defaults */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ResetAction;

	// ─── Tuning Parameters ───────────────────────────────────────

	/** How fast the gun rotates when dragging (degrees per pixel of mouse movement) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Rotation")
	float RotationSpeed = 0.3f;

	/** How quickly rotation interpolates to the target (higher = snappier) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Rotation")
	float RotationInterpSpeed = 10.0f;

	/** Min pitch angle in degrees (looking down at the gun) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Rotation")
	float MinPitch = -80.0f;

	/** Max pitch angle in degrees (looking up at the gun) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Rotation")
	float MaxPitch = 80.0f;

	/** How much each scroll tick moves the camera (smaller = finer control) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Zoom")
	float ZoomStep = 5.0f;

	/** How quickly zoom interpolates to the target distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Zoom")
	float ZoomInterpSpeed = 8.0f;

	/** Closest the camera can get to the gun */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Zoom")
	float MinZoomDistance = 30.0f;

	/** Farthest the camera can get from the gun */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Zoom")
	float MaxZoomDistance = 200.0f;

	/** Starting distance from the gun */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Zoom")
	float DefaultZoomDistance = 80.0f;

	/** How fast panning moves the pivot (units per pixel) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Pan")
	float PanSpeed = 0.15f;

	/** How quickly pan interpolates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Pan")
	float PanInterpSpeed = 8.0f;

	/** Max distance the pivot can be panned from its origin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Pan")
	float MaxPanDistance = 50.0f;

private:
	// ─── Input Callbacks ─────────────────────────────────────────

	void OnRotate(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnPan(const FInputActionValue& Value);
	void OnReset(const FInputActionValue& Value);

	// ─── Internal State ──────────────────────────────────────────

	// Target values that we interpolate toward for smoothness
	FRotator TargetRotation = FRotator::ZeroRotator;
	FRotator CurrentRotation = FRotator::ZeroRotator;

	float TargetZoomDistance;
	float CurrentZoomDistance;

	FVector TargetPanOffset = FVector::ZeroVector;
	FVector CurrentPanOffset = FVector::ZeroVector;

	// Defaults stored on BeginPlay so Reset can restore them
	FRotator DefaultRotation = FRotator::ZeroRotator;
	FVector DefaultPanOffset = FVector::ZeroVector;
};
