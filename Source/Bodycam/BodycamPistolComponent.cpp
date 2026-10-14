// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamPistolComponent.h"
#include "BodycamCharacter.h"
#include "BodycamRigComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/DecalComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"

UBodycamPistolComponent::UBodycamPistolComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBodycamPistolComponent::BeginPlay()
{
	Super::BeginPlay();

	Character = Cast<ABodycamCharacter>(GetOwner());

	// Start fully loaded: full magazine plus one in the chamber
	AmmoInMagazine = MagazineSize;
	bRoundChambered = true;

	if (Character)
	{
		// Hands and pistol are one mesh, so nothing is visible until the pistol is picked up
		Character->GetMesh1P()->SetVisibility(false, true);

		HipMeshLocation = Character->GetMesh1P()->GetRelativeLocation();
		HipMeshRotation = Character->GetMesh1P()->GetRelativeRotation();
		BaseWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
	}

	if (bStartEquipped)
	{
		Equip();
	}
}

void UBodycamPistolComponent::Equip()
{
	if (Character == nullptr || bEquipped)
	{
		return;
	}

	bEquipped = true;
	USkeletalMeshComponent* Mesh = Character->GetMesh1P();
	Mesh->SetVisibility(true, true);

	const bool bPlayed = PlayBusyMontage(DrawMontage);
	UE_LOG(LogTemp, Log, TEXT("[Pistol] Equip: Mesh=%s AnimInstance=%s DrawMontage=%s Played=%d MeshLoc=%s"),
		*GetNameSafe(Mesh->GetSkeletalMeshAsset()),
		*GetNameSafe(Mesh->GetAnimInstance()),
		*GetNameSafe(DrawMontage),
		bPlayed,
		*Mesh->GetRelativeLocation().ToString());
}

void UBodycamPistolComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Character == nullptr)
	{
		return;
	}

	// Can't hold sights while reloading or drawing
	const bool bAiming = bEquipped && !bBusy && (bWantsToAim || bDebugForceAim);
	AimAlpha = FMath::FInterpTo(AimAlpha, bAiming ? 1.f : 0.f, DeltaTime, AimSpeed);

	// Smoothstep so the raise eases in and out instead of moving linearly
	const float Blend = FMath::SmoothStep(0.f, 1.f, AimAlpha);

	USkeletalMeshComponent* Mesh = Character->GetMesh1P();
	Mesh->SetRelativeLocationAndRotation(
		HipMeshLocation + AimMeshOffset * Blend,
		HipMeshRotation + AimMeshRotation * Blend);

	Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * FMath::Lerp(1.f, AimWalkSpeedScale, Blend);
	Character->GetBodycamRig()->SetAimAlpha(Blend);
}

void UBodycamPistolComponent::BindInput(UEnhancedInputComponent* InputComponent)
{
	// Started (not Triggered) = one shot per click, like a real semi-auto
	if (FireAction)
	{
		InputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &UBodycamPistolComponent::Fire);
	}
	if (ReloadAction)
	{
		InputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &UBodycamPistolComponent::Reload);
	}
	if (AimAction)
	{
		// Hold to aim: pressed -> Started, released -> Completed
		InputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &UBodycamPistolComponent::StartAim);
		InputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &UBodycamPistolComponent::StopAim);
	}
}

void UBodycamPistolComponent::Fire()
{
	if (Character == nullptr || !bEquipped || bBusy)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastShotTime < MinShotInterval)
	{
		return;
	}
	LastShotTime = Now;

	if (!bRoundChambered)
	{
		if (DryFireSound)
		{
			UGameplayStatics::PlaySound2D(this, DryFireSound);
		}
		return;
	}

	FireHitscan();
	PlayFireSound();
	ApplyRecoil();
	PlayMontage(FireMontage);

	// The slide feeds the next round from the magazine; with an empty magazine the chamber stays empty
	if (AmmoInMagazine > 0)
	{
		--AmmoInMagazine;
	}
	else
	{
		bRoundChambered = false;
	}
}

void UBodycamPistolComponent::Reload()
{
	if (Character == nullptr || !bEquipped || bBusy || AmmoInMagazine >= MagazineSize)
	{
		return;
	}

	bReloadFromEmpty = !bRoundChambered;
	UAnimMontage* Montage = bReloadFromEmpty ? ReloadEmptyMontage : ReloadMontage;

	// No animation assigned: reload instantly so the gun is still usable
	if (!PlayBusyMontage(Montage))
	{
		FinishReload();
	}
}

void UBodycamPistolComponent::FinishReload()
{
	// From empty the slide strips the first round off the new magazine into the chamber
	AmmoInMagazine = bReloadFromEmpty ? MagazineSize - 1 : MagazineSize;
	bRoundChambered = true;
}

bool UBodycamPistolComponent::PlayMontage(UAnimMontage* Montage)
{
	if (Montage == nullptr)
	{
		return false;
	}

	UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance();
	return AnimInstance && AnimInstance->Montage_Play(Montage) > 0.f;
}

bool UBodycamPistolComponent::PlayBusyMontage(UAnimMontage* Montage)
{
	if (!PlayMontage(Montage))
	{
		return false;
	}

	bBusy = true;

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UBodycamPistolComponent::OnBusyMontageEnded);
	Character->GetMesh1P()->GetAnimInstance()->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

void UBodycamPistolComponent::OnBusyMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bBusy = false;

	// Only count the reload if the animation actually finished
	if (!bInterrupted && (Montage == ReloadMontage || Montage == ReloadEmptyMontage))
	{
		FinishReload();
	}
}

void UBodycamPistolComponent::FireHitscan()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Shoot where the gun actually points (the arms blend between body and aim), not the camera center
	const FVector Direction = Character->GetAimRoot()->GetComponentRotation().Vector();
	USkeletalMeshComponent* Mesh = Character->GetMesh1P();
	const FVector Start = Mesh->DoesSocketExist(MuzzleSocketName)
		? Mesh->GetSocketLocation(MuzzleSocketName)
		: Character->GetAimRoot()->GetComponentLocation();
	const FVector End = Start + Direction * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamPistolShot), true, Character);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	if (bHit)
	{
		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (HitComponent && HitComponent->IsSimulatingPhysics())
		{
			HitComponent->AddImpulseAtLocation(Direction * ImpactImpulse, Hit.ImpactPoint);
		}

		SpawnImpactEffects(Hit);
	}

	if (bDrawDebug)
	{
		DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, FColor::Red, false, 1.f);
		if (bHit)
		{
			DrawDebugPoint(World, Hit.ImpactPoint, 10.f, FColor::Yellow, false, 1.f);
		}
	}
}

void UBodycamPistolComponent::SpawnImpactEffects(const FHitResult& Hit)
{
	// Effects face out of the surface, so dust sprays back toward the shooter's side
	const FRotator SurfaceRotation = Hit.ImpactNormal.Rotation();

	if (ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, Hit.ImpactPoint, SurfaceRotation);
	}

	if (ImpactDecal)
	{
		// Decals project along their X axis, so point it into the surface; random roll so holes don't all look the same
		FRotator DecalRotation = (-Hit.ImpactNormal).Rotation();
		DecalRotation.Roll = FMath::FRandRange(-180.f, 180.f);

		// Attach to what we hit so holes follow moving/physics objects
		if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAttached(ImpactDecal, ImpactDecalSize, Hit.GetComponent(), NAME_None,
			Hit.ImpactPoint, DecalRotation, EAttachLocation::KeepWorldPosition, ImpactDecalLifetime))
		{
			// Start fading a couple of seconds before the lifetime ends
			if (ImpactDecalLifetime > 0.f)
			{
				Decal->SetFadeOut(FMath::Max(ImpactDecalLifetime - 2.f, 0.f), 2.f);
			}
		}
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Hit.ImpactPoint);
	}
}

void UBodycamPistolComponent::PlayFireSound()
{
	if (FireSound == nullptr)
	{
		return;
	}

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

void UBodycamPistolComponent::ApplyRecoil()
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
