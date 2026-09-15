// CloningExt -- per-clone initialisation (veterancy + initial strength).
//
// Hook: 0x443C81, size 7. This is the ENTRY of BuildingClass::KickOutUnit's
// dispatch body (function starts 0x443C60): after `sub esp,0x130; push
// ebx/ebp/esi/edi; mov edi,[esp+0x144]; mov esi,ecx`. Register layout VERIFIED
// against gamemd disassembly AND Phobos' own hook here
// (BuildingClass_ExitObject_InitialClonedHealth):
//
//     ESI = BuildingClass*  (the building kicking the object out)
//     EDI = FootClass*      (the object being kicked out)
//
// NOTE: the web-session context block said "ECX=clone, ESI=building" -- the ECX
// half was wrong; the object is in EDI. Corrected here from the disassembly.
//
// CONTENTION: only Phobos hooks 0x443C81, and both its handler and ours return
// 0, so Syringe chains them. Antares does NOT touch this address -- it is the
// un-contended convergence point every kicked-out unit (clones included) passes
// through, which is exactly why the per-clone properties live here.
//
// Every clone Antares kicks (B->KickOutUnit(Clone,...)) re-enters this dispatch,
// so setting HP/veterancy here covers Antares-produced clones as well as vanilla
// CloningVats clones and our own extra clones.

#include <Ext/TechnoType/Body.h>
#include <Ext/BuildingType/Body.h>
#include <Cloning/State.h>

#include <Utilities/Macro.h>

#include <BuildingClass.h>
#include <FootClass.h>
#include <InfantryTypeClass.h>
#include <UnitTypeClass.h>
#include <Helpers/Cast.h>

namespace
{
	// Is a real infantry/unit production factory (Factory=infantry|unit), as
	// opposed to a dedicated cloning vat (Factory=none)?
	bool IsProductionFactory(BuildingClass* pBuilding)
	{
		auto const k = pBuilding->Type->Factory;
		return k == InfantryTypeClass::AbsID || k == UnitTypeClass::AbsID;
	}

	// Is `pExiting` a CLONE leaving `pBuilding` (as opposed to a building's own
	// primary product)? Two independent signals, either of which means "clone":
	//
	//   1. We are mid-dispatch producing our own extra clones (ProducingExtras) --
	//      covers the "factory that also clones" case, where the clone exits the
	//      same barracks that made the primary, so building identity alone can't
	//      tell them apart.
	//   2. The building is a cloning source but NOT a production factory, i.e. a
	//      dedicated cloning vat -- everything it kicks out is a clone (this also
	//      catches Antares'/vanilla base clones from real Cloning= vats).
	//
	// The primary product of a factory-that-clones is excluded by both: it is not
	// one of our extras, and it exits a production factory.
	bool IsCloneExit(BuildingClass* pBuilding, TechnoClass* pExiting)
	{
		if (!pBuilding || !pExiting)
			return false;

		auto const pBExt = BuildingTypeExt::ExtMap.Find(pBuilding->Type);
		if (!pBExt || !pBExt->IsCloningSource())
			return false;

		return CloningExt::ProducingExtras || !IsProductionFactory(pBuilding);
	}
}

DEFINE_HOOK(0x443C81, BuildingClass_KickOutUnit_CloneInit_CloningExt, 0x7)
{
	GET(BuildingClass*, pBuilding, ESI);
	GET(FootClass*, pExiting, EDI);

	// Clones WE make get their per-spec HP and veterancy set in the dispatch layer
	// (Hooks.CloneDispatch.cpp), so skip them here to avoid double-applying. This
	// hook only handles clones we don't create: Antares'/vanilla base clones from a
	// dedicated vat, which take the unit's FIRST base spec's settings.
	if (CloningExt::ProducingExtras)
		return 0;

	if (!IsCloneExit(pBuilding, pExiting))
		return 0;

	// Full NACLON override: if this (dedicated) vat is flagged to let CloningExt
	// own its whole output, ABORT Antares' own base clone here. Returning the
	// KickOutUnit "Failed" epilogue (0x445696: pops the 4 prologue regs, eax=0,
	// add esp,0x130, ret 8 -- stack-correct from 0x443C81) makes Antares' own
	// `if (KickOutUnit != Succeeded) Clone->UnInit()` clean the clone up. Our
	// dispatch layer then produces every clone from this vat with full spec control.
	if (auto const pBExt = BuildingTypeExt::ExtMap.Find(pBuilding->Type))
	{
		if (pBExt->OverrideBaseClone)
			return 0x445696; // suppress Antares' base clone (it UnInits it)
	}

	auto const pType = pExiting->GetTechnoType();
	auto const pExt = TechnoTypeExt::ExtMap.Find(pType);
	if (!pExt || !pExt->Cloneable)
		return 0;

	// --- veterancy ---------------------------------------------------------
	if (pExt->Veterancy.IsActive())
	{
		double const src = pExiting->Veterancy.Veterancy;
		double const resolved = pExt->Veterancy.Resolve(src);
		pExiting->Veterancy.Veterancy = static_cast<float>(resolved);
	}

	// --- initial strength (first base spec, percent) -----------------------
	if (pExt->Clones.HasStrength())
	{
		double const pct = pExt->Clones.StrengthPctAt(0);
		int const full = pType->Strength;
		int strength = static_cast<int>(full * pct / 100.0);
		if (strength < 1) strength = 1;
		if (strength > full) strength = full;

		pExiting->Health = strength;
		pExiting->EstimatedHealth = strength;
	}

	return 0;
}
