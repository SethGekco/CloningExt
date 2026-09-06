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
	// Returns true when a clone was successfully placed.
	//
	// We DON'T use KickOutUnit: it only ever clears one exit cell per production,
	// so every clone after the first failed and was discarded (the "always one
	// clone" bug). Instead we ask the game's own placement finder,
	// MapClass::NearByLocation, for a free cell near the building and Unlimbo the
	// clone there. NearByLocation skips cells that are already occupied -- including
	// clones we just placed this frame -- so repeated calls scatter the clones.
	// This is the same NearByLocation + Unlimbo pattern Antares uses to deliver
	// units to the map (SWTypes/UnitDelivery.cpp).
	bool KickOneClone(BuildingClass* pFrom, TechnoTypeClass* pCloneType, HouseClass* pOwner)
	{
		auto const pClone = static_cast<TechnoClass*>(pCloneType->CreateObject(pOwner));
		if (!pClone)
			return false;

		auto const pOriginCell = MapClass::Instance.GetCellAt(pFrom->Location);
		CellStruct const origin = pOriginCell ? pOriginCell->MapCoords : CellStruct::Empty;

		CellStruct const place = MapClass::Instance.NearByLocation(
			origin, pCloneType->SpeedType, -1, pCloneType->MovementZone,
			false, 1, 1, false, false, false, false, CellStruct::Empty, false, false);

		auto const pCell = MapClass::Instance.TryGetCellAt(place);
		if (!pCell)
		{
			pClone->UnInit();
			return false;
		}

		auto const xyz = pCell->GetCoordsWithBridge();
		auto const facing = static_cast<DirType>(
			(MapClass::GetCellIndex(pCell->MapCoords) & 7u) << 5);

		pClone->QueueMission(Mission::Guard, false);
		if (!pClone->Unlimbo(xyz, facing))
		{
			pClone->UnInit();
			return false;
		}
		return true;
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
				// clones; subtract the one Antares already made from this source.
				int mult = pBExt->CloningMult;
				if (mult < 0)
					mult = 0;
				int const mine = cloneCount * mult - antaresBase;
				for (int k = 0; k < mine; ++k)
				{
					++attempted;
					made += KickOneClone(pB, pCloneType, pOwner) ? 1 : 0;
				}
			}
		}

		// --- slot bonus: additive extra clones, kicked from the factory ---
		for (int k = 0; k < slotBonus; ++k)
		{
			++attempted;
			made += KickOneClone(pFactory, pCloneType, pOwner) ? 1 : 0;
		}

		// Log both what we tried and what actually placed, so an off count is easy
		// to diagnose (attempted<expected => tag/mult not read; made<attempted =>
		// placement failed).
		if (attempted > 0)
			Debug::Log("[CloningExt] %s from %s: made %d/%d (bailed=%d, count=%d, slots=%d)\n",
				pType->ID, pFactory->Type->ID, made, attempted,
				antaresBailed ? 1 : 0, cloneCount, slotBonus);
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
