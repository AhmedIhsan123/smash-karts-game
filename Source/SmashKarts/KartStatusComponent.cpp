#include "KartStatusComponent.h"
#include "KartRules.h"
#include "SmashKarts.h"
#include "Net/UnrealNetwork.h"

UKartStatusComponent::UKartStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // enabled on the server in BeginPlay
	PrimaryComponentTick.TickInterval = 0.05f;
	SetIsReplicatedByDefault(true);

	EndServerTimes.Init(0.0, static_cast<int32>(EKartStatus::Count));
}

void UKartStatusComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKartStatusComponent, StatusFlags);
	DOREPLIFETIME(UKartStatusComponent, EndServerTimes);
}

void UKartStatusComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(GetOwner() && GetOwner()->HasAuthority());
}

void UKartStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (StatusFlags == 0)
	{
		return;
	}

	const double Now = KartTime::ServerNow(this);
	uint8 NewFlags = StatusFlags;
	for (const EKartStatus Status : TEnumRange<EKartStatus>())
	{
		const int32 Index = static_cast<int32>(Status);
		const double End = EndServerTimes[Index];
		if ((NewFlags & ToBit(Status)) && End >= 0.0 && Now >= End)
		{
			NewFlags &= ~ToBit(Status);
			EndServerTimes[Index] = 0.0;
		}
	}
	SetFlags(NewFlags);
}

void UKartStatusComponent::ApplyStatus(EKartStatus Status, float Duration)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Status >= EKartStatus::Count)
	{
		return;
	}
	const int32 Index = static_cast<int32>(Status);
	EndServerTimes[Index] = Duration > 0.f ? KartTime::ServerNow(this) + Duration : -1.0;
	SetFlags(StatusFlags | ToBit(Status));
}

void UKartStatusComponent::ClearStatus(EKartStatus Status)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Status >= EKartStatus::Count)
	{
		return;
	}
	EndServerTimes[static_cast<int32>(Status)] = 0.0;
	SetFlags(StatusFlags & ~ToBit(Status));
}

void UKartStatusComponent::ClearAll()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	for (double& End : EndServerTimes)
	{
		End = 0.0;
	}
	SetFlags(0);
}

bool UKartStatusComponent::HasStatus(EKartStatus Status) const
{
	return Status < EKartStatus::Count && (StatusFlags & ToBit(Status)) != 0;
}

float UKartStatusComponent::GetTimeRemaining(EKartStatus Status) const
{
	if (!HasStatus(Status) || !EndServerTimes.IsValidIndex(static_cast<int32>(Status)))
	{
		return 0.f;
	}
	const double End = EndServerTimes[static_cast<int32>(Status)];
	if (End < 0.0)
	{
		return -1.f;
	}
	return static_cast<float>(FMath::Max(0.0, End - KartTime::ServerNow(this)));
}

void UKartStatusComponent::SetFlags(uint8 NewFlags)
{
	if (NewFlags == StatusFlags)
	{
		return;
	}
	const uint8 Old = StatusFlags;
	StatusFlags = NewFlags;
	OnRep_StatusFlags(Old); // RepNotify doesn't run on the server
}

void UKartStatusComponent::OnRep_StatusFlags(uint8 OldFlags)
{
	UE_LOG(LogSmashKarts, Verbose, TEXT("[Status] %s | %s | 0x%02X -> 0x%02X"), *GetNameSafe(GetOwner()),
		(GetOwner() && GetOwner()->HasAuthority()) ? TEXT("Server") : TEXT("Client"), OldFlags, StatusFlags);
	OnStatusChanged.Broadcast(OldFlags, StatusFlags);
}
