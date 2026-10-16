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
#include "Components/SpotLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	// The map's post process volume blooms small bright spots into a screen-filling haze with colored lens flare ghosts.
	// Camera settings win over volumes, so tame both here: standard bloom, low intensity, no flare ghosts.
	PostProcess.bOverride_BloomMethod = true;
	PostProcess.BloomMethod = BM_SOG;
	PostProcess.bOverride_BloomIntensity = true;
	PostProcess.BloomIntensity = 0.3f;
	PostProcess.bOverride_LensFlareIntensity = true;
	PostProcess.LensFlareIntensity = 0.f;

	// Crouch: smaller capsule and slower walk; the rig smooths the height change
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(55.f);
	GetCharacterMovement()->MaxWalkSpeedCrouched = 150.f;

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

	// Flashlight starts strapped to the chest next to the bodycam; MountFlashlightOnGun moves it to the muzzle
	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FirstPersonCameraComponent);
	Flashlight->SetIntensityUnits(ELightUnits::Candelas);
	Flashlight->SetIntensity(400.f);
	Flashlight->SetAttenuationRadius(1500.f);
	// Lumen bounce from a light at the eye floods the whole room; keep it low so only the beam is bright
	Flashlight->SetIndirectLightingIntensity(0.3f);
	// Fog right at the lens otherwise glows into a bright blob in front of the gun; keep a faint beam
	Flashlight->SetVolumetricScatteringIntensity(0.3f);
	Flashlight->SetInnerConeAngle(12.f);
	Flashlight->SetOuterConeAngle(28.f);
	Flashlight->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
	// The first person arms cast no shadow, so the light can sit right at the gun without giving them away
	Flashlight->SetCastShadows(true);
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

	Flashlight->SetRelativeLocation(FlashlightChestOffset);
	Flashlight->SetVisibility(bFlashlightOn);

	// Pick a random moment in tonight's 22:00-04:00 window (04:00 is 28h after today's midnight)
	const int32 NightStartSeconds = 22 * 3600;
	const int32 NightLengthSeconds = 6 * 3600;
	TimestampStart = FDateTime::Today() + FTimespan::FromSeconds(NightStartSeconds + FMath::RandRange(0, NightLengthSeconds - 1));
}

FText ABodycamCharacter::GetBodycamTimestamp() const
{
	// Real time, so pausing or slow motion doesn't freeze the clock
	const FDateTime Now = TimestampStart + FTimespan::FromSeconds(GetWorld()->GetRealTimeSeconds());
	return FText::FromString(Now.ToString(TEXT("%Y-%m-%d %H:%M:%S")));
}

void ABodycamCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// On the gun: sit at the muzzle but point where the shots go (AimRoot), same as the pistol's hitscan,
	// so the beam always shows where a bullet would land regardless of how the muzzle socket is oriented
	if (bFlashlightOnGun)
	{
		const FName MuzzleSocket = Pistol->MuzzleSocketName;
		const FVector Muzzle = Mesh1P->DoesSocketExist(MuzzleSocket)
			? Mesh1P->GetSocketLocation(MuzzleSocket)
			: AimRoot->GetComponentLocation();
		const FRotator Aim = AimRoot->GetComponentRotation();
		Flashlight->SetWorldLocationAndRotation(Muzzle + Aim.RotateVector(FlashlightGunOffset), Aim);
	}
}

void ABodycamCharacter::MountFlashlightOnGun()
{
	bFlashlightOnGun = true;
	Flashlight->AttachToComponent(Mesh1P, FAttachmentTransformRules::KeepWorldTransform);

	// Equipping shows Mesh1P and everything under it; keep the flashlight in whatever state the player left it
	Flashlight->SetVisibility(bFlashlightOn);
}

void ABodycamCharacter::ToggleFlashlight()
{
	bFlashlightOn = !bFlashlightOn;
	Flashlight->SetVisibility(bFlashlightOn);

	if (FlashlightToggleSound)
	{
		UGameplayStatics::PlaySound2D(this, FlashlightToggleSound);
	}
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

void ABodycamCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// The capsule just shrank and the actor dropped by this much in one frame
	BodycamRig->AbsorbHeightChange(-ScaledHalfHeightAdjust);
}

void ABodycamCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// The capsule grew back and the actor rose by this much in one frame
	BodycamRig->AbsorbHeightChange(ScaledHalfHeightAdjust);
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

		// Crouch (hold)
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABodycamCharacter::StartCrouch);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ABodycamCharacter::StopCrouch);

		// Flashlight
		if (FlashlightAction)
		{
			EnhancedInputComponent->BindAction(FlashlightAction, ETriggerEvent::Started, this, &ABodycamCharacter::ToggleFlashlight);
		}

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
