// CloningExt -- extra-clone dispatch (CloneCount + prerequisite/house slots).
//
// These hooks CHAIN AFTER Antares. Antares owns the cloning dispatch and kicks
// exactly one base clone per qualifying source; we run at the same sites, after
// it, to add:
//   * (CloneCount - 1) extra clones per qualifying source building, and
//   * the extra clones granted by any satisfied prerequisite/house slot.
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
// RECURSION: every clone we (or Antares) kick re-enters KickOutUnit and reaches
// these hooks again with EDI=the clone. We only act on the PRIMARY product --
// identified by pFactory->Factory->Object == Production -- so clones (which are
// not in any factory) are skipped and there is no runaway.

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

	// Create and place one clone. Returns true when it ends up on the map.
	//
	// asBuilt: when true, the clone is first run through BuildingClass::KickOutUnit
	// so co-DLLs that record production at its entry (GiftBox/Host @0x443C60) mark
	// it "built". KickOutUnit places it when the exit is clear; when it can't, the
	// clone is left in limbo and we scatter it via Unlimbo -- either way the mark
	// already happened at the entry. asBuilt=false (the default) skips KickOutUnit
	// entirely, so with no built-detection DLL loaded the tag changes nothing.
	bool KickOneClone(BuildingClass* pFrom, TechnoTypeClass* pCloneType,
		HouseClass* pOwner, bool asBuilt)
	{
		auto const pClone = static_cast<TechnoClass*>(pCloneType->CreateObject(pOwner));
		if (!pClone)
			return false;

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

	// The Cloning.Mult a building applies to a given unit, honouring the
	// Cloning.Mult.Blacklist exception from EITHER side (the building lists the
	// unit, or the unit lists the building). A blacklisted pairing falls back to 1.
	int EffectiveMult(BuildingTypeExt::ExtData* pBExt, BuildingTypeClass* pBType,
		TechnoTypeExt::ExtData* pUExt, TechnoTypeClass* pUType)
	{
		int const m = pBExt->CloningMult;
		if (m <= 1)
			return m < 0 ? 0 : m; // 0 suppresses; 1 (and <0 clamp) is a no-op

		if (pBExt->MultBlacklist.Contains(pUType))
			return 1;
		if (pUExt && pUExt->MultBlacklist.Contains(pBType))
			return 1;

		return m;
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

		int const cloneCount = pExt->CloneCount;
		int const slotBonus = pExt->ResolveSlotBonus(pOwner);
		if (cloneCount <= 0 && slotBonus <= 0)
			return;

		// The clone comes out as the produced type itself. NOTE: this does not
		// honour Antares' ClonedAs= override (which lives in Antares' ext); the
		// base clone Antares makes still respects it, only our EXTRAS use the
		// produced type. Documented in INI_REFERENCE.md.
		auto const pCloneType = pType;

		int made = 0;      // clones actually placed
		int attempted = 0; // clones we tried to make (reflects CloneCount * mult + slots)

		bool const isInfantry = (abstract_cast<InfantryClass*>(pProduction) != nullptr);

		// Did Antares' KickOutClones bail out entirely for this production? It
		// bails when the producing factory is itself a cloning vat, or is not an
		// infantry/unit factory (mirrors Antares Body.cpp:1140). When it bails it
		// made zero base clones from every source.
		auto const factoryKind = pFactory->Type->Factory;
		bool const antaresBailed = pFactory->Type->Cloning
			|| (factoryKind != InfantryTypeClass::AbsID
				&& factoryKind != UnitTypeClass::AbsID);

		// --- per-source: top each qualifying building up to CloneCount ---
		if (cloneCount > 0)
		{
			bool const factoryNaval = pFactory->Type->Naval;

			for (auto const pB : pOwner->Buildings)
			{
				if (!pB || pB->InLimbo)
					continue;

				auto const pBExt = BuildingTypeExt::ExtMap.Find(pB->Type);
				if (!pBExt)
					continue;

				bool isSource;
				int antaresBase;
				if (isInfantry)
				{
					// Antares infantry clones come only from vanilla Cloning=
					// vats (and only when it didn't bail); we additionally treat
					// our CloningFacility= as a source.
					isSource = pBExt->IsCloningSource();
					antaresBase = (!antaresBailed && pB->Type->Cloning) ? 1 : 0;
				}
				else
				{
					// Antares unit/naval clones come from CloningFacility= with
					// a matching Naval flag.
					bool const navalMatch = (pB->Type->Naval == factoryNaval);
					isSource = pBExt->CloningFacility && navalMatch;
					antaresBase = (!antaresBailed && isSource) ? 1 : 0;
				}

				if (!isSource)
					continue;

				// Each source makes CloneCount * this building's Cloning.Mult
				// clones (blacklist exceptions honoured); subtract the one Antares
				// already made from this source.
				int const mult = EffectiveMult(pBExt, pB->Type, pExt, pType);
				int const mine = cloneCount * mult - antaresBase;
				bool const asBuilt = ResolveConsideredBuilt(pBExt, pExt);
				for (int k = 0; k < mine; ++k)
				{
					++attempted;
					made += KickOneClone(pB, pCloneType, pOwner, asBuilt) ? 1 : 0;
				}
			}
		}

		// --- slot bonus: additive extra clones, kicked from the factory. These
		// scale by the producing building's Cloning.Mult too (same blacklist rule).
		auto const pFacExt = BuildingTypeExt::ExtMap.Find(pFactory->Type);
		int const slotMult = pFacExt ? EffectiveMult(pFacExt, pFactory->Type, pExt, pType) : 1;
		int const slotTotal = slotBonus * slotMult;
		bool const slotBuilt = ResolveConsideredBuilt(pFacExt, pExt);
		for (int k = 0; k < slotTotal; ++k)
		{
			++attempted;
			made += KickOneClone(pFactory, pCloneType, pOwner, slotBuilt) ? 1 : 0;
		}

		// Log both what we tried and what actually placed, so an off count is easy
		// to diagnose (attempted<expected => tag/mult not read; made<attempted =>
		// placement failed).
		if (attempted > 0)
			Debug::Log("[CloningExt] %s from %s: made %d/%d (bailed=%d, count=%d, slots=%d)\n",
				pType->ID, pFactory->Type->ID, made, attempted,
				antaresBailed ? 1 : 0, cloneCount, slotTotal);
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
