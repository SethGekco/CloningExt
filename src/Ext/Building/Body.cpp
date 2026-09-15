#include "Body.h"

#include <Utilities/Macro.h>

BuildingExt::ExtContainer BuildingExt::ExtMap;

// ============================================================================
// Serialization -- same manual [count][(key,value)...] scheme as HouseExt, since
// the stream helpers have no std::map support.
// ============================================================================

void BuildingExt::ExtData::LoadFromStream(PhobosStreamReader& Stm)
{
	Extension<BuildingClass>::LoadFromStream(Stm);

	this->LocalEscalationCounts.clear();
	int n = 0;
	Stm.Process(n);
	for (int i = 0; i < n; ++i)
	{
		int key = 0;
		int value = 0;
		Stm.Process(key);
		Stm.Process(value);
		this->LocalEscalationCounts[key] = value;
	}
}

void BuildingExt::ExtData::SaveToStream(PhobosStreamWriter& Stm)
{
	Extension<BuildingClass>::SaveToStream(Stm);

	int n = static_cast<int>(this->LocalEscalationCounts.size());
	Stm.Process(n);
	for (auto& kv : this->LocalEscalationCounts)
	{
		int key = kv.first;
		int value = kv.second;
		Stm.Process(key);
		Stm.Process(value);
	}
}

// ============================================================================
// Container
// ============================================================================

BuildingExt::ExtContainer::ExtContainer()
	: Container("BuildingClass")
{ }

BuildingExt::ExtContainer::~ExtContainer() = default;

// Instance-level lifecycle. Addresses from Phobos (develop, pinned 47475624)
// src/Ext/Building/Body.cpp; all handlers return 0 so Syringe chains us with
// Phobos' own BuildingClass container.

DEFINE_HOOK(0x43BCBD, BuildingClass_CTOR_CloningExt, 0x6)
{
	GET(BuildingClass*, pItem, ESI);
	BuildingExt::ExtMap.TryAllocate(pItem);
	return 0;
}

DEFINE_HOOK(0x43C022, BuildingClass_DTOR_CloningExt, 0x6)
{
	GET(BuildingClass*, pItem, ESI);
	BuildingExt::ExtMap.Remove(pItem);
	return 0;
}

DEFINE_HOOK_AGAIN(0x454190, BuildingClass_SaveLoad_Prefix_CloningExt, 0x5)
DEFINE_HOOK(0x453E20, BuildingClass_SaveLoad_Prefix_CloningExt, 0x5)
{
	GET_STACK(BuildingClass*, pItem, 0x4);
	GET_STACK(IStream*, pStm, 0x8);
	BuildingExt::ExtMap.PrepareStream(pItem, pStm);
	return 0;
}

DEFINE_HOOK(0x45417E, BuildingClass_Load_Suffix_CloningExt, 0x5)
{
	BuildingExt::ExtMap.LoadStatic();
	return 0;
}

DEFINE_HOOK(0x454244, BuildingClass_Save_Suffix_CloningExt, 0x7)
{
	BuildingExt::ExtMap.SaveStatic();
	return 0;
}
