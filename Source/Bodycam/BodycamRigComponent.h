// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BodycamRigComponent.generated.h"

class UCameraComponent;
class ACharacter;
class USceneComponent;

/**
 * Drives a chest-mounted "bodycam" camera.
 * The camera is decoupled from the control rotation and follows it through a spring,
 * so the view lags behind the aim while the arms/weapon track the aim directly.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BODYCAM_API UBodycamRigComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBodycamRigComponent();

	/** Kick the camera (Pitch/Yaw/Roll in degrees). Used for recoil, hits, landings. */
	UFUNCTION(BlueprintCallable, Category="Bodycam")
	void AddKick(const FRotator& Kick);

	/** Kick the camera position (cm, in camera space). */
	UFUNCTION(BlueprintCallable, Category="Bodycam")
	void AddPositionKick(const FVector& Kick);

	/**
	 * Cancels a sudden actor height change (crouch resizes the capsule and teleports the actor)
	 * so the camera and arms ease to the new height instead of snapping.
	 */
	void AbsorbHeightChange(float DeltaZ) { HeightSmoothingOffset -= DeltaZ; }

	/** 0 = hip, 1 = fully aiming down sights. Blends the camera toward the Aim settings below. */
	UFUNCTION(BlueprintCallable, Category="Bodycam")
	void SetAimAlpha(float Alpha) { AimAlpha = FMath::Clamp(Alpha, 0.f, 1.f); }

	// --- Mount ---

	/** Camera position relative to the actor (chest height). Capsule half height is 96, eyes are at +60. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Mount")
	FVector MountOffset = FVector(10.f, 0.f, 45.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Mount", meta=(ClampMin="60", ClampMax="150"))
	float FieldOfView = 100.f;

	// --- Arms ---

	/** Pivot the arms hang from, relative to the actor (eye height). Hip/aim mesh offsets are measured from here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Arms")
	FVector ArmPivotOffset = FVector(-10.f, 0.f, 60.f);

	/**
	 * How much the arms turn toward the true aim instead of riding with the body camera.
	 * 0 = locked to the camera (classic FPS), 1 = locked to the aim (floaty). In between = attached to the body, gun leads a little.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Arms", meta=(ClampMin="0", ClampMax="1"))
	float ArmAimFollow = 0.35f;

	// --- Follow (camera chasing the aim) ---

	/** Spring stiffness toward the aim. Lower = more lag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Follow")
	float FollowStiffness = 250.f;

	/** 1 = settles without overshoot, below 1 = overshoots and wobbles, above 1 = sluggish. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Follow", meta=(ClampMin="0.1", ClampMax="2"))
	float FollowDampingRatio = 0.9f;

	/** How far (degrees) the camera may fall behind the aim. Past this it rides along with the aim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Follow")
	float MaxLagAngle = 8.f;

	/** Fraction of aim pitch the chest follows. 1 = full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Follow", meta=(ClampMin="0", ClampMax="1"))
	float PitchFollowRatio = 1.f;

	/** Roll (degrees) per 100 deg/s of yaw speed: the camera leans into turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Follow")
	float TurnRollScale = 0.6f;

	// --- Walk bob ---

	/** Distance (cm) of one full step cycle (left + right foot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Bob")
	float StrideLength = 140.f;

	/** Ground speed at which bob reaches full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Bob")
	float BobReferenceSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Bob")
	float BobVertical = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Bob")
	float BobLateral = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Bob")
	float BobRoll = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Bob")
	float BobPitch = 0.6f;

	// --- Idle sway ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Idle")
	float IdleSwayAngle = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Idle")
	float IdleSwaySpeed = 0.4f;

	// --- Aim (values used while aiming down sights) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Aim", meta=(ClampMin="60", ClampMax="150"))
	float AimFieldOfView = 100.f;

	/** Follow stiffness while aiming. Higher = camera locks onto the aim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Aim")
	float AimFollowStiffness = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Aim")
	float AimMaxLagAngle = 5.f;

	/** Walk bob multiplier while aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Aim", meta=(ClampMin="0", ClampMax="1"))
	float AimBobScale = 0.7f;

	/** Idle sway multiplier while aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Aim", meta=(ClampMin="0", ClampMax="1"))
	float AimSwayScale = 0.7f;

	/** How fast the view settles after crouching / standing (higher = snappier) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Mount")
	float HeightSmoothingSpeed = 8.f;

	// --- Kicks (recoil, landing) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Kick")
	float KickStiffness = 220.f;

	/** 1 = no bounce, lower = kicks bounce back past rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Kick", meta=(ClampMin="0.1", ClampMax="2"))
	float KickDampingRatio = 0.5f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Damped spring toward a target, stepped at a fixed rate so it stays stable at low frame rates. */
	static void StepSpring(FVector& Value, FVector& Velocity, const FVector& Target, float Stiffness, float DampingRatio, float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	/** Component the arms are attached to. Placed by this rig every frame. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ArmRoot;

	/** Lagged view angles as (Pitch, Yaw, Roll). Yaw is unwound (not clamped to +-180). */
	FVector FollowAngles = FVector::ZeroVector;
	FVector FollowVelocity = FVector::ZeroVector;

	/** Aim target from last frame, to know how fast the aim is moving. */
	FVector LastFollowTarget = FVector::ZeroVector;

	/** Transient rotation offset from kicks, springs back to zero. */
	FVector KickAngles = FVector::ZeroVector;
	FVector KickAngularVelocity = FVector::ZeroVector;

	/** Transient position offset from kicks, springs back to zero. */
	FVector KickOffset = FVector::ZeroVector;
	FVector KickOffsetVelocity = FVector::ZeroVector;

	float BobPhase = 0.f;
	float BobAlpha = 0.f;
	float SmoothedTurnRoll = 0.f;
	float NoiseTime = 0.f;
	float AimAlpha = 0.f;
	/** World Z offset that hides capsule resizes; decays to zero */
	float HeightSmoothingOffset = 0.f;
	bool bInitialized = false;
};
