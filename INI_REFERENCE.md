# CloningExt — INI reference

All tags are read independently of Antares. CloningExt **augments** Antares'
cloning system (it does not replace it), so Antares must be loaded for the
count/slot/veterancy features to matter. See `HOOKS_LOG.md` for why.

## TechnoType (the unit being cloned)

```ini
[SOMEUNIT]
; --- how many clones each cloning source makes of this unit ---
CloneCount=1              ; int, default 1 (=vanilla). Each cloning source makes
                          ; EXACTLY N clones of this unit: we produce N minus
                          ; whatever Antares already made from that source (0 when
                          ; Antares bailed, e.g. a factory that clones itself).
Cloneable=yes             ; mirror of Antares' tag; no = our layer never clones
                          ; this unit.
Cloning.Mult.Blacklist=   ; BuildingType list. These buildings do NOT apply their
                          ; Cloning.Mult to this unit (it is cloned at multiplier
                          ; 1). The exception fires from either side -- building's
                          ; list of units OR unit's list of buildings.

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
Cloning.Mult=1            ; int, default 1. This building makes
                          ; CloneCount * Cloning.Mult clones of each unit, so
                          ; Cloning.Mult=2 doubles whatever the unit's CloneCount
                          ; asks for (2 clones from a default CloneCount=1 unit,
                          ; 4 from a CloneCount=2 unit). Also scales the
                          ; slot-bonus clones. 0 = this building never clones.
Cloning.Mult.Blacklist=   ; TechnoType list. These units are exempt from THIS
                          ; building's Cloning.Mult (cloned at multiplier 1).
```

The vanilla `Cloning=yes` (Cloning Vats) flag is honoured directly — no extra tag
needed for classic infantry cloning.

## Known limitations (this build)

* **`CloneCount=0` suppression is partial.** Where Antares bailed (a factory
  that clones itself, e.g. GAPILE with `Cloning=yes`) it produces zero clones, so
  0 fully suppresses. Where Antares makes a base clone from a dedicated vat, that
  one clone remains (we can't un-make Antares' clone in augment mode).
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
