#include "KartRules.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

int32 KartRules::PickWeightedIndex(const TArray<float>& Weights, float Roll01)
{
	float Total = 0.f;
	for (const float W : Weights)
	{
		Total += FMath::Max(0.f, W);
	}
	if (Total <= 0.f)
	{
		return INDEX_NONE;
	}

	float Roll = FMath::Clamp(Roll01, 0.f, 1.f) * Total;
	int32 LastValid = INDEX_NONE;
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		if (Weights[i] <= 0.f)
		{
			continue;
		}
		LastValid = i;
		if (Roll < Weights[i])
		{
			return i;
		}
		Roll -= Weights[i];
	}
	return LastValid; // Roll01 == 1 or float rounding
}

void KartRules::SortStandings(TArray<FStandingEntry>& Entries)
{
	Entries.StableSort([](const FStandingEntry& A, const FStandingEntry& B)
	{
		if (A.Score != B.Score)
		{
			return A.Score > B.Score;
		}
		if (A.Deaths != B.Deaths)
		{
			return A.Deaths < B.Deaths;
		}
		return A.JoinOrder < B.JoinOrder;
	});
}

FString KartRules::FormatMatchTime(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds - KINDA_SMALL_NUMBER));
	return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
}

float KartRules::DamageByDistance(float Distance, float MaxRange, float NearDamage, float FarDamage)
{
	if (MaxRange <= 0.f)
	{
		return NearDamage;
	}
	const float Alpha = FMath::Clamp(Distance / MaxRange, 0.f, 1.f);
	return FMath::Lerp(NearDamage, FarDamage, Alpha);
}

int32 KartRules::PickSpawnIndex(const TArray<float>& Scores, int32 TopN, float Rand01)
{
	if (Scores.Num() == 0)
	{
		return INDEX_NONE;
	}

	TArray<int32> Order;
	Order.Reserve(Scores.Num());
	for (int32 i = 0; i < Scores.Num(); ++i)
	{
		Order.Add(i);
	}
	Order.StableSort([&Scores](int32 A, int32 B) { return Scores[A] > Scores[B]; });

	const int32 Count = FMath::Clamp(TopN, 1, Order.Num());
	const int32 Pick = FMath::Clamp(FMath::FloorToInt(FMath::Clamp(Rand01, 0.f, 0.9999f) * Count), 0, Count - 1);
	return Order[Pick];
}

float KartRules::LineVolleySpeedScale(float BaseSpeed, float PitchDegrees, float Gravity, int32 Index, float Spacing, float MinFraction)
{
	if (Index <= 0 || Spacing <= 0.f || BaseSpeed <= 0.f || Gravity <= 0.f)
	{
		return 1.f;
	}
	const float Pitch = FMath::DegreesToRadians(FMath::Clamp(PitchDegrees, 1.f, 89.f));
	const float FarDistance = FMath::Square(BaseSpeed) * FMath::Sin(2.f * Pitch) / Gravity;
	if (FarDistance <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}
	const float Distance = FMath::Max(FarDistance - Index * Spacing, FarDistance * FMath::Clamp(MinFraction, 0.01f, 1.f));
	return FMath::Sqrt(Distance / FarDistance);
}

int32 KartRules::AuraZapCount(float Duration, float Interval)
{
	if (Interval <= 0.f || Duration <= 0.f)
	{
		return 0;
	}
	return FMath::Max(1, FMath::FloorToInt(Duration / Interval + 1.0e-3f));
}

FString KartRules::SanitizePlayerName(const FString& InName, int32 MaxLength)
{
	FString Out;
	Out.Reserve(InName.Len());
	for (const TCHAR C : InName)
	{
		if (!FChar::IsControl(C) && C != TEXT('?') && C != TEXT('#') && C != TEXT('='))
		{
			Out.AppendChar(C);
		}
	}
	Out.TrimStartAndEndInline();
	return Out.Left(FMath::Max(1, MaxLength));
}

KartRules::FContactResult KartRules::ResolveContact(const FContactSide& A, const FContactSide& B, bool bTouching,
	bool bAReachesB, bool bBReachesA, bool bARamsB, bool bBRamsA)
{
	auto Hits = [bTouching](const FContactSide& Attacker, const FContactSide& Victim, bool bReaches, bool bRams)
	{
		if (Victim.bInvincible)
		{
			return false; // nothing beats invincibility (two invincible karts just bounce)
		}
		return (Attacker.bInvincible && bTouching)
			|| (Attacker.bSpiky && bReaches)
			|| (Victim.bFrozen && bRams);
	};

	FContactResult Result;
	Result.bAHitsB = Hits(A, B, bAReachesB, bARamsB);
	Result.bBHitsA = Hits(B, A, bBReachesA, bBRamsA);
	return Result;
}

double KartTime::ServerNow(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return 0.0;
	}
	if (const AGameStateBase* GS = World->GetGameState())
	{
		return GS->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}
