// Fill out your copyright notice in the Description page of Project Settings.


#include "KartHealthComponent.h"
#include "KartFeedback.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "SmashKarts.h"

UKartHealthComponent::UKartHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	MaxHealth = 100.f;
	Health = MaxHealth; // clients show full health before the first replication
}

void UKartHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority()) 
	{
		Health = MaxHealth;
	}
}

void UKartHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UKartHealthComponent, Health);
}

void UKartHealthComponent::ApplyHealthDamage(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - Amount, 0.f, MaxHealth);
	OnRep_Health(OldHealth);
}

void UKartHealthComponent::OnRep_Health(float OldHealth)
{
	const TCHAR* Machine = (GetOwner() && GetOwner()->HasAuthority()) ? TEXT("Server") : TEXT("Client");
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Health] %s | %s | %.0f -> %.0f"), *GetNameSafe(GetOwner()), Machine, OldHealth, Health);

	// The player driving this kart feels the hit (shake + red flash); does nothing on other machines
	if (Health < OldHealth)
	{
		KartFeedback::KartDamaged(Cast<APawn>(GetOwner()), OldHealth - Health);
	}
}

float UKartHealthComponent::GetHealth() const
{
	return Health;
}

float UKartHealthComponent::GetMaxHealth() const
{
	return MaxHealth;
}