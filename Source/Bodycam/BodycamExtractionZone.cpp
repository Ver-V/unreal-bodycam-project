// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamExtractionZone.h"
#include "BodycamCharacter.h"
#include "Components/BoxComponent.h"

ABodycamExtractionZone::ABodycamExtractionZone()
{
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	RootComponent = Zone;
	Zone->SetBoxExtent(FVector(150.f, 150.f, 100.f));
	Zone->SetCollisionProfileName(TEXT("Trigger"));
	Zone->SetCanEverAffectNavigation(false); // a trigger must not carve the NavMesh
	Zone->OnComponentBeginOverlap.AddDynamic(this, &ABodycamExtractionZone::OnEnter);

	// Visible in the editor so it can be placed, hidden in game
	Zone->SetHiddenInGame(true);
	Zone->ShapeColor = FColor::Green;
}

void ABodycamExtractionZone::OnEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ABodycamCharacter* Player = Cast<ABodycamCharacter>(OtherActor))
	{
		Player->Escape();
	}
}
