// CloningExt -- make [<Type>.clone] profile sections into REAL registered types.
//
// `CloneProfileOf()` in Hooks.CloneDispatch.cpp already redirects every clone to
// `<T>.clone` when a type with that ID exists. The missing half was that the
// section had to become a type at all, and INI_REFERENCE.md told the modder to
// do it by hand: "list it in [InfantryTypes] (or let the inheritance ext create
// it by inheriting [GGI])". Both halves of that advice were problems.
//
// Hooking by hand is a step nobody should have to take, and it is easy to get
// subtly wrong: a profile section listed as a type is ALSO sidebar-eligible, so
// copying the parent's Prerequisite/TechLevel/Owner across puts a duplicate unit
// in the build menu.
//
// Deferring to inheritance is worse, and this is the part worth recording.
// `$Inherits` is NOT load-time flattening -- it is a Phobos fallback on the INI
// READ path (INIClass::GetString's not-found branch plus companion typed
// readers), and those typed hooks are flow-replacing rather than chainable, so
// whatever else hooks the same readers takes the slot. On 2026-10-23 a type
// built that way parsed with Strength = ZERO: string reads inherited, the typed
// int read did not. A clone of such a profile comes out with no health and, if
// Locomotor did not inherit either, teleports. See
// YR-Hook-Encyclopedia/encyclopedia/INI-Read-Inheritance.md.
//
// So: copy the parent's keys LITERALLY, here, before the engine parses anything.
// A literal copy cannot be defeated by read-path hook ordering and does not care
// whether Phobos is loaded.

#include <Ext/BuildingType/Body.h>

#include <Utilities/Debug.h>
#include <Utilities/Macro.h>

#include <CCINIClass.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	const char* const kTypeLists[] = {
		"InfantryTypes", "VehicleTypes", "AircraftTypes", "BuildingTypes",
	};

	// Asked of the HOUSE while deciding what can be built, before any clone
	// exists. Copying these onto a profile type is what would put a duplicate
	// in the sidebar, and they could not be clone-specific anyway.
	bool IsBuildScopeKey(const char* k)
	{
		static const char* const kBuild[] = {
			"Prerequisite", "Prerequisite.List", "Prerequisite.Lists",
			"Prerequisite.StolenTech", "Prerequisite.Negative",
			"PrerequisiteOverride", "TechLevel", "Owner",
			"ForbiddenHouses", "RequiredHouses", "BuildLimit",
			"AIBuildThis", "AIBuildCount", "Unbuildable",
		};
		for (const char* b : kBuild)
			if (!_stricmp(k, b))
				return true;
		return false;
	}

	std::string ReadKey(CCINIClass* pINI, const char* sect, const char* key)
	{
		char buf[0x200] { };
		pINI->ReadString(sect, key, "", buf, sizeof(buf));
		return std::string(buf);
	}

	// Fill the profile section out from its parent WITHOUT disturbing anything
	// the modder wrote: the authored keys are the overrides, so a key already
	// present in the profile is left exactly as it is.
	int InheritParentKeys(CCINIClass* pINI, const char* parent, const char* profile)
	{
		// LITERAL keys only, enumerated rather than read.
		//
		// ReadString() routes through Phobos' $Inherits fallback, so asking
		// "does the profile already have this key" with a read would answer YES
		// for every key the moment a modder writes $Inherits= on their profile
		// section -- and we would then copy nothing and hand the dispatch a
		// hollow type, which is the exact failure this file exists to prevent.
		// GetKeyName enumerates what is physically in the section.
		std::vector<std::string> authored;
		{
			const int pn = pINI->GetKeyCount(profile);
			authored.reserve(static_cast<size_t>(pn));
			for (int i = 0; i < pn; ++i)
				if (const char* const k = pINI->GetKeyName(profile, i))
					authored.emplace_back(k);
		}
		auto const isAuthored = [&authored](const char* k) -> bool
		{
			for (const auto& a : authored)
				if (!_stricmp(a.c_str(), k))
					return true;
			return false;
		};

		const int n = pINI->GetKeyCount(parent);
		int copied = 0, skipped = 0, kept = 0;

		for (int i = 0; i < n; ++i)
		{
			const char* const k = pINI->GetKeyName(parent, i);
			if (!k || !*k || *k == '$')
				continue;

			if (IsBuildScopeKey(k))
			{
				++skipped;
				continue;
			}

			// The author's own value wins outright.
			if (isAuthored(k))
			{
				++kept;
				continue;
			}

			const std::string v = ReadKey(pINI, parent, k);
			if (v.empty())
				continue;

			pINI->WriteString(profile, k, v.c_str());
			++copied;
		}

		// Not buildable, whatever the parent said. Note the profile would
		// otherwise have been kept out of the sidebar only by accident --
		// TechLevel is an int, and int inheritance is the half that does not
		// work, so relying on that would be relying on a bug.
		pINI->WriteString(profile, "TechLevel", "-1");

		// Image= is usually ABSENT from a unit section, because the engine
		// falls back to the section NAME. A profile called "GGI.clone" would
		// therefore go looking for GGI.clone.shp, load with null art, and take
		// the game down on the first draw. The dot makes this worse than usual,
		// since the ID is used as a filename. Always state it.
		if (!isAuthored("Image"))
		{
			const std::string parentImage = ReadKey(pINI, parent, "Image");
			pINI->WriteString(profile, "Image",
				parentImage.empty() ? parent : parentImage.c_str());
		}

		Debug::Log("[CloningExt]   [%s]: inherited %d key(s) from [%s] literally, "
			"kept %d authored override(s), skipped %d build key(s)\n",
			profile, copied, parent, kept, skipped);
		return copied;
	}

	bool ListContains(CCINIClass* pINI, const char* list, const char* id)
	{
		const int n = pINI->GetKeyCount(list);
		for (int i = 0; i < n; ++i)
		{
			const char* const k = pINI->GetKeyName(list, i);
			if (!k)
				continue;
			if (!_stricmp(ReadKey(pINI, list, k).c_str(), id))
				return true;
		}
		return false;
	}

	void AppendToList(CCINIClass* pINI, const char* list, const char* id)
	{
		char idx[16];
		std::snprintf(idx, sizeof(idx), "%d", pINI->GetKeyCount(list));
		pINI->WriteString(list, idx, id);
	}

	int g_Registered = 0;
	int g_Pass = 0;
}

namespace CloningExt
{
	// Names we registered, so the dispatch side can say WHICH profile failed to
	// materialise rather than just missing it silently.
	std::vector<std::string> RegisteredProfiles;
}

DEFINE_HOOK(0x668BF0, RulesClass_ReadFile_CloneProfileInjection, 0x5)
{
	GET_STACK(CCINIClass*, pINI, 0x4);
	if (!pINI)
		return 0;

	++g_Pass;

	// The rules read happens in more than one pass, and the first pass is the
	// one that matters: writing on a later pass risks the section having been
	// read already.
	if (g_Pass > 1)
		return 0;

	CloningExt::RegisteredProfiles.clear();
	g_Registered = 0;

	for (const char* const list : kTypeLists)
	{
		// Snapshot the membership BEFORE appending, or we would walk over the
		// profiles we just added and look for "GGI.clone.clone".
		std::vector<std::string> members;
		const int n = pINI->GetKeyCount(list);
		members.reserve(static_cast<size_t>(n));
		for (int i = 0; i < n; ++i)
		{
			const char* const k = pINI->GetKeyName(list, i);
			if (!k)
				continue;
			const std::string id = ReadKey(pINI, list, k);
			if (!id.empty())
				members.push_back(id);
		}

		for (const auto& parent : members)
		{
			// A profile of a profile is not a thing. The dispatch side already
			// refuses to redirect a type whose ID ends in .clone; this is the
			// same rule one level up, and it is why the membership snapshot
			// above has to be taken first.
			const size_t pl = parent.size();
			if (pl >= 6 && !_stricmp(parent.c_str() + (pl - 6), ".clone"))
				continue;

			const std::string profile = parent + ".clone";

			// IDs are a fixed 24-byte field, so the suffix has to fit or the
			// type can never be found by name.
			if (profile.size() > 23)
			{
				if (pINI->GetSection(profile.c_str()))
					Debug::Log("[CloningExt] WARN [%s] cannot be registered: the ID is "
						"%d characters and the limit is 23. Rename [%s] shorter.\n",
						profile.c_str(), static_cast<int>(profile.size()), parent.c_str());
				continue;
			}

			if (!pINI->GetSection(profile.c_str()))
				continue;       // no profile authored for this type

			if (ListContains(pINI, list, profile.c_str()))
			{
				// The modder registered it by hand. Still fill it out, because
				// the hand-registered case is exactly where $Inherits was being
				// relied on, but do not list it twice.
				Debug::Log("[CloningExt] [%s] already listed in [%s]; completing it "
					"in place\n", profile.c_str(), list);
				InheritParentKeys(pINI, parent.c_str(), profile.c_str());
				CloningExt::RegisteredProfiles.push_back(profile);
				continue;
			}

			InheritParentKeys(pINI, parent.c_str(), profile.c_str());
			AppendToList(pINI, list, profile.c_str());
			CloningExt::RegisteredProfiles.push_back(profile);
			++g_Registered;

			Debug::Log("[CloningExt] [%s] registered as a type in [%s] - clones of "
				"%s now come out as %s\n",
				profile.c_str(), list, parent.c_str(), profile.c_str());
		}
	}

	if (g_Registered)
		Debug::Log("[CloningExt] %d clone profile(s) registered. They are "
			"TechLevel=-1 and carry no build keys, so none of them can appear in "
			"the sidebar.\n", g_Registered);

	return 0;
}
