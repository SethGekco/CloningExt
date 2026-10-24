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
ClonedAt=                 ; BuildingType list (Ares/Antares tag, read by us). The
                          ; EXCLUSIVE set of buildings that clone this unit --
                          ; overrides the Cloning=/CloningFacility= source search,
                          ; so a unit can be cloned only at named buildings without
                          ; flagging them as vats. Empty = normal source search.
CloneInitialStrength=100  ; percent list, default 100 (full HP).
CloneInitialStrength.Min= ; percent list. If set, HP is a synced-RNG roll in
                          ; [Min, CloneInitialStrength] per clone.
CloneChance=100           ; percent list, default 100. Per-clone synced-RNG spawn
                          ; chance for each spec (100=always, 50=coin-flip each).
CloneAs.LowPower=         ; TechnoType list. When the OWNER is in low power, spec i
                          ; comes out as this "defect" type instead of CloneAs (bake
                          ; the AttachEffect debuffs into that type's INI). Unset =
                          ; no change. Same idea per slot: CloneSlotN.As.LowPower.
Cloneable=yes             ; mirror of Antares' tag; no = our layer never clones
                          ; this unit.
CloneRank=                ; per-spec veterancy list (0=rookie,1=veteran,2=elite).
                          ; Sets the clone's rank directly; overrides the
                          ; CloneVeterancy.* inherit resolver. Unset = inherit.
CloneAs.Random=no         ; yes = each clone rolls ONE weighted spec from the
                          ; CloneAs list instead of producing every spec.
CloneAs.Weights=          ; int list, per-spec weights for CloneAs.Random (default 1).
Clone.MaxPerProduction=0  ; int, <=0 = no cap. Hard ceiling on total clones one
                          ; build event may spawn (safety valve vs mult x slots x
                          ; escalation runaways).
Clone.RemoveOriginal=no   ; yes = delete the produced unit itself, leaving only
                          ; its clones (build-X-get-clones transmute). Only fires
                          ; when >=1 clone actually spawned; removal is deferred to
                          ; the next logic frame (safe).
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
CloneSlot0.Chance=100                      ; percent list, per-clone spawn chance
CloneSlot0.As.LowPower=                    ; TechnoType list, defect type on low power
CloneSlot0.Rank=                           ; per-spec veterancy (0/1/2)
CloneSlot0.As.Random=no                    ; roll one weighted spec per clone
CloneSlot0.As.Weights=                     ; per-spec weights for the roll
```

## BuildingType (the cloning building)

```ini
[SOMEBUILDING]
CloningFacility=yes       ; Antares' tag, re-read by us so our extra-clone layer
                          ; can enumerate the same source buildings. A barracks
                          ; may set this AND produce normally (Fix 1): its primary
                          ; product is never treated as a clone.
Clone.OverrideBaseClone=no ; on a DEDICATED vat (e.g. NACLON): suppress Antares'
                          ; own base clone so CloningExt produces EVERY clone with
                          ; full type/HP/count control (not just the extras). Lifts
                          ; the NACLON caveat. Default no.
Cloning.Mult=1            ; int, default 1. This building makes
                          ; CloneCount * Cloning.Mult clones of each unit, so
                          ; Cloning.Mult=2 doubles whatever the unit's CloneCount
                          ; asks for (2 clones from a default CloneCount=1 unit,
                          ; 4 from a CloneCount=2 unit). Also scales the
                          ; slot-bonus clones. 0 = this building never clones.
Cloning.Mult.Slots=       ; int, optional. Separate multiplier for the slot-bonus
                          ; clones. Unset => slots use Cloning.Mult; set => base
                          ; clones and slot clones scale independently.
Cloning.Mult.Blacklist=   ; TechnoType list. These units are exempt from THIS
                          ; building's Cloning.Mult AND Cloning.Mult.Slots
                          ; (cloned at multiplier 1).
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

## Clone escalation (Local + Global + Universal scopes)

Cloning a unit repeatedly walks its clones along a variant ladder. Full design in
[docs/DESIGN.CloneEscalation.md](docs/DESIGN.CloneEscalation.md).

Unit — the ladder (scanned until the first gap; entries are real TechnoTypes,
normally authored as inheritances of the base):
```ini
[GGI]
Clone.Escalate[0]=GGI_Superior
Clone.Escalate[1]=GGI_Standard
Clone.Escalate[2]=GGI_Inferior
Clone.Escalate[3]=GGI_Sludge
```

Building (cloning vat) — the per-house counter that picks the index:
```ini
[NACLON]
Clone.Escalate.Index=0                    ; index before any threshold (default 0)
Clone.Escalate.Global.Count=5,10,15,50    ; ascending thresholds (per-house count)
Clone.Escalate.Global.Index=1,2,3,0       ; index used once each threshold is passed
Clone.Escalate.Global.CountMultiples=yes  ; batch of N counts as +N (yes) or +1 (no)
```
The active index = the `Index[]` of the highest passed `Count[]` (else the starting
index); it selects `Clone.Escalate[index]` as the default clone type. Unresolved
ladder entries fall back to the produced type (safe without the inheritance ext).
Every cloning source feeds the counter; escalation-configured vats also change
their output.

**Universal scope (the "race"):** same table shape against the sum of ALL houses'
counts — whoever mass-clones first escalates the unit for everyone.
```ini
[NADRAFT]
Clone.Escalate.Universal.Count=50   ; world-wide total across all players
Clone.Escalate.Universal.Index=3
```
**Local scope (per-vat):** evaluated against THIS building instance's own count.
```ini
[GADRAFT]
Clone.Escalate.Local.Count=5,10,15
Clone.Escalate.Local.Index=1,2,3
Clone.Escalate.Local.CountMultiples=no   ; a batch counts as +1 for this vat
```
Any scope a vat configures is resolved independently and the **higher resulting
index wins**. Local has its own `CountMultiples` (separate per-instance store);
Universal shares Global's (it is derived from the per-house tally). All three
counters are serialized (save/load-safe) and MP-sync-safe.

## Clone profile sections (`[<Type>.clone]`)

Lets a unit's **clone** be mechanically different from the **built** original.
When a clone's resolved type is `T` and a type with ID `T.clone` is registered,
the clone comes out as `T.clone` instead; the built primary is unaffected.

```ini
[GGI]
Strength=20
[GGI.clone]      ; must be a real, registered type (see note) -- here "GGI but weaker"
Strength=10
```
Now a vat-made GGI yields a full GGI (20) plus a GGI.clone (10). It composes with
everything, because the redirect is applied at the single point every clone is
created: `ClonedAs=GGI`, `CloneAs=...GGI...` and `Clone.Escalate[i]=GGI` all route
their GGI output to `GGI.clone` too.

* **Automatic — no toggle.** Presence of `[T.clone]` is the opt-in. The `.clone`
  suffix is therefore reserved; a type already ending in `.clone` is never
  redirected again (no recursion).
* **Registration is automatic — just author the section.** CloningExt scans the
  four type lists at rules-read time, and for every type `T` with a `[T.clone]`
  section it fills the profile out from `[T]` and appends it to the same list.
  You do **not** list it yourself, and you must **not** rely on `$Inherits` for
  it (see below). If you have already listed it by hand that is fine — it is
  completed in place rather than listed twice. IDs cap at 23 chars, so
  `<base>.clone` must fit; a profile that is too long is named in a WARN.
* **What inheriting `[T]` means here: a literal key copy, at load.** Every key
  of `[T]` is copied into `[T.clone]` except the ones you wrote yourself (yours
  win) and the build-list keys. `TechLevel=-1` is forced and `Image=` is always
  written explicitly.

  > ⚠ **Do not use `$Inherits` on a profile section.** `$Inherits` is not
  > load-time flattening — it is a Phobos fallback on the INI *read* path, and
  > its typed readers are flow-replacing rather than chainable, so whichever
  > framework hooks them last wins. On 2026-10-23 a type built that way parsed
  > with `Strength=0`: string reads inherited, the typed int read did not. A
  > clone of such a profile has no health, and teleports if `Locomotor` did not
  > inherit either. The literal copy above exists precisely to not depend on it.

* **Why build keys are skipped and `TechLevel=-1` is forced.** A profile listed
  in `[InfantryTypes]` is also sidebar-eligible, so copying
  `Prerequisite`/`TechLevel`/`Owner` across would put a duplicate unit in your
  build menu. Note a profile would otherwise be kept out of the sidebar only by
  accident — `TechLevel` is an int, and int inheritance is the half that does
  not work.
* **If a profile does not parse, you are told.** Once the type arrays exist,
  every profile registered at load is checked against them and any that is
  missing is named in a WARN. Clones then come out as the base type (safe).
* **`BuiltAt=` and `Factory.ExplicitOnly=` are production-only, deliberately.**
  They gate which factories *build* a unit and have no effect on cloning: a
  `CloningFacility=` clones everything otherwise eligible regardless of either.
  This matches Ares, which does not define them for cloning. Use `ClonedAt=`
  (unit side) to restrict which buildings clone a unit.

## Construct profile sections (`[<Type>.construct]`)

The sibling of `[<Type>.clone]`, and its mirror image. Where that one says
"this is what a **clone** of me looks like", this says "this is what a
**factory-built** one looks like" — so a unit that arrives any other way keeps
the base type.

```ini
[GGI]
Strength=200
[GGI.construct]      ; a GGI you BUILT
Strength=300
```

A GGI from a barracks comes out at 300. A GGI from a superweapon, a free unit,
a crate, a paradrop, the map, another DLL, or a cloning vat comes out at 200.

### What counts as "constructed"

Exactly one thing:

```
the building's factory was producing this object
```

That is stricter than "a factory kicked it out", and deliberately so — it also
excludes anything else the same building might eject. Concretely:

| arrives by | gets `.construct`? | why |
|---|---|---|
| barracks / war factory / naval yard / helipad | ✅ | it is the factory's product |
| AI production | ✅ | same path |
| cloning vat, or a `CloningFacility=` barracks' clone | ❌ | a clone never matches the factory's current product |
| `FreeUnit=` on a building | ❌ | leaves through `Grand_Opening`, a different path |
| superweapon (paradrop, drops) | ❌ | never reaches the production path |
| crate, map-placed, DLL-spawned | ❌ | never reaches the production path |
| **buildings** | ❌ | see below |

**Buildings are excluded.** A building's foundation is written into map cells
when it is placed, so changing its type afterwards leaves the old footprint
occupied and never claims the new one. Infantry, vehicles and aircraft only.

### It composes with `[<Type>.clone]`, in one direction

**Clones are clones of what you built.** If `GGI` has both profiles, a vat-made
pair is one `GGI.construct` (the built one) and one `GGI.clone` — *not* a clone
of the construct profile. `Cloneable=` and `ClonedAs=` are likewise read off
`[GGI]`, where you set them, not off the profile.

There is no `[GGI.construct.clone]` and no `[GGI.clone.construct]`: a clone was
never constructed, and a construct-variant is not itself cloned. A type whose ID
already ends in either suffix never gets a profile of its own.

### Same rules as `[<Type>.clone]`

Registration is automatic, inheritance is a literal key copy at load, build keys
are skipped, `TechLevel=-1` is forced, `Image=` is always written, and a profile
that fails to parse is named in a WARN. **Do not put `$Inherits` on it** — see
the warning under the clone profiles above. `.construct` costs 10 characters
against the 23-character ID limit, so it runs out sooner than `.clone` does; an
over-long profile is named in a WARN rather than silently ignored.

## Known limitations (this build)

* **`CloneCount=0` suppression is partial** *unless* the vat sets
  `Clone.OverrideBaseClone=yes`. Where Antares bailed (a factory that clones itself,
  e.g. GAPILE with `Cloning=yes`) it produces zero clones, so 0 fully suppresses.
  At a dedicated vat, Antares' one base clone remains unless `Clone.OverrideBaseClone`
  is set (which aborts it, letting CloningExt own the full output).
* Our EXTRA clones use the produced type and do **not** honour Antares'
  `ClonedAs=` (Antares' base clone still does) — unless `Clone.OverrideBaseClone`
  is set, in which case there is no Antares base clone and our `CloneAs` fully
  controls every clone.
* `CloneVeterancy.Inherit.Academy` / `.StolenTech` are non-decomposable from a
  co-loaded DLL (that data lives in Antares' ext). Setting **either** to `no`
  currently drops **all** inherited veterancy (conservative; never over-grants).
  `.CountryBonus` and the master `Inherit`/`Ratio`/`Cap` work fully.
* **Fix-1 mixed config edge:** if a producing factory both clones (via
  `CloningFacility=`) *and* the house also owns separate vanilla Cloning Vats,
  Antares may add a single extra vat-clone of one of our extra clones. Our own
  layer never recurses (primary-only gate); this is a one-level Antares artifact
  in an unusual setup. Pure Fix-1 (factory clones, no separate vats) is clean.
