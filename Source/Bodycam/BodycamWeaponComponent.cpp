// Copyright Epic Games, Inc. All Rights Reserved.


#include "BodycamWeaponComponent.h"
#include "BodycamCharacter.h"
#include "BodycamProjectile.h"
#include "BodycamRigComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Animation/AnimInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Sound/SoundAttenuation.h"

// Sets default values for this component's properties
UBodycamWeaponComponent::UBodycamWeaponComponent()
{
	// Default offset from the character location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
}


void UBodycamWeaponComponent::Fire()
{
	if (Character == nullptr || Character->GetController() == nullptr)
	{
		return;
	}

	// Try and fire a projectile
	if (ProjectileClass != nullptr)
	{
		UWorld* const World = GetWorld();
		if (World != nullptr)
		{
			// Shoot where the player aims, not where the lagging bodycam looks
			const FRotator SpawnRotation = Character->GetControlRotation();
			const FVector SpawnLocation = DoesSocketExist(MuzzleSocketName)
				? GetSocketLocation(MuzzleSocketName)
				// MuzzleOffset is in aim space, so transform it to world space before offsetting from the aim pivot
				: Character->GetAimRoot()->GetComponentLocation() + SpawnRotation.RotateVector(MuzzleOffset);
	
			//Set Spawn Collision Handling Override
			FActorSpawnParameters ActorSpawnParams;
			ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ActorSpawnParams.Owner = Character;
			ActorSpawnParams.Instigator = Character;
	
			// Spawn the projectile at the muzzle; the muzzle can sit inside our own capsule, so ignore it
			if (ABodycamProjectile* Projectile = World->SpawnActor<ABodycamProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams))
			{
				Projectile->GetCollisionComp()->IgnoreActorWhenMoving(Character, true);
			}
		}
	}

	ApplyRecoil();

	// Try and play the sound if specified
	if (FireSound != nullptr)
	{
		// Plain 2D sounds get no reverb send by default, so hand it one explicitly (no distance falloff, no panning)
		if (FireAttenuation == nullptr)
		{
			FireAttenuation = NewObject<USoundAttenuation>(this);
			FireAttenuation->Attenuation.bAttenuate = false;
			FireAttenuation->Attenuation.bSpatialize = false;
			FireAttenuation->Attenuation.bEnableReverbSend = true;
			FireAttenuation->Attenuation.ReverbSendMethod = EReverbSendMethod::Manual;
		}
		FireAttenuation->Attenuation.ManualReverbSendLevel = FireReverbSend;

		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation(), 1.f, 1.f, 0.f, FireAttenuation);
	}
	
	// Try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

void UBodycamWeaponComponent::ApplyRecoil()
{
	// Climb the actual aim
	if (AController* Controller = Character->GetController())
	{
		const FRotator Climb(AimRecoilPitch, FMath::RandRange(-AimRecoilYaw, AimRecoilYaw), 0.f);
		Controller->SetControlRotation(Controller->GetControlRotation() + Climb);
	}

	// Jolt the chest camera
	if (UBodycamRigComponent* Rig = Character->GetBodycamRig())
	{
		Rig->AddKick(FRotator(
			CameraKick.Pitch,
			FMath::RandRange(-CameraKick.Yaw, CameraKick.Yaw),
			FMath::RandRange(-CameraKick.Roll, CameraKick.Roll)));
		Rig->AddPositionKick(FVector(-CameraPushBack, 0.f, 0.f));
	}
}

bool UBodycamWeaponComponent::AttachWeapon(ABodycamCharacter* TargetCharacter)
{
	Character = TargetCharacter;

	// Check that the character is valid, and has no weapon component yet
	if (Character == nullptr || Character->GetInstanceComponents().FindItemByClass<UBodycamWeaponComponent>())
	{
		return false;
	}

	// Attach the weapon to the First Person Character
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));

	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
			Subsystem->AddMappingContext(FireMappingContext, 1);
		}

		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
		{
			// Fire
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UBodycamWeaponComponent::Fire);
		}
	}

	return true;
}

void UBodycamWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ensure we have a character owner
	if (Character != nullptr)
	{
		// remove the input mapping context from the Player Controller
		if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(FireMappingContext);
			}
		}
	}

	// maintain the EndPlay call chain
	Super::EndPlay(EndPlayReason);
}