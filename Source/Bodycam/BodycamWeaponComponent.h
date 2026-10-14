// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "BodycamWeaponComponent.generated.h"

class ABodycamCharacter;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BODYCAM_API UBodycamWeaponComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, Category=Projectile)
	TSubclassOf<class ABodycamProjectile> ProjectileClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	USoundBase* FireSound;
	
	/** How much of the shot goes into the room reverb (0..1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay, meta=(ClampMin="0", ClampMax="1"))
	float FireReverbSend = 0.6f;

	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	UAnimMontage* FireAnimation;

	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	FVector MuzzleOffset;

	/** Socket on the weapon mesh to spawn projectiles from. Falls back to MuzzleOffset if missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Degrees the aim climbs per shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Recoil)
	float AimRecoilPitch = 0.6f;

	/** Max random horizontal aim drift per shot (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Recoil)
	float AimRecoilYaw = 0.25f;

	/** Bodycam jolt per shot (degrees). Roll is randomized in +-Roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Recoil)
	FRotator CameraKick = FRotator(1.2f, 0.4f, 1.0f);

	/** Bodycam push back per shot (cm, camera space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Recoil)
	float CameraPushBack = 1.5f;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputMappingContext* FireMappingContext;

	/** Fire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputAction* FireAction;

	/** Sets default values for this component's properties */
	UBodycamWeaponComponent();

	/** Attaches the actor to a FirstPersonCharacter */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	bool AttachWeapon(ABodycamCharacter* TargetCharacter);

	/** Make the weapon Fire a Projectile */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	void Fire();

protected:
	/** Ends gameplay for this component. */
	UFUNCTION()
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Kicks the aim and the bodycam after a shot */
	void ApplyRecoil();

	/** Built at runtime so the 2D fire sound is sent to the reverb */
	UPROPERTY(Transient)
	TObjectPtr<class USoundAttenuation> FireAttenuation;

	/** The Character holding this weapon*/
	ABodycamCharacter* Character;
};
