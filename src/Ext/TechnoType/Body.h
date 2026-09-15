#pragma once

#include <Utilities/Container.h>
#include <Utilities/TemplateDef.h>

#include <TechnoTypeClass.h>
#include <BuildingTypeClass.h>

#include <Cloning/Slot.h>
#include <Cloning/Resolver.h>
#include <Cloning/CloneList.h>

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
		// The always-on clone spec list for this unit: CloneAmount / CloneAs /
		// CloneInitialStrength (+ .Min), index-aligned. CloneCount is a back-compat
		// alias for CloneAmount. Empty => one full-HP clone of the produced type.
		CloneList Clones;

		// Ares/Antares ClonedAs=, read by us as the default clone TYPE when a spec's
		// CloneAs is unset (and for the NACLON caveat -- see INI_REFERENCE.md).
		Nullable<TechnoTypeClass*> ClonedAsFallback;

		// Ares/Antares ClonedAt=, hijacked by us: the exact buildings that clone
		// this unit. When non-empty it REPLACES the Cloning=/CloningFacility= source
		// search (exclusive, mirroring Antares), so a unit can be cloned at specific
		// buildings without those buildings being flagged cloning vats. Empty => use
		// the normal cloning-source enumeration.
		ValueableVector<BuildingTypeClass*> ClonedAt;

		// Escalation variant ladder: Clone.Escalate[0], [1], ... scanned until the
		// first gap. The active index is chosen by the cloning building's escalation
		// counter; the chosen entry becomes the default clone type. Null entries
		// (unresolved IDs) are guarded at use. See docs/DESIGN.CloneEscalation.md.
		ValueableVector<TechnoTypeClass*> EscalateLadder;

		// Mirror of Antares' Cloneable=. Lets a modder suppress our extra-clone
		// layer for a type without depending on Antares' invisible ext.
		Valueable<bool> Cloneable { true };

		// Veterancy resolver (see Cloning/Resolver.h). HP is handled per-spec by
		// CloneList now, not by CloneStrengthSpec.
		CloneVeterancySpec Veterancy;

		// Prerequisite/house-gated extra cloning slots. Count-prefixed list.
		std::vector<CloneSlot> Slots;

		// Buildings that do NOT apply their Cloning.Mult to this unit
		// (Cloning.Mult.Blacklist=). This unit is cloned at multiplier 1 by any
		// listed building -- the exception works from either side.
		ValueableVector<BuildingTypeClass*> MultBlacklist;

		// Whether clones OF this unit should count as "built" for co-DLLs that
		// detect production at KickOutUnit. Unset = defer to the building/default.
		// On a conflict with the building's tag, the higher .Weight decides.
		Nullable<bool> ConsideredBuilt;
		Valueable<int> ConsideredBuiltWeight;

		ExtData(TechnoTypeClass* OwnerObject)
			: Extension<TechnoTypeClass>(OwnerObject)
			, Clones {}
			, ClonedAsFallback {}
			, ClonedAt {}
			, EscalateLadder {}
			, Cloneable { true }
			, Veterancy {}
			, Slots {}
			, MultBlacklist {}
			, ConsideredBuilt {}
			, ConsideredBuiltWeight { 0 }
		{ }

		virtual ~ExtData() = default;

		virtual void LoadFromINIFile(CCINIClass* pINI) override;
		virtual void Initialize() override { }
		virtual void InvalidatePointer(void*, bool) override { }

		virtual void LoadFromStream(PhobosStreamReader& Stm) override;
		virtual void SaveToStream(PhobosStreamWriter& Stm) override;

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
