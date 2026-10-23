#!/usr/bin/env python3
"""
Generate a synthetic vector tile dataset in the Protomaps basemap schema.

The geography is fictional so that the fixture contains no OpenStreetMap
data and can be distributed freely. Using the Protomaps schema lets the
tests exercise the real Protomaps styles.

    data/synthetic/world.pmtiles   zoom 0-4, whole world, coarse
    data/synthetic/nordic.pmtiles  zoom 0-7, lon 0...27, lat 52...72, detailed

Run from the directory of this script. The output is deterministic.
"""

import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pmtiles_writer  # noqa: E402

EXTENT = 4096
BUFFER = 64

# ----------------------------------------------------------------------
# Geography in longitude/latitude
# ----------------------------------------------------------------------


def blob(cx, cy, rx, ry, bumps, n=96):
    """A smooth closed shape with some coastline variation"""
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        r = 1.0
        for k, a, phase in bumps:
            r += a * math.sin(k * t + phase)
        pts.append((cx + rx * r * math.cos(t), cy + ry * r * math.sin(t)))
    return pts


CONTINENTS = [
    ("Nordland", blob(20, 63, 13, 7.5, [(3, 0.12, 0.3), (5, 0.07, 1.1), (11, 0.03, 0.2)])),
    ("Westland", blob(-100, 45, 30, 18, [(2, 0.15, 0.7), (7, 0.05, 2.0)])),
    ("Southland", blob(135, -25, 20, 12, [(3, 0.1, 1.5), (9, 0.04, 0.4)])),
]

ISLANDS = [blob(-8, 54, 3, 2, [(4, 0.1, 0.0)], 32), blob(36, 70, 2.5, 1.2, [(3, 0.1, 1.0)], 32)]

LAKES = [
    ("Lake Alpha", blob(26, 62, 1.6, 0.6, [(3, 0.15, 0.5)], 48), 0),
    ("Lake Beta", blob(16, 60.5, 1.0, 0.4, [(4, 0.1, 0.2)], 48), 0),
    ("Lake Gamma", blob(29, 65, 0.5, 0.25, [(3, 0.1, 0.9)], 32), 6),  # only in the detailed file
    ("Lake Delta", blob(22, 66, 0.3, 0.15, [(3, 0.1, 0.1)], 24), 6),
]

FOREST = blob(23, 64.5, 5, 2.5, [(5, 0.1, 0.4)], 64)

# (name, lon, lat, population rank, minimum zoom, capital)
CITIES = [
    ("Alphaville", 24.9, 60.2, 12, 2, True),
    ("Betaburg", 18.0, 59.3, 11, 3, False),
    ("Gammaby", 25.5, 65.0, 9, 4, False),
    ("Deltaport", 10.7, 59.9, 10, 3, False),
    ("Epsilon", 28.0, 62.6, 7, 5, False),
    ("Zetaholm", 21.5, 63.1, 6, 6, False),
    ("Etaniemi", 26.7, 67.0, 5, 6, False),
    ("Westcity", -100.0, 40.0, 13, 2, True),
    ("Southtown", 135.0, -25.0, 10, 3, True),
]

ROADS = [
    ("highway", ["Alphaville", "Epsilon", "Gammaby"], 3),
    ("highway", ["Alphaville", "Betaburg", "Deltaport"], 3),
    ("major_road", ["Betaburg", "Zetaholm", "Gammaby", "Etaniemi"], 5),
    ("major_road", ["Epsilon", "Zetaholm"], 6),
]

BORDERS = [
    ("country", 2, [(21.0, 54.0), (21.5, 60.0), (20.5, 66.0), (22.0, 72.0)], 0),
    ("region", 4, [(24.0, 60.0), (24.5, 63.0), (27.0, 64.0), (31.0, 64.5)], 5),
]

COUNTRIES = [("Westnord", 14.0, 63.0), ("Eastnord", 27.0, 63.5)]


# ----------------------------------------------------------------------
# Tile geometry
# ----------------------------------------------------------------------


def lonlat_to_tile_px(lon, lat, z, tx, ty):
    n = 2 ** z
    x = (lon + 180.0) / 360.0 * n
    lat = max(min(lat, 85.0511), -85.0511)
    r = math.radians(lat)
    y = (1.0 - math.asinh(math.tan(r)) / math.pi) / 2.0 * n
    return ((x - tx) * EXTENT, (y - ty) * EXTENT)


def clip_ring(ring, lo, hi):
    def clip(pts, inside, intersect):
        out = []
        if not pts:
            return out
        prev = pts[-1]
        for cur in pts:
            if inside(cur):
                if not inside(prev):
                    out.append(intersect(prev, cur))
                out.append(cur)
            elif inside(prev):
                out.append(intersect(prev, cur))
            prev = cur
        return out

    def ix(a, b, x):
        t = (x - a[0]) / (b[0] - a[0])
        return (x, a[1] + t * (b[1] - a[1]))

    def iy(a, b, y):
        t = (y - a[1]) / (b[1] - a[1])
        return (a[0] + t * (b[0] - a[0]), y)

    pts = clip(ring, lambda p: p[0] >= lo, lambda a, b: ix(a, b, lo))
    pts = clip(pts, lambda p: p[0] <= hi, lambda a, b: ix(a, b, hi))
    pts = clip(pts, lambda p: p[1] >= lo, lambda a, b: iy(a, b, lo))
    pts = clip(pts, lambda p: p[1] <= hi, lambda a, b: iy(a, b, hi))
    return pts


def clip_line(line, lo, hi):
    """Split a polyline into the pieces inside the box (simple sampling clip)"""
    pieces = []
    cur = []
    for i in range(len(line) - 1):
        a, b = line[i], line[i + 1]
        steps = 32
        for s in range(steps + 1 if i == len(line) - 2 else steps):
            t = s / steps
            p = (a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]))
            if lo <= p[0] <= hi and lo <= p[1] <= hi:
                cur.append(p)
            elif cur:
                if len(cur) >= 2:
                    pieces.append(cur)
                cur = []
    if len(cur) >= 2:
        pieces.append(cur)
    return pieces


def signed_area(ring):
    a = 0.0
    for i in range(len(ring)):
        x1, y1 = ring[i]
        x2, y2 = ring[(i + 1) % len(ring)]
        a += x1 * y2 - x2 * y1
    return a / 2


def orient(ring, exterior):
    # MVT: exterior rings have positive area in tile coordinates (y down)
    if (signed_area(ring) > 0) != exterior:
        ring = list(reversed(ring))
    return ring


def to_int(pts):
    out = []
    for x, y in pts:
        p = (int(round(x)), int(round(y)))
        if not out or out[-1] != p:
            out.append(p)
    if len(out) > 1 and out[0] == out[-1]:
        out.pop()
    return out


# ----------------------------------------------------------------------
# MVT encoding
# ----------------------------------------------------------------------


def varint(v):
    out = bytearray()
    while v >= 0x80:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    out.append(v)
    return bytes(out)


def zigzag(v):
    return (v << 1) ^ (v >> 31)


def field(num, wire, payload):
    key = varint((num << 3) | wire)
    if wire == 2:
        return key + varint(len(payload)) + payload
    return key + payload


def packed(values):
    return b"".join(varint(v) for v in values)


def encode_geometry(gtype, parts):
    cmds = []
    cx = cy = 0
    if gtype == 1:
        cmds.append(1 | (len(parts) << 3))
        for (x, y) in parts:
            cmds += [zigzag(x - cx), zigzag(y - cy)]
            cx, cy = x, y
        return cmds
    for part in parts:
        cmds.append(1 | (1 << 3))
        cmds += [zigzag(part[0][0] - cx), zigzag(part[0][1] - cy)]
        cx, cy = part[0]
        cmds.append(2 | ((len(part) - 1) << 3))
        for (x, y) in part[1:]:
            cmds += [zigzag(x - cx), zigzag(y - cy)]
            cx, cy = x, y
        if gtype == 3:
            cmds.append(7 | (1 << 3))
    return cmds


def encode_value(v):
    if isinstance(v, bool):
        return field(7, 0, varint(1 if v else 0))
    if isinstance(v, int):
        return field(6, 0, varint(zigzag(v)))
    if isinstance(v, float):
        return field(3, 1, struct.pack("<d", v))
    return field(1, 2, str(v).encode())


def encode_layer(name, features):
    keys = []
    values = []
    body = field(15, 0, varint(2)) + field(1, 2, name.encode())
    for gtype, parts, props in features:
        tags = []
        for k, v in sorted(props.items()):
            if k not in keys:
                keys.append(k)
            if (type(v), v) not in values:
                values.append((type(v), v))
            tags += [keys.index(k), values.index((type(v), v))]
        feat = b""
        if tags:
            feat += field(2, 2, packed(tags))
        feat += field(3, 0, varint(gtype))
        feat += field(4, 2, packed(encode_geometry(gtype, parts)))
        body += field(2, 2, feat)
    for k in keys:
        body += field(3, 2, k.encode())
    for _, v in values:
        body += field(4, 2, encode_value(v))
    body += field(5, 0, varint(EXTENT))
    return field(3, 2, body)


# ----------------------------------------------------------------------
# Tile content
# ----------------------------------------------------------------------


def make_tile(z, tx, ty, detailed):
    lo, hi = -BUFFER, EXTENT + BUFFER

    def proj(pts):
        return [lonlat_to_tile_px(lon, lat, z, tx, ty) for lon, lat in pts]

    def polygon(ring_ll, exterior=True):
        ring = to_int(clip_ring(proj(ring_ll), lo, hi))
        if len(ring) < 3:
            return None
        return orient(ring, exterior)

    def point(lon, lat):
        x, y = lonlat_to_tile_px(lon, lat, z, tx, ty)
        if 0 <= x < EXTENT and 0 <= y < EXTENT:
            return (int(round(x)), int(round(y)))
        return None

    layers = {}

    # Land
    lands = [c[1] for c in CONTINENTS] + ISLANDS
    earth = []
    holes = []
    for ring_ll in lands:
        ring = polygon(ring_ll)
        if ring:
            earth.append((3, [ring], {"kind": "earth"}))
            holes.append(orient(ring, False))
    layers["earth"] = earth

    # Ocean is the tile minus the land, then lakes
    square = [(lo, lo), (hi, lo), (hi, hi), (lo, hi)]
    water = [(3, [orient(square, True)] + holes, {"kind": "ocean"})]
    for name, ring_ll, minzoom in LAKES:
        if minzoom > z and not detailed:
            continue
        if minzoom > z:
            continue
        ring = polygon(ring_ll)
        if ring:
            water.append((3, [ring], {"kind": "lake", "name": name, "min_zoom": float(minzoom)}))
    sea = point(5.0, 57.0)
    if sea and z >= 3:
        water.append((1, [sea], {"kind": "sea", "name": "Synthetic Sea"}))
    layers["water"] = water

    # Landcover
    if z >= 3:
        ring = polygon(FOREST)
        if ring:
            layers["landcover"] = [(3, [ring], {"kind": "forest"})]

    # Boundaries
    bounds = []
    for kind, detail, line_ll, minzoom in BORDERS:
        if minzoom > z:
            continue
        for piece in clip_line(proj(line_ll), lo, hi):
            piece = to_int(piece)
            if len(piece) >= 2:
                bounds.append((2, [piece], {"kind": kind, "kind_detail": detail}))
    layers["boundaries"] = bounds

    # Roads
    cities = {c[0]: (c[1], c[2]) for c in CITIES}
    roads = []
    for kind, names, minzoom in ROADS:
        if minzoom > z:
            continue
        line = proj([cities[n] for n in names])
        for piece in clip_line(line, lo, hi):
            piece = to_int(piece)
            if len(piece) >= 2:
                roads.append((2, [piece], {"kind": kind, "kind_detail": "primary"}))
    layers["roads"] = roads

    # Places
    places = []
    for name, lon, lat in COUNTRIES:
        p = point(lon, lat)
        if p and z <= 6:
            places.append((1, [p], {"kind": "country", "name": name, "min_zoom": 0.0,
                                    "population_rank": 15}))
    for name, lon, lat, rank, minzoom, capital in CITIES:
        if minzoom > z:
            continue
        p = point(lon, lat)
        if p:
            props = {"kind": "locality", "name": name, "min_zoom": float(minzoom),
                     "population_rank": rank}
            if capital:
                props["capital"] = "yes"
            places.append((1, [p], props))
    layers["places"] = places

    data = b""
    for name in ["earth", "water", "landcover", "boundaries", "roads", "places"]:
        feats = layers.get(name, [])
        if feats:
            data += encode_layer(name, feats)
    return data


def tile_range(bounds, z):
    w, s, e, n = bounds

    def tx(lon):
        return int((lon + 180.0) / 360.0 * 2 ** z)

    def ty(lat):
        r = math.radians(max(min(lat, 85.0511), -85.0511))
        return int((1.0 - math.asinh(math.tan(r)) / math.pi) / 2.0 * 2 ** z)

    top = 2 ** z - 1
    for x in range(max(tx(w), 0), min(tx(e), top) + 1):
        for y in range(max(ty(n), 0), min(ty(s), top) + 1):
            yield x, y


METADATA = {
    "name": "Synthetic basemap",
    "description": "Fictional geography in the Protomaps basemap schema for tests",
    "attribution": "Synthetic test data",
    "vector_layers": [{"id": n, "fields": {}} for n in
                      ["earth", "water", "landcover", "boundaries", "roads", "places"]],
}


def make(path, bounds, maxzoom, detailed):
    tiles = {}
    for z in range(maxzoom + 1):
        for x, y in tile_range(bounds, z):
            tiles[(z, x, y)] = make_tile(z, x, y, detailed)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    pmtiles_writer.write(path, tiles, METADATA, bounds, leaf_size=64)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    make(os.path.join(here, "data", "synthetic", "world.pmtiles"),
         (-180.0, -85.0511, 180.0, 85.0511), 4, False)
    make(os.path.join(here, "data", "synthetic", "nordic.pmtiles"),
         (0.0, 52.0, 27.0, 72.0), 7, True)


if __name__ == "__main__":
    main()
