// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamZombieSpawnPoint.generated.h"

class ABodycamZombie;
class UArrowComponent;
class UCapsuleComponent;

/** Story moments that can bring in more zombies */
UENUM(BlueprintType)
enum class EBodycamSpawnEvent : uint8
{
	PistolPickedUp,
	KeyPickedUp
};

/**
 * Spot where a zombie appears when a story event happens (e.g. picking up the key fills the corridor behind you).
 * Place it out of the player's line of sight; the arrow is the direction the zombie faces.
 */
UCLASS()
class BODYCAM_API ABodycamZombieSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ABodycamZombieSpawnPoint();

	/** Fires every spawn point in the world listening for Event */
	static void TriggerEvent(const UObject* WorldContext, EBodycamSpawnEvent Event);

	/** Which moment brings this zombie in */
	UPROPERTY(EditAnywhere, Category="Spawn")
	EBodycamSpawnEvent SpawnOn = EBodycamSpawnEvent::KeyPickedUp;

	/** Usually BP_Zombie */
	UPROPERTY(EditAnywhere, Category="Spawn")
	TSubclassOf<ABodycamZombie> ZombieClass;

	/** Starts hunting the player right away instead of wandering until it notices them */
	UPROPERTY(EditAnywhere, Category="Spawn")
	bool bHuntOnSpawn = true;

	/** Seconds after the event before it appears, to stagger a group */
	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ClampMin="0"))
	float Delay = 0.f;

private:
	void Spawn();

#if WITH_EDITORONLY_DATA
	/** Zombie-sized outline so it's easy to place in the editor */
	UPROPERTY(VisibleAnywhere, Category="Spawn")
	TObjectPtr<UCapsuleComponent> Preview;

	UPROPERTY(VisibleAnywhere, Category="Spawn")
	TObjectPtr<UArrowComponent> Arrow;
#endif

	bool bUsed = false;
	FTimerHandle DelayTimer;
};
