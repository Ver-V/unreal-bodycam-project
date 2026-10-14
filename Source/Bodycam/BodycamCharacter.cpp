// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamCharacter.h"
#include "BodycamProjectile.h"
#include "BodycamRigComponent.h"
#include "BodycamPistolComponent.h"
#include "BodycamInteractable.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/ReverbEffect.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ABodycamCharacter

ABodycamCharacter::ABodycamCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	// Aim pivot at eye height. BodycamRig places and rotates it every frame so the arms move with the body.
	AimRoot = CreateDefaultSubobject<USpringArmComponent>(TEXT("AimRoot"));
	AimRoot->SetupAttachment(GetCapsuleComponent());
	AimRoot->SetRelativeLocation(FVector(-10.f, 0.f, 60.f));
	AimRoot->TargetArmLength = 0.f;
	AimRoot->bDoCollisionTest = false;
	AimRoot->bUsePawnControlRotation = false;

	// Create a CameraComponent; its transform is driven by BodycamRig, not the controller
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(12.f, 0.f, 30.f));
	FirstPersonCameraComponent->bUsePawnControlRotation = false;

	// Cheap sensor grain, strongest in the dark. Tweak under the camera's Post Process > Film Grain.
	FPostProcessSettings& PostProcess = FirstPersonCameraComponent->PostProcessSettings;
	PostProcess.bOverride_FilmGrainIntensity = true;
	PostProcess.FilmGrainIntensity = 0.3f;
	PostProcess.bOverride_FilmGrainIntensityShadows = true;
	PostProcess.FilmGrainIntensityShadows = 1.f;
	PostProcess.bOverride_FilmGrainIntensityHighlights = true;
	PostProcess.FilmGrainIntensityHighlights = 0.3f;
	PostProcess.bOverride_FilmGrainTexelSize = true;
	PostProcess.FilmGrainTexelSize = 1.5f;

	BodycamRig = CreateDefaultSubobject<UBodycamRigComponent>(TEXT("BodycamRig"));
	Pistol = CreateDefaultSubobject<UBodycamPistolComponent>(TEXT("Pistol"));

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	// Attached to the aim pivot rather than the camera, so the weapon moves around inside the bodycam frame
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(AimRoot, USpringArmComponent::SocketName);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

}

void ABodycamCharacter::BeginPlay()
{
	Super::BeginPlay();

	UReverbEffect* Reverb = DefaultReverb;
	if (Reverb == nullptr)
	{
		// Concrete-room style reverb so gunshots ring out
		Reverb = NewObject<UReverbEffect>(this);
		Reverb->DecayTime = 1.6f;
		Reverb->ReflectionsGain = 0.4f;
		Reverb->LateGain = 1.4f;
		Reverb->Gain = 0.5f;
	}
	UGameplayStatics::ActivateReverbEffect(this, Reverb, TEXT("BodycamDefault"), 0.f, DefaultReverbVolume, 0.f);
}

void ABodycamCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// Harder landings jolt the chest camera down
	const float FallSpeed = -GetVelocity().Z;
	const float Strength = FMath::Clamp((FallSpeed - 200.f) / 800.f, 0.f, 1.f);
	if (Strength > 0.f)
	{
		BodycamRig->AddKick(FRotator(-4.f * Strength, 0.f, FMath::RandRange(-2.f, 2.f) * Strength));
		BodycamRig->AddPositionKick(FVector(0.f, 0.f, -6.f * Strength));
	}
}

//////////////////////////////////////////////////////////////////////////// Input

void ABodycamCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);

			// Priority 1 so Fire overrides Jump on shared touch input, same as the template weapon
			if (Pistol->GetMappingContext())
			{
				Subsystem->AddMappingContext(Pistol->GetMappingContext(), 1);
			}
		}
	}
}

void ABodycamCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABodycamCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABodycamCharacter::Look);

		// Interact
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ABodycamCharacter::Interact);

		// Fire / Reload / Aim
		Pistol->BindInput(EnhancedInputComponent);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void ABodycamCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void ABodycamCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
void ABodycamCharacter::Interact()
{
	// Sweep from the eyes along the aim, not the lagging bodycam view
	const FVector Start = AimRoot->GetComponentLocation();
	const FVector End = Start + GetControlRotation().Vector() * InteractDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamInteract), false, this);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(InteractRadius), Params))
	{
		return;
	}

	AActor* Target = Hit.GetActor();
	if (Target && Target->Implements<UBodycamInteractable>())
	{
		IBodycamInteractable::Execute_Interact(Target, this);
	}
}
