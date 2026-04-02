#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "GunViewerPawn.generated.h"

class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class USceneComponent;

/**
 * AGunViewerPawn
 *
 * Product-viewer pawn styled after the Teidore Armory website / CoD gunsmith:
 *   - Left-click drag to rotate the gun — auto-returns to default when released
 *   - Camera auto-frames the gun based on its bounding box (no manual zoom)
 *   - Rotation lock: when enabled, the gun stays where you leave it
 *   - Call RecalculateFraming() when parts are added/removed to re-fit the camera
 *
 * Attach gun mesh actors as children of the GunPivot component in the editor.
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Attach all gun mesh actors as children of this component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> GunPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ViewCamera;

	// ─── Enhanced Input Assets ───────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Left-click drag — rotate the gun */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RotateAction;

	/** Toggle rotation lock (e.g. bound to a key or UI button) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleLockAction;

	// ─── Tuning Parameters ───────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Rotation")
	float RotationSpeed = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Rotation")
	float RotationInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Rotation")
	float MinPitch = -80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Rotation")
	float MaxPitch = 80.0f;

	/** Extra padding multiplier around the gun when auto-framing (1.0 = tight fit, 1.5 = more breathing room) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Framing")
	float FramingPadding = 1.3f;

	/** How quickly the camera distance interpolates when the gun size changes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Framing")
	float FramingInterpSpeed = 5.0f;

	/** Minimum camera distance even for very small parts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Framing")
	float MinFramingDistance = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|AutoReturn")
	float AutoReturnInterpSpeed = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|AutoReturn")
	float AutoReturnDelay = 0.5f;

	// ─── Rotation Lock ───────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Lock")
	bool bRotationLocked = false;

	UFUNCTION(BlueprintCallable, Category = "Viewer|Lock")
	void SetRotationLocked(bool bLocked);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Viewer|Lock")
	bool IsRotationLocked() const { return bRotationLocked; }

	// ─── Auto-Framing ────────────────────────────────────────────

	/**
	 * Recalculates the ideal camera distance based on the bounding box of
	 * everything attached to GunPivot. Call this from Blueprint whenever
	 * you add/remove/swap gun parts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Viewer|Framing")
	void RecalculateFraming();

private:
	void OnRotate(const FInputActionValue& Value);
	void OnRotateReleased(const FInputActionValue& Value);
	void OnToggleLock(const FInputActionValue& Value);

	float CalculateIdealDistance() const;

	FRotator TargetRotation = FRotator::ZeroRotator;
	FRotator CurrentRotation = FRotator::ZeroRotator;
	FRotator DefaultRotation = FRotator::ZeroRotator;

	float TargetCameraDistance;
	float CurrentCameraDistance;

	bool bIsDraggingRotation = false;
	float TimeSinceRotateRelease = 0.0f;
	bool bWaitingToReturnRotation = false;
};
