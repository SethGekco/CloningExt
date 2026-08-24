#pragma once

// Per-clone property resolvers: veterancy and initial strength.
//
// Both operate on a clone that has ALREADY been created (by Antares or by us)
// and is exiting a cloning building. They take the produced/source unit as the
// reference and compute what the clone should get.

#include <Utilities/TemplateDef.h>

#include <Randomizer.h>
#include <ScenarioClass.h>
#include <RulesClass.h>

#include <algorithm>
#include <utility>

// -------------------------------------------------------------------------
// Veterancy
//
// CloneVeterancy.Ratio scales the SOURCE unit's veterancy onto the clone.
// CloneVeterancy.Cap clamps the result (defaults to Rules VeteranCap).
//
// The three Inherit.* toggles decide which bonus sources are allowed to carry
// into the clone. IMPORTANT / KNOWN LIMITATION: at clone-exit time the source
// unit's veterancy is a single scalar; Academy and Stolen-Tech contributions
// live in Antares' own ext data, which a co-loaded DLL cannot see, so they
// cannot be individually subtracted. Therefore:
//
//   * All three allowed (default): clone inherits source veterancy * Ratio.
//   * ANY forbidden: we cannot decompose, so we conservatively drop ALL
//     inherited veterancy (clone exits rookie). This never over-grants; a
//     modder forbidding one source also loses the others until a future pass
//     wires Antares veterancy-input interop. Documented in INI_REFERENCE.md.
// -------------------------------------------------------------------------
struct CloneVeterancySpec
{
	Valueable<double> Ratio { 1.0 };
	Nullable<double>  Cap;                    // default: Rules VeteranCap
	Valueable<bool>   Inherit { true };       // master
	Valueable<bool>   InheritAcademy { true };
	Valueable<bool>   InheritStolenTech { true };
	Valueable<bool>   InheritCountryBonus { true };

	bool GranularStrip() const
	{
		return !this->InheritAcademy || !this->InheritStolenTech || !this->InheritCountryBonus;
	}

	// True when this spec would change a clone's veterancy from the game default
	// (which is to inherit the source value unchanged). Lets the hook skip work.
	bool IsActive() const
	{
		return this->Ratio != 1.0 || this->Cap.isset() || !this->Inherit || this->GranularStrip();
	}

	double Resolve(double sourceVeterancy) const
	{
		double const cap = this->Cap.isset()
			? this->Cap.Get()
			: (RulesClass::Instance ? RulesClass::Instance->VeteranCap : 2.0);

		if (!this->Inherit || this->GranularStrip())
			return 0.0;

		return std::clamp(sourceVeterancy * this->Ratio, 0.0, cap);
	}
};

// -------------------------------------------------------------------------
// Initial strength
//
// CloneInitialStrength is a fraction (0..1] of the clone type's full HP applied
// on exit. CloneInitialStrength.Min, when set, turns it into a randomised range
// [Min, CloneInitialStrength] rolled on the SYNCED scenario RNG so multiplayer
// stays in lockstep. Unset => full health (vanilla).
// -------------------------------------------------------------------------
struct CloneStrengthSpec
{
	Nullable<double> Fraction;    // CloneInitialStrength
	Nullable<double> FractionMin; // CloneInitialStrength.Min

	bool IsActive() const { return this->Fraction.isset(); }

	// Resolve the HP fraction to apply. Uses the synced RNG only when a Min is
	// set, so single-value configs stay fully deterministic without touching it.
	double ResolveFraction() const
	{
		double hi = this->Fraction.Get(1.0);

		if (!this->FractionMin.isset())
			return hi;

		double lo = this->FractionMin.Get();
		if (lo > hi)
			std::swap(lo, hi);

		// RandomRanged works on ints; scale the [lo,hi] window to 0..10000.
		int const loI = static_cast<int>(lo * 10000.0);
		int const hiI = static_cast<int>(hi * 10000.0);
		if (hiI <= loI)
			return lo;

		int const roll = ScenarioClass::Instance->Random.RandomRanged(loI, hiI);
		return static_cast<double>(roll) / 10000.0;
	}
};
