#pragma once

#include <Utilities/Container.h>
#include <Utilities/TemplateDef.h>

#include <HouseClass.h>

#include <map>

// Per-house cloning state: the GLOBAL clone-escalation counter, keyed by cloned
// unit-type index (TechnoTypeClass::GetArrayIndex on the unified array). Counts
// how many clones of each type this house has produced, driving the escalation
// ladder (see docs/DESIGN.CloneEscalation.md).
class HouseExt
{
public:
	using base_type = HouseClass;

	static constexpr DWORD Canary = 0x0C10E003;

	class ExtData final : public Extension<HouseClass>
	{
	public:
		// unit-type index -> cumulative clone count for this house.
		std::map<int, int> EscalationCounts;

		ExtData(HouseClass* OwnerObject)
			: Extension<HouseClass>(OwnerObject)
			, EscalationCounts {}
		{ }

		virtual ~ExtData() = default;

		virtual void LoadFromINIFile(CCINIClass*) override { }
		virtual void Initialize() override { }
		virtual void InvalidatePointer(void*, bool) override { }

		virtual void LoadFromStream(PhobosStreamReader& Stm) override;
		virtual void SaveToStream(PhobosStreamWriter& Stm) override;

		int GetEscalationCount(int typeIndex) const
		{
			auto const it = this->EscalationCounts.find(typeIndex);
			return (it != this->EscalationCounts.end()) ? it->second : 0;
		}

		void AddEscalationCount(int typeIndex, int delta)
		{
			if (delta != 0)
				this->EscalationCounts[typeIndex] += delta;
		}
	};

	class ExtContainer final : public Container<HouseExt>
	{
	public:
		ExtContainer();
		~ExtContainer();
	};

	static ExtContainer ExtMap;
};
