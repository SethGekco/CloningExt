# CloningExt

A standalone Syringe/YRpp extension DLL for Yuri's Revenge that **augments
Antares' cloning system** with configurable clone counts, per-clone veterancy and
HP control, and prerequisite/country-gated extra clone slots.

It co-loads alongside Antares and does **not** replace or fight its cloning
hooks — see [`HOOKS_LOG.md`](HOOKS_LOG.md) for the architecture and
[`INI_REFERENCE.md`](INI_REFERENCE.md) for every tag.

## Features

| Feature | Tag(s) | Status |
|---|---|---|
| Clones per source building | `CloneCount=` | ✅ infantry + naval + vehicle |
| Clone veterancy ratio / cap | `CloneVeterancy.Ratio/.Cap/.Inherit*` | ✅ (ratio/cap/master); ⚠ granular academy/stolen partial |
| Clone initial HP (fixed or synced-random) | `CloneInitialStrength[.Min]` | ✅ |
| Extra clone slots by prerequisite/country | `CloneSlotN.*` incl. negative prereq & forbidden houses | ✅ |
| Barracks that also clones | `CloningFacility=` on a factory | ✅ (Fix 1) |

## Compatibility with TraitExt and other co-DLLs

CloningExt has **no build- or run-time dependency on TraitExt** (or any co-DLL),
so it is safe whether or not TraitExt is loaded — nothing to break if it is
absent. Compatibility with TraitExt's features is inherent by construction, for
three reasons:

1. **Clones are created through the standard game paths** — `CreateObject` +
   `Unlimbo`, or `KickOutUnit` when `Cloning.ConsideredBuilt=yes`. Any DLL that
   hooks unit creation/production (TraitExt, Antares, Phobos) therefore processes
   a clone exactly like any other unit, so **type-applied traits carry
   automatically**.
2. **Type references resolve TraitExt-authored types.** `CloneAs`,
   `Clone.Escalate[i]`, `ClonedAt=` etc. are resolved by type ID after the type
   arrays are populated. When TraitExt authors a variant via `$Inherits` (a real,
   registered section), CloningExt resolves and clones it. An unresolved ID
   null-guards to the produced type — so a ladder written against
   not-yet-defined variants degrades safely instead of crashing.
3. **`Cloning.ConsideredBuilt` is the bridge for production-detecting co-DLLs.**
   If TraitExt (or GiftBox/Host, etc.) applies its effect on the "built" event,
   marking a clone considered-built routes it through `KickOutUnit` so that DLL
   sees it.

What CloningExt does **not** do (by design): copy a *source unit's per-instance
runtime traits* onto a clone — that state lives in TraitExt's own ext and a clone
is a fresh instance. TraitExt re-applies runtime/conditional traits to the clone
per its own rules. No CloningExt code is required for any of the above; the
escalation ladder is the intended pairing point with TraitExt's inheritance.

## Build

Windows, MSVC v142, x86. Requires the `YRpp` and `Phobos` submodules
(`git submodule update --init --recursive`).

```
msbuild CloningExt.sln /p:Configuration=DevBuild /p:Platform=x86
```

CI (`.github/workflows/build.yml`) builds `DevBuild|x86` and uploads
`CloningExt.dll` + `.map`.

## Status

Deployed and validated in-game: clone quantity (`CloneCount`/`CloneAmount` ×
`Cloning.Mult` + `Cloning.Mult.Slots`, `Cloning.Mult.Blacklist`), per-clone spec
lists (`CloneAs` / `CloneInitialStrength[.Min]` / `CloneChance`, base + per-slot),
prerequisite/house slots, `ClonedAt=` hijack, `CloneAs.LowPower` defects,
`Cloning.ConsideredBuilt` (built-detection), the three-scope escalation ladder
(Local/Global/Universal), and `Clone.OverrideBaseClone` (full vat ownership).

The most invasive path is the override (`Clone.OverrideBaseClone`), which aborts
Antares' own base clone mid-`KickOutUnit` via the `0x445696` Failed return — the
one worth a focused in-game sanity check.
