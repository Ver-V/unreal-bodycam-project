// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "BodycamCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class USpringArmComponent;
class UBodycamRigComponent;
class UBodycamPistolComponent;
class UInputAction;
class UInputMappingContext;
class UReverbEffect;
class USpotLightComponent;
class USoundBase;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class ABodycamCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Arms and weapon hang off this. Driven by BodycamRig: rides with the body camera and leans toward the aim. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* AimRoot;

	/** Drives the chest-mounted camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta = (AllowPrivateAccess = "true"))
	UBodycamRigComponent* BodycamRig;

	/** Sidearm logic: fire, ammo, reload, recoil */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Weapon, meta = (AllowPrivateAccess = "true"))
	UBodycamPistolComponent* Pistol;

	/** Room reverb applied to all sounds. If empty, a built-in indoor reverb is used. Replace with Audio Volumes per area later. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Audio, meta = (AllowPrivateAccess = "true"))
	UReverbEffect* DefaultReverb;

	/** How strongly DefaultReverb is applied (0..1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Audio, meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "1"))
	float DefaultReverbVolume = 0.8f;

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh1P;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** Crouch Input Action (hold) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

	/** Interact Input Action (use / pick up) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* InteractAction;

	/** How far ahead (cm) the player can reach to interact */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Interaction, meta = (AllowPrivateAccess = "true"))
	float InteractDistance = 200.f;

	/** Thickness (cm) of the interact sweep. There is no crosshair, so be forgiving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Interaction, meta = (AllowPrivateAccess = "true"))
	float InteractRadius = 15.f;

	/** Chest-mounted until the pistol is picked up, then rides on the muzzle. Tweak brightness/cone on the component itself. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Flashlight, meta = (AllowPrivateAccess = "true"))
	USpotLightComponent* Flashlight;

	/** Toggle Flashlight Input Action (F) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* FlashlightAction;

	/** Whether the flashlight is on when the game starts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Flashlight, meta = (AllowPrivateAccess = "true"))
	bool bFlashlightOn = false;

	/** Chest mount position relative to the bodycam (cm). Slightly off the lens so shadows stay visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Flashlight, meta = (AllowPrivateAccess = "true"))
	FVector FlashlightChestOffset = FVector(5.f, 10.f, -5.f);

	/** On the gun, light position relative to the muzzle in aim space (cm): X forward, Y right, Z up.
	 *  Ahead so the cone clears the pistol, slightly below like a rail-mounted weapon light. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Flashlight, meta = (AllowPrivateAccess = "true"))
	FVector FlashlightGunOffset = FVector(20.f, 0.f, -4.f);

	/** Click played when toggling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Flashlight, meta = (AllowPrivateAccess = "true"))
	USoundBase* FlashlightToggleSound;

public:
	ABodycamCharacter();

protected:
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Uses whatever interactable the player is aiming at */
	void Interact();

	void StartCrouch() { Crouch(); }
	void StopCrouch() { UnCrouch(); }

	void ToggleFlashlight();

	/** True once the flashlight has moved from the chest to the pistol */
	bool bFlashlightOnGun = false;

	/** Bodycam clock at the moment play started: a random time tonight between 22:00 and 04:00 */
	FDateTime TimestampStart;

public:
	/** Called by the pistol when it is picked up: moves the flashlight from the chest to the muzzle */
	void MountFlashlightOnGun();

	/** Bodycam overlay clock, e.g. "2026-10-16 23:47:12". Always night: starts between 22:00 and 04:00 and runs in real time. */
	UFUNCTION(BlueprintPure, Category = Bodycam)
	FText GetBodycamTimestamp() const;

protected:
	// APawn interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControllerChanged() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	/** Returns AimRoot subobject **/
	USpringArmComponent* GetAimRoot() const { return AimRoot; }
	/** Returns BodycamRig subobject **/
	UBodycamRigComponent* GetBodycamRig() const { return BodycamRig; }
	/** Returns Pistol subobject **/
	UBodycamPistolComponent* GetPistol() const { return Pistol; }

};

