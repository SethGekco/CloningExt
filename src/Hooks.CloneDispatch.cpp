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

#include <BuildingClass.h>
#include <InfantryClass.h>
#include <FactoryClass.h>
#include <HouseClass.h>
#include <Unsorted.h>
#include <Helpers/Cast.h>

namespace
{
	void KickOneClone(BuildingClass* pFrom, TechnoTypeClass* pCloneType, HouseClass* pOwner)
	{
		auto const pClone = static_cast<TechnoClass*>(pCloneType->CreateObject(pOwner));
		if (!pClone)
			return;

		if (pFrom->KickOutUnit(pClone, CellStruct::Empty) != KickOutResult::Succeeded)
			pClone->UnInit();
	}

	// Produce our extra clones for a single primary-production event.
	void ProduceExtraClones(BuildingClass* pFactory, TechnoClass* pProduction)
	{
		auto const pOwner = pFactory->Owner;
		if (!pOwner)
			return;

		auto const pType = pProduction->GetTechnoType();
		auto const pExt = TechnoTypeExt::ExtMap.Find(pType);
		if (!pExt || !pExt->Cloneable)
			return;

		int const perSource = pExt->CloneCount > 1 ? pExt->CloneCount - 1 : 0;
		int const slotBonus = pExt->ResolveSlotBonus(pOwner);
		if (perSource <= 0 && slotBonus <= 0)
			return;

		// The clone comes out as the produced type itself. NOTE: this does not
		// honour Antares' ClonedAs= override (which lives in Antares' ext); the
		// base clone Antares makes still respects it, only our EXTRAS use the
		// produced type. Documented in INI_REFERENCE.md.
		auto const pCloneType = pType;

		bool const isInfantry = (abstract_cast<InfantryClass*>(pProduction) != nullptr);

		// --- per-source extras: (CloneCount-1) from each qualifying building ---
		if (perSource > 0)
		{
			for (auto const pB : pOwner->Buildings)
			{
				if (!pB || pB->InLimbo)
					continue;

				auto const pBExt = BuildingTypeExt::ExtMap.Find(pB->Type);
				if (!pBExt)
					continue;

				bool isSource;
				if (isInfantry)
					isSource = pBExt->IsCloningSource();
				else
					isSource = pBExt->CloningFacility
						&& (pB->Type->Naval == pFactory->Type->Naval);

				if (!isSource)
					continue;

				for (int k = 0; k < perSource; ++k)
					KickOneClone(pB, pCloneType, pOwner);
			}
		}

		// --- slot bonus: kicked from the producing factory (always present) ---
		for (int k = 0; k < slotBonus; ++k)
			KickOneClone(pFactory, pCloneType, pOwner);
	}

	// Only the primary product triggers our extras; clones do not (they are not
	// the object in the factory's production slot). This also prevents recursion.
	bool IsPrimaryProduction(BuildingClass* pFactory, TechnoClass* pProduction)
	{
		return pFactory
			&& pFactory->Factory
			&& pFactory->Factory->Object == pProduction;
	}

	void Dispatch(BuildingClass* pFactory, TechnoClass* pProduction)
	{
		if (!pProduction || !IsPrimaryProduction(pFactory, pProduction))
			return;

		// KickOutUnit toggles ScenarioInit off around clone creation the way
		// Antares does; mirror that so the game re-disables it as expected.
		--Unsorted::ScenarioInit;
		ProduceExtraClones(pFactory, pProduction);
		++Unsorted::ScenarioInit;
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
