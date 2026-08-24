#pragma once

// CloneSlot -- one conditionally-unlocked source of extra clones.
//
// A slot grants `Amount` additional clones of the produced unit when its
// prerequisite/house conditions are met by the PRODUCING house. This is how the
// "extra cloning slots gated by prerequisite or required country" feature is
// expressed. All four gates are optional and independent; an empty gate is a
// pass. Both polarities are supported per the request:
//
//   CloneSlotN.Prerequisite=          house must own ALL of these (positive)
//   CloneSlotN.Prerequisite.Negative= house must own NONE of these (negative)
//   CloneSlotN.RequiredHouses=        house Country must be one of these
//   CloneSlotN.ForbiddenHouses=       house Country must NOT be one of these
//   CloneSlotN.Amount=                clones granted when satisfied (default 1)
//
// Requirement semantics deliberately mirror how a modder reads them: positive
// prerequisites are AND-combined (own every listed building), which matches the
// vanilla prerequisite list convention.

#include <Utilities/Container.h>
#include <Utilities/TemplateDef.h>

#include <BuildingTypeClass.h>
#include <HouseTypeClass.h>
#include <HouseClass.h>

#include <cstdio>
#include <vector>

struct CloneSlot
{
	ValueableVector<BuildingTypeClass*> Prerequisite;         // own ALL
	ValueableVector<BuildingTypeClass*> PrerequisiteNegative; // own NONE
	ValueableVector<HouseTypeClass*>    RequiredHouses;       // Country in-list
	ValueableVector<HouseTypeClass*>    ForbiddenHouses;      // Country not-in-list
	Valueable<int>                      Amount { 1 };

	// Parse CloneSlot<index>.* from the given section.
	void Read(INI_EX& exINI, const char* section, int index)
	{
		char key[0x40];

		_snprintf_s(key, sizeof(key), "CloneSlot%d.Prerequisite", index);
		this->Prerequisite.Read(exINI, section, key);

		_snprintf_s(key, sizeof(key), "CloneSlot%d.Prerequisite.Negative", index);
		this->PrerequisiteNegative.Read(exINI, section, key);

		_snprintf_s(key, sizeof(key), "CloneSlot%d.RequiredHouses", index);
		this->RequiredHouses.Read(exINI, section, key);

		_snprintf_s(key, sizeof(key), "CloneSlot%d.ForbiddenHouses", index);
		this->ForbiddenHouses.Read(exINI, section, key);

		_snprintf_s(key, sizeof(key), "CloneSlot%d.Amount", index);
		this->Amount.Read(exINI, section, key);
	}

	// Does the producing house satisfy every declared gate?
	bool Satisfied(HouseClass* pHouse) const
	{
		if (!pHouse)
			return false;

		// Positive prerequisites: must own (built + present) every listed type.
		for (auto const pBld : this->Prerequisite)
		{
			if (pBld && pHouse->CountOwnedAndPresent(pBld) <= 0)
				return false;
		}

		// Negative prerequisites: must own none of these.
		for (auto const pBld : this->PrerequisiteNegative)
		{
			if (pBld && pHouse->CountOwnedAndPresent(pBld) > 0)
				return false;
		}

		// Required houses (countries): if any listed, the house Country must match.
		if (!this->RequiredHouses.empty())
		{
			bool matched = false;
			for (auto const pHType : this->RequiredHouses)
			{
				if (pHType && pHouse->Type == pHType) { matched = true; break; }
			}
			if (!matched)
				return false;
		}

		// Forbidden houses (countries): the house Country must not be listed.
		for (auto const pHType : this->ForbiddenHouses)
		{
			if (pHType && pHouse->Type == pHType)
				return false;
		}

		return true;
	}
};
