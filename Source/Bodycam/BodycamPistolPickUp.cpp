// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamPistolPickUp.h"
#include "BodycamCharacter.h"
#include "BodycamPistolComponent.h"
#include "Components/StaticMeshComponent.h"

ABodycamPistolPickUp::ABodycamPistolPickUp()
{
	DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
	RootComponent = DisplayMesh;

	// Only the interact trace (Visibility) should notice it; the player walks through it
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DisplayMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	DisplayMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ABodycamPistolPickUp::Interact_Implementation(ABodycamCharacter* Interactor)
{
	UBodycamPistolComponent* Pistol = Interactor ? Interactor->GetPistol() : nullptr;
	if (Pistol == nullptr || Pistol->IsEquipped())
	{
		return;
	}

	Pistol->Equip();
	Destroy();
}
