// CloningExt -- [<Type>.construct]: a profile that applies ONLY to units a
// factory actually built.
//
// The sibling of [<Type>.clone]. Where that one says "this is what a CLONE of
// me looks like", this one says "this is what a BUILT one looks like" -- so a
// unit that arrives any other way (superweapon, free unit, crate, paradrop,
// map-placed, DLL-spawned, or cloned) keeps the base type.
//
// ## Why 0x443C60 and not one of the four category branches
//
// BuildingClass::KickOutUnit is the closest thing the engine has to a "a
// factory finished this unit" event, and crucially paradrop, crates,
// map-placed and DLL-spawned units do NOT pass through it. Free units do not
// either: those go out through Grand_Opening's own NearByLocation placement
// (0x446AA9-0x446EE1), a different path entirely.
//
// The per-category branches (0x444119 / 0x444131 / 0x44531F / 0x443CCA) are the
// busiest cluster in the subsystem -- triple-hooked by Antares, Ares and Phobos
// -- and they hold the house in a DIFFERENT REGISTER at each site, so a handler
// copied between them is a silent wrong-pointer bug. The function ENTRY is
// unclaimed by every framework, is one site instead of four, and the
// encyclopedia records it as verified in-game for exactly this purpose: a
// built-only gate stamped here fixed paradropped units wrongly triggering an
// ability. See YR-Hook-Encyclopedia/encyclopedia/Building-Production-KickOut.md.
//
// ## What "constructed" means, precisely
//
//     pBuilding->Factory && pBuilding->Factory->Object == pTechno
//
// "This is the object the factory was producing." That is stronger than asking
// whether the building is a production factory, because it also excludes
// anything else the same building might kick out -- a clone (which never
// matches the factory's current product) or a free unit.
//
// ## Ordering against [<Type>.clone] -- read this before changing anything
//
// This hook runs at the function ENTRY, which is BEFORE the clone dispatch
// sites later in the same function. So by the time cloning runs, the primary
// product is already a .construct type, and asking it what to clone would
// yield GGI.construct -- or send us hunting for GGI.construct.clone.
//
// That is wrong on purpose-grounds, not just mechanically: you built a GGI, so
// the vat clones a GGI. A clone was never constructed, so cloning the
// construct-profile is incoherent. OriginalTypeOf() below records what the unit
// was before conversion, and Hooks.CloneDispatch.cpp resolves clones from that
// instead. The rule, stated once: CLONES ARE CLONES OF WHAT YOU BUILT.

#include <Ext/BuildingType/Body.h>

#include <Utilities/Debug.h>
#include <Utilities/Macro.h>

#include <BuildingClass.h>
#include <FactoryClass.h>
#include <TechnoClass.h>
#include <FootClass.h>
#include <UnitClass.h>
#include <UnitTypeClass.h>
#include <InfantryClass.h>
#include <InfantryTypeClass.h>
#include <AircraftClass.h>
#include <AircraftTypeClass.h>
#include <HouseClass.h>
#include <LocomotionClass.h>
#include <TemporalClass.h>

#include <string>
#include <unordered_map>

namespace
{
	// unit -> the type it was built as, before the construct profile replaced
	// it. Read by the clone dispatch so clones follow the BUILT type.
	std::unordered_map<TechnoClass*, TechnoTypeClass*> g_PreConstruct;

	// Where the instance keeps its own type pointer. There is no single
	// TechnoClass::Type -- each subclass declares its own typed field, so the
	// slot has to be chosen on RTTI.
	TechnoTypeClass** TypeSlot(TechnoClass* pThis)
	{
		switch (pThis->WhatAmI())
		{
		case AbstractType::Infantry:
			return reinterpret_cast<TechnoTypeClass**>(
				&static_cast<InfantryClass*>(pThis)->Type);
		case AbstractType::Unit:
			return reinterpret_cast<TechnoTypeClass**>(
				&static_cast<UnitClass*>(pThis)->Type);
		case AbstractType::Aircraft:
			return reinterpret_cast<TechnoTypeClass**>(
				&static_cast<AircraftClass*>(pThis)->Type);
		default:
			// Buildings are excluded deliberately. A foundation is written into
			// map cells at Unlimbo, so repointing the type leaves the old
			// footprint occupied and never claims the new one.
			return nullptr;
		}
	}

	// Find <id>.construct among the registered types. Cached, because this runs
	// per unit built.
	std::unordered_map<TechnoTypeClass*, TechnoTypeClass*> g_ProfileOf;
	bool g_MapBuilt = false;

	void BuildMap()
	{
		g_MapBuilt = true;

		std::unordered_map<std::string, TechnoTypeClass*> byID;
		for (int i = 0; i < TechnoTypeClass::Array.Count; ++i)
			if (auto const pT = TechnoTypeClass::Array.GetItem(i))
				byID.emplace(pT->ID, pT);

		int paired = 0;
		for (auto const& kv : byID)
		{
			const std::string& id = kv.first;
			if (id.size() < 11)
				continue;
			if (_stricmp(id.c_str() + (id.size() - 10), ".construct") != 0)
				continue;

			auto const parent = byID.find(id.substr(0, id.size() - 10));
			if (parent == byID.end())
			{
				Debug::Log("[CloningExt] WARN construct profile '%s' exists but its "
					"parent does not, so nothing will ever redirect to it\n",
					id.c_str());
				continue;
			}
			g_ProfileOf.emplace(parent->second, kv.second);
			++paired;
		}

		if (paired)
			Debug::Log("[CloningExt] construct profile map: %d profile(s) paired "
				"with a parent type\n", paired);
	}

	// Repoint the instance's type pointer, fixing up the state that does NOT
	// follow the pointer.
	//
	// This is much safer here than it would be mid-battle: the unit has just
	// been produced and has not been placed yet, so it is at full health (the
	// proportional HP rescale is therefore exact rather than lossy) and has
	// never moved. The full fix-up set is still applied rather than trimmed on
	// that reasoning -- "it is fresh so this cannot matter" is exactly the
	// assumption that produces a 1-HP unit.
	bool Repoint(TechnoClass* pThis, TechnoTypeClass* pTo)
	{
		TechnoTypeClass** const slot = TypeSlot(pThis);
		if (!slot)
			return false;

		TechnoTypeClass* const pFrom = *slot;
		if (!pFrom || !pTo || pFrom == pTo)
			return false;

		if (pFrom->WhatAmI() != pTo->WhatAmI())
		{
			Debug::Log("[CloningExt] CONSTRUCT refused %s -> %s: different "
				"categories\n", pFrom->ID, pTo->ID);
			return false;
		}

		// Sanity-check the target BEFORE touching the unit. A profile section
		// that did not get filled in carries Strength=0, and converting onto it
		// produces a unit with no health that also teleports, because a zeroed
		// Locomotor CLSID is not a locomotor. Refuse instead: a profile that
		// does nothing is a bug report, one that hollows out the unit is a
		// broken game.
		if (pTo->Strength <= 0)
		{
			Debug::Log("[CloningExt] CONSTRUCT REFUSED %s -> %s: target has "
				"Strength=%d. The profile section is incomplete - look for an "
				"'inherited N key(s)' line for %s at load.\n",
				pFrom->ID, pTo->ID, pTo->Strength, pTo->ID);
			return false;
		}

		auto* const pFoot = static_cast<FootClass*>(pThis);

		if (auto* const pTemporal = pThis->TemporalImUsing)
			if (pTemporal->Target)
				pTemporal->LetGo();

		HouseClass* const pOwner = pThis->Owner;
		const int oldHealth = pThis->Health;
		const int oldMax = pFrom->Strength;

		// House tallies are keyed BY TYPE, so the old type has to be untracked
		// before the pointer moves and the new one tracked after. The
		// RegisterLoss/RegisterGain pair is skipped while the unit is in limbo,
		// which it is here -- KickOutUnit has not placed it yet.
		if (!pThis->InLimbo)
			pOwner->RegisterLoss(pThis, false);
		pOwner->RemoveTracking(pThis);

		*slot = pTo;

		if (oldMax > 0)
			pThis->SetHealthPercentage(
				static_cast<double>(oldHealth) / static_cast<double>(oldMax));
		pThis->EstimatedHealth = pThis->Health;

		pOwner->AddTracking(pThis);
		if (!pThis->InLimbo)
			pOwner->RegisterGain(pThis, false);
		pOwner->RecheckTechTree = true;

		if (pThis->Ammo > pTo->Ammo)
		{
			pThis->Ammo = pTo->Ammo;
			pThis->Mark(MarkType::Change);
		}

		if (pTo->WhatAmI() == AbstractType::AircraftType)
			pThis->SecondaryFacing.SetROT(pTo->ROT);
		else
			pThis->PrimaryFacing.SetROT(pTo->ROT);

		// A zeroed CLSID is an unfilled field, not a locomotor; building one
		// from zeros is how a unit ends up teleporting.
		static const CLSID kNoLoco { };
		CLSID nowLoco { };
		ILocomotion* const pILoco = pFoot->Locomotor;
		if (pTo->Locomotor == kNoLoco)
		{
			Debug::Log("[CloningExt]   %s: no Locomotor set, keeping the current "
				"one rather than building one from zeros\n", pTo->ID);
		}
		else if (pILoco
			&& SUCCEEDED(static_cast<LocomotionClass*>(pILoco)->GetClassID(&nowLoco))
			&& nowLoco != pTo->Locomotor)
		{
			// Drain the piggyback stack first: releasing a piggybacked
			// locomotor from underneath leaves the unit stuck.
			while (LocomotionClass::End_Piggyback(pFoot->Locomotor))
				;
			if (auto newLoco = LocomotionClass::CreateInstance(pTo->Locomotor))
			{
				newLoco->Link_To_Object(pFoot);
				pFoot->Locomotor = std::move(newLoco);
			}
		}

		Debug::Log("[CloningExt] CONSTRUCT %s @%p: %s -> %s (built by a factory) "
			"hp %d/%d -> %d/%d\n",
			pFrom->ID, pThis, pFrom->ID, pTo->ID,
			oldHealth, oldMax, pThis->Health, pTo->Strength);
		return true;
	}
}

namespace CloningExt
{
	// What this unit was BUILT as, before a construct profile replaced its
	// type. Clones must follow this, not the construct profile: you built a
	// GGI, so the vat clones a GGI.
	TechnoTypeClass* OriginalTypeOf(TechnoClass* pThis)
	{
		auto const it = g_PreConstruct.find(pThis);
		return it == g_PreConstruct.end() ? nullptr : it->second;
	}

	void ForgetConstruct(TechnoClass* pThis)
	{
		g_PreConstruct.erase(pThis);
	}
}

DEFINE_HOOK(0x443C60, BuildingClass_KickOutUnit_ConstructProfile, 0x6)
{
	GET(BuildingClass*, pBuilding, ECX);
	GET_STACK(TechnoClass*, pTechno, 0x4);

	if (!pBuilding || !pTechno)
		return 0;

	// "This is the object the factory was producing." Excludes clones (they
	// never match the factory's current product) and anything else the same
	// building might kick out.
	if (!pBuilding->Factory || pBuilding->Factory->Object != pTechno)
		return 0;

	if (!g_MapBuilt)
		BuildMap();
	if (g_ProfileOf.empty())
		return 0;

	TechnoTypeClass* const pType = pTechno->GetTechnoType();
	if (!pType)
		return 0;

	auto const it = g_ProfileOf.find(pType);
	if (it == g_ProfileOf.end())
		return 0;

	// Record what it was built as BEFORE converting, so the clone dispatch
	// later in this same function clones the built type rather than the
	// construct profile.
	g_PreConstruct[pTechno] = pType;

	if (!Repoint(pTechno, it->second))
		g_PreConstruct.erase(pTechno);

	return 0;
}

// NOTE on cleanup: the pre-construct record is per object, so it has to die
// with the object -- but this file adds no destructor hook for it. CloningExt
// already sits on the engine's own pointer-invalidation broadcast
// (AnnounceInvalidPointer, 0x7258D0) for the removal queue, and that is the
// right place for any per-object map, so ForgetConstruct() is called from
// there instead of putting a second handler on TechnoClass's destructor.
