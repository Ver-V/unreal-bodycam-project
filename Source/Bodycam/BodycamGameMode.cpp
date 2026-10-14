// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamGameMode.h"
#include "BodycamCharacter.h"
#include "UObject/ConstructorHelpers.h"

ABodycamGameMode::ABodycamGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
