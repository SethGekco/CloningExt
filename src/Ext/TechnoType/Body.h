#pragma once

#include <Utilities/Container.h>
#include <Utilities/TemplateDef.h>

#include <TechnoTypeClass.h>

#include <Cloning/Slot.h>
#include <Cloning/Resolver.h>

#include <vector>

// Per-TechnoType cloning settings -- read off the PRODUCED unit's type. When a
// unit rolls out of a factory, these tags decide how many clones each cloning
// source makes of it, what veterancy/HP those clones get, and which extra
// prerequisite/country-gated slots contribute more.
//
// One container keyed on TechnoTypeClass covers Infantry/Unit/Aircraft because
// they share the TechnoTypeClass CTOR/DTOR (see Body.cpp lifecycle hooks).
class TechnoTypeExt
{
public:
	using base_type = TechnoTypeClass;

	static constexpr DWORD Canary = 0x0C10E002;

	class ExtData final : public Extension<TechnoTypeClass>
	{
	public:
		// Clones produced per qualifying cloning source. 1 == vanilla. Values >1
		// add (CloneCount-1) extra clones per source on top of Antares' base one.
		Valueable<int> CloneCount { 1 };

		// Mirror of Antares' Cloneable=. Lets a modder suppress our extra-clone
		// layer for a type without depending on Antares' invisible ext.
		Valueable<bool> Cloneable { true };

		// Veterancy + initial-strength resolvers (see Cloning/Resolver.h).
		CloneVeterancySpec Veterancy;
		CloneStrengthSpec  Strength;

		// Prerequisite/house-gated extra cloning slots. Count-prefixed list.
		std::vector<CloneSlot> Slots;

		ExtData(TechnoTypeClass* OwnerObject)
			: Extension<TechnoTypeClass>(OwnerObject)
			, CloneCount { 1 }
			, Cloneable { true }
			, Veterancy {}
			, Strength {}
			, Slots {}
		{ }

		virtual ~ExtData() = default;

		virtual void LoadFromINIFile(CCINIClass* pINI) override;
		virtual void Initialize() override { }
		virtual void InvalidatePointer(void*, bool) override { }

		virtual void LoadFromStream(PhobosStreamReader& Stm) override;
		virtual void SaveToStream(PhobosStreamWriter& Stm) override;

		// Total extra clones the satisfied slots grant to `pHouse` (0 if none).
		int ResolveSlotBonus(HouseClass* pHouse) const;

	private:
		template <typename T>
		void Serialize(T& Stm);
	};

	class ExtContainer final : public Container<TechnoTypeExt>
	{
	public:
		ExtContainer();
		~ExtContainer();
	};

	static ExtContainer ExtMap;
};
