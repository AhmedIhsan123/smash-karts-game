// Pure-logic automation tests. Run: Automation RunTests SmashKarts

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "KartRules.h"

#if WITH_DEV_AUTOMATION_TESTS

constexpr EAutomationTestFlags KartTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartWeightedRollTest, "SmashKarts.Rules.WeightedRoll", KartTestFlags)
bool FKartWeightedRollTest::RunTest(const FString& Parameters)
{
	const TArray<float> Weights = { 14.f, 0.f, 6.f, 20.f }; // total 40
	TestEqual(TEXT("roll 0 picks the first"), KartRules::PickWeightedIndex(Weights, 0.f), 0);
	TestEqual(TEXT("roll just under 14/40 picks the first"), KartRules::PickWeightedIndex(Weights, 0.349f), 0);
	TestEqual(TEXT("roll 14/40 skips the zero weight"), KartRules::PickWeightedIndex(Weights, 0.35f), 2);
	TestEqual(TEXT("roll 0.5 lands in the third bucket"), KartRules::PickWeightedIndex(Weights, 0.5f), 3);
	TestEqual(TEXT("roll 1.0 picks the last valid"), KartRules::PickWeightedIndex(Weights, 1.f), 3);
	TestEqual(TEXT("all zero weights"), KartRules::PickWeightedIndex({ 0.f, 0.f }, 0.5f), INDEX_NONE);
	TestEqual(TEXT("empty"), KartRules::PickWeightedIndex({}, 0.5f), INDEX_NONE);

	// Distribution sanity: 10k evenly spaced rolls match the weights within 1%
	TArray<int32> Counts;
	Counts.Init(0, Weights.Num());
	for (int32 i = 0; i < 10000; ++i)
	{
		++Counts[KartRules::PickWeightedIndex(Weights, (i + 0.5f) / 10000.f)];
	}
	TestTrue(TEXT("first ~35%"), FMath::Abs(Counts[0] - 3500) <= 100);
	TestEqual(TEXT("zero weight never wins"), Counts[1], 0);
	TestTrue(TEXT("third ~15%"), FMath::Abs(Counts[2] - 1500) <= 100);
	TestTrue(TEXT("fourth ~50%"), FMath::Abs(Counts[3] - 5000) <= 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStandingsSortTest, "SmashKarts.Rules.StandingsSort", KartTestFlags)
bool FKartStandingsSortTest::RunTest(const FString& Parameters)
{
	TArray<KartRules::FStandingEntry> Entries;
	auto Add = [&Entries](float Score, int32 Deaths, int32 Join, int32 Id)
	{
		KartRules::FStandingEntry E;
		E.Score = Score; E.Deaths = Deaths; E.JoinOrder = Join; E.Id = Id;
		Entries.Add(E);
	};
	Add(3.f, 5, 0, 0);
	Add(5.f, 9, 1, 1);
	Add(3.f, 2, 2, 2);
	Add(3.f, 2, 3, 3);
	Add(0.f, 0, 4, 4);
	KartRules::SortStandings(Entries);

	TestEqual(TEXT("highest score first"), Entries[0].Id, 1);
	TestEqual(TEXT("tie: fewer deaths wins"), Entries[1].Id, 2);
	TestEqual(TEXT("tie on deaths: earlier join wins"), Entries[2].Id, 3);
	TestEqual(TEXT("more deaths after"), Entries[3].Id, 0);
	TestEqual(TEXT("lowest last"), Entries[4].Id, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartFormatTimeTest, "SmashKarts.Rules.FormatMatchTime", KartTestFlags)
bool FKartFormatTimeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("180"), KartRules::FormatMatchTime(180.f), FString(TEXT("3:00")));
	TestEqual(TEXT("125"), KartRules::FormatMatchTime(125.f), FString(TEXT("2:05")));
	TestEqual(TEXT("59.2 rounds up"), KartRules::FormatMatchTime(59.2f), FString(TEXT("1:00")));
	TestEqual(TEXT("0.4 rounds up"), KartRules::FormatMatchTime(0.4f), FString(TEXT("0:01")));
	TestEqual(TEXT("0"), KartRules::FormatMatchTime(0.f), FString(TEXT("0:00")));
	TestEqual(TEXT("negative"), KartRules::FormatMatchTime(-3.f), FString(TEXT("0:00")));
	TestEqual(TEXT("600"), KartRules::FormatMatchTime(600.f), FString(TEXT("10:00")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartDamageFalloffTest, "SmashKarts.Rules.DamageByDistance", KartTestFlags)
bool FKartDamageFalloffTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("point blank"), KartRules::DamageByDistance(0.f, 1500.f, 1.2f, 0.3f), 1.2f);
	TestEqual(TEXT("max range"), KartRules::DamageByDistance(1500.f, 1500.f, 1.2f, 0.3f), 0.3f);
	TestEqual(TEXT("beyond range clamps"), KartRules::DamageByDistance(5000.f, 1500.f, 1.2f, 0.3f), 0.3f);
	TestTrue(TEXT("half way"), FMath::IsNearlyEqual(KartRules::DamageByDistance(2000.f, 4000.f, 7.f, 4.f), 5.5f));
	TestEqual(TEXT("zero range"), KartRules::DamageByDistance(100.f, 0.f, 7.f, 4.f), 7.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartSpawnPickTest, "SmashKarts.Rules.SpawnPick", KartTestFlags)
bool FKartSpawnPickTest::RunTest(const FString& Parameters)
{
	const TArray<float> Scores = { 100.f, 900.f, 50.f, 700.f, 800.f };
	TestEqual(TEXT("rand 0 -> best"), KartRules::PickSpawnIndex(Scores, 3, 0.f), 1);
	TestEqual(TEXT("rand mid -> second best"), KartRules::PickSpawnIndex(Scores, 3, 0.5f), 4);
	TestEqual(TEXT("rand ~1 -> third best"), KartRules::PickSpawnIndex(Scores, 3, 0.999f), 3);
	TestEqual(TEXT("top 1 is always the best"), KartRules::PickSpawnIndex(Scores, 1, 0.9f), 1);
	TestEqual(TEXT("top N larger than array"), KartRules::PickSpawnIndex({ 5.f }, 3, 0.7f), 0);
	TestEqual(TEXT("empty"), KartRules::PickSpawnIndex({}, 3, 0.5f), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartNameSanitizeTest, "SmashKarts.Rules.SanitizePlayerName", KartTestFlags)
bool FKartNameSanitizeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("trim"), KartRules::SanitizePlayerName(TEXT("  Ahmed  ")), FString(TEXT("Ahmed")));
	TestEqual(TEXT("url chars removed"), KartRules::SanitizePlayerName(TEXT("a?b=c#d")), FString(TEXT("abcd")));
	TestEqual(TEXT("clamped to 16"), KartRules::SanitizePlayerName(TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ")).Len(), 16);
	TestEqual(TEXT("empty"), KartRules::SanitizePlayerName(TEXT("   ")), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartContactRulesTest, "SmashKarts.Rules.ContactSmash", KartTestFlags)
bool FKartContactRulesTest::RunTest(const FString& Parameters)
{
	using namespace KartRules;
	FContactSide Normal;
	FContactSide Spiky;      Spiky.bSpiky = true;
	FContactSide Invincible; Invincible.bInvincible = true;
	FContactSide Frozen;     Frozen.bFrozen = true;

	// Spiky vs normal: the spiky kart smashes, the normal one doesn't
	FContactResult R = ResolveContact(Spiky, Normal, false, true, true, false, false);
	TestTrue(TEXT("spiky smashes normal in reach"), R.bAHitsB);
	TestFalse(TEXT("normal does not smash spiky"), R.bBHitsA);
	R = ResolveContact(Spiky, Normal, false, false, false, false, false);
	TestFalse(TEXT("out of reach: nothing"), R.bAHitsB || R.bBHitsA);

	// Two spiky karts smash each other
	R = ResolveContact(Spiky, Spiky, false, true, true, false, false);
	TestTrue(TEXT("spiky vs spiky: both"), R.bAHitsB && R.bBHitsA);

	// Invincible beats spiky; two invincible just bounce
	R = ResolveContact(Invincible, Spiky, true, true, true, false, false);
	TestTrue(TEXT("invincible smashes spiky on touch"), R.bAHitsB);
	TestFalse(TEXT("spiky can't hurt invincible"), R.bBHitsA);
	R = ResolveContact(Invincible, Normal, false, true, true, false, false);
	TestFalse(TEXT("invincible needs to touch"), R.bAHitsB);
	R = ResolveContact(Invincible, Invincible, true, true, true, true, true);
	TestFalse(TEXT("invincible vs invincible: bounce"), R.bAHitsB || R.bBHitsA);

	// Ramming a frozen kart
	R = ResolveContact(Normal, Frozen, true, false, false, true, false);
	TestTrue(TEXT("ram smashes frozen"), R.bAHitsB);
	R = ResolveContact(Normal, Frozen, true, false, false, false, false);
	TestFalse(TEXT("slow touch doesn't"), R.bAHitsB);
	FContactSide FrozenInvincible; FrozenInvincible.bFrozen = true; FrozenInvincible.bInvincible = true;
	R = ResolveContact(Normal, FrozenInvincible, true, false, false, true, false);
	TestFalse(TEXT("invincible frozen kart can't be smashed"), R.bAHitsB);

	// Normal vs normal touching: nothing
	R = ResolveContact(Normal, Normal, true, true, true, true, true);
	TestFalse(TEXT("normal bump: nothing"), R.bAHitsB || R.bBHitsA);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
