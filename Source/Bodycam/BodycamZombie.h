// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BodycamZombie.generated.h"

class UAnimSequence;
class UAudioComponent;
class USoundAttenuation;
class USoundConcurrency;
class USoundBase;

/**
 * Basic zombie: notices the player, shambles toward them on the NavMesh and swipes when close.
 * Takes pistol damage (headshots count extra) and collapses into a ragdoll on death.
 *
 * Animation is three plain sequences (idle / walk / attack) played directly on the mesh, no Anim Blueprint.
 * Bullets hit the skeletal mesh's physics bodies rather than the capsule, so the hit bone is known.
 */
UCLASS()
class BODYCAM_API ABodycamZombie : public ACharacter
{
	GENERATED_BODY()

public:
	ABodycamZombie();

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category="Zombie")
	bool IsDead() const { return bDead; }

	/** Start chasing right away (spawners call this; also triggered by being shot) */
	UFUNCTION(BlueprintCallable, Category="Zombie")
	void Aggro();

	/** A gun went off at Location: zombies within GunshotHearingRange start chasing */
	static void AlertGunshot(const UObject* WorldContext, const FVector& Location);

	// --- Health ---

	UPROPERTY(EditAnywhere, Category="Zombie|Health", meta=(ClampMin="1"))
	float MaxHealth = 100.f;

	/** Damage multiplier when the hit bone's name contains "head" (works for both "head" and "mixamorig:Head") */
	UPROPERTY(EditAnywhere, Category="Zombie|Health")
	float HeadshotMultiplier = 3.f;

	// --- Chase ---

	/** Wander around the spot it was placed until it notices the player. Off = stands still. */
	UPROPERTY(EditAnywhere, Category="Zombie|Patrol")
	bool bPatrol = true;

	/** How far (cm) from its starting spot it wanders */
	UPROPERTY(EditAnywhere, Category="Zombie|Patrol")
	float PatrolRadius = 600.f;

	/** Slow aimless shuffle while patrolling (cm/s) */
	UPROPERTY(EditAnywhere, Category="Zombie|Patrol")
	float PatrolSpeed = 70.f;

	/** Random pause (seconds, min/max) at each spot before moving on */
	UPROPERTY(EditAnywhere, Category="Zombie|Patrol")
	FVector2D PatrolWait = FVector2D(3.f, 8.f);

	/** Can see the player up to this far (cm), but only inside its view cone and with nothing in between */
	UPROPERTY(EditAnywhere, Category="Zombie|Chase")
	float SightRange = 1000.f;

	/** Full width of the view cone (degrees) in front of the zombie */
	UPROPERTY(EditAnywhere, Category="Zombie|Chase", meta=(ClampMin="1", ClampMax="360"))
	float SightAngle = 90.f;

	/** Notices a standing player this close (cm) even from behind or around a corner: footsteps, breathing */
	UPROPERTY(EditAnywhere, Category="Zombie|Chase")
	float SenseRadius = 500.f;

	/** Same, while the player is crouching: sneaking past is quieter */
	UPROPERTY(EditAnywhere, Category="Zombie|Chase")
	float SenseRadiusCrouched = 150.f;

	/** Hears a gunshot within this distance (cm) with a clear line to the shooter */
	UPROPERTY(EditAnywhere, Category="Zombie|Chase")
	float GunshotHearingRange = 1500.f;

	/** Fraction of GunshotHearingRange that still carries through walls (0 = walls block gunshots completely) */
	UPROPERTY(EditAnywhere, Category="Zombie|Chase", meta=(ClampMin="0", ClampMax="1"))
	float GunshotThroughWallFactor = 0.35f;

	/** Shambling speed (cm/s) */
	UPROPERTY(EditAnywhere, Category="Zombie|Chase")
	float WalkSpeed = 180.f;

	/** Speed (cm/s) at which the walk animation's feet match the ground. The walk plays faster/slower by WalkSpeed / this, so feet don't slide. */
	UPROPERTY(EditAnywhere, Category="Zombie|Chase", meta=(ClampMin="1"))
	float WalkAnimSpeed = 120.f;

	// --- Attack ---

	/** Starts a swipe when the player is within this distance (cm, capsule to capsule centers) */
	UPROPERTY(EditAnywhere, Category="Zombie|Attack")
	float AttackRange = 130.f;

	UPROPERTY(EditAnywhere, Category="Zombie|Attack")
	float AttackDamage = 25.f;

	/** Seconds into the attack animation when the hand lands. Damage only applies if the player is still in reach then. */
	UPROPERTY(EditAnywhere, Category="Zombie|Attack")
	float AttackHitTime = 1.f;

	/** Small pause after a swipe before the next one */
	UPROPERTY(EditAnywhere, Category="Zombie|Attack")
	float AttackCooldown = 0.4f;

	// --- Death ---

	/** Push (velocity change, cm/s) given to the hit bone when the zombie dies, so it falls away from the shot */
	UPROPERTY(EditAnywhere, Category="Zombie|Death")
	float DeathImpulse = 600.f;

	/** Seconds the corpse stays (0 = forever) */
	UPROPERTY(EditAnywhere, Category="Zombie|Death")
	float CorpseLifetime = 0.f;

	// --- Animation ---

	UPROPERTY(EditAnywhere, Category="Zombie|Animation")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, Category="Zombie|Animation")
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(EditAnywhere, Category="Zombie|Animation")
	TObjectPtr<UAnimSequence> AttackAnim;

	// --- Sound ---

	/** Volume multiplier for every zombie sound except footsteps */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound", meta=(ClampMin="0", ClampMax="2"))
	float SoundVolume = 0.7f;

	/** Volume multiplier for footsteps (separate so shuffling feet can stand out from the groans) */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound", meta=(ClampMin="0", ClampMax="3"))
	float FootstepVolume = 1.3f;

	// Distance falloff is built in code from these (no attenuation asset needed)

	/** Full volume within this distance (cm) */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	float SoundInnerRadius = 100.f;

	/** Fades to silence over this distance (cm) beyond SoundInnerRadius */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	float SoundFalloffDistance = 1300.f;

	/** Most room reverb a distant zombie gets. Too much and far zombies stay loud through the echo. */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound", meta=(ClampMin="0", ClampMax="1"))
	float SoundMaxReverb = 0.2f;

	/** Groans/shrieks from all zombies heard at once; extra ones stop, farthest first */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound", meta=(ClampMin="1"))
	int32 MaxZombieVoices = 3;

	/** Same for footsteps */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound", meta=(ClampMin="1"))
	int32 MaxZombieFootsteps = 4;

	/** Groan played every now and then while alive (leave the sound's Looping off). Follows the zombie as it moves. */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	TObjectPtr<USoundBase> IdleSound;

	/** Random gap (seconds, min/max) between groans while idle */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	FVector2D IdleSoundInterval = FVector2D(20.f, 25.f);

	/** Played instead of IdleSound while chasing (empty = keep using IdleSound) */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	TObjectPtr<USoundBase> ChaseSound;

	/** Random gap (seconds, min/max) between chase sounds, measured from the start of one to the next */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	FVector2D ChaseSoundInterval = FVector2D(13.f, 17.f);

	/** Played once, the moment it notices the player and starts chasing */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	TObjectPtr<USoundBase> AlertSound;

	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	TObjectPtr<USoundBase> AttackSound;

	/** One shuffling footstep. Heard from behind too: steps are timed in code, not by animation notifies (which stop off screen). */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	TObjectPtr<USoundBase> FootstepSound;

	/** Distance (cm) per footstep. 0 = work it out from the walk animation: one cycle covers WalkAnimSpeed x its length, two steps per cycle. */
	UPROPERTY(EditAnywhere, Category="Zombie|Sound", meta=(ClampMin="0"))
	float FootstepDistance = 0.f;

	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	TObjectPtr<USoundBase> HurtSound;

	UPROPERTY(EditAnywhere, Category="Zombie|Sound")
	TObjectPtr<USoundBase> DeathSound;

protected:
	virtual void BeginPlay() override;

private:
	/** True if the player is close, or in front of us with a clear line of sight */
	bool CanSenseTarget(const FVector& ToTarget) const;

	/** Before noticing the player: stroll to random spots near HomeLocation, pausing at each */
	void UpdatePatrol();

	/** Walk/idle animation and footsteps from the current ground speed */
	void UpdateLocomotion(float DeltaSeconds);

	FVector HomeLocation = FVector::ZeroVector;
	float NextPatrolTime = 0.f;
	bool bPatrolMoving = false;

	void StartAttack();
	void AttackHit();
	void EndAttack();
	void Die();

	/** Plays Anim on the mesh unless it is already playing */
	void PlayAnim(UAnimSequence* Anim, bool bLoop);
	/** Plays a one-shot at the zombie with its attenuation; Concurrency (optional) caps how many play at once */
	void PlaySoundHere(USoundBase* Sound, USoundConcurrency* Concurrency = nullptr) const;

	/** Shared by every zombie so the cap counts all of them together, not per zombie */
	USoundConcurrency* GetVoiceConcurrency() const;
	USoundConcurrency* GetFootstepConcurrency() const;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Plays one groan and schedules the next */
	void Groan();
	void ScheduleGroan();

	FTimerHandle GroanTimer;

	/** Ground distance walked since the last footstep */
	float FootstepTravel = 0.f;

	UPROPERTY(VisibleAnywhere, Category="Zombie|Sound")
	TObjectPtr<UAudioComponent> IdleAudio;

	UPROPERTY(Transient)
	TObjectPtr<APawn> Target;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentAnim;

	FTimerHandle AttackHitTimer;
	FTimerHandle AttackEndTimer;

	float Health = 0.f;
	bool bDead = false;
	bool bAggro = false;
	bool bAttacking = false;
	/** World time after which the next swipe may start (cooldown) */
	float NextAttackTime = 0.f;
	/** Sight checks trace through the level, so idle zombies only look a few times a second */
	float NextSenseTime = 0.f;

	/** Remembered from the killing shot to aim the ragdoll push */
	FName LastHitBone = NAME_None;
	FVector LastHitDirection = FVector::ZeroVector;
};
