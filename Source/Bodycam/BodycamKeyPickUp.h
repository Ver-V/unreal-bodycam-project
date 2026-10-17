// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamInteractable.h"
#include "BodycamKeyPickUp.generated.h"

class ABodycamCharacter;
class USoundBase;
class UStaticMeshComponent;

/** Key to the locked exit. Interacting picks it up and updates the objective. */
UCLASS()
class BODYCAM_API ABodycamKeyPickUp : public AActor, public IBodycamInteractable
{
	GENERATED_BODY()

public:
	ABodycamKeyPickUp();

	virtual void Interact_Implementation(ABodycamCharacter* Interactor) override;

	UPROPERTY(EditAnywhere, Category="PickUp")
	TObjectPtr<USoundBase> PickUpSound;

private:
	/** What the key looks like. Also what the interact trace hits. */
	UPROPERTY(VisibleAnywhere, Category="PickUp")
	TObjectPtr<UStaticMeshComponent> DisplayMesh;
};
