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
 * Weapon type determines default orientation, scale, and exploded view offsets.
 */
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Pistol          UMETA(DisplayName = "Pistol"),
	SemiAutoRifle   UMETA(DisplayName = "Semi-Auto Rifle"),
	BoltActionRifle UMETA(DisplayName = "Bolt-Action Rifle"),
	Shotgun         UMETA(DisplayName = "Shotgun"),
};

/**
 * AGunViewerPawn
 *
 * Teidore Armory desktop weapon viewer — Tarkov/CoD gunsmith style.
 *
 * Matches the web builder behavior:
 *   - Orthographic camera with viewport-based zoom
 *   - Full 360° drag rotation with spring-based damping
 *   - Auto snap-back to default orientation (unless rotation locked)
 *   - Auto-grabs all StaticMeshActors in the level as weapon parts
 *   - Architecture ready for exploded view, weapon switching, part labels
 */
UCLASS()
class TEIDOREARMORY_API AGunViewerPawn : public APawn
{
	GENERATED_BODY()

public:
	AGunViewerPawn();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ─── Components ──────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ViewCamera;

	// ─── Input ───────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleLockAction;

	// ─── Weapon Type ─────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Weapon")
	EWeaponType CurrentWeaponType = EWeaponType::Pistol;

	// ─── Rotation ────────────────────────────────────────────────

	/** Multiplier for drag-to-rotation conversion (web uses 1.2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Rotation")
	float RotationSpeed = 1.2f;

	/** Spring tension — higher = faster snap-back (web: 250) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Spring")
	float SpringTension = 250.0f;

	/** Spring friction — higher = less overshoot (web: 20) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Spring")
	float SpringFriction = 20.0f;

	// ─── Rotation Lock ───────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Lock")
	bool bRotationLocked = false;

	UFUNCTION(BlueprintCallable, Category = "Viewer|Lock")
	void SetRotationLocked(bool bLocked);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Viewer|Lock")
	bool IsRotationLocked() const { return bRotationLocked; }

	// ─── Orthographic Zoom ───────────────────────────────────────

	/** Updates orthographic width based on viewport size (matches web zoom mapping) */
	void UpdateOrthoZoom();

private:
	void OnToggleLock(const FInputActionValue& Value);

	/** Returns default rotation for the current weapon type */
	FRotator GetDefaultRotation() const;

	// All weapon mesh actors in the level
	UPROPERTY()
	TArray<AActor*> WeaponActors;

	FVector WeaponCenter = FVector::ZeroVector;
	TArray<FVector> ActorOffsets;
	TArray<FRotator> ActorBaseRotations;

	// Spring physics for rotation
	FVector2D RotationAngle = FVector2D::ZeroVector;    // Current angle (X=yaw, Y=pitch)
	FVector2D RotationVelocity = FVector2D::ZeroVector;  // Angular velocity
	FVector2D TargetAngle = FVector2D::ZeroVector;       // Where spring pulls toward

	// Mouse state
	bool bIsDragging = false;
	FVector2D LastMousePos = FVector2D::ZeroVector;
	bool bLastMouseValid = false;

	// Track last viewport width for zoom recalculation
	int32 LastViewportWidth = 0;
};
