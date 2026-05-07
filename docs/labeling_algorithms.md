# LocationLayer label placement algorithms

This file describes the placement algorithms used by `LocationLayer` and the design choices behind them. The configurable knobs are listed in [reference.md](reference.md#labelconfig-settings); this document explains *why* each knob exists and what tradeoffs it controls.

## Pipeline

For every WMS / WMTS / OGC-Tiles request that hits `LocationLayer::generate`, the work is:

```
keyword data (~hundreds to ~200k WGS84 points)
   │
   ▼
candidate query    ← optional WGS84 prefilter for large keywords;
                     bbox-strict OR margin-buffered (pan_invariant)
   │
   ▼
projection         ← WGS84 → CRS → image-pixel
   │
   ▼
mindistance filter ← NearTree, geonames-priority order
   │
   ▼
candidate enrich   ← measure label width/height, resolve per-class style,
                     build marker obstacle bboxes
   │
   ▼
PLACEMENT          ← one of: none / fixed / greedy / priority-greedy /
                     simulated-annealing
   │
   ▼
visibility filter  ← (pan_invariant only) skip labels with anchor outside
                     the original request bbox
   │
   ▼
emit SVG           ← labels first, markers last (markers on top)
                     reverse-priority order so highest-priority renders last
```

Each placement algorithm is a *pure function* of the projected candidate list. Identical inputs → identical outputs, regardless of which backend serves the request. The algorithm is reentrant and stateless.

## Geometry: where labels land

For a marker at anchor `A` with symbol bounding box of half-extents `(sym_half_w, sym_half_h)`, each candidate position `p` has:

- a **direction unit vector** `(dx, dy)` (e.g. NE = `(1/√2, -1/√2)` in screen-y-down coordinates)
- an **alignment fraction** `(afx, afy)` saying which point of the label rectangle aligns with the directional anchor (e.g. NE: `(0, 1)` = label's bottom-left corner)

The directional anchor is the point on the marker rectangle's boundary along direction `(dx, dy)`:

```
t   = min(sym_half_w / |dx|, sym_half_h / |dy|)
D   = t + offset
ap  = (A.x + D·dx, A.y + D·dy)
```

The label's bbox top-left then is:

```
x1 = ap.x − dw · afx
y1 = ap.y − dh · afy
```

where `(dw, dh)` is the measured pixel size of the rendered text.

This gives **uniform `offset` clearance** between the label's closest point and the marker's bounding-box edge — exactly `offset` pixels in every of the 8 (or 16) directions, regardless of which side of the marker the label sits on.

For a circular marker inscribed in the bounding box, the gap from the *circle* to a corner-position label is slightly larger than `offset` (the corner of the bbox protrudes past the circle), but this excess corner gap is what makes corner labels feel "comfortably" diagonal rather than crammed against the circle edge.

### Position priority order

Eight standard positions in priority order: **NE, SE, NW, SW, E, W, N, S**.

The priority follows two preference rules, applied in order:

1. East of the anchor preferred over west (right wins).
2. Above the anchor preferred over below.

This gives the corner ranking `NE > SE > NW > SW`. Imhof's 1975 paper used `NE > NW > SE > SW` (above wins ties before right does); we follow the modern dynamic-visualisation convention from arxiv:1209.5765 where right beats above.

The 16-position set inserts intermediate angular directions (ENE, ESE, etc.) but uses the same priority spine.

### Label baseline alignment

SVG default text baseline is *alphabetic* — y is the baseline, descenders sit below it. If we just put the baseline at `bbox.y2` (the bottom of the placement bbox), then a label like "Tampere" (with a 'p' descender) ends up with its visible *letter body* hanging higher than a label like "Helsinki" (no descender), even though both share the same algorithm decision. The user perceives this as Tampere being mispositioned.

The fix is *position-aware baseline*:

- **Above-anchor labels** (NE/NW/N/...): baseline = `bbox.y2`. Letter body's bottom touches the marker's top edge. The 'p' descender of a label like "Tampere" hangs into the empty area below the bbox to the side of the marker — visible, not under the marker.
- **Below-anchor labels** (SE/SW/S/...): baseline = `bbox.y1 + ascent` (= `bbox.y1 - y_bearing` from cairo). Letter body's top touches the marker's bottom edge.
- **Side labels** (E/W): baseline = `anchor.y + ascent/2`. Letter body's vertical centre lines up with the marker's centre.

`y_bearing` (cairo's "negative ascent") is measured per-text by `TextUtility::getTextDimension` and threaded through `LabelCandidate → PlacedLabel` so the renderer has it at emit time.

## Algorithms

### `none`

No labels. Markers only. Backward-compatible default.

### `fixed`

Every label at a single predetermined position (default NE). Processed in geonames priority order. A label is dropped if its chosen position would overlap an *already-kept higher-priority* marker; if dropped, the candidate's own marker is dropped too. Labels may overlap each other (no label-vs-label conflict detection by design).

Useful for sparse data where you know overlaps won't matter, or for debugging the candidate set.

### `greedy`

For each candidate in geonames priority order, try positions in priority order; place at the first position that:

- fits inside the map bounds,
- doesn't overlap any already-placed label,
- doesn't overlap any other candidate's marker (own marker is skipped — the geometry already clears it).

If no position works, drop the label. The marker is dropped with it, and removed from the obstacle set used by *subsequent* candidates so an invisible marker can't push another label out of its preferred position.

Recommended for most weather-map use cases. `O(n · k · m)` where `n` = candidates, `k` = positions per candidate, `m` = neighbours-per-collision-test ≈ small constant.

### `priority-greedy`

Like `greedy` but candidates are pre-sorted by population descending before placement. Result is unscattered back to input order so callers can correlate `result[i] ↔ input[i]`.

Use when the geonames autocomplete sort order doesn't match the population priority you care about.

The optional `priority_bucket_ratio` knob (default `1.0` = strict population) groups close populations into log-buckets so census jitter (e.g., 100,000 vs 100,010) doesn't flip placement. Within a bucket the **shorter label** (smaller `label_w`) wins; remaining ties fall through to stable input order.

### `simulated-annealing`

Christensen, Marks & Shieber (1995). Treats placement as an energy minimisation:

```
E(assignment) = overlap_weight  · Σ (label-vs-label overlap area)
              + overlap_weight  · Σ (label-vs-other-marker overlap area)
              + position_weight · Σ position_index(i)
              + free_space_term (optional)
```

The temperature decays geometrically (`T ← T · cooling_rate`, default `0.99`). At each iteration a random label gets a random new position; the move is accepted if it lowers energy or with probability `exp(-ΔE/T)`. The default `2000` iterations are enough for ~150 labels.

Delta-energy is computed incrementally: each move only re-checks the row for the label being moved against all currently-assigned others, so per-move cost is `O(n)` and total cost is `O(iterations · n)`.

Random seed is fixed (`42`) so the result is deterministic — the layer-image cache (`hash_value`) only works correctly if identical requests produce identical outputs.

SA converges toward a near-global optimum that pure greedy can miss in dense areas, especially when several labels need to coordinate to fit at all.

## Optional energy / scoring terms

### Free-space bias

When `free_space_weight > 0`, each candidate's positions are scored by alignment with a *free direction*:

```
free_dir(i) = normalize( Σ_j  (anchor[i] − P[j]) / |anchor[i] − P[j]|² )
                    where j ranges over all OTHER occupied points within
                    free_space_radius of anchor[i].
```

Occupied points include both **other candidates' markers** and **labels already placed earlier in this run**. The latter is what prevents "Varkaus ends up underneath Pieksämäki's label" cases where the markers are 49 px apart but Pieksämäki's NE label stretches all the way over to Varkaus.

In greedy: positions are sorted by `position_index − weight × alignment` so high-alignment positions come first.
In SA: the same `−weight × alignment` term is added to `posPenalty` and recomputed at each move so the bias reflects the current state.

Coastal cities, where neighbours all sit on one side, see a strong free-direction pointing into the empty water; their labels naturally end up offshore. Inland cities surrounded by neighbours have direction contributions that cancel — they fall back to plain priority order.

The bias does *not* require a sea mask. The other markers serve as proxies for "occupied space".

### Population bucketing

For `priority-greedy` with `priority_bucket_ratio > 1`:

```
bucket(p) = floor( log(max(p, 0) + 1) / log(ratio) )
```

Two cities with `bucket(p_a) == bucket(p_b)` compare as equal. The within-bucket tiebreak uses `label_w ascending` (shorter labels less obtrusive) and falls through to stable geonames order.

Shorter labels packed into prime NE positions free up room for other labels, so on dense maps bucketing typically places **more** total labels than strict-population sort: in our `location_labels_priority_greedy_bucketed` test (mindistance=18) it places 71 cities vs 69 for the strict-population variant — Kauhava and Nurmes win their bucket against slightly-bigger but longer-named neighbours.

## Cross-platform stability

Cairo text measurement varies sub-pixel between distros (Rocky10's fontconfig vs RHEL9's). A 1–2 px difference in `label_w` for a single label can flip a close greedy decision and cascade into completely different placement.

Two knobs absorb this:

- `bbox_padding` inflates each label bbox by N px on every edge during overlap testing only (visual position is unchanged). Two close labels that pass overlap testing without padding may fail with `bbox_padding=2`, so the algorithm is forced to a slightly more spacious arrangement that *every* font-metric variant agrees on.
- `bbox_quantum` rounds measured `label_w` and `label_h` up to the next multiple of N pixels before placement. A 1-px font-metric drift is hidden inside the same 4-px bin.

Both default to 0 (off). Set `bbox_padding: 3, bbox_quantum: 4` for products that ship to mixed-distro fleets.

## Pan invariance

The `LocationLayer.pan_invariant` flag (true/false, default false) makes label-direction decisions stable when a viewer pans by issuing successive overlapping GetMap requests at the same zoom + projection.

The mechanism:

```
margin = max(mindistance, free_space_radius) + max_label_extent +
         symbol_half + safety_buffer

buffered_image_bbox = visible_bbox grown by margin pixels on each side

candidates are filtered to buffered_image_bbox (with an optional WGS84
prefilter that pre-trims the keyword list before per-point projection)

placement runs on the candidates inside buffered_image_bbox

at render time, anchors outside the original (un-buffered) bbox are
skipped — they only existed to influence neighbours' decisions
```

The invariance argument: two pan requests at the same zoom share most of their `buffered_image_bbox`. Because the placement algorithm is deterministic and reentrant, any visible city whose `mindistance`/`free_space_radius` neighbourhood lies entirely inside both buffers gets the *exact same* placement decision in both requests. The user sees no jitter when panning.

The boundary case: cities near the buffer edge can see slightly different neighbourhoods between two requests. The user-visible effect is confined to cities right at the edge of the visible region (which the user is panning toward anyway), and disappears as soon as the pan settles.

This is **stateless** — no cache, no per-feature memoization, no shared state across backends. A clustered deployment where consecutive client requests hit different backends still produces consistent results because the algorithm is a pure function of `(keyword data, projection, bbox)`.

For larger keywords (the user's largest is ~200,000 world locations), a cheap WGS84 prefilter inverse-projects ~12 sample points around the buffered bbox to estimate a (loose) lon/lat rectangle, then a single linear pass over the keyword list drops most points before any per-point projection is paid for. At 200k entries the prefilter typically reduces projection workload by ~99 %.

The `pan_invariant` flag does not address *cross-zoom* stability — labels can still flip position when the user zooms in or out. That problem is genuinely harder and would require either persistent labelling (Been-Daiches-Yap) with per-feature zoom ranges, or a server-side cache of placement decisions per `(zoom, projection, feature_id)`. Both are incompatible with a clustered stateless deployment unless you accept that all backends do the multilevel computation independently. None of that is implemented here.

## What's *not* in this code

- **SHW 4-position max-IS** (Schwartges, Haunert, Wolff 2012, arxiv:1209.5765). Our `candidates: 4` mode already restricts to the four corners, but uses greedy/SA priority placement rather than maximum-independent-set on the conflict graph. The MaxIS objective explicitly drops "place high-priority first" in favour of "place as many as possible", which doesn't fit a weather-map use case where Helsinki must always win.
- **Persistent labelling** (Been, Daiches, Yap 2006, "Dynamic Map Labeling"). Per-feature `[zoom_min, zoom_max]` ranges with stable positions across zoom. Would solve cross-zoom jitter but requires either a known dataset extent at compile-time or a per-feature server-side state.
- **Sea / land masks**. The free-space bias is a cheap proxy that works because *other markers* tend to mark "occupied" land. A genuine land/water mask would do better at coastlines but adds substantial query and computation cost per request.
