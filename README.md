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

## Build

Windows, MSVC v142, x86. Requires the `YRpp` and `Phobos` submodules
(`git submodule update --init --recursive`).

```
msbuild CloningExt.sln /p:Configuration=DevBuild /p:Platform=x86
```

CI (`.github/workflows/build.yml`) builds `DevBuild|x86` and uploads
`CloningExt.dll` + `.map`.

## Status

Infantry, naval and vehicle clone paths are all covered. Every hook address and
register layout is verified against the `gamemd.exe` disassembly and upstream
(Antares/Phobos) source, but **nothing has been exercised in a running game
yet**. The `0x443C81` clone detection and the extra-clone recursion guard are the
two things to watch first in-game.
