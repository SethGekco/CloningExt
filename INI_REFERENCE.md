# CloningExt — INI reference

All tags are read independently of Antares. CloningExt **augments** Antares'
cloning system (it does not replace it), so Antares must be loaded for the
count/slot/veterancy features to matter. See `HOOKS_LOG.md` for why.

## TechnoType (the unit being cloned)

```ini
[SOMEUNIT]
; --- how many clones each cloning source makes of this unit ---
CloneCount=1              ; int, default 1 (=vanilla). N>1 adds (N-1) extra
                          ; clones per qualifying cloning source, on top of the
                          ; one Antares already makes.
Cloneable=yes             ; mirror of Antares' tag; no = our layer never clones
                          ; this unit.

; --- clone veterancy ---
CloneVeterancy.Ratio=1.0             ; clone veterancy = source * Ratio
CloneVeterancy.Cap=2.0               ; clamp (default = [General]VeteranCap)
CloneVeterancy.Inherit=yes           ; master: no = clones exit rookie
CloneVeterancy.Inherit.Academy=yes
CloneVeterancy.Inherit.StolenTech=yes
CloneVeterancy.Inherit.CountryBonus=yes

; --- clone initial HP ---
CloneInitialStrength=                ; fraction 0<..<=1 of full HP on exit
CloneInitialStrength.Min=            ; if set, HP fraction is a synced-RNG roll
                                     ; in [Min, CloneInitialStrength]

; --- extra clone slots gated by prerequisite / country ---
CloneSlots.Count=0                   ; how many CloneSlotN blocks follow
CloneSlot0.Prerequisite=NAWEAP,NARADR      ; house must own ALL of these
CloneSlot0.Prerequisite.Negative=NATECH    ; house must own NONE of these
CloneSlot0.RequiredHouses=Americans,French ; house Country must be one of these
CloneSlot0.ForbiddenHouses=Russians        ; house Country must NOT be one
CloneSlot0.Amount=1                        ; extra clones when satisfied
```

## BuildingType (the cloning building)

```ini
[SOMEBUILDING]
CloningFacility=yes       ; Antares' tag, re-read by us so our extra-clone layer
                          ; can enumerate the same source buildings. A barracks
                          ; may set this AND produce normally (Fix 1): its primary
                          ; product is never treated as a clone.
```

The vanilla `Cloning=yes` (Cloning Vats) flag is honoured directly — no extra tag
needed for classic infantry cloning.

## Known limitations (this build)

* **`CloneCount=0` cannot suppress** Antares' base clone from a co-loaded DLL
  (Antares already made it); it is treated as 1. True suppression needs takeover
  mode.
* Our EXTRA clones use the produced type and do **not** honour Antares'
  `ClonedAs=` (Antares' base clone still does).
* `CloneVeterancy.Inherit.Academy` / `.StolenTech` are non-decomposable from a
  co-loaded DLL (that data lives in Antares' ext). Setting **either** to `no`
  currently drops **all** inherited veterancy (conservative; never over-grants).
  `.CountryBonus` and the master `Inherit`/`Ratio`/`Cap` work fully.
* **Fix-1 mixed config edge:** if a producing factory both clones (via
  `CloningFacility=`) *and* the house also owns separate vanilla Cloning Vats,
  Antares may add a single extra vat-clone of one of our extra clones. Our own
  layer never recurses (primary-only gate); this is a one-level Antares artifact
  in an unusual setup. Pure Fix-1 (factory clones, no separate vats) is clean.
