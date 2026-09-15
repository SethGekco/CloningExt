#pragma once

#include <Utilities/Container.h>
#include <Utilities/TemplateDef.h>

#include <BuildingClass.h>

#include <map>

// Per-building-INSTANCE cloning state: the LOCAL clone-escalation counter, keyed
// by cloned unit-type index (unified TechnoTypeClass::Array index). Counts how
// many clones of each type THIS specific vat has produced, for the per-vat
// escalation scope (see docs/DESIGN.CloneEscalation.md). Distinct from
// BuildingTypeExt, which is per-type config.
class BuildingExt
{
public:
	using base_type = BuildingClass;

	static constexpr DWORD Canary = 0x0C10E004;

	class ExtData final : public Extension<BuildingClass>
	{
	public:
		// unit-type index -> clones of that type made by this building instance.
		std::map<int, int> LocalEscalationCounts;

		ExtData(BuildingClass* OwnerObject)
			: Extension<BuildingClass>(OwnerObject)
			, LocalEscalationCounts {}
		{ }

		virtual ~ExtData() = default;

		virtual void LoadFromINIFile(CCINIClass*) override { }
		virtual void Initialize() override { }
		virtual void InvalidatePointer(void*, bool) override { }

		virtual void LoadFromStream(PhobosStreamReader& Stm) override;
		virtual void SaveToStream(PhobosStreamWriter& Stm) override;

		int GetLocalCount(int typeIndex) const
		{
			auto const it = this->LocalEscalationCounts.find(typeIndex);
			return (it != this->LocalEscalationCounts.end()) ? it->second : 0;
		}

		void AddLocalCount(int typeIndex, int delta)
		{
			if (delta != 0)
				this->LocalEscalationCounts[typeIndex] += delta;
		}
	};

	class ExtContainer final : public Container<BuildingExt>
	{
	public:
		ExtContainer();
		~ExtContainer();
	};

	static ExtContainer ExtMap;
};
