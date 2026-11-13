#include "KartQuestionMarksComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"

UKartQuestionMarksComponent::UKartQuestionMarksComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	Symbol = TEXT("?");
	TextSize = 70.f;
	FaceDistance = 51.f;
	TextColor = FColor(255, 210, 40);
}

void UKartQuestionMarksComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!bShowSigns || GetNetMode() == NM_DedicatedServer || !GetOwner())
	{
		return;
	}

	// A text render faces its +X axis, so turn one sign toward each side face of the box
	static const float Yaws[] = { 0.f, 90.f, 180.f, -90.f };
	for (const float Yaw : Yaws)
	{
		UTextRenderComponent* Sign = NewObject<UTextRenderComponent>(GetOwner(), NAME_None, RF_Transient);
		Sign->SetText(FText::FromString(Symbol));
		Sign->SetHorizontalAlignment(EHTA_Center);
		Sign->SetVerticalAlignment(EVRTA_TextCenter);
		Sign->SetWorldSize(TextSize);
		Sign->SetTextRenderColor(TextColor);
		Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sign->SetGenerateOverlapEvents(false);
		Sign->SetCastShadow(false);
		const FRotator Facing(0.f, Yaw, 0.f);
		Sign->SetRelativeLocationAndRotation(Facing.Vector() * FaceDistance, Facing);
		Sign->SetupAttachment(this);
		Sign->RegisterComponent();
		Signs.Add(Sign);
	}
}
