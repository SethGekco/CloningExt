#pragma once

// A set of index-aligned lists describing one or more clone "specs". Spec i is
// "Amount[i] clones of type As[i] at InitialStrength[i]% health". Shorter lists
// repeat their last entry; absent lists fall back to defaults. HP is a PERCENT
// (100 = full). An optional InitialStrength.Min turns HP into a synced-RNG roll
// in [Min, InitialStrength] per clone.
//
// This is the per-clone control surface: CloneAmount/CloneAs/CloneInitialStrength
// on a unit, and CloneSlotN.Amount/.As/.InitialStrength on each slot.

#include <Utilities/Container.h>
#include <Utilities/TemplateDef.h>

#include <TechnoTypeClass.h>
#include <Randomizer.h>
#include <ScenarioClass.h>

#include <algorithm>
#include <utility>
#include <cstdio>

struct CloneList
{
	ValueableVector<int> Amount;
	ValueableVector<TechnoTypeClass*> As;
	ValueableVector<double> InitialStrength;    // percent (100 = full)
	ValueableVector<double> InitialStrengthMin; // percent; if set, HP rolls [Min, Strength]
	ValueableVector<double> Chance;             // percent per-clone spawn chance (default 100)
	ValueableVector<TechnoTypeClass*> AsLowPower; // alt type used when owner power is low
	ValueableVector<double> Rank;               // explicit veterancy per spec (0/1/2); <0 = unset
	Valueable<bool> RandomType { false };       // pick ONE spec per clone (weighted) instead of all
	ValueableVector<int> Weights;               // per-spec weights for RandomType (default 1)

	// Read every key from a common prefix: <prefix>Amount, <prefix>As,
	// <prefix>InitialStrength[.Min], <prefix>Chance, <prefix>As.LowPower,
	// <prefix>Rank, <prefix>As.Random, <prefix>As.Weights. amountAliasKey (optional,
	// e.g. "CloneCount") is read first so the primary <prefix>Amount overrides it.
	void Read(INI_EX& exINI, const char* section, const char* prefix,
		const char* amountAliasKey = nullptr)
	{
		char key[0x40];
		auto const K = [&](const char* suffix) -> const char*
		{
			_snprintf_s(key, sizeof(key), "%s%s", prefix, suffix);
			return key;
		};

		if (amountAliasKey)
			this->Amount.Read(exINI, section, amountAliasKey);
		this->Amount.Read(exINI, section, K("Amount"));
		this->As.Read(exINI, section, K("As"));
		this->InitialStrength.Read(exINI, section, K("InitialStrength"));
		this->InitialStrengthMin.Read(exINI, section, K("InitialStrength.Min"));
		this->Chance.Read(exINI, section, K("Chance"));
		this->AsLowPower.Read(exINI, section, K("As.LowPower"));
		this->Rank.Read(exINI, section, K("Rank"));
		this->RandomType.Read(exINI, section, K("As.Random"));
		this->Weights.Read(exINI, section, K("As.Weights"));
	}

	bool Empty() const
	{
		return this->Amount.empty() && this->As.empty()
			&& this->InitialStrength.empty() && this->InitialStrengthMin.empty();
	}

	// Does the list configure any HP override (so a caller should apply it)?
	bool HasStrength() const
	{
		return !this->InitialStrength.empty() || !this->InitialStrengthMin.empty();
	}

	// Number of distinct specs = longest list, but always at least 1 so an
	// all-default list still yields one clone (matches CloneCount=1).
	int SpecCount() const
	{
		size_t n = this->Amount.size();
		n = std::max(n, this->As.size());
		n = std::max(n, this->InitialStrength.size());
		n = std::max(n, this->InitialStrengthMin.size());
		return n < 1 ? 1 : static_cast<int>(n);
	}

	int AmountAt(int i) const
	{
		if (this->Amount.empty())
			return 1;
		return (i < static_cast<int>(this->Amount.size()))
			? this->Amount[static_cast<size_t>(i)]
			: this->Amount.back();
	}

	// Clone type for spec i. When lowPower is true and an As.LowPower entry exists,
	// the low-power (defect) type is used instead; otherwise the normal type, else def.
	TechnoTypeClass* AsAt(int i, TechnoTypeClass* def, bool lowPower = false) const
	{
		if (lowPower && !this->AsLowPower.empty())
		{
			return (i < static_cast<int>(this->AsLowPower.size()))
				? this->AsLowPower[static_cast<size_t>(i)]
				: this->AsLowPower.back();
		}
		if (this->As.empty())
			return def;
		return (i < static_cast<int>(this->As.size()))
			? this->As[static_cast<size_t>(i)]
			: this->As.back();
	}

	// Per-clone spawn chance for spec i as a percent (default 100 = always).
	double ChancePctAt(int i) const
	{
		return ValueAt(this->Chance, i, 100.0);
	}

	// Roll whether one clone of spec i should spawn. No RNG is consumed when the
	// chance is >= 100 (the common case), keeping default behaviour deterministic.
	bool RollChanceAt(int i) const
	{
		double const pct = ChancePctAt(i);
		if (pct >= 100.0)
			return true;
		if (pct <= 0.0)
			return false;
		return ScenarioClass::Instance->Random.RandomRanged(1, 100) <= static_cast<int>(pct);
	}

	// HP percent for spec i, rolling the synced RNG when a Min is present.
	double StrengthPctAt(int i) const
	{
		if (this->InitialStrength.empty() && this->InitialStrengthMin.empty())
			return 100.0;

		double hi = ValueAt(this->InitialStrength, i, 100.0);
		if (this->InitialStrengthMin.empty())
			return hi;

		double lo = ValueAt(this->InitialStrengthMin, i, hi);
		if (lo > hi)
			std::swap(lo, hi);

		int const loI = static_cast<int>(lo * 100.0);
		int const hiI = static_cast<int>(hi * 100.0);
		if (hiI <= loI)
			return lo;

		int const roll = ScenarioClass::Instance->Random.RandomRanged(loI, hiI);
		return static_cast<double>(roll) / 100.0;
	}

	// Explicit veterancy rank for spec i (0=rookie, 1=veteran, 2=elite). Returns
	// <0 when unset, meaning "leave the clone's veterancy to the inherit system".
	double RankAt(int i) const
	{
		if (this->Rank.empty())
			return -1.0;
		return (i < static_cast<int>(this->Rank.size()))
			? this->Rank[static_cast<size_t>(i)]
			: this->Rank.back();
	}

	// Weighted-random spec index over [0, specCount) using the synced RNG. Used
	// only in RandomType mode. Weights default to 1; non-positive total => uniform.
	int RollSpecIndex(int specCount) const
	{
		if (specCount <= 1)
			return 0;

		int total = 0;
		for (int i = 0; i < specCount; ++i)
			total += WeightAt(i);

		if (total <= 0)
			return ScenarioClass::Instance->Random.RandomRanged(0, specCount - 1);

		int const roll = ScenarioClass::Instance->Random.RandomRanged(1, total);
		int acc = 0;
		for (int i = 0; i < specCount; ++i)
		{
			acc += WeightAt(i);
			if (roll <= acc)
				return i;
		}
		return specCount - 1;
	}

private:
	int WeightAt(int i) const
	{
		if (this->Weights.empty())
			return 1;
		int const w = (i < static_cast<int>(this->Weights.size()))
			? this->Weights[static_cast<size_t>(i)]
			: this->Weights.back();
		return w < 0 ? 0 : w;
	}

	static double ValueAt(const ValueableVector<double>& v, int i, double def)
	{
		if (v.empty())
			return def;
		return (i < static_cast<int>(v.size())) ? v[static_cast<size_t>(i)] : v.back();
	}
};
