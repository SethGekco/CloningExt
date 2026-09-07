#include "Body.h"

#include <Utilities/Macro.h>

TechnoTypeExt::ExtContainer TechnoTypeExt::ExtMap;

// ============================================================================
// INI
// ============================================================================

void TechnoTypeExt::ExtData::LoadFromINIFile(CCINIClass* pINI)
{
	auto pID = this->OwnerObject()->ID;

	INI_EX exINI(pINI);

	this->CloneCount.Read(exINI, pID, "CloneCount");
	this->Cloneable.Read(exINI, pID, "Cloneable");
	this->MultBlacklist.Read(exINI, pID, "Cloning.Mult.Blacklist");

	// Veterancy controls.
	this->Veterancy.Ratio.Read(exINI, pID, "CloneVeterancy.Ratio");
	this->Veterancy.Cap.Read(exINI, pID, "CloneVeterancy.Cap");
	this->Veterancy.Inherit.Read(exINI, pID, "CloneVeterancy.Inherit");
	this->Veterancy.InheritAcademy.Read(exINI, pID, "CloneVeterancy.Inherit.Academy");
	this->Veterancy.InheritStolenTech.Read(exINI, pID, "CloneVeterancy.Inherit.StolenTech");
	this->Veterancy.InheritCountryBonus.Read(exINI, pID, "CloneVeterancy.Inherit.CountryBonus");

	// Initial strength.
	this->Strength.Fraction.Read(exINI, pID, "CloneInitialStrength");
	this->Strength.FractionMin.Read(exINI, pID, "CloneInitialStrength.Min");

	// Prerequisite/house-gated extra slots. Count-prefixed so parsing is O(N)
	// and a modder never has to leave gaps.
	this->Slots.clear();
	int count = 0;
	count = pINI->ReadInteger(pID, "CloneSlots.Count", 0);
	if (count > 0)
	{
		if (count > 64)
			count = 64; // sanity clamp; nobody needs more and it bounds the loop
		this->Slots.resize(static_cast<size_t>(count));
		for (int i = 0; i < count; ++i)
			this->Slots[static_cast<size_t>(i)].Read(exINI, pID, i);
	}
}

int TechnoTypeExt::ExtData::ResolveSlotBonus(HouseClass* pHouse) const
{
	int bonus = 0;
	for (auto const& slot : this->Slots)
	{
		if (slot.Satisfied(pHouse))
			bonus += slot.Amount;
	}
	return bonus;
}

// ============================================================================
// Serialization -- all fields are type data re-parsed from rules on every load,
// so nothing here is persisted (matches SuperWeaponExt).
// ============================================================================

template <typename T>
void TechnoTypeExt::ExtData::Serialize(T&) { }

void TechnoTypeExt::ExtData::LoadFromStream(PhobosStreamReader& Stm)
{
	Extension<TechnoTypeClass>::LoadFromStream(Stm);
	this->Serialize(Stm);
}

void TechnoTypeExt::ExtData::SaveToStream(PhobosStreamWriter& Stm)
{
	Extension<TechnoTypeClass>::SaveToStream(Stm);
	this->Serialize(Stm);
}

// ============================================================================
// Container
// ============================================================================

TechnoTypeExt::ExtContainer::ExtContainer()
	: Container("TechnoTypeClass")
{ }

TechnoTypeExt::ExtContainer::~ExtContainer() = default;

// Container lifecycle. TechnoTypeClass has a shared base CTOR/DTOR, so ONE hook
// pair covers Infantry/Unit/Aircraft/BuildingType. Addresses copied from Phobos
// (develop, pinned 47475624) via SuperWeaponExt; every handler returns 0 so
// Syringe chains us with Phobos'/Antares' own TechnoType containers.

DEFINE_HOOK(0x711835, TechnoTypeClass_CTOR_CloningExt, 0x5)
{
	GET(TechnoTypeClass*, pItem, ESI);
	TechnoTypeExt::ExtMap.TryAllocate(pItem);
	return 0;
}

DEFINE_HOOK(0x711AE0, TechnoTypeClass_DTOR_CloningExt, 0x5)
{
	GET(TechnoTypeClass*, pItem, ECX);
	TechnoTypeExt::ExtMap.Remove(pItem);
	return 0;
}

DEFINE_HOOK_AGAIN(0x716DC0, TechnoTypeClass_SaveLoad_Prefix_CloningExt, 0x5)
DEFINE_HOOK(0x7162F0, TechnoTypeClass_SaveLoad_Prefix_CloningExt, 0x6)
{
	GET_STACK(TechnoTypeClass*, pItem, 0x4);
	GET_STACK(IStream*, pStm, 0x8);
	TechnoTypeExt::ExtMap.PrepareStream(pItem, pStm);
	return 0;
}

DEFINE_HOOK(0x716DAC, TechnoTypeClass_Load_Suffix_CloningExt, 0xA)
{
	TechnoTypeExt::ExtMap.LoadStatic();
	return 0;
}

DEFINE_HOOK(0x717094, TechnoTypeClass_Save_Suffix_CloningExt, 0x5)
{
	TechnoTypeExt::ExtMap.SaveStatic();
	return 0;
}

DEFINE_HOOK(0x716123, TechnoTypeClass_LoadFromINI_CloningExt, 0x5)
{
	GET(TechnoTypeClass*, pItem, EBP);
	GET_STACK(CCINIClass*, pINI, 0x380);
	TechnoTypeExt::ExtMap.LoadFromINI(pItem, pINI);
	return 0;
}
