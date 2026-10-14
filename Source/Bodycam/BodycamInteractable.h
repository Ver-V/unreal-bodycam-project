// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BodycamInteractable.generated.h"

class ABodycamCharacter;

UINTERFACE(MinimalAPI, BlueprintType)
class UBodycamInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything the player can use with the Interact key (pickups, doors, extraction points...).
 * Implement in C++ by overriding Interact_Implementation, or in Blueprint via Class Settings > Interfaces.
 */
class BODYCAM_API IBodycamInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void Interact(ABodycamCharacter* Interactor);
};
