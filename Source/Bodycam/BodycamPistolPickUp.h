// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamInteractable.h"
#include "BodycamPistolPickUp.generated.h"

class ABodycamCharacter;
class UStaticMeshComponent;

/** Pistol lying in the world. Interacting with it equips the character's pistol and removes this actor. */
UCLASS()
class BODYCAM_API ABodycamPistolPickUp : public AActor, public IBodycamInteractable
{
	GENERATED_BODY()

public:
	ABodycamPistolPickUp();

	virtual void Interact_Implementation(ABodycamCharacter* Interactor) override;

private:
	/** What the pistol looks like on the ground. Also what the interact trace hits. */
	UPROPERTY(VisibleAnywhere, Category="PickUp")
	TObjectPtr<UStaticMeshComponent> DisplayMesh;
};
