// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamZombie.h"
#include "BodycamCharacter.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Animation/AnimSequence.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/StrongObjectPtr.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"

ABodycamZombie::ABodycamZombie()
{
	PrimaryActorTick.bCanEverTick = true;

	// A plain AIController is enough: no behavior tree, Tick decides chase/attack
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Slim capsule so zombies fit through narrow doorways (height left at the Character default 88,
	// so the mesh offset set in BP_Zombie still lines up). The NavMesh agent radius must not be larger than this.
	GetCapsuleComponent()->SetCapsuleRadius(28.f);
	GetCharacterMovement()->NavAgentProps.AgentRadius = 28.f;

	// Turn toward where it walks, not where a controller looks
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 180.f, 0.f);

	// Let shots pass the capsule and land on the mesh's physics bodies, so we know which bone was hit.
	// The capsule still blocks movement; the mesh needs a Physics Asset for this to work.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Cheap animation savings: skip posing zombies nobody can see (attacks are timer-driven, not anim notifies),
	// and update distant ones less often
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	GetMesh()->bEnableUpdateRateOptimizations = true;

	IdleAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("IdleAudio"));
	IdleAudio->SetupAttachment(GetMesh());
	IdleAudio->bAutoActivate = false;
}

void ABodycamZombie::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	GetCharacterMovement()->MaxWalkSpeed = bPatrol ? PatrolSpeed : WalkSpeed;

	// Patrol around where it was placed; first stroll at a random moment so zombies don't all set off together
	HomeLocation = GetActorLocation();
	NextPatrolTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.f, PatrolWait.Y);

	// Sequences are played straight on the mesh; no Anim Blueprint needed
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	PlayAnim(IdleAnim, true);

	// Distance falloff for every zombie sound: full volume up close, silent past InnerRadius + FalloffDistance.
	// Reverb grows with distance (far = more echo) but is capped, or distant zombies stay loud through the room reverb.
	Attenuation = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
	Settings.bAttenuate = true;
	Settings.bSpatialize = true;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	Settings.AttenuationShapeExtents = FVector(SoundInnerRadius, 0.f, 0.f);
	Settings.FalloffDistance = SoundFalloffDistance;
	// Natural (dB-based) falloff sounds like real distance; Silent = nothing at all past the falloff
	Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Settings.FalloffMode = ENaturalSoundFalloffMode::Silent;
	Settings.bEnableReverbSend = true;
	Settings.ReverbSendMethod = EReverbSendMethod::Linear;
	Settings.ReverbWetLevelMin = 0.05f;
	Settings.ReverbWetLevelMax = SoundMaxReverb;
	Settings.ReverbDistanceMin = SoundInnerRadius;
	Settings.ReverbDistanceMax = SoundInnerRadius + SoundFalloffDistance;

	IdleAudio->AttenuationSettings = Attenuation;
	IdleAudio->SetVolumeMultiplier(SoundVolume);
	IdleAudio->ConcurrencySet.Add(GetVoiceConcurrency());

	if (IdleSound || ChaseSound)
	{
		ScheduleGroan();
	}
}

USoundConcurrency* ABodycamZombie::GetVoiceConcurrency() const
{
	// One object for all zombies: a concurrency limit counts every sound that uses the same object
	static TStrongObjectPtr<USoundConcurrency> Shared;
	if (!Shared.IsValid())
	{
		Shared.Reset(NewObject<USoundConcurrency>(GetTransientPackage()));
		Shared->Concurrency.MaxCount = MaxZombieVoices;
		Shared->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
	}
	return Shared.Get();
}

USoundConcurrency* ABodycamZombie::GetFootstepConcurrency() const
{
	static TStrongObjectPtr<USoundConcurrency> Shared;
	if (!Shared.IsValid())
	{
		Shared.Reset(NewObject<USoundConcurrency>(GetTransientPackage()));
		Shared->Concurrency.MaxCount = MaxZombieFootsteps;
		Shared->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
	}
	return Shared.Get();
}

void ABodycamZombie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		return;
	}

	// The player pawn may spawn after us, so look it up until found
	if (Target == nullptr)
	{
		Target = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Target == nullptr)
		{
			return;
		}
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float Distance = ToTarget.Size();

	if (!bAggro)
	{
		UpdatePatrol();
		UpdateLocomotion(DeltaSeconds);

		const float Now = GetWorld()->GetTimeSeconds();
		if (Now < NextSenseTime)
		{
			return;
		}
		NextSenseTime = Now + 0.2f;

		if (CanSenseTarget(ToTarget))
		{
			Aggro(); // the chase starts next frame
		}
		return;
	}

	if (bAttacking)
	{
		// Keep facing the player during the swipe, otherwise sidestepping makes it miss comically
		FRotator Look = ToTarget.Rotation();
		Look.Pitch = 0.f;
		Look.Roll = 0.f;
		SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Look, DeltaSeconds, 360.f));
		return;
	}

	AAIController* AI = Cast<AAIController>(GetController());
	if (Distance <= AttackRange)
	{
		if (AI)
		{
			AI->StopMovement();
		}
		if (GetWorld()->GetTimeSeconds() >= NextAttackTime)
		{
			StartAttack();
		}
		else
		{
			PlayAnim(IdleAnim, true);
		}
		return;
	}

	// Re-request only when not already walking; MoveToActor keeps tracking the moving player by itself
	if (AI && AI->GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		AI->MoveToActor(Target, AttackRange * 0.7f);
	}

	UpdateLocomotion(DeltaSeconds);
}

void ABodycamZombie::UpdatePatrol()
{
	AAIController* AI = Cast<AAIController>(GetController());
	if (!bPatrol || AI == nullptr)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (AI->GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}

	// Just arrived: stand around for a while before the next stroll
	if (bPatrolMoving)
	{
		bPatrolMoving = false;
		NextPatrolTime = Now + FMath::FRandRange(PatrolWait.X, PatrolWait.Y);
	}
	if (Now < NextPatrolTime)
	{
		return;
	}

	// Any NavMesh point within PatrolRadius of where it started, so each zombie haunts its own area
	FNavLocation Point;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (Nav && Nav->GetRandomReachablePointInRadius(HomeLocation, PatrolRadius, Point))
	{
		AI->MoveToLocation(Point.Location, 30.f);
		bPatrolMoving = true;
	}
	else
	{
		NextPatrolTime = Now + 2.f; // no NavMesh here; try again later
	}
}

void ABodycamZombie::UpdateLocomotion(float DeltaSeconds)
{
	const float GroundSpeed = GetVelocity().Size2D();
	const bool bWalking = GroundSpeed > 10.f;
	PlayAnim(bWalking ? WalkAnim : IdleAnim, true);

	// Footsteps by distance walked, matched to the walk cycle's stride
	if (bWalking && FootstepSound)
	{
		const float StepDistance = FootstepDistance > 0.f
			? FootstepDistance
			: WalkAnimSpeed * (WalkAnim ? WalkAnim->GetPlayLength() : 1.f) * 0.5f;

		FootstepTravel += GroundSpeed * DeltaSeconds;
		if (FootstepTravel >= StepDistance)
		{
			FootstepTravel -= StepDistance;
			UGameplayStatics::PlaySoundAtLocation(this, FootstepSound, GetMesh()->GetComponentLocation(),
				FootstepVolume, FMath::FRandRange(0.9f, 1.1f), 0.f, Attenuation, GetFootstepConcurrency());
		}
	}
}

bool ABodycamZombie::CanSenseTarget(const FVector& ToTarget) const
{
	const float Distance = ToTarget.Size();

	// Close by: noticed no matter where we're facing. Crouching lets the player get much closer.
	const ACharacter* TargetCharacter = Cast<ACharacter>(Target);
	const bool bTargetCrouched = TargetCharacter && TargetCharacter->bIsCrouched;
	if (Distance <= (bTargetCrouched ? SenseRadiusCrouched : SenseRadius))
	{
		return true;
	}
	if (Distance > SightRange)
	{
		return false;
	}

	// Inside the view cone? (compare the angle via dot product: cos(half angle))
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SightAngle * 0.5f));
	if (FVector::DotProduct(GetActorForwardVector(), ToTarget / Distance) < CosHalfAngle)
	{
		return false;
	}

	// Nothing solid between our eyes and the player's
	FVector EyeLocation;
	FRotator EyeRotation;
	GetActorEyesViewPoint(EyeLocation, EyeRotation);
	FVector TargetEyes;
	Target->GetActorEyesViewPoint(TargetEyes, EyeRotation);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSight), false, this);
	Params.AddIgnoredActor(Target);
	return !GetWorld()->LineTraceTestByChannel(EyeLocation, TargetEyes, ECC_Visibility, Params);
}

void ABodycamZombie::Aggro()
{
	if (bAggro || bDead)
	{
		return;
	}
	bAggro = true;

	// Drop the stroll and speed up to the chase pace. Stopping the patrol move matters:
	// the chase only re-targets when no move is running.
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	if (CurrentAnim == WalkAnim)
	{
		GetMesh()->SetPlayRate(WalkSpeed / WalkAnimSpeed);
	}

	// Shriek the moment it notices you (sight, footsteps, gunshot or being hit), and the player reacts
	PlaySoundHere(AlertSound, GetVoiceConcurrency());
	if (ABodycamCharacter* Player = Cast<ABodycamCharacter>(Target ? Target.Get() : UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->OnSpottedByZombie();
	}
}

void ABodycamZombie::AlertGunshot(const UObject* WorldContext, const FVector& Location)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		return;
	}
	for (TActorIterator<ABodycamZombie> It(World); It; ++It)
	{
		ABodycamZombie* Zombie = *It;
		if (Zombie->bDead || Zombie->bAggro)
		{
			continue;
		}

		const float Distance = FVector::Dist(Zombie->GetActorLocation(), Location);
		if (Distance > Zombie->GunshotHearingRange)
		{
			continue; // too far even in the open; skip the trace
		}

		// Walls muffle the shot: something solid between us cuts the range down
		FVector Ears;
		FRotator Unused;
		Zombie->GetActorEyesViewPoint(Ears, Unused);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieHearing), false, Zombie);
		Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(World, 0));
		const bool bMuffled = World->LineTraceTestByChannel(Ears, Location, ECC_Visibility, Params);

		const float Range = Zombie->GunshotHearingRange * (bMuffled ? Zombie->GunshotThroughWallFactor : 1.f);
		if (Distance <= Range)
		{
			Zombie->Aggro();
		}
	}
}

void ABodycamZombie::StartAttack()
{
	bAttacking = true;
	PlaySoundHere(AttackSound);

	// Clear first so a second swipe restarts the animation from the beginning
	CurrentAnim = nullptr;
	PlayAnim(AttackAnim, false);

	const float Length = AttackAnim ? AttackAnim->GetPlayLength() : 1.f;
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(AttackHitTimer, this, &ABodycamZombie::AttackHit, FMath::Min(AttackHitTime, Length), false);
	Timers.SetTimer(AttackEndTimer, this, &ABodycamZombie::EndAttack, Length, false);
}

void ABodycamZombie::AttackHit()
{
	// A little extra reach: the player backing off at the last moment still gets clipped
	if (bDead || Target == nullptr || FVector::Dist(Target->GetActorLocation(), GetActorLocation()) > AttackRange * 1.3f)
	{
		return;
	}
	UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
}

void ABodycamZombie::EndAttack()
{
	bAttacking = false;
	NextAttackTime = GetWorld()->GetTimeSeconds() + AttackCooldown;
}

float ABodycamZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead)
	{
		return 0.f;
	}

	float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Point damage (the pistol) carries the hit result: which bone, and from which direction
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		LastHitBone = PointEvent.HitInfo.BoneName;
		LastHitDirection = PointEvent.ShotDirection;

		if (LastHitBone.ToString().Contains(TEXT("head")))
		{
			Damage *= HeadshotMultiplier;
		}
	}

	Health -= Damage;
	if (Health <= 0.f)
	{
		Die();

		// The pistol passes the shooter as the damage causer
		if (ABodycamCharacter* Killer = Cast<ABodycamCharacter>(DamageCauser))
		{
			Killer->OnZombieKilled();
		}
	}
	else if (!bAggro)
	{
		// Shot from the dark wakes it up: the alert shriek doubles as the pain sound so the two don't overlap.
		// A one-shot kill from stealth stays silent apart from the death sound.
		Aggro();
	}
	else
	{
		PlaySoundHere(HurtSound);
	}
	return Damage;
}

void ABodycamZombie::Die()
{
	bDead = true;

	GetWorldTimerManager().ClearTimer(AttackHitTimer);
	GetWorldTimerManager().ClearTimer(AttackEndTimer);
	GetWorldTimerManager().ClearTimer(GroanTimer);
	IdleAudio->Stop();
	PlaySoundHere(DeathSound);

	// Stop thinking and moving; the AI controller is let go
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
	DetachFromControllerPendingDestroy();

	// The capsule would hold the ragdoll upright and block the player; drop it
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Hand the body to physics and push the hit bone along the shot
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	if (!LastHitDirection.IsNearlyZero())
	{
		Body->AddImpulse(LastHitDirection * DeathImpulse, LastHitBone, true);
	}

	if (CorpseLifetime > 0.f)
	{
		SetLifeSpan(CorpseLifetime);
	}
}

void ABodycamZombie::PlayAnim(UAnimSequence* Anim, bool bLoop)
{
	if (Anim == nullptr || Anim == CurrentAnim)
	{
		return;
	}
	CurrentAnim = Anim;
	GetMesh()->PlayAnimation(Anim, bLoop);

	// Speed the walk cycle up with the move speed so the feet keep up with the ground
	GetMesh()->SetPlayRate(Anim == WalkAnim ? GetCharacterMovement()->MaxWalkSpeed / WalkAnimSpeed : 1.f);
}

void ABodycamZombie::ScheduleGroan()
{
	// Random gaps so a group of zombies never groans in unison
	const FVector2D Interval = bAggro ? ChaseSoundInterval : IdleSoundInterval;
	GetWorldTimerManager().SetTimer(GroanTimer, this, &ABodycamZombie::Groan, FMath::FRandRange(Interval.X, Interval.Y), false);
}

void ABodycamZombie::Groan()
{
	// Skip a groan while swiping so it doesn't talk over the attack sound
	if (!bAttacking)
	{
		IdleAudio->SetSound(bAggro && ChaseSound ? ChaseSound.Get() : IdleSound.Get());
		IdleAudio->Play();
	}
	ScheduleGroan();
}

void ABodycamZombie::PlaySoundHere(USoundBase* Sound, USoundConcurrency* Concurrency) const
{
	// Attack, hurt and death sounds pass no concurrency: they always play
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), SoundVolume, 1.f, 0.f, Attenuation, Concurrency);
	}
}
