#!/usr/bin/env python3
"""
Minimal PMTiles v3 writer for test fixtures.

Writes gzip-compressed directories, metadata and tiles, deduplicates identical
tiles, run-length encodes consecutive identical tiles and splits the directory
into leaf directories when there are more entries than fit the root, so that
fixtures exercise the same code paths as real planet files.

Reference: https://github.com/protomaps/PMTiles/blob/main/spec/v3/spec.md
"""

import gzip
import json
import struct


def xy2d(n, x, y):
    d = 0
    s = n // 2
    while s > 0:
        rx = 1 if (x & s) > 0 else 0
        ry = 1 if (y & s) > 0 else 0
        d += s * s * ((3 * rx) ^ ry)
        if ry == 0:
            if rx == 1:
                x = s - 1 - x
                y = s - 1 - y
            x, y = y, x
        s //= 2
    return d


def tile_id(z, x, y):
    acc = 0
    for i in range(z):
        acc += 4 ** i
    return acc + xy2d(1 << z, x, y)


def varint(v):
    out = bytearray()
    while v >= 0x80:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    out.append(v)
    return bytes(out)


def encode_directory(entries):
    """entries: list of (tileid, runlength, length, offset)"""
    out = bytearray(varint(len(entries)))
    last = 0
    for e in entries:
        out += varint(e[0] - last)
        last = e[0]
    for e in entries:
        out += varint(e[1])
    for e in entries:
        out += varint(e[2])
    for i, e in enumerate(entries):
        if i > 0 and e[3] == entries[i - 1][3] + entries[i - 1][2]:
            out += varint(0)
        else:
            out += varint(e[3] + 1)
    return gzip.compress(bytes(out), mtime=0)


def write(path, tiles, metadata, bounds, center_zoom=0, leaf_size=4096):
    """
    tiles: dict (z, x, y) -> uncompressed tile bytes
    bounds: (west, south, east, north)
    leaf_size: maximum number of entries in one leaf directory
    """
    ordered = sorted((tile_id(z, x, y), data) for (z, x, y), data in tiles.items())

    # Tile data with deduplication
    blob = bytearray()
    offsets = {}
    entries = []
    for tid, data in ordered:
        compressed = gzip.compress(data, mtime=0)
        if compressed in offsets:
            offset = offsets[compressed]
        else:
            offset = len(blob)
            offsets[compressed] = offset
            blob += compressed
        length = len(compressed)
        prev = entries[-1] if entries else None
        if prev and prev[0] + prev[1] == tid and prev[3] == offset and prev[2] == length:
            entries[-1] = (prev[0], prev[1] + 1, prev[2], prev[3])
        else:
            entries.append((tid, 1, length, offset))

    # Root directory, with leaves if needed
    if len(entries) <= leaf_size:
        root = encode_directory(entries)
        leaves = b""
    else:
        leaves = bytearray()
        rootentries = []
        for i in range(0, len(entries), leaf_size):
            chunk = entries[i:i + leaf_size]
            leaf = encode_directory(chunk)
            rootentries.append((chunk[0][0], 0, len(leaf), len(leaves)))
            leaves += leaf
        root = encode_directory(rootentries)
        leaves = bytes(leaves)

    meta = gzip.compress(json.dumps(metadata, sort_keys=True).encode(), mtime=0)

    zooms = [z for (z, _, _) in tiles]
    root_off = 127
    meta_off = root_off + len(root)
    leaf_off = meta_off + len(meta)
    tile_off = leaf_off + len(leaves)

    w, s, e, n = bounds
    header = bytearray(b"PMTiles") + bytes([3])
    header += struct.pack("<QQQQQQQQ", root_off, len(root), meta_off, len(meta),
                          leaf_off, len(leaves), tile_off, len(blob))
    header += struct.pack("<QQQ", len(tiles), sum(1 for _ in entries), len(offsets))
    header += bytes([1, 2, 2, 1])  # clustered, gzip dirs, gzip tiles, MVT
    header += bytes([min(zooms), max(zooms)])
    header += struct.pack("<iiii", round(w * 1e7), round(s * 1e7), round(e * 1e7), round(n * 1e7))
    header += bytes([center_zoom])
    header += struct.pack("<ii", round((w + e) / 2 * 1e7), round((s + n) / 2 * 1e7))
    assert len(header) == 127

    with open(path, "wb") as f:
        f.write(header)
        f.write(root)
        f.write(meta)
        f.write(leaves)
        f.write(blob)
