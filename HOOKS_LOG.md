# CloningExt — hook log

Every `gamemd.exe` address this DLL touches, why it was chosen, and what was
verified vs. assumed. Per the standing rule, findings here get promoted to the
[YR Hook Encyclopedia](https://github.com/SethGekco/YR-Hook-Encyclopedia) once
exercised in a real game.

Disassembly: `objdump -D -b binary -m i386 --adjust-vma=0x400000 gamemd.exe`.
Symbol map: `~/Claude/Antares/gamemd_names_from_antares_pdb.txt`.
Registry consulted: `~/Claude/YR-Hook-Encyclopedia/registry/hooks.csv`.

Status legend: **VERIFIED** = read out of a disassembly or upstream source in
this session. **ASSUMED** = copied from a framework that uses it, not
independently confirmed. **UNTESTED** = never run in a game.

---

## Architecture decision — augment Antares, do not fight it

Antares **already owns the entire cloning system**: `Cloneable=`, `ClonedAs=`,
`ClonedAt=`, `CloningFacility=` and the four dispatch hooks
(`0x444DBC / 0x4445F6 / 0x44441A / 0x4449DF`, all in
`BuildingExt::ExtData::KickOutClones`). Re-hooking those to "provide our own
dispatch" (the original web-session plan) would double-clone and risk hook-range
corruption.

So CloningExt is a **cooperative augmentation layer**, following the
SuperWeaponExt "hook the funnel the framework doesn't own" philosophy:

* per-clone properties (HP, veterancy) go on the **un-contended** `0x443C81`;
* extra-clone counting chains **after** Antares at its `return 0` sites.

No same-address race is relied upon anywhere.

---

## `0x443C81` — `KickOutUnit` dispatch entry — per-clone init  *(size 7)*

| | |
|---|---|
| Contention | Phobos only (`BuildingClass_ExitObject_InitialClonedHealth`); **Antares does NOT hook it**. Both handlers return 0 → Syringe chains. |
| Status | **VERIFIED** (disassembly + Phobos source), **UNTESTED in game** |

The dispatch body of `BuildingClass::KickOutUnit` starts at `0x443C60`
(`sub esp,0x130; push ebx/ebp/esi/edi; mov edi,[esp+0x144]; mov esi,ecx`). At
`0x443C81` (`c6 87 d5 03 00 00 01` = `mov byte ptr [edi+0x3D5], 1`):

```
ESI = BuildingClass*   (the building kicking the object out)
EDI = FootClass*       (the object being kicked out)
```

> ⚠ The web-session context block said **"ECX=clone, ESI=building"**. The ECX
> half is **wrong** — the object is in **EDI**. Corrected from the disassembly
> and cross-checked against Phobos `src/Ext/Building/Hooks.cpp:86`.

Every kicked-out unit passes here, **including every clone** Antares kicks via a
nested `B->KickOutUnit(Clone,...)`. We apply `CloneInitialStrength[.Min]` and the
`CloneVeterancy.*` controls here so they cover Antares clones, vanilla CloningVats
clones and our own extras uniformly.

Clone-vs-primary discrimination (handles "a barracks that also clones", Fix 1):
`isCloneExit = building is a cloning source (Type->Cloning || our CloningFacility)`
`&& !(building->Factory && building->Factory->Object == exitingObject)`. The
primary product matches `Factory->Object` and is left untouched.

---

## `0x444DBC` — infantry exit — extra-clone dispatch  *(size 5)*
## `0x44441A` — naval-unit clone — extra-clone dispatch  *(size 6)*

| | |
|---|---|
| Contention | Antares hooks both; **both its handlers `return 0`**, so Syringe chains ours after them. No race. |
| Status | **VERIFIED** (disassembly + Antares `src/Ext/TechnoType/Hooks.cpp`), **UNTESTED in game** |

Register layout at both sites (matches Antares' own `GET`s):

```
ESI = BuildingClass*   (the producing factory)
EDI = TechnoClass*     (the Production — unit that just rolled out)
```

`0x444DBC` stolen bytes: `8b 06 57 6a 02` (`mov eax,[esi]; push edi; push 2`).
`0x44441A` stolen bytes: `8b b6 18 02 00 00` (`mov esi,[esi+0x218]`) — ESI is read
as the factory **before** this reassigns it, exactly as Antares does.

We only act on the **primary product** (`factory->Factory->Object == Production`),
which is what makes each production event fire once and prevents clone re-entry
from recursing.

---

## `0x4445F0` — vehicle (non-naval unit) exit — extra-clone dispatch  *(size 6)*

| | |
|---|---|
| Contention | **none.** Antares is at `0x4445F6` (adjacent, not overlapping); `0x4445F0` is absent from the encyclopedia registry and the PDB map, and nothing branches into `0x4445F0..F5`. |
| Status | **VERIFIED** (disassembly), **UNTESTED in game** |

The vehicle branch keeps `ESI = factory`, `EDI = Production` all the way to its
end. Antares takes the branch's last instruction, `0x4445F6`
(`e9 76 03 00 00` = `jmp 0x444971`), and returns that non-zero jump — which would
short-circuit a same-address chain if Antares ran first. Rather than depend on
load order, we hook the instruction **immediately before** it:

```
4445f0:  ff 90 e8 01 00 00    call [eax+0x1E8]      <- our 6-byte hook
4445f6:  e9 76 03 00 00       jmp  0x444971          <- Antares' hook
```

`0x4445F0 + 6 == 0x4445F6`, so the two patches are perfectly adjacent with no
overlap. Our handler runs the extra-clone dispatch, Syringe restores EAX/ECX and
executes the stolen `call [eax+0x1E8]` (a mission/voice call, not placement — the
primary vehicle is already out), then control falls into Antares' hook at
`0x4445F6`, which makes the base clone and jumps to the epilogue. Same
primary-product gate as the other two sites.

> Recursion is bounded the same way vanilla/Antares bounds it: a clone kicked
> from a **dedicated** cloning building (vanilla `Cloning=`, or `CloningFacility=`
> which Antares only allows on non-factory buildings) re-enters and hits Antares'
> own bail (`FactoryType->Cloning || Factory not Infantry/Unit`). Our extras add a
> primary-only gate on top, so our layer never recurses. The one documented edge
> is a Fix-1 factory that clones *and* owns separate cloning vats: Antares may add
> a single extra vat-clone of our clone. See INI_REFERENCE.md.

---

## Container lifecycle (no game logic, all `return 0`, chained)

Copied from Phobos develop @47475624 via AcademyExt/SuperWeaponExt; ASSUMED-safe
because those projects drive the same sites for their own containers.

* BuildingType: `0x45E50C` CTOR, `0x45E707` DTOR, `0x465300`/`0x465010`
  SaveLoad-prefix, `0x4652ED` load-suffix, `0x46536A` save-suffix, `0x464A49`
  LoadFromINI.
* TechnoType: `0x711835` CTOR, `0x711AE0` DTOR, `0x716DC0`/`0x7162F0`
  SaveLoad-prefix, `0x716DAC` load-suffix, `0x717094` save-suffix, `0x716123`
  LoadFromINI.
* Static bootstrap: `0x7CD810` (ExeRun / `Patch::ApplyStatic`), `0x52F639`
  (cmdline-parsed / flush deferred log).
