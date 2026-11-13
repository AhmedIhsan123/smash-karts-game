#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "KartQuestionMarksComponent.generated.h"

class UTextRenderComponent;

/**
 * Four "?" signs on the faces of a box (loot boxes, and the Fake Loot Box so it looks the same).
 * Attach it to the box mesh: the signs spin and hide with the mesh.
 *
 * Authority: none, cosmetic only. Replicates: nothing (each machine builds its own signs in BeginPlay).
 * Client prediction: not needed.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SMASHKARTS_API UKartQuestionMarksComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UKartQuestionMarksComponent();

	/** Off = no signs (traps keep this off unless they're disguised as a loot box). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Question Marks")
	bool bShowSigns = true;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Question Marks")
	FString Symbol;

	/** Text height in the parent mesh's local units (engine cube = 100). */
	UPROPERTY(EditAnywhere, Category = "Question Marks", meta = (ClampMin = "1"))
	float TextSize;

	/** Distance from the box centre to each sign, in the parent mesh's local units (cube face = 50). */
	UPROPERTY(EditAnywhere, Category = "Question Marks", meta = (ClampMin = "0"))
	float FaceDistance;

	UPROPERTY(EditAnywhere, Category = "Question Marks")
	FColor TextColor;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Signs;
};
