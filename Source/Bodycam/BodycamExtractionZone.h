// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamExtractionZone.generated.h"

class UBoxComponent;

/** Area outside the exit door. The player walking into it escapes and the run ends. */
UCLASS()
class BODYCAM_API ABodycamExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	ABodycamExtractionZone();

private:
	UFUNCTION()
	void OnEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category="Extraction")
	TObjectPtr<UBoxComponent> Zone;
};
