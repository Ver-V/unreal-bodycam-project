// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Styling/SlateColor.h"
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

	/** How far ahead (cm) the player can reach to interact, measured from the chest camera */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Interaction, meta = (AllowPrivateAccess = "true"))
	float InteractDistance = 250.f;

	/** Thickness (cm) of the interact sweep. There is no crosshair, so be forgiving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Interaction, meta = (AllowPrivateAccess = "true"))
	float InteractRadius = 25.f;

	/** If the traces miss, anything interactable within this angle (degrees) of the screen center is picked, closest to center first */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Interaction, meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "60"))
	float InteractAngle = 25.f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Health, meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	float MaxHealth = 100.f;

	/** Played when a zombie lands a hit. Always plays, cutting off any other voice line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice, meta = (AllowPrivateAccess = "true"))
	USoundBase* HurtSound;

	/** Played when a zombie notices the player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice, meta = (AllowPrivateAccess = "true"))
	USoundBase* SpottedSound;

	/** Played shortly after killing a zombie */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice, meta = (AllowPrivateAccess = "true"))
	USoundBase* KillSound;

	/** Minimum seconds between spotted/kill lines, so a group of zombies doesn't make the character chatter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice, meta = (AllowPrivateAccess = "true"))
	float VoiceCooldown = 8.f;

	/** One footstep. Pick a Sound Cue with a few variations if you have them; pitch is also varied a little per step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Footsteps, meta = (AllowPrivateAccess = "true"))
	USoundBase* FootstepSound;

	/** Distance (cm) walked per footstep. Half of the bodycam rig's StrideLength keeps steps on the camera bob's dips. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Footsteps, meta = (AllowPrivateAccess = "true", ClampMin = "10"))
	float FootstepDistance = 70.f;

	/** Footstep volume while crouching (sneaking is quieter) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Footsteps, meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "1"))
	float CrouchFootstepVolume = 0.35f;

	/** Heavy breathing that fades in while standing still. Turn the sound's Looping on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice, meta = (AllowPrivateAccess = "true"))
	USoundBase* BreathingSound;

	/** Breathing volume at full health; it gets louder as health drops */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice, meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "2"))
	float BreathingVolume = 0.5f;

	/** Seconds of standing still before the breathing fades in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice, meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float BreathingDelay = 1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Voice, meta = (AllowPrivateAccess = "true"))
	class UAudioComponent* BreathingAudio;

	/** The character's own voice: one line at a time, not spatialized */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Voice, meta = (AllowPrivateAccess = "true"))
	class UAudioComponent* VoiceAudio;

	// --- Objectives (shown briefly on the HUD whenever they change) ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective, meta = (AllowPrivateAccess = "true"))
	FText ObjectivePickUpGun;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective, meta = (AllowPrivateAccess = "true"))
	FText ObjectiveEscape;

	/** Shown when the player reaches the locked exit without the key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective, meta = (AllowPrivateAccess = "true"))
	FText ObjectiveFindKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective, meta = (AllowPrivateAccess = "true"))
	FText ObjectiveUnlockExit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective, meta = (AllowPrivateAccess = "true"))
	FText EscapedText;

	/** Seconds an objective stays fully visible before fading out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective, meta = (AllowPrivateAccess = "true"))
	float ObjectiveShowTime = 6.f;

	/** Seconds after dying before the screen starts fading to black (the fall itself takes about 1.4s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Health, meta = (AllowPrivateAccess = "true"))
	float DeathFadeDelay = 1.8f;

	/** Seconds the fade to black takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Health, meta = (AllowPrivateAccess = "true"))
	float DeathFadeDuration = 1.5f;

	/** Seconds after dying before the level restarts. Keep it after DeathFadeDelay + DeathFadeDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Health, meta = (AllowPrivateAccess = "true"))
	float RestartDelay = 4.f;

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

	float Health = 0.f;
	bool bDead = false;
	FTimerHandle RestartTimer;
	FTimerHandle DeathFadeTimer;

	void Die();

	/** Disables input, fades to black after FadeDelay, restarts the level after RestartDelay */
	void EndRun(float FadeDelay);

	FText ObjectiveText;
	/** Real time the current objective was set, drives the HUD fade */
	float ObjectiveSetTime = -1000.f;

	bool bHasKey = false;
	bool bEscaped = false;

	/** Plays a voice line. Unless bForce, skipped while VoiceCooldown hasn't passed since the last one. */
	void PlayVoice(USoundBase* Sound, bool bForce);

	float LastVoiceTime = -1000.f;
	FTimerHandle VoiceTimer;

	/** Ground distance walked since the last footstep */
	float FootstepTravel = 0.f;

	/** How long the player has been standing still, for the breathing */
	float StillTime = 0.f;
	bool bBreathing = false;

	void UpdateBreathing(float DeltaSeconds);

public:
	/** Called by the pistol when it is picked up: moves the flashlight from the chest to the muzzle and moves on to the escape objective */
	void OnPistolEquipped();

	/** Shows a new objective line on the HUD (fades in, stays ObjectiveShowTime, fades out) */
	UFUNCTION(BlueprintCallable, Category = Objective)
	void SetObjective(const FText& Text);

	UFUNCTION(BlueprintPure, Category = Objective)
	FText GetObjectiveText() const { return ObjectiveText; }

	/** Current fade of the objective line: 0 = hidden, 1 = fully shown */
	UFUNCTION(BlueprintPure, Category = Objective)
	float GetObjectiveAlpha() const;

	/** White with the current fade as alpha; bind the HUD text's Color and Opacity to this */
	UFUNCTION(BlueprintPure, Category = Objective)
	FSlateColor GetObjectiveColor() const;

	/** Black with the same fade; bind the HUD text's Shadow Color to this, or the shadow stays behind after the text fades */
	UFUNCTION(BlueprintPure, Category = Objective)
	FLinearColor GetObjectiveShadowColor() const;

	UFUNCTION(BlueprintPure, Category = Objective)
	bool HasKey() const { return bHasKey; }

	/** Picked up the exit key */
	void GiveKey();

	/** Reached (or tried) the locked exit; prompts for the key if we don't have it */
	void NotifyExitLocked();

	/** Made it out of the building */
	void Escape();

	/** A zombie just noticed us */
	void OnSpottedByZombie();

	/** We just killed a zombie */
	void OnZombieKilled();

	/** Bodycam overlay clock, e.g. "2026-10-16 23:47:12". Always night: starts between 22:00 and 04:00 and runs in real time. */
	UFUNCTION(BlueprintPure, Category = Bodycam)
	FText GetBodycamTimestamp() const;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = Health)
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = Health)
	bool IsDead() const { return bDead; }

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

