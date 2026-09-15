#include "Body.h"

#include <Utilities/Macro.h>

HouseExt::ExtContainer HouseExt::ExtMap;

// ============================================================================
// Serialization
//
// The stream helpers have no std::map support, so the escalation map is written
// as [count][ (key,value) x count ] using plain int Process calls, symmetric
// between save and load.
// ============================================================================

void HouseExt::ExtData::LoadFromStream(PhobosStreamReader& Stm)
{
	Extension<HouseClass>::LoadFromStream(Stm);

	this->EscalationCounts.clear();
	int n = 0;
	Stm.Process(n);
	for (int i = 0; i < n; ++i)
	{
		int key = 0;
		int value = 0;
		Stm.Process(key);
		Stm.Process(value);
		this->EscalationCounts[key] = value;
	}
}

void HouseExt::ExtData::SaveToStream(PhobosStreamWriter& Stm)
{
	Extension<HouseClass>::SaveToStream(Stm);

	int n = static_cast<int>(this->EscalationCounts.size());
	Stm.Process(n);
	for (auto& kv : this->EscalationCounts)
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

HouseExt::ExtContainer::ExtContainer()
	: Container("HouseClass")
{ }

HouseExt::ExtContainer::~ExtContainer() = default;

// Container lifecycle. Addresses copied from Phobos (develop, pinned 47475624)
// via AcademyExt, which drives the same sites for its own HouseClass container.
// All handlers return 0 so Syringe chains us with every other consumer.

DEFINE_HOOK(0x4F6532, HouseClass_CTOR_CloningExt, 0x5)
{
	GET(HouseClass*, pItem, EAX);
	HouseExt::ExtMap.TryAllocate(pItem);
	return 0;
}

DEFINE_HOOK(0x4F7371, HouseClass_DTOR_CloningExt, 0x6)
{
	GET(HouseClass*, pItem, ESI);
	HouseExt::ExtMap.Remove(pItem);
	return 0;
}

DEFINE_HOOK_AGAIN(0x504080, HouseClass_SaveLoad_Prefix_CloningExt, 0x5)
DEFINE_HOOK(0x503040, HouseClass_SaveLoad_Prefix_CloningExt, 0x5)
{
	GET_STACK(HouseClass*, pItem, 0x4);
	GET_STACK(IStream*, pStm, 0x8);
	HouseExt::ExtMap.PrepareStream(pItem, pStm);
	return 0;
}

DEFINE_HOOK(0x504069, HouseClass_Load_Suffix_CloningExt, 0x7)
{
	HouseExt::ExtMap.LoadStatic();
	return 0;
}

DEFINE_HOOK(0x5046DE, HouseClass_Save_Suffix_CloningExt, 0x7)
{
	HouseExt::ExtMap.SaveStatic();
	return 0;
}
