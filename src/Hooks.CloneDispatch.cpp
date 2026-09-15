// CloningExt -- extra-clone dispatch (per-clone spec lists + prereq/house slots).
//
// These hooks CHAIN AFTER Antares. Antares owns the cloning dispatch; we run at
// the same sites, after it, to produce the unit's clone SPEC LIST from each
// qualifying source (CloneAmount/CloneAs/CloneInitialStrength, times the source
// building's Cloning.Mult), plus the specs granted by any satisfied slot.
//
// Hook sites (register layout VERIFIED against gamemd disassembly + Antares
// src/Ext/TechnoType/Hooks.cpp):
//
//   0x444DBC  infantry exit        ESI=BuildingClass* factory, EDI=Production
//   0x44441A  naval-unit clone     ESI=BuildingClass* factory, EDI=Production
//   0x4445F0  vehicle exit         ESI=BuildingClass* factory, EDI=Production
//
// Antares' handlers at 0x444DBC and 0x44441A `return 0`, so Syringe chains ours
// after them without a race. The non-naval *vehicle* path is different: Antares
// occupies 0x4445F6 (size 5) and returns a non-zero jump to the shared epilogue
// 0x444971 (where ESI/EDI are clobbered), so chaining at 0x4445F6 would depend on
// winning load order. Instead we hook the adjacent 6-byte instruction at 0x4445F0
// (0x4445F0 + 6 == 0x4445F6) -- no overlap, no contention -- and fall through into
// Antares' hook. See HOOKS_LOG.md.
//
// RECURSION: our own extra clones re-enter KickOutUnit (when placed asBuilt) and
// reach these hooks again. Dispatch() short-circuits on the CloningExt::Producing-
// Extras flag, and only real production events (kicked from an infantry/unit
// FACTORY, not a cloning vat) get here in the first place -- so no runaway.

#include "CloningExt.h"

#include <Ext/TechnoType/Body.h>
#include <Ext/BuildingType/Body.h>

#include <Utilities/Macro.h>

#include <Cloning/State.h>

#include <BuildingClass.h>
#include <InfantryClass.h>
#include <InfantryTypeClass.h>
#include <UnitTypeClass.h>
#include <HouseClass.h>
#include <MapClass.h>
#include <CellClass.h>
#include <Unsorted.h>
#include <Helpers/Cast.h>
#include <Utilities/Debug.h>

namespace
{
	// Place an already-created clone at a free cell near pFrom via Unlimbo.
	//
	// We ask the game's own placement finder, MapClass::NearByLocation, for a free
	// cell near the building and Unlimbo the clone there. NearByLocation skips
	// cells that are already occupied -- including clones placed moments ago -- so
	// repeated calls scatter the clones (the same pattern Antares uses in
	// SWTypes/UnitDelivery.cpp). This is why we do NOT rely on KickOutUnit for
	// placement: it only clears one exit cell per production.
	bool UnlimboClone(BuildingClass* pFrom, TechnoClass* pClone, TechnoTypeClass* pCloneType)
	{
		auto const pOriginCell = MapClass::Instance.GetCellAt(pFrom->Location);
		CellStruct const origin = pOriginCell ? pOriginCell->MapCoords : CellStruct::Empty;

		CellStruct const place = MapClass::Instance.NearByLocation(
			origin, pCloneType->SpeedType, -1, pCloneType->MovementZone,
			false, 1, 1, false, false, false, false, CellStruct::Empty, false, false);

		auto const pCell = MapClass::Instance.TryGetCellAt(place);
		if (!pCell)
			return false;

		auto const xyz = pCell->GetCoordsWithBridge();
		auto const facing = static_cast<DirType>(
			(MapClass::GetCellIndex(pCell->MapCoords) & 7u) << 5);

		pClone->QueueMission(Mission::Guard, false);
		return pClone->Unlimbo(xyz, facing);
	}

	// Place an already-created clone. Returns true when it ends up on the map.
	//
	// asBuilt: when true, the clone is first run through BuildingClass::KickOutUnit
	// so co-DLLs that record production at its entry (GiftBox/Host @0x443C60) mark
	// it "built". KickOutUnit places it when the exit is clear; when it can't, the
	// clone is left in limbo and we scatter it via Unlimbo -- either way the mark
	// already happened at the entry. asBuilt=false (the default) skips KickOutUnit
	// entirely, so with no built-detection DLL loaded the tag changes nothing.
	bool PlaceClone(BuildingClass* pFrom, TechnoClass* pClone,
		TechnoTypeClass* pCloneType, bool asBuilt)
	{
		if (asBuilt)
		{
			auto const pOriginCell = MapClass::Instance.GetCellAt(pFrom->Location);
			CellStruct const origin = pOriginCell ? pOriginCell->MapCoords : CellStruct::Empty;

			if (pFrom->KickOutUnit(pClone, origin) == KickOutResult::Succeeded)
				return true;
			if (!pClone->InLimbo)
				return true; // placed despite a non-Succeeded return
			// otherwise it is still in limbo -- scatter it ourselves below
		}

		if (UnlimboClone(pFrom, pClone, pCloneType))
			return true;

		pClone->UnInit();
		return false;
	}

	// Apply an HP percentage (100 = full) to a freshly created clone.
	void ApplyStrengthPct(TechnoClass* pClone, TechnoTypeClass* pCloneType, double pct)
	{
		int const full = pCloneType->Strength;
		int hp = static_cast<int>(full * pct / 100.0);
		if (hp < 1) hp = 1;
		if (hp > full) hp = full;
		pClone->Health = hp;
		pClone->EstimatedHealth = hp;
	}

	// Create one clone of pCloneType, apply this spec's HP + the unit's veterancy,
	// and place it. Returns true when it lands on the map.
	bool MakeClone(BuildingClass* pFrom, TechnoTypeClass* pCloneType, HouseClass* pOwner,
		double hpPct, TechnoTypeExt::ExtData* pUExt, double vetSrc, bool asBuilt)
	{
		auto const pClone = static_cast<TechnoClass*>(pCloneType->CreateObject(pOwner));
		if (!pClone)
			return false;

		if (!PlaceClone(pFrom, pClone, pCloneType, asBuilt))
			return false;

		// Apply HP/veterancy AFTER placement. The ConsideredBuilt path routes the
		// clone through KickOutUnit, which re-initialises its health to full while
		// exiting -- setting HP beforehand was being clobbered (the "clones come out
		// full health" bug). Post-placement the value sticks.
		ApplyStrengthPct(pClone, pCloneType, hpPct);

		if (pUExt->Veterancy.IsActive())
			pClone->Veterancy.Veterancy = static_cast<float>(pUExt->Veterancy.Resolve(vetSrc));

		return true;
	}

	// Apply the Cloning.Mult.Blacklist exception (from EITHER side -- the building
	// lists the unit, or the unit lists the building) to a raw multiplier value.
	// A blacklisted pairing falls back to 1; <=0 is clamped to 0 (suppress).
	int ApplyMultBlacklist(int m, BuildingTypeExt::ExtData* pBExt, BuildingTypeClass* pBType,
		TechnoTypeExt::ExtData* pUExt, TechnoTypeClass* pUType)
	{
		if (m <= 1)
			return m < 0 ? 0 : m; // 0 suppresses; 1 (and <0 clamp) is a no-op

		if (pBExt->MultBlacklist.Contains(pUType))
			return 1;
		if (pUExt && pUExt->MultBlacklist.Contains(pBType))
			return 1;

		return m;
	}

	// Multiplier for BASE (per-source) clones: Cloning.Mult.
	int EffectiveMult(BuildingTypeExt::ExtData* pBExt, BuildingTypeClass* pBType,
		TechnoTypeExt::ExtData* pUExt, TechnoTypeClass* pUType)
	{
		return ApplyMultBlacklist(pBExt->CloningMult, pBExt, pBType, pUExt, pUType);
	}

	// Multiplier for SLOT-bonus clones: Cloning.Mult.Slots if set, else Cloning.Mult.
	int EffectiveSlotMult(BuildingTypeExt::ExtData* pBExt, BuildingTypeClass* pBType,
		TechnoTypeExt::ExtData* pUExt, TechnoTypeClass* pUType)
	{
		int const base = pBExt->CloningMultSlots.isset()
			? pBExt->CloningMultSlots.Get() : pBExt->CloningMult;
		return ApplyMultBlacklist(base, pBExt, pBType, pUExt, pUType);
	}

	// Should a clone made by this building, of this unit, be treated as "built"
	// (routed through KickOutUnit so production-detecting co-DLLs record it)?
	// Unset on both sides => false (silent spawn, no game change). One side set =>
	// that side. Both set => the heavier .Weight wins; the unit wins exact ties.
	bool ResolveConsideredBuilt(BuildingTypeExt::ExtData* pBExt, TechnoTypeExt::ExtData* pUExt)
	{
		bool const haveB = pBExt && pBExt->ConsideredBuilt.isset();
		bool const haveU = pUExt && pUExt->ConsideredBuilt.isset();

		if (!haveB && !haveU)
			return false;
		if (haveB && !haveU)
			return pBExt->ConsideredBuilt.Get();
		if (haveU && !haveB)
			return pUExt->ConsideredBuilt.Get();

		int const wB = pBExt->ConsideredBuiltWeight;
		int const wU = pUExt->ConsideredBuiltWeight;
		return (wU >= wB) ? pUExt->ConsideredBuilt.Get() : pBExt->ConsideredBuilt.Get();
	}

	// Produce our extra clones for a single primary-production event.
	//
	// Target semantics: each cloning-source building makes EXACTLY CloneCount
	// clones of the produced unit. Antares already makes some of those, so we
	// produce (CloneCount - antaresBaseFromThatSource) per source, where
	// antaresBase is 1 or 0 depending on whether Antares would clone from that
	// specific building. This makes CloneCount exact even in the "factory that
	// also clones" case, where Antares bails entirely (base 0 everywhere).
	void ProduceExtraClones(BuildingClass* pFactory, TechnoClass* pProduction)
	{
		auto const pOwner = pFactory->Owner;
		if (!pOwner)
			return;

		auto const pType = pProduction->GetTechnoType();
		auto const pExt = TechnoTypeExt::ExtMap.Find(pType);
		if (!pExt || !pExt->Cloneable)
			return;

		// Default clone type when a spec's CloneAs is unset: the ClonedAs fallback
		// if the modder set it (also the NACLON caveat), else the produced unit.
		TechnoTypeClass* const defaultAs = pExt->ClonedAsFallback.isset()
			? pExt->ClonedAsFallback.Get() : pType;

		double const vetSrc = pProduction->Veterancy.Veterancy;

		int made = 0;      // clones actually placed
		int attempted = 0; // clones we tried to make

		bool const isInfantry = (abstract_cast<InfantryClass*>(pProduction) != nullptr);
		bool const factoryNaval = pFactory->Type->Naval;

		// Owner power state drives the CloneAs.LowPower defect-type swap.
		bool const lowPower = pOwner->HasLowPower();

		// Did Antares' KickOutClones bail out entirely for this production? It
		// bails when the producing factory is itself a cloning vat, or is not an
		// infantry/unit factory (mirrors Antares Body.cpp:1140). When it bails it
		// made zero base clones from every source.
		auto const factoryKind = pFactory->Type->Factory;
		bool const antaresBailed = pFactory->Type->Cloning
			|| (factoryKind != InfantryTypeClass::AbsID
				&& factoryKind != UnitTypeClass::AbsID);

		int const baseSpecs = pExt->Clones.SpecCount();

		// When the unit sets ClonedAt=, that list is the EXCLUSIVE source set
		// (mirrors Antares): clone only at those buildings, ignoring the
		// Cloning=/CloningFacility= search.
		bool const hasClonedAt = !pExt->ClonedAt.empty();

		// --- base clone specs, per qualifying source building ---
		for (auto const pB : pOwner->Buildings)
		{
			if (!pB || pB->InLimbo)
				continue;

			auto const pBExt = BuildingTypeExt::ExtMap.Find(pB->Type);
			if (!pBExt)
				continue;

			bool isSource;
			int antaresBase;
			if (hasClonedAt)
			{
				// Explicit per-unit source list. Antares clones 1 from each owned
				// ClonedAt building (when it did not bail), regardless of that
				// building's own cloning flags.
				isSource = pExt->ClonedAt.Contains(pB->Type);
				antaresBase = (!antaresBailed && isSource) ? 1 : 0;
			}
			else if (isInfantry)
			{
				// Antares infantry clones come only from vanilla Cloning= vats
				// (and only when it didn't bail); we also treat CloningFacility=.
				isSource = pBExt->IsCloningSource();
				antaresBase = (!antaresBailed && pB->Type->Cloning) ? 1 : 0;
			}
			else
			{
				// Antares unit/naval clones come from CloningFacility= with a
				// matching Naval flag.
				bool const navalMatch = (pB->Type->Naval == factoryNaval);
				isSource = pBExt->CloningFacility && navalMatch;
				antaresBase = (!antaresBailed && isSource) ? 1 : 0;
			}

			if (!isSource)
				continue;

			int const mult = EffectiveMult(pBExt, pB->Type, pExt, pType);
			bool const asBuilt = ResolveConsideredBuilt(pBExt, pExt);

			// Each source makes (CloneAmount[i] * Cloning.Mult) clones of spec i;
			// Antares' one base clone counts against spec 0.
			for (int i = 0; i < baseSpecs; ++i)
			{
				int amount = pExt->Clones.AmountAt(i) * mult;
				if (i == 0)
					amount -= antaresBase;
				auto const pCloneType = pExt->Clones.AsAt(i, defaultAs, lowPower);

				for (int k = 0; k < amount; ++k)
				{
					if (!pExt->Clones.RollChanceAt(i))
						continue; // CloneChance says this one didn't spawn
					++attempted;
					double const hp = pExt->Clones.StrengthPctAt(i);
					made += MakeClone(pB, pCloneType, pOwner, hp, pExt, vetSrc, asBuilt) ? 1 : 0;
				}
			}
		}

		// --- slot clone specs, kicked from the producing factory ---
		auto const pFacExt = BuildingTypeExt::ExtMap.Find(pFactory->Type);
		int const slotMult = pFacExt ? EffectiveSlotMult(pFacExt, pFactory->Type, pExt, pType) : 1;
		bool const slotBuilt = ResolveConsideredBuilt(pFacExt, pExt);

		for (auto const& slot : pExt->Slots)
		{
			if (!slot.Satisfied(pOwner))
				continue;

			int const specs = slot.Clones.SpecCount();
			for (int j = 0; j < specs; ++j)
			{
				int const amount = slot.Clones.AmountAt(j) * slotMult;
				auto const pCloneType = slot.Clones.AsAt(j, defaultAs, lowPower);

				for (int k = 0; k < amount; ++k)
				{
					if (!slot.Clones.RollChanceAt(j))
						continue; // CloneSlotN.Chance says this one didn't spawn
					++attempted;
					double const hp = slot.Clones.StrengthPctAt(j);
					made += MakeClone(pFactory, pCloneType, pOwner, hp, pExt, vetSrc, slotBuilt) ? 1 : 0;
				}
			}
		}

		// Log what we tried vs what placed so off counts are easy to diagnose
		// (attempted<expected => tag/mult not read; made<attempted => placement).
		if (attempted > 0)
			Debug::Log("[CloningExt] %s from %s: made %d/%d (bailed=%d, specs=%d)\n",
				pType->ID, pFactory->Type->ID, made, attempted,
				antaresBailed ? 1 : 0, baseSpecs);
	}

	// A genuine production event kicks the unit out of a real infantry/unit
	// FACTORY. A clone is kicked out of a cloning VAT (Factory=none). This is the
	// reliable primary-vs-clone discriminator -- BuildingClass::Factory is tracked
	// house-side and is null here, so the old Factory->Object check never fired.
	bool IsProductionFactory(BuildingClass* pFactory)
	{
		if (!pFactory)
			return false;
		auto const k = pFactory->Type->Factory;
		return k == InfantryTypeClass::AbsID || k == UnitTypeClass::AbsID;
	}

	void Dispatch(BuildingClass* pFactory, TechnoClass* pProduction)
	{
		// Skip re-entry from our own extra-clone kicks (they come back through
		// these same hooks) -- that both stops recursion and stops us counting a
		// clone as a fresh production.
		if (CloningExt::ProducingExtras)
			return;

		// Only real production events trigger extras. Clones kicked from a vat
		// (Factory=none) fall out here, so Antares' base clones never get topped up.
		if (!pProduction || !IsProductionFactory(pFactory))
			return;

		CloningExt::ProducingExtras = true;
		// KickOutUnit toggles ScenarioInit off around clone creation the way
		// Antares does; mirror that so the game re-disables it as expected.
		--Unsorted::ScenarioInit;
		ProduceExtraClones(pFactory, pProduction);
		++Unsorted::ScenarioInit;
		CloningExt::ProducingExtras = false;
	}
}

DEFINE_HOOK(0x444DBC, BuildingClass_KickOutUnit_ExtraClones_Infantry, 0x5)
{
	GET(TechnoClass*, pProduction, EDI);
	GET(BuildingClass*, pFactory, ESI);

	Dispatch(pFactory, pProduction);
	return 0;
}

DEFINE_HOOK(0x44441A, BuildingClass_KickOutUnit_ExtraClones_NavalUnit, 0x6)
{
	GET(TechnoClass*, pProduction, EDI);
	GET(BuildingClass*, pFactory, ESI);

	Dispatch(pFactory, pProduction);
	return 0;
}

// Non-naval unit (vehicle) path. Antares occupies 0x4445F6 (size 5) and returns
// a non-zero jump, so chaining there would lose the race if Antares runs first.
// Instead we hook the 6-byte `call [eax+0x1E8]` at 0x4445F0 that sits IMMEDIATELY
// before it: 0x4445F0 + 6 == 0x4445F6, so our patch is adjacent to Antares' with
// zero overlap and no contention (verified: not in the encyclopedia registry, no
// inbound branches into 0x4445F0..F5). Our extras run, the stolen call executes
// (Syringe restores EAX/ECX first), then control falls into Antares' hook, which
// makes the base clone. ESI/EDI are the factory/Production throughout this branch.
DEFINE_HOOK(0x4445F0, BuildingClass_KickOutUnit_ExtraClones_Vehicle, 0x6)
{
	GET(TechnoClass*, pProduction, EDI);
	GET(BuildingClass*, pFactory, ESI);

	Dispatch(pFactory, pProduction);
	return 0;
}
