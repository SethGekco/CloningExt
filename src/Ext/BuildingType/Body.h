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
		// source makes CloneCount * Cloning.Mult clones of the produced unit, so
		// this scales whatever the unit's own CloneCount asks for (and the
		// slot-bonus clones it kicks out).
		Valueable<int> CloningMult;

		// Units exempt from THIS building's Cloning.Mult (Cloning.Mult.Blacklist=).
		// A listed unit is cloned at multiplier 1 by this building.
		ValueableVector<TechnoTypeClass*> MultBlacklist;

		ExtData(BuildingTypeClass* OwnerObject)
			: Extension<BuildingTypeClass>(OwnerObject)
			, CloningFacility { false }
			, CloningMult { 1 }
			, MultBlacklist {}
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
