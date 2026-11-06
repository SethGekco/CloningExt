# Changelog — CloningExt

> Newest first. Every change gets an entry in the same commit, and says *why*:
> the reason is what saves the next session.

## 2026-11-06 — `.construct` now sees units a human player builds

### Fixed
- `[<Type>.construct]` (`src/Hooks.ConstructProfile.cpp`, hook `0x443C60`)
  decided "built by a factory" with
  `pBuilding->Factory && pBuilding->Factory->Object == pTechno`. That never
  matches a unit built from the sidebar. Sidebar production belongs to the
  house: `HouseClass::Begin_Production` creates the `FactoryClass` (ctor call
  at `0x4FA4F5`) and never writes `BuildingClass::Factory` (`+0x524`), so the
  field is null when `KickOutUnit` runs. The only writes to it are a
  building's own factory (`0x450387`) and `KickOutUnit` lending a busy
  factory's pointer to an idle twin (`0x44451F`–`0x444552`). So the profile
  most likely never fired for human players.
- The test now keeps the building check and otherwise accepts the unit if
  any `FactoryClass::Array` entry's `Object` is the unit (`IsFactoryProduct`).
  A clone or a free unit is still no factory's object, so they still keep the
  base type. The factory clears `Object` only after a successful kick-out, so
  it is still set at the hook. The pre-construct type is still recorded before
  the conversion, so clones still follow the type you built.
- Same fix as CosmeticExt `aa65ffc` (`IsFactoryProduct` in
  `src/Color/ColorRules.cpp`). Its `(factory)` lines appeared in Rex's 11-01
  game for units built at `GAWEAP`, which is evidence for the factory scan.
  It is **not** evidence for `.construct`: that still needs a
  `CONSTRUCT ... (built by a factory)` line in `debug.log` for a unit Rex
  built.
- No other code in this repo uses the flawed test. The clone hooks had already
  moved to "is a production factory" plus `ProducingExtras`
  (`Hooks.CloneDispatch.cpp` notes why). `HOOKS_LOG.md` still described the old
  test for `0x443C81` and the dispatch sites; those notes now match the code.
- Encyclopedia: `Building-Production-KickOut.md` carries the correction
  (YR-Hook-Encyclopedia `9943d24`).

Not deployed. Not seen in game.
