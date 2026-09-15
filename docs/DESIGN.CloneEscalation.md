# Clone escalation ("clone fatigue / race")

Cloning a unit repeatedly walks its clones up or down a **variant ladder**. The
ladder is defined on the unit; the progression (a counter + threshold table) is
defined on the cloning building. Three counter scopes give per-vat, per-player,
and world-wide ("race") behaviour.

## Unit — the variant ladder (indexed)

```ini
[GGI]
Clone.Escalate[0]=GGI_Superior
Clone.Escalate[1]=GGI_Standard
Clone.Escalate[2]=GGI_Inferior
Clone.Escalate[3]=GGI_Sludge
```

* Scanned `[0]`, `[1]`, … until the first missing index.
* Each entry is a **real, registered TechnoType** (must be in `[InfantryTypes]`
  etc.), normally authored as an inheritance of the base (see below).
* The ladder is a pure lookup table; the *index* is chosen by the building.

## Building — the counter + threshold table

```ini
[NACLON]
Clone.Escalate.Index=0                    ; index before any threshold (default 0)
Clone.Escalate.Global.Count=5,10,15,50    ; ascending count thresholds
Clone.Escalate.Global.Index=1,2,3,0       ; index used once each threshold is passed
Clone.Escalate.Global.CountMultiples=yes  ; a batch of N clones counts as +N (yes) or +1 (no)
```

The same `.Count`/`.Index`/`.CountMultiples` shape exists for three scopes:

| scope | tag prefix | counter keyed by | meaning |
|---|---|---|---|
| **Local** | `Clone.Escalate.Local.*` | (this building instance, unit) | per-vat wear |
| **Global** | `Clone.Escalate.Global.*` | (house, unit) | this player's total |
| **Universal** | `Clone.Escalate.Universal.*` | (scenario, unit) | ALL players → a race |

### Index is derived, not stored
Effective index = the `Index[]` entry for the **highest `Count[]` threshold the
counter has passed**, else the starting `Clone.Escalate.Index`. The last/highest
threshold is automatically permanent (nothing higher overrides it). The index may
jump anywhere (`1,2,3,0`). Only the *count* is state — no separate index machine,
so it stays desync-proof.

### Multiple scopes on one building
If more than one scope defines a table, the effective index is the **higher of the
resolved indices** (whichever escalation is further along wins; order-independent).

### Out-of-range index
Clamped to the last ladder entry (repeat-last, like the other CloningExt lists).

## How it feeds the clone system
The resolved ladder type becomes the **default clone type** for that production —
it drops into the existing `AsAt(i, defaultAs)` fallback. Specs that pin `CloneAs=`
stay pinned; specs using the default follow the ladder. Escalation is resolved
**per source building** (each vat uses its own table + the snapshot count), and the
count is snapshotted at the start of the production so all sources in one event use
the same index; increments apply afterwards.

## Counting
* Keyed **per (scope, unit-type)** — cloning GGI is independent from E1.
* Increment per source building B = `CountMultiples ? clonesFromB : 1`.
* Snapshot-then-increment avoids intra-production drift.

## Storage & sync
| scope | store | serialized in |
|---|---|---|
| Global | **HouseExt** `map<unitTypeIdx,int>` | house stream |
| Local | **instance BuildingExt** `map<unitTypeIdx,int>` (NEW per-instance container) | building stream |
| Universal | one scenario-global `map<unitTypeIdx,int>` | a global save hook (piggyback a single stream) |

All increments happen inside the synced `KickOutUnit` path and are serialized →
MP-sync-safe and save/load-safe.

## Inheritance-extension connection
CloningExt is **decoupled but pairs with** the INI inheritance extension:

* Ladder variants (`GGI_Superior`…) are best authored as **inheritances of the
  base** (define "GGI but weaker", override a few keys). CloningExt never calls the
  inheritance ext — it only resolves the type ID.
* **Without the inheritance ext:** author the sections manually, *or* an unresolved
  ID simply **null-guards → falls back to the base/produced type** (no crash). Safe
  either way.
* Requirement regardless: ladder variants must be **registered TechnoTypes**; the
  inheritance ext should handle registration, and CloningExt reads refs afterward.

## Phases — ALL BUILT
1. **Global** ✅ — HouseExt counter, ladder parse, index resolution, dispatch wiring.
2. **Universal** ✅ — no separate store; sum of every house's per-type tally (the "race").
3. **Local** ✅ — per-instance BuildingExt counter (canary 0x0C10E004), serialized in
   the building stream; own `CountMultiples`.

## Open decisions (defaults chosen)
* Multi-scope precedence → **higher resolved index wins**.
* Index past ladder end → **clamp**.
* `CountMultiples` default → **yes**.
* Reset → **none** (Global cumulative; Local dies with the building; Universal per scenario).
