// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamAmmoPickUp.h"
#include "BodycamCharacter.h"
#include "BodycamPistolComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ABodycamAmmoPickUp::ABodycamAmmoPickUp()
{
	DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
	RootComponent = DisplayMesh;

	// Same as the other pickups: only the interact trace (Visibility) notices it
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DisplayMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	DisplayMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	DisplayMesh->SetCanEverAffectNavigation(false); // small pickup, zombies walk over it
}

void ABodycamAmmoPickUp::Interact_Implementation(ABodycamCharacter* Interactor)
{
	UBodycamPistolComponent* Pistol = Interactor ? Interactor->GetPistol() : nullptr;
	if (Pistol == nullptr)
	{
		return;
	}

	Pistol->AddReserveAmmo(Rounds);
	if (PickUpSound)
	{
		UGameplayStatics::PlaySound2D(this, PickUpSound);
	}
	Destroy();
}
