// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamDoor.h"
#include "BodycamCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

ABodycamDoor::ABodycamDoor()
{
	// Only ticks while swinging
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(SceneRoot);

	// Solid door: blocks the player and zombies, and the interact trace (Visibility) lands on it.
	// It doesn't carve the NavMesh, so zombies still path through doorways; a closed door just physically stops them.
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
	DoorMesh->SetCanEverAffectNavigation(false);

	ApproachTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ApproachTrigger"));
	ApproachTrigger->SetupAttachment(SceneRoot);
	ApproachTrigger->SetBoxExtent(FVector(120.f, 100.f, 100.f));
	ApproachTrigger->SetRelativeLocation(FVector(-120.f, 0.f, 100.f));
	ApproachTrigger->SetCollisionProfileName(TEXT("Trigger"));
	// Triggers carve a hole in the NavMesh by default, which cut the doorway off for zombies
	ApproachTrigger->SetCanEverAffectNavigation(false);
	ApproachTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABodycamDoor::OnApproach);
}

void ABodycamDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// The hinge's turn is fully decided here, so every editor rebuild gives the same picture (no flip-flopping)
	Hinge->SetRelativeRotation(FRotator(0.f, bPreviewOpen ? OpenAngle * (bReverseSwing ? -1.f : 1.f) : 0.f, 0.f));

	UStaticMesh* Mesh = DoorMesh->GetStaticMesh();
	if (!bAutoHinge || Mesh == nullptr)
	{
		return;
	}

	// Hinge at the actor's pivot; slide the door so one of its vertical edges lands on it.
	// The door's width runs along whichever horizontal axis of the mesh is longer; the other is its thickness.
	Hinge->SetRelativeLocation(FVector::ZeroVector);

	// Bounds as the door actually sits under the hinge: the DoorMesh's own rotation and scale applied
	// (so turning the mesh to stand it up the right way doesn't throw the hinge off)
	const FBox Bounds = Mesh->GetBoundingBox().TransformBy(
		FTransform(DoorMesh->GetRelativeRotation(), FVector::ZeroVector, DoorMesh->GetRelativeScale3D()));
	const FVector Min = Bounds.Min;
	const FVector Max = Bounds.Max;
	const FVector Center = Bounds.GetCenter();
	const bool bWidthAlongY = (Max.Y - Min.Y) >= (Max.X - Min.X);

	FVector Offset = FVector::ZeroVector;
	if (bWidthAlongY)
	{
		Offset.X = -Center.X;                          // center the thickness on the hinge line
		Offset.Y = -(bHingeOnRight ? Max.Y : Min.Y);   // chosen edge onto the pivot
	}
	else
	{
		Offset.Y = -Center.Y;
		Offset.X = -(bHingeOnRight ? Max.X : Min.X);
	}
	Offset.Z = DoorMesh->GetRelativeLocation().Z; // keep whatever height was set
	DoorMesh->SetRelativeLocation(Offset);
}

void ABodycamDoor::BeginPlay()
{
	Super::BeginPlay();

	// Always start closed, even if Preview Open was left ticked in the editor
	Hinge->SetRelativeRotation(FRotator::ZeroRotator);
	ClosedYaw = 0.f;

	// Plain doors have no use for the trigger
	if (!bLockedExit)
	{
		ApproachTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABodycamDoor::OnApproach(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ABodycamCharacter* Player = Cast<ABodycamCharacter>(OtherActor))
	{
		if (bLockedExit && !bOpen)
		{
			Player->NotifyExitLocked();
		}
	}
}

void ABodycamDoor::Interact_Implementation(ABodycamCharacter* Interactor)
{
	if (Interactor == nullptr || IsActorTickEnabled())
	{
		return; // still swinging
	}

	if (bOpen)
	{
		// The exit stays open once unlocked; other doors close again
		if (!bLockedExit)
		{
			bOpen = false;
			SwingTo(0.f, CloseSound);
		}
		return;
	}

	if (bLockedExit && !Interactor->HasKey())
	{
		Interactor->NotifyExitLocked();
		if (LockedSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, LockedSound, DoorMesh->GetComponentLocation());
		}
		return;
	}

	// Open away from the player: which side of the door are they on?
	const FVector ToPlayer = Interactor->GetActorLocation() - Hinge->GetComponentLocation();
	float Side = FVector::DotProduct(ToPlayer, SceneRoot->GetForwardVector()) >= 0.f ? -1.f : 1.f;
	if (bReverseSwing)
	{
		Side = -Side;
	}

	bOpen = true;
	SwingTo(OpenAngle * Side, OpenSound);
}

void ABodycamDoor::SwingTo(float TargetYawOffset, USoundBase* Sound)
{
	SwingFrom = Hinge->GetRelativeRotation().Yaw - ClosedYaw;
	SwingTarget = TargetYawOffset;
	SwingTime = 0.f;
	SetActorTickEnabled(true);

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, DoorMesh->GetComponentLocation());
	}
}

void ABodycamDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SwingTime += DeltaSeconds;
	const float T = FMath::Clamp(SwingTime / SwingDuration, 0.f, 1.f);

	// Ease in and out so the heavy door starts and stops gently
	FRotator Rotation = Hinge->GetRelativeRotation();
	Rotation.Yaw = ClosedYaw + FMath::Lerp(SwingFrom, SwingTarget, FMath::SmoothStep(0.f, 1.f, T));
	Hinge->SetRelativeRotation(Rotation);

	if (T >= 1.f)
	{
		SetActorTickEnabled(false);
	}
}
