#pragma once

#include <Utilities/Container.h>
#include <Utilities/TemplateDef.h>

#include <BuildingTypeClass.h>
#include <TechnoTypeClass.h>

// Per-BuildingType cloning settings.
//
// We keep our OWN copy of Antares' `CloningFacility=` flag. Antares reads that
// tag into its own (invisible-to-us) BuildingType ext to decide which buildings
// clone; we re-read the same key so that our augmentation layer can enumerate
// the same source buildings without reaching into Antares' data. A mod that uses
// Antares cloning already sets this key, so nothing extra is required of modders.
//
// Container<T> runs in unordered_map mode (Canary defined, no ExtPointerOffset),
// so we claim no pointer slot inside BuildingTypeClass and never collide with
// Antares' or Phobos' extension storage.
class BuildingTypeExt
{
public:
	using base_type = BuildingTypeClass;

	// Distinct from Phobos (0x11111111 / 0xAFFEAFFE), AcademyExt (0x0ACADE0n),
	// SquadExt (0x50DEC77), PrerequisiteExt (0xB2B2B2B2), SuperWeaponExt
	// (0x5C5C5C5C).
	static constexpr DWORD Canary = 0x0C10E001;

	class ExtData final : public Extension<BuildingTypeClass>
	{
	public:
		// Mirror of Antares' tag. Default false. The vanilla `Cloning=` flag is
		// a real BuildingTypeClass member (Type->Cloning) and is read directly,
		// so it is not duplicated here.
		Valueable<bool> CloningFacility;

		// Per-building clone multiplier (Cloning.Mult=, default 1). A cloning
		// source makes CloneCount * Cloning.Mult clones of the produced unit.
		Valueable<int> CloningMult;

		// Separate multiplier for the prerequisite/house SLOT-bonus clones
		// (Cloning.Mult.Slots=). Unset => slots use Cloning.Mult (as before); set =>
		// slots scale by this instead, letting base clones and slot clones multiply
		// independently.
		Nullable<int> CloningMultSlots;

		// Units exempt from THIS building's Cloning.Mult (Cloning.Mult.Blacklist=).
		// A listed unit is cloned at multiplier 1 by this building.
		ValueableVector<TechnoTypeClass*> MultBlacklist;

		// Whether clones this building makes should count as "built" for co-DLLs
		// that detect production at KickOutUnit (e.g. GiftBox/Host). Nullable so
		// "unset" is a real state: unset = defer to the unit / default (no). On a
		// yes-vs-no conflict with the unit's tag, the higher .Weight decides.
		Nullable<bool> ConsideredBuilt;
		Valueable<int> ConsideredBuiltWeight;

		// --- clone escalation (Global scope, Phase 1) ---------------------------
		// Starting ladder index (before any threshold). Nullable so we can tell if
		// this building opts into escalation at all.
		Nullable<int> EscalateStartIndex;             // Clone.Escalate.Index
		// Ascending count thresholds and the ladder index each switches to.
		ValueableVector<int> EscalateGlobalCount;     // Clone.Escalate.Global.Count
		ValueableVector<int> EscalateGlobalIndex;     // Clone.Escalate.Global.Index
		// Does a batch of N clones count as +N (yes) or +1 (no)? This drives the
		// per-house tally that BOTH Global and Universal read.
		Valueable<bool> EscalateGlobalCountMultiples; // Clone.Escalate.Global.CountMultiples

		// Universal scope: same table shape, but evaluated against the sum of ALL
		// houses' counts (the "race"). Derived from the per-house tally, so it needs
		// no separate store and shares Global's CountMultiples.
		ValueableVector<int> EscalateUniversalCount;  // Clone.Escalate.Universal.Count
		ValueableVector<int> EscalateUniversalIndex;  // Clone.Escalate.Universal.Index

		// Is this building escalation-configured (uses the ladder for output)?
		bool HasEscalation() const
		{
			return this->EscalateStartIndex.isset()
				|| !this->EscalateGlobalCount.empty()
				|| !this->EscalateUniversalCount.empty();
		}

		ExtData(BuildingTypeClass* OwnerObject)
			: Extension<BuildingTypeClass>(OwnerObject)
			, CloningFacility { false }
			, CloningMult { 1 }
			, CloningMultSlots {}
			, MultBlacklist {}
			, ConsideredBuilt {}
			, ConsideredBuiltWeight { 0 }
			, EscalateStartIndex {}
			, EscalateGlobalCount {}
			, EscalateGlobalIndex {}
			, EscalateGlobalCountMultiples { true }
			, EscalateUniversalCount {}
			, EscalateUniversalIndex {}
		{ }

		virtual ~ExtData() = default;

		virtual void LoadFromINIFile(CCINIClass* pINI) override;
		virtual void Initialize() override { }
		virtual void InvalidatePointer(void*, bool) override { }

		virtual void LoadFromStream(PhobosStreamReader& Stm) override;
		virtual void SaveToStream(PhobosStreamWriter& Stm) override;

		// A building is a cloning source if it is a vanilla Cloning Vat OR carries
		// Antares' CloningFacility= flag.
		bool IsCloningSource() const
		{
			return this->OwnerObject()->Cloning || this->CloningFacility;
		}

	private:
		template <typename T>
		void Serialize(T& Stm);
	};

	class ExtContainer final : public Container<BuildingTypeExt>
	{
	public:
		ExtContainer();
		~ExtContainer();
	};

	static ExtContainer ExtMap;
};
