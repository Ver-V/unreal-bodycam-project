// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamZombieSpawnPoint.h"
#include "BodycamZombie.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ABodycamZombieSpawnPoint::ABodycamZombieSpawnPoint()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	// Editor-only helpers: a zombie-sized capsule standing on the floor, and an arrow for facing
	Preview = CreateEditorOnlyDefaultSubobject<UCapsuleComponent>(TEXT("Preview"));
	if (Preview)
	{
		Preview->SetupAttachment(RootComponent);
		Preview->InitCapsuleSize(42.f, 96.f);
		Preview->SetRelativeLocation(FVector(0.f, 0.f, 96.f));
		Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Preview->ShapeColor = FColor::Red;
		Preview->bHiddenInGame = true;
	}

	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(RootComponent);
		Arrow->SetRelativeLocation(FVector(0.f, 0.f, 96.f));
		Arrow->ArrowColor = FColor::Red;
	}
#endif
}

void ABodycamZombieSpawnPoint::TriggerEvent(const UObject* WorldContext, EBodycamSpawnEvent Event)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ABodycamZombieSpawnPoint> It(World); It; ++It)
	{
		ABodycamZombieSpawnPoint* Point = *It;
		if (Point->bUsed || Point->SpawnOn != Event)
		{
			continue;
		}

		Point->bUsed = true;
		if (Point->Delay > 0.f)
		{
			World->GetTimerManager().SetTimer(Point->DelayTimer, Point, &ABodycamZombieSpawnPoint::Spawn, Point->Delay, false);
		}
		else
		{
			Point->Spawn();
		}
	}
}

void ABodycamZombieSpawnPoint::Spawn()
{
	if (ZombieClass == nullptr)
	{
		return;
	}

	// The root sits on the floor; the zombie's origin is its capsule center, so lift it by the half height
	const float HalfHeight = ZombieClass->GetDefaultObject<ABodycamZombie>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Location = GetActorLocation() + FVector(0.f, 0.f, HalfHeight);
	const FRotator Rotation(0.f, GetActorRotation().Yaw, 0.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// AutoPossessAI on the zombie gives it its AI controller as it spawns
	ABodycamZombie* Zombie = GetWorld()->SpawnActor<ABodycamZombie>(ZombieClass, Location, Rotation, Params);
	if (Zombie && bHuntOnSpawn)
	{
		Zombie->Aggro();
	}
}
