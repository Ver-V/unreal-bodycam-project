// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BodycamPistolComponent.generated.h"

class ABodycamCharacter;
class UAnimMontage;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class USoundAttenuation;
class USoundBase;
class UNiagaraSystem;
class UMaterialInterface;
class UPointLightComponent;

/**
 * Sidearm built into the first person mesh (hands + pistol in one skeletal mesh).
 * Handles semi-auto fire, magazine/chamber ammo, reloads and recoil.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BODYCAM_API UBodycamPistolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBodycamPistolComponent();

	/** Called by the owning character when its input component is set up */
	void BindInput(UEnhancedInputComponent* InputComponent);

	UInputMappingContext* GetMappingContext() const { return WeaponMappingContext; }

	/** Reveals the hands + pistol and plays the draw animation. Does nothing if already equipped. */
	UFUNCTION(BlueprintCallable, Category="Pistol")
	void Equip();

	UFUNCTION(BlueprintPure, Category="Pistol")
	bool IsEquipped() const { return bEquipped; }

	UFUNCTION(BlueprintCallable, Category="Pistol")
	void Fire();

	UFUNCTION(BlueprintCallable, Category="Pistol")
	void Reload();

	/** Rounds in the magazine plus the one in the chamber */
	UFUNCTION(BlueprintPure, Category="Pistol")
	int32 GetLoadedRounds() const { return AmmoInMagazine + (bRoundChambered ? 1 : 0); }

	/** Start with the pistol out instead of waiting for a pickup */
	UPROPERTY(EditAnywhere, Category="Pistol")
	bool bStartEquipped = false;

	// --- Input ---

	UPROPERTY(EditAnywhere, Category="Pistol|Input")
	TObjectPtr<UInputMappingContext> WeaponMappingContext;

	UPROPERTY(EditAnywhere, Category="Pistol|Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditAnywhere, Category="Pistol|Input")
	TObjectPtr<UInputAction> ReloadAction;

	/** Hold to aim down sights */
	UPROPERTY(EditAnywhere, Category="Pistol|Input")
	TObjectPtr<UInputAction> AimAction;

	// --- Aim down sights ---

	/** How fast the pistol comes up / goes down (higher = snappier) */
	UPROPERTY(EditAnywhere, Category="Pistol|Aim")
	float AimSpeed = 10.f;

	/** Move the hands+pistol mesh by this much (relative to its hip placement) when fully aiming. Tune so the sights sit in screen center. */
	UPROPERTY(EditAnywhere, Category="Pistol|Aim")
	FVector AimMeshOffset = FVector::ZeroVector;

	/** Rotate the hands+pistol mesh by this much when fully aiming */
	UPROPERTY(EditAnywhere, Category="Pistol|Aim")
	FRotator AimMeshRotation = FRotator::ZeroRotator;

	/** Walk speed multiplier while fully aiming */
	UPROPERTY(EditAnywhere, Category="Pistol|Aim", meta=(ClampMin="0.1", ClampMax="1"))
	float AimWalkSpeedScale = 0.6f;

	/** Keep aiming without holding the button. Turn on while tuning AimMeshOffset in PIE. */
	UPROPERTY(EditAnywhere, Category="Pistol|Aim")
	bool bDebugForceAim = false;

	// --- Shooting ---

	/** How far a shot reaches (cm) */
	UPROPERTY(EditAnywhere, Category="Pistol|Shooting")
	float Range = 10000.f;

	/** Push applied to physics objects that get hit */
	UPROPERTY(EditAnywhere, Category="Pistol|Shooting")
	float ImpactImpulse = 30000.f;

	// --- Muzzle flash ---

	/** Particle flash spawned at the muzzle, attached so it follows the gun */
	UPROPERTY(EditAnywhere, Category="Pistol|MuzzleFlash")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	/** Brief light at the muzzle that lights up the surroundings on each shot (candelas) */
	UPROPERTY(EditAnywhere, Category="Pistol|MuzzleFlash")
	float MuzzleFlashIntensity = 2000.f;

	UPROPERTY(EditAnywhere, Category="Pistol|MuzzleFlash")
	float MuzzleFlashRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category="Pistol|MuzzleFlash")
	FLinearColor MuzzleFlashColor = FLinearColor(1.f, 0.75f, 0.45f);

	/** Seconds the flash light stays on */
	UPROPERTY(EditAnywhere, Category="Pistol|MuzzleFlash")
	float MuzzleFlashDuration = 0.05f;

	// --- Impact ---

	/** Particle burst at the hit point (dust, sparks...). Spawned facing out of the surface. */
	UPROPERTY(EditAnywhere, Category="Pistol|Impact")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	/** Bullet hole decal left on the surface */
	UPROPERTY(EditAnywhere, Category="Pistol|Impact")
	TObjectPtr<UMaterialInterface> ImpactDecal;

	/** Decal size (cm). X is projection depth, Y/Z are the hole size. */
	UPROPERTY(EditAnywhere, Category="Pistol|Impact")
	FVector ImpactDecalSize = FVector(4.f, 3.f, 3.f);

	/** Seconds before a bullet hole fades away (0 = never) */
	UPROPERTY(EditAnywhere, Category="Pistol|Impact")
	float ImpactDecalLifetime = 30.f;

	UPROPERTY(EditAnywhere, Category="Pistol|Impact")
	TObjectPtr<USoundBase> ImpactSound;

	/** Used instead of ImpactEffect when a character (zombie) is hit. No bullet hole is left on characters. */
	UPROPERTY(EditAnywhere, Category="Pistol|Impact")
	TObjectPtr<UNiagaraSystem> FleshImpactEffect;

	UPROPERTY(EditAnywhere, Category="Pistol|Impact")
	TObjectPtr<USoundBase> FleshImpactSound;

	/** Draw each shot's trace for a second (testing) */
	UPROPERTY(EditAnywhere, Category="Pistol|Shooting")
	bool bDrawDebug = false;

	/** Bone or socket on the first person mesh where shots start */
	UPROPERTY(EditAnywhere, Category="Pistol|Shooting")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Minimum time between shots. Stops macro-clicking faster than the slide can cycle. */
	UPROPERTY(EditAnywhere, Category="Pistol|Shooting")
	float MinShotInterval = 0.12f;

	// --- Ammo ---

	UPROPERTY(EditAnywhere, Category="Pistol|Ammo", meta=(ClampMin="1"))
	int32 MagazineSize = 17;

	// --- Animation ---

	UPROPERTY(EditAnywhere, Category="Pistol|Animation")
	TObjectPtr<UAnimMontage> FireMontage;

	/** Reload with a round still chambered (faster, no slide rack) */
	UPROPERTY(EditAnywhere, Category="Pistol|Animation")
	TObjectPtr<UAnimMontage> ReloadMontage;

	/** Reload from empty (slide release / rack) */
	UPROPERTY(EditAnywhere, Category="Pistol|Animation")
	TObjectPtr<UAnimMontage> ReloadEmptyMontage;

	/** Played when the pistol is equipped */
	UPROPERTY(EditAnywhere, Category="Pistol|Animation")
	TObjectPtr<UAnimMontage> DrawMontage;

	// --- Sound ---

	UPROPERTY(EditAnywhere, Category="Pistol|Sound")
	TObjectPtr<USoundBase> FireSound;

	/** Click when pulling the trigger on an empty chamber */
	UPROPERTY(EditAnywhere, Category="Pistol|Sound")
	TObjectPtr<USoundBase> DryFireSound;

	/** How much of the shot goes into the room reverb (0..1) */
	UPROPERTY(EditAnywhere, Category="Pistol|Sound", meta=(ClampMin="0", ClampMax="1"))
	float FireReverbSend = 0.6f;

	// --- Recoil ---

	/** Degrees the aim climbs per shot */
	UPROPERTY(EditAnywhere, Category="Pistol|Recoil")
	float AimRecoilPitch = 1.2f;

	/** Max random horizontal aim drift per shot (degrees) */
	UPROPERTY(EditAnywhere, Category="Pistol|Recoil")
	float AimRecoilYaw = 0.4f;

	/** Bodycam jolt per shot (degrees). Yaw and Roll are randomized in +-value. */
	UPROPERTY(EditAnywhere, Category="Pistol|Recoil")
	FRotator CameraKick = FRotator(2.5f, 0.8f, 2.f);

	/** Bodycam push back per shot (cm, camera space) */
	UPROPERTY(EditAnywhere, Category="Pistol|Recoil")
	float CameraPushBack = 2.5f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Plays a montage on the first person mesh. Returns false if it could not play. */
	bool PlayMontage(UAnimMontage* Montage);

	void StartAim() { bWantsToAim = true; }
	void StopAim() { bWantsToAim = false; }

	/** Instant shot: traces a line from the muzzle along the aim */
	void FireHitscan();

	/** Dust / bullet hole / sound where a shot landed */
	void SpawnImpactEffects(const FHitResult& Hit);
	void PlayFireSound();
	void PlayMuzzleFlash();
	void ApplyRecoil();

	/** Plays a montage that blocks firing/reloading until it ends. */
	bool PlayBusyMontage(UAnimMontage* Montage);
	void OnBusyMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Refill the magazine (and chamber) once a reload completes */
	void FinishReload();

	UPROPERTY(Transient)
	TObjectPtr<ABodycamCharacter> Character;

	/** Created at BeginPlay on the muzzle; switched on briefly per shot */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> MuzzleLight;

	FTimerHandle MuzzleLightTimer;

	/** Built at runtime so the 2D fire sound is sent to the reverb */
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> FireAttenuation;

	int32 AmmoInMagazine = 0;
	bool bRoundChambered = true;
	/** True while a reload or draw is playing */
	bool bBusy = false;
	bool bEquipped = false;
	/** Whether the running reload started from empty (affects how many rounds end up loaded) */
	bool bReloadFromEmpty = false;
	float LastShotTime = -1000.f;

	bool bWantsToAim = false;
	/** 0 = hip, 1 = fully aiming */
	float AimAlpha = 0.f;
	/** Mesh placement and walk speed captured at BeginPlay, used as the hip pose */
	FVector HipMeshLocation = FVector::ZeroVector;
	FRotator HipMeshRotation = FRotator::ZeroRotator;
	float BaseWalkSpeed = 0.f;
};
