#include "Body.h"

#include <Utilities/Macro.h>

BuildingTypeExt::ExtContainer BuildingTypeExt::ExtMap;

// ============================================================================
// INI
// ============================================================================

void BuildingTypeExt::ExtData::LoadFromINIFile(CCINIClass* pINI)
{
	auto pID = this->OwnerObject()->ID;

	INI_EX exINI(pINI);

	// Antares' own key name, read independently. See Body.h.
	this->CloningFacility.Read(exINI, pID, "CloningFacility");

	this->CloningMult.Read(exINI, pID, "Cloning.Mult");
	this->MultBlacklist.Read(exINI, pID, "Cloning.Mult.Blacklist");
}

// ============================================================================
// Serialization -- type data is re-parsed from rules on every load, so there
// is nothing session-specific to persist. (Same reasoning as SuperWeaponExt.)
// ============================================================================

template <typename T>
void BuildingTypeExt::ExtData::Serialize(T&) { }

void BuildingTypeExt::ExtData::LoadFromStream(PhobosStreamReader& Stm)
{
	Extension<BuildingTypeClass>::LoadFromStream(Stm);
	this->Serialize(Stm);
}

void BuildingTypeExt::ExtData::SaveToStream(PhobosStreamWriter& Stm)
{
	Extension<BuildingTypeClass>::SaveToStream(Stm);
	this->Serialize(Stm);
}

// ============================================================================
// Container
// ============================================================================

BuildingTypeExt::ExtContainer::ExtContainer()
	: Container("BuildingTypeClass")
{ }

BuildingTypeExt::ExtContainer::~ExtContainer() = default;

// Container lifecycle. Addresses taken from Phobos (develop, pinned commit
// 47475624) via AcademyExt, which drives the same sites for its own container.
// All handlers return 0 so Syringe chains us with every other consumer.

DEFINE_HOOK(0x45E50C, BuildingTypeClass_CTOR_CloningExt, 0x6)
{
	GET(BuildingTypeClass*, pItem, EAX);
	BuildingTypeExt::ExtMap.TryAllocate(pItem);
	return 0;
}

DEFINE_HOOK(0x45E707, BuildingTypeClass_DTOR_CloningExt, 0x6)
{
	GET(BuildingTypeClass*, pItem, ESI);
	BuildingTypeExt::ExtMap.Remove(pItem);
	return 0;
}

DEFINE_HOOK_AGAIN(0x465300, BuildingTypeClass_SaveLoad_Prefix_CloningExt, 0x5)
DEFINE_HOOK(0x465010, BuildingTypeClass_SaveLoad_Prefix_CloningExt, 0x5)
{
	GET_STACK(BuildingTypeClass*, pItem, 0x4);
	GET_STACK(IStream*, pStm, 0x8);
	BuildingTypeExt::ExtMap.PrepareStream(pItem, pStm);
	return 0;
}

DEFINE_HOOK(0x4652ED, BuildingTypeClass_Load_Suffix_CloningExt, 0x7)
{
	BuildingTypeExt::ExtMap.LoadStatic();
	return 0;
}

DEFINE_HOOK(0x46536A, BuildingTypeClass_Save_Suffix_CloningExt, 0x7)
{
	BuildingTypeExt::ExtMap.SaveStatic();
	return 0;
}

DEFINE_HOOK(0x464A49, BuildingTypeClass_LoadFromINI_CloningExt, 0xA)
{
	GET(BuildingTypeClass*, pItem, EBP);
	GET_STACK(CCINIClass*, pINI, 0x364);
	BuildingTypeExt::ExtMap.LoadFromINI(pItem, pINI);
	return 0;
}
