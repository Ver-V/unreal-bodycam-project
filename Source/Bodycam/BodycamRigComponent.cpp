// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamRigComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

UBodycamRigComponent::UBodycamRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Run after character movement so we place the camera on this frame's position.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UBodycamRigComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	Camera = GetOwner()->FindComponentByClass<UCameraComponent>();

	if (Camera)
	{
		Camera->bUsePawnControlRotation = false;
		Camera->SetFieldOfView(FieldOfView);
	}

	// The arms hang off the character's spring arm; we drive it ourselves so it shares the body motion
	if (USpringArmComponent* SpringArm = GetOwner()->FindComponentByClass<USpringArmComponent>())
	{
		SpringArm->bUsePawnControlRotation = false;
		ArmRoot = SpringArm;
	}
}

void UBodycamRigComponent::AddKick(const FRotator& Kick)
{
	// Treat the kick as an angular velocity impulse sized so the peak offset is roughly Kick degrees.
	KickAngularVelocity += FVector(Kick.Pitch, Kick.Yaw, Kick.Roll) * FMath::Sqrt(KickStiffness);
}

void UBodycamRigComponent::AddPositionKick(const FVector& Kick)
{
	KickOffsetVelocity += Kick * FMath::Sqrt(KickStiffness);
}

void UBodycamRigComponent::StepSpring(FVector& Value, FVector& Velocity, const FVector& Target, float Stiffness, float DampingRatio, float DeltaTime)
{
	const float Damping = 2.f * FMath::Sqrt(Stiffness) * DampingRatio;

	constexpr float MaxStep = 1.f / 120.f;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime / MaxStep), 1, 16);
	const float H = DeltaTime / Steps;

	for (int32 i = 0; i < Steps; ++i)
	{
		const FVector Accel = (Target - Value) * Stiffness - Velocity * Damping;
		Velocity += Accel * H;
		Value += Velocity * H;
	}
}

void UBodycamRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Camera || !OwnerCharacter || DeltaTime <= 0.f)
	{
		return;
	}

	// Avoid spring explosions after hitches (level load, breakpoints).
	DeltaTime = FMath::Min(DeltaTime, 0.1f);

	// Blend hip -> aim tuning
	const float Stiffness = FMath::Lerp(FollowStiffness, AimFollowStiffness, AimAlpha);
	const float MaxLag = FMath::Lerp(MaxLagAngle, AimMaxLagAngle, AimAlpha);
	const float BobScale = FMath::Lerp(1.f, AimBobScale, AimAlpha);
	const float SwayScale = FMath::Lerp(1.f, AimSwayScale, AimAlpha);
	Camera->SetFieldOfView(FMath::Lerp(FieldOfView, AimFieldOfView, AimAlpha));

	// --- Follow the aim with a lagging spring ---
	const FRotator Aim = OwnerCharacter->GetControlRotation();
	const float AimPitch = FRotator::NormalizeAxis(Aim.Pitch) * PitchFollowRatio;

	if (!bInitialized)
	{
		FollowAngles = FVector(AimPitch, Aim.Yaw, 0.f);
		LastFollowTarget = FollowAngles;
		bInitialized = true;
	}

	// Express the target yaw relative to the current yaw so the spring never takes the long way round.
	const FVector FollowTarget(AimPitch, FollowAngles.Y + FMath::FindDeltaAngleDegrees(FollowAngles.Y, Aim.Yaw), 0.f);
	const FVector AimVelocity(
		(FollowTarget.X - LastFollowTarget.X) / DeltaTime,
		FMath::FindDeltaAngleDegrees(LastFollowTarget.Y, FollowTarget.Y) / DeltaTime,
		0.f);
	LastFollowTarget = FVector(FollowTarget.X, FRotator::NormalizeAxis(FollowTarget.Y), 0.f);

	StepSpring(FollowAngles, FollowVelocity, FollowTarget, Stiffness, FollowDampingRatio, DeltaTime);

	// Leash: past MaxLagAngle the camera is dragged along at the aim's speed.
	// Matching the velocity (not just the angle) avoids a snap when the leash lets go.
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const float Lag = FollowTarget[Axis] - FollowAngles[Axis];
		if (FMath::Abs(Lag) > MaxLag)
		{
			FollowAngles[Axis] = FollowTarget[Axis] - FMath::Sign(Lag) * MaxLag;
			FollowVelocity[Axis] = AimVelocity[Axis];
		}
	}
	FollowAngles.Y = FRotator::NormalizeAxis(FollowAngles.Y);

	// Lean into turns based on how fast the chest is swinging.
	const float TargetTurnRoll = FMath::Clamp(FollowVelocity.Y / 100.f * TurnRollScale, -8.f, 8.f);
	SmoothedTurnRoll = FMath::FInterpTo(SmoothedTurnRoll, TargetTurnRoll, DeltaTime, 6.f);

	// --- Walk bob ---
	const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	const float GroundSpeed = OwnerCharacter->GetVelocity().Size2D();
	const bool bGrounded = Movement && Movement->IsMovingOnGround();

	const float TargetBobAlpha = bGrounded ? FMath::Clamp(GroundSpeed / BobReferenceSpeed, 0.f, 1.f) : 0.f;
	BobAlpha = FMath::FInterpTo(BobAlpha, TargetBobAlpha, DeltaTime, 8.f);

	if (bGrounded)
	{
		BobPhase = FMath::Fmod(BobPhase + GroundSpeed * DeltaTime / StrideLength * UE_TWO_PI, UE_TWO_PI);
	}

	const float StepSin = FMath::Sin(BobPhase);
	// |sin| gives a sharp dip at each foot plant (two per stride) instead of a smooth float.
	const float StepImpact = FMath::Abs(StepSin);

	const FVector BobOffset(
		0.f,
		StepSin * BobLateral,
		(StepImpact - 0.5f) * 2.f * BobVertical);
	const FVector BobAngles(
		-StepImpact * BobPitch,
		0.f,
		StepSin * BobRoll);

	// --- Idle sway ---
	NoiseTime += DeltaTime * IdleSwaySpeed;
	const FVector IdleAngles(
		FMath::PerlinNoise1D(NoiseTime) * IdleSwayAngle,
		FMath::PerlinNoise1D(NoiseTime + 37.1f) * IdleSwayAngle,
		FMath::PerlinNoise1D(NoiseTime + 91.7f) * IdleSwayAngle * 0.5f);

	// --- Kicks spring back to rest ---
	StepSpring(KickAngles, KickAngularVelocity, FVector::ZeroVector, KickStiffness, KickDampingRatio, DeltaTime);
	StepSpring(KickOffset, KickOffsetVelocity, FVector::ZeroVector, KickStiffness, KickDampingRatio, DeltaTime);

	HeightSmoothingOffset = FMath::FInterpTo(HeightSmoothingOffset, 0.f, DeltaTime, HeightSmoothingSpeed);
	const FVector HeightSmoothing(0.f, 0.f, HeightSmoothingOffset);

	// --- Compose ---
	const FVector Angles = FollowAngles + BobAngles * (BobAlpha * BobScale) + IdleAngles * SwayScale + KickAngles + FVector(0.f, 0.f, SmoothedTurnRoll);
	const FRotator ViewRotation(Angles.X, Angles.Y, Angles.Z);
	const FRotator ChestYaw(0.f, FollowAngles.Y, 0.f);

	const FVector ViewLocation = OwnerCharacter->GetActorLocation() + HeightSmoothing
		+ ChestYaw.RotateVector(MountOffset)
		+ ViewRotation.RotateVector(BobOffset * (BobAlpha * BobScale) + KickOffset);

	Camera->SetWorldLocationAndRotation(ViewLocation, ViewRotation);

	// --- Arms: ride on the body (same bob, sway and kicks), then turn partway toward the true aim ---
	if (ArmRoot)
	{
		const FRotator TrueAim(FRotator::NormalizeAxis(Aim.Pitch), Aim.Yaw, 0.f);
		const FQuat ArmRotation = FQuat::Slerp(ViewRotation.Quaternion(), TrueAim.Quaternion(), ArmAimFollow);

		const FVector ArmLocation = OwnerCharacter->GetActorLocation() + HeightSmoothing
			+ ChestYaw.RotateVector(ArmPivotOffset)
			+ ViewRotation.RotateVector(BobOffset * (BobAlpha * BobScale) + KickOffset);

		ArmRoot->SetWorldLocationAndRotation(ArmLocation, ArmRotation);
	}
}
