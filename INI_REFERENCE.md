# CloningExt — INI reference

All tags are read independently of Antares. CloningExt **augments** Antares'
cloning system (it does not replace it), so Antares must be loaded for the
count/slot/veterancy features to matter. See `HOOKS_LOG.md` for why.

## TechnoType (the unit being cloned)

Per-clone control uses **index-aligned lists**: entry `i` across `CloneAmount` /
`CloneAs` / `CloneInitialStrength` defines clone-spec `i`. Shorter lists repeat
their last entry; absent lists use defaults. Example:

```ini
[SOMEUNIT]
CloneAmount=1,1              ; spec 0 = 1 clone, spec 1 = 1 clone (CloneCount is an alias)
CloneAs=GGI,GGI_REJECT       ; spec 0 comes out as GGI, spec 1 as GGI_REJECT
CloneInitialStrength=100,50  ; spec 0 at 100% HP, spec 1 at 50% HP
```

```ini
[SOMEUNIT]
; --- the clone spec list (all index-aligned) ---
CloneAmount=1             ; int list, default 1. Clones per spec, per cloning
                          ; source. CloneCount= is a back-compat alias.
CloneAs=                  ; TechnoType list. What each spec comes out as. Unset =
                          ; ClonedAs= if set, else this unit. (See NACLON caveat.)
CloneInitialStrength=100  ; percent list, default 100 (full HP).
CloneInitialStrength.Min= ; percent list. If set, HP is a synced-RNG roll in
                          ; [Min, CloneInitialStrength] per clone.
Cloneable=yes             ; mirror of Antares' tag; no = our layer never clones
                          ; this unit.
Cloning.Mult.Blacklist=   ; BuildingType list. These buildings do NOT apply their
                          ; Cloning.Mult to this unit (it is cloned at multiplier
                          ; 1). The exception fires from either side -- building's
                          ; list of units OR unit's list of buildings.
Cloning.ConsideredBuilt=  ; yes/no, unset by default. Whether clones OF this unit
                          ; count as "built" for production-detecting co-DLLs.
Cloning.ConsideredBuilt.Weight=0  ; int. Conflict resolver vs the building's tag:
                          ; heavier weight wins; the unit wins exact ties.

; --- clone veterancy ---
CloneVeterancy.Ratio=1.0             ; clone veterancy = source * Ratio
CloneVeterancy.Cap=2.0               ; clamp (default = [General]VeteranCap)
CloneVeterancy.Inherit=yes           ; master: no = clones exit rookie
CloneVeterancy.Inherit.Academy=yes
CloneVeterancy.Inherit.StolenTech=yes
CloneVeterancy.Inherit.CountryBonus=yes

; --- extra clone slots gated by prerequisite / country ---
CloneSlots.Count=0                   ; how many CloneSlotN blocks follow
CloneSlot0.Prerequisite=NAWEAP,NARADR      ; house must own ALL of these
CloneSlot0.Prerequisite.Negative=NATECH    ; house must own NONE of these
CloneSlot0.RequiredHouses=Americans,French ; house Country must be one of these
CloneSlot0.ForbiddenHouses=Russians        ; house Country must NOT be one
; each satisfied slot grants its own clone spec list (same list rules as the base):
CloneSlot0.Amount=1                        ; int list of clones per spec
CloneSlot0.As=                             ; TechnoType list (defaults like CloneAs)
CloneSlot0.InitialStrength=100             ; percent list
CloneSlot0.InitialStrength.Min=            ; percent list -> synced-RNG HP range
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
Cloning.ConsideredBuilt=  ; yes/no, unset by default. Whether clones this building
                          ; makes count as "built" for co-DLLs that detect
                          ; production (GiftBox/Host). Unset = defer to the unit /
                          ; default no. See the weight note below.
Cloning.ConsideredBuilt.Weight=0  ; int. On a yes-vs-no conflict between this
                          ; building and the unit, the higher weight decides.
```

### "Considered built" resolution

`Cloning.ConsideredBuilt` exists on both the cloning **building** and the **unit**.
When a clone is made:

* neither side sets it -> **no** (clone is a silent spawn; the game is unchanged
  and no built-detection DLL sees it);
* only one side sets it -> that side wins;
* both set it -> the side with the higher `.Weight` wins (unit wins exact ties).

A `yes` result routes that clone through `KickOutUnit` so co-DLLs that mark
production at its entry (e.g. GiftBox/Host at `0x443C60`) record it as built.
Placement still scatters via the normal free-cell finder. With no such DLL
loaded the only difference a `yes` makes is that the clone exits like a produced
unit rather than being placed silently.

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
