// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamCharacter.h"
#include "BodycamProjectile.h"
#include "BodycamRigComponent.h"
#include "BodycamPistolComponent.h"
#include "BodycamInteractable.h"
#include "BodycamZombieSpawnPoint.h"
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
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"

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

	// Objective lines. Override per level in the Blueprint.
	ObjectivePickUpGun = FText::FromString(TEXT("OBJECTIVE: Pick up the gun"));
	ObjectiveEscape = FText::FromString(TEXT("OBJECTIVE: Get out of the building"));
	ObjectiveFindKey = FText::FromString(TEXT("OBJECTIVE: Find the key"));
	ObjectiveUnlockExit = FText::FromString(TEXT("OBJECTIVE: Unlock the door and leave the building"));
	EscapedText = FText::FromString(TEXT("ESCAPED"));

	// The character's own voice sits "in the listener's head": no 3D panning
	VoiceAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("VoiceAudio"));
	VoiceAudio->SetupAttachment(GetCapsuleComponent());
	VoiceAudio->bAutoActivate = false;
	VoiceAudio->bAllowSpatialization = false;

	// Breathing has its own channel so voice lines don't cut it off
	BreathingAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BreathingAudio"));
	BreathingAudio->SetupAttachment(GetCapsuleComponent());
	BreathingAudio->bAutoActivate = false;
	BreathingAudio->bAllowSpatialization = false;

	// Flashlight starts strapped to the chest next to the bodycam; OnPistolEquipped moves it to the muzzle
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

	Health = MaxHealth;

	// The pistol may already be in hand (bStartEquipped), which set the escape objective during Super::BeginPlay
	if (!Pistol->IsEquipped())
	{
		SetObjective(ObjectivePickUpGun);
	}

	// Pick a random moment in tonight's 22:00-04:00 window (04:00 is 28h after today's midnight)
	const int32 NightStartSeconds = 22 * 3600;
	const int32 NightLengthSeconds = 6 * 3600;
	TimestampStart = FDateTime::Today() + FTimespan::FromSeconds(NightStartSeconds + FMath::RandRange(0, NightLengthSeconds - 1));
}

float ABodycamCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead)
	{
		return 0.f;
	}

	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health -= Damage;

	// A swipe knocks the chest camera hard, tilting away from the attacker's side
	const float Side = DamageCauser
		? FMath::Sign(FVector::DotProduct(DamageCauser->GetActorLocation() - GetActorLocation(), GetActorRightVector()))
		: 1.f;
	BodycamRig->AddKick(FRotator(-6.f, 4.f * Side, -8.f * Side));
	BodycamRig->AddPositionKick(FVector(-8.f, 0.f, -3.f));

	PlayVoice(HurtSound, true);

	if (Health <= 0.f)
	{
		Die();
	}
	return Damage;
}

void ABodycamCharacter::Die()
{
	bDead = true;

	// The bodycam sinks to the floor and tips onto its side, then the screen fades once it's down
	BodycamRig->StartDeathFall();
	EndRun(DeathFadeDelay);
}

void ABodycamCharacter::EndRun(float FadeDelay)
{
	GetCharacterMovement()->DisableMovement();
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		DisableInput(PC);

		TWeakObjectPtr<APlayerController> WeakPC = PC;
		const float FadeDuration = DeathFadeDuration;
		auto StartFade = [WeakPC, FadeDuration]()
		{
			if (WeakPC.IsValid() && WeakPC->PlayerCameraManager)
			{
				WeakPC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeDuration, FLinearColor::Black, false, true);
			}
		};

		// SetTimer with a delay of 0 clears the timer instead of firing it, so fade right away
		if (FadeDelay > 0.f)
		{
			GetWorldTimerManager().SetTimer(DeathFadeTimer, StartFade, FadeDelay, false);
		}
		else
		{
			StartFade();
		}

		GetWorldTimerManager().SetTimer(RestartTimer, PC, &APlayerController::RestartLevel, RestartDelay, false);
	}
}

void ABodycamCharacter::SetObjective(const FText& Text)
{
	// Same line still on screen: leave it alone instead of restarting its fade (e.g. bumping the locked door again)
	if (Text.EqualTo(ObjectiveText) && GetObjectiveAlpha() > 0.f)
	{
		return;
	}
	ObjectiveText = Text;
	ObjectiveSetTime = GetWorld()->GetRealTimeSeconds();
}

float ABodycamCharacter::GetObjectiveAlpha() const
{
	// 0.5s fade in, hold, 1s fade out
	const float Age = GetWorld()->GetRealTimeSeconds() - ObjectiveSetTime;
	const float FadeIn = FMath::Clamp(Age / 0.5f, 0.f, 1.f);
	const float FadeOut = FMath::Clamp(1.f - (Age - 0.5f - ObjectiveShowTime) / 1.f, 0.f, 1.f);
	return FMath::Min(FadeIn, FadeOut);
}

FSlateColor ABodycamCharacter::GetObjectiveColor() const
{
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, GetObjectiveAlpha()));
}

FLinearColor ABodycamCharacter::GetObjectiveShadowColor() const
{
	return FLinearColor(0.f, 0.f, 0.f, GetObjectiveAlpha());
}

void ABodycamCharacter::GiveKey()
{
	bHasKey = true;
	SetObjective(ObjectiveUnlockExit);

	// The way back gets dangerous
	ABodycamZombieSpawnPoint::TriggerEvent(this, EBodycamSpawnEvent::KeyPickedUp);
}

void ABodycamCharacter::NotifyExitLocked()
{
	if (!bHasKey)
	{
		SetObjective(ObjectiveFindKey);
	}
}

void ABodycamCharacter::PlayVoice(USoundBase* Sound, bool bForce)
{
	if (Sound == nullptr || bDead)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bForce && Now - LastVoiceTime < VoiceCooldown)
	{
		return;
	}
	LastVoiceTime = Now;

	// One mouth: a new line replaces whatever was being said
	VoiceAudio->SetSound(Sound);
	VoiceAudio->Play();
}

void ABodycamCharacter::OnSpottedByZombie()
{
	// A beat after the zombie's shriek, so the two don't land on top of each other
	GetWorldTimerManager().SetTimer(VoiceTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { PlayVoice(SpottedSound, false); }), 0.5f, false);
}

void ABodycamCharacter::OnZombieKilled()
{
	// Let the body drop first
	GetWorldTimerManager().SetTimer(VoiceTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { PlayVoice(KillSound, false); }), 0.8f, false);
}

void ABodycamCharacter::Escape()
{
	if (bEscaped || bDead)
	{
		return;
	}
	bEscaped = true;
	SetObjective(EscapedText);
	EndRun(0.f);
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

	UpdateBreathing(DeltaSeconds);

	// Footsteps by distance walked on the ground, the same measure the rig's walk bob uses, so sound and camera dips line up
	if (FootstepSound && !bDead && GetCharacterMovement()->IsMovingOnGround())
	{
		FootstepTravel += GetVelocity().Size2D() * DeltaSeconds;
		if (FootstepTravel >= FootstepDistance)
		{
			FootstepTravel -= FootstepDistance;
			const float Volume = bIsCrouched ? CrouchFootstepVolume : 1.f;
			UGameplayStatics::PlaySound2D(this, FootstepSound, Volume, FMath::FRandRange(0.92f, 1.08f));
		}
	}

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

void ABodycamCharacter::UpdateBreathing(float DeltaSeconds)
{
	if (BreathingSound == nullptr)
	{
		return;
	}

	// Footsteps cover it while moving; once you stop, you hear yourself panting
	const bool bStill = !bDead && GetVelocity().Size2D() < 10.f;
	StillTime = bStill ? StillTime + DeltaSeconds : 0.f;

	const bool bShouldBreathe = StillTime >= BreathingDelay;
	if (bShouldBreathe && !bBreathing)
	{
		bBreathing = true;
		BreathingAudio->SetSound(BreathingSound);
		BreathingAudio->FadeIn(1.f, 1.f, FMath::FRandRange(0.f, 2.f)); // random start point so it doesn't restart the same breath each time
	}
	else if (!bShouldBreathe && bBreathing)
	{
		bBreathing = false;
		BreathingAudio->FadeOut(0.8f, 0.f);
	}

	// Louder the more hurt you are: full health = BreathingVolume, near death = up to 2x
	const float Hurt = 1.f - FMath::Clamp(Health / MaxHealth, 0.f, 1.f);
	BreathingAudio->SetVolumeMultiplier(BreathingVolume * (1.f + Hurt));
}

void ABodycamCharacter::OnPistolEquipped()
{
	SetObjective(bHasKey ? ObjectiveUnlockExit : ObjectiveEscape);
	ABodycamZombieSpawnPoint::TriggerEvent(this, EBodycamSpawnEvent::PistolPickedUp);

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
	// Trace along what the player actually sees: the center of the bodycam image.
	// The aim (control rotation) leads the chest camera by up to several degrees and starts higher up,
	// so tracing along it missed items that were dead center on screen.
	const FVector Start = FirstPersonCameraComponent->GetComponentLocation();
	const FVector Direction = FirstPersonCameraComponent->GetForwardVector();
	const FVector End = Start + Direction * InteractDistance;

	AActor* Target = nullptr;
	FHitResult Hit;

	// 1) Precise: a line against the real mesh triangles (works for scanned meshes with no simple collision)
	FCollisionQueryParams ComplexParams(SCENE_QUERY_STAT(BodycamInteract), true, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, ComplexParams))
	{
		Target = Hit.GetActor();
	}

	// 2) Forgiving: a thick sweep, since there is no crosshair (only hits simple collision)
	if (Target == nullptr || !Target->Implements<UBodycamInteractable>())
	{
		FCollisionQueryParams SimpleParams(SCENE_QUERY_STAT(BodycamInteract), false, this);
		Target = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(InteractRadius), SimpleParams)
			? Hit.GetActor()
			: nullptr;
	}

	// 3) Fallback: the interactable closest to the aim within reach, as long as nothing solid is in between.
	// Catches small items on shelves where the shelf soaks up the traces above.
	if (Target == nullptr || !Target->Implements<UBodycamInteractable>())
	{
		Target = nullptr;
		float BestDot = FMath::Cos(FMath::DegreesToRadians(InteractAngle));
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Candidate = *It;
			if (!Candidate->Implements<UBodycamInteractable>())
			{
				continue;
			}

			FVector Center, Extent;
			Candidate->GetActorBounds(true, Center, Extent);
			const FVector ToCandidate = Center - Start;
			const float Distance = ToCandidate.Size();
			if (Distance > InteractDistance + Extent.Size() || Distance < KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float Dot = FVector::DotProduct(Direction, ToCandidate / Distance);
			if (Dot <= BestDot)
			{
				continue;
			}

			// Visible unless something blocks the way well before the candidate itself.
			// An open door's center often sits inside the frame or wall it swung against; that still counts as visible.
			FCollisionQueryParams SightParams(SCENE_QUERY_STAT(BodycamInteract), false, this);
			SightParams.AddIgnoredActor(Candidate);
			FHitResult Blocker;
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Blocker, Start, Center, ECC_Visibility, SightParams)
				&& Blocker.Distance < Distance - Extent.Size();
			if (!bBlocked)
			{
				BestDot = Dot;
				Target = Candidate;
			}
		}
	}

	if (Target && Target->Implements<UBodycamInteractable>())
	{
		IBodycamInteractable::Execute_Interact(Target, this);
	}
}
