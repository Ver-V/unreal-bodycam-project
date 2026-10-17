// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamKeyPickUp.h"
#include "BodycamCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ABodycamKeyPickUp::ABodycamKeyPickUp()
{
	DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
	RootComponent = DisplayMesh;

	// Same as the pistol pickup: only the interact trace (Visibility) notices it
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DisplayMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	DisplayMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	DisplayMesh->SetCanEverAffectNavigation(false); // small pickup, zombies walk over it
}

void ABodycamKeyPickUp::Interact_Implementation(ABodycamCharacter* Interactor)
{
	if (Interactor == nullptr || Interactor->HasKey())
	{
		return;
	}

	Interactor->GiveKey();
	if (PickUpSound)
	{
		UGameplayStatics::PlaySound2D(this, PickUpSound);
	}
	Destroy();
}
