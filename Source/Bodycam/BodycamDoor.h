// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamInteractable.h"
#include "BodycamDoor.generated.h"

class ABodycamCharacter;
class UBoxComponent;
class USoundBase;
class UStaticMeshComponent;

/**
 * Hinged door used with the Interact key (E). Swings open away from the player, E again closes it.
 *
 * Tick "Locked Exit" for the building's exit: it won't open without the key, and walking up to it
 * (or trying it) without the key changes the objective to "find the key".
 *
 * Setup: move the Hinge component to the door's hinge edge and the DoorMesh so it hangs off it.
 * The ApproachTrigger only matters on the locked exit; size it to cover the floor in front of the door.
 */
UCLASS()
class BODYCAM_API ABodycamDoor : public AActor, public IBodycamInteractable
{
	GENERATED_BODY()

public:
	ABodycamDoor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Interact_Implementation(ABodycamCharacter* Interactor) override;

	/** Editor only: show the door swung open to check the hinge. The game always starts with it closed. */
	UPROPERTY(EditAnywhere, Category="Door")
	bool bPreviewOpen = false;

	/** The building exit: needs the key, and drives the find-the-key objective */
	UPROPERTY(EditAnywhere, Category="Door")
	bool bLockedExit = false;

	/** Degrees the door swings open. It always opens away from whoever opens it. */
	UPROPERTY(EditAnywhere, Category="Door", meta=(ClampMin="0", ClampMax="180"))
	float OpenAngle = 95.f;

	/**
	 * Hang the door off its edge automatically: the DoorMesh is shifted so its edge sits on the actor's pivot.
	 * Then just place the actor's pivot where the hinges are on the frame. Off = position Hinge/DoorMesh by hand.
	 */
	UPROPERTY(EditAnywhere, Category="Door")
	bool bAutoHinge = false;

	/** Which edge of the door mesh (seen from its front, +X) carries the hinges */
	UPROPERTY(EditAnywhere, Category="Door", meta=(EditCondition="bAutoHinge"))
	bool bHingeOnRight = false;

	/** Tick if the door swings toward the player instead of away (depends on which edge the hinge is on) */
	UPROPERTY(EditAnywhere, Category="Door")
	bool bReverseSwing = false;

	UPROPERTY(EditAnywhere, Category="Door")
	float SwingDuration = 1.f;

	UPROPERTY(EditAnywhere, Category="Door|Sound")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, Category="Door|Sound")
	TObjectPtr<USoundBase> CloseSound;

	/** Handle rattle when the locked exit is tried without the key */
	UPROPERTY(EditAnywhere, Category="Door|Sound")
	TObjectPtr<USoundBase> LockedSound;

private:
	UFUNCTION()
	void OnApproach(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Starts swinging toward TargetYawOffset (0 = closed) */
	void SwingTo(float TargetYawOffset, USoundBase* Sound);

	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Pivot the door turns around */
	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/** Locked exit only: reaching this area without the key updates the objective */
	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<UBoxComponent> ApproachTrigger;

	bool bOpen = false;
	/** Hinge yaw when closed. The hinge is driven by code: closed = 0, rotate the whole actor to orient the door. */
	float ClosedYaw = 0.f;
	float SwingFrom = 0.f;
	float SwingTarget = 0.f;
	float SwingTime = 0.f;

protected:
	virtual void BeginPlay() override;
};
