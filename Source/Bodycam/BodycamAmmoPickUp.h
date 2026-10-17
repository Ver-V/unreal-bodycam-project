// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamInteractable.h"
#include "BodycamAmmoPickUp.generated.h"

class ABodycamCharacter;
class USoundBase;
class UStaticMeshComponent;

/** Box of pistol rounds. Interacting adds them to the spare ammo. */
UCLASS()
class BODYCAM_API ABodycamAmmoPickUp : public AActor, public IBodycamInteractable
{
	GENERATED_BODY()

public:
	ABodycamAmmoPickUp();

	virtual void Interact_Implementation(ABodycamCharacter* Interactor) override;

	/** Rounds added to the pistol's reserve (17 = one magazine) */
	UPROPERTY(EditAnywhere, Category="PickUp", meta=(ClampMin="1"))
	int32 Rounds = 17;

	UPROPERTY(EditAnywhere, Category="PickUp")
	TObjectPtr<USoundBase> PickUpSound;

private:
	/** What the box looks like. Also what the interact trace hits. */
	UPROPERTY(VisibleAnywhere, Category="PickUp")
	TObjectPtr<UStaticMeshComponent> DisplayMesh;
};
