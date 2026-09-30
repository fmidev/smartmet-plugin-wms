#!/usr/bin/env python3
"""Rewrite MVT tile reference fixtures after the makeCmd CommandInteger fix.

The .get fixtures under test/output/ store the tile as protobuf TextFormat, i.e.
a flat list of `geometry: N` integers per feature. They were frozen from the
buggy encoder, whose makeCmd packed the MVT CommandInteger as (id<<3)|count
instead of the spec's (count<<3)|(id&0x7). The fix changes ONLY the command
integers; the zigzag coordinate parameters and their positions are byte-identical
between the buggy and fixed builds. So the corrected fixture is exactly the
current one with the command integers transformed in place.

This rewriter re-derives the command boundaries from the rigid structure the
buggy encoder produced -- every part begins with MoveTo (encoded 9), polygon
parts end with ClosePath (encoded 57), and a LineTo is encoded 16|count -- using
a backtracking parse that requires the whole feature to consume exactly. The
only ambiguity is a LineTo whose count has bit 4 set (count>=16), giving two
candidates {V, V^16}; we resolve it by global consistency and, if a feature does
not have a UNIQUE parse, we abort rather than guess. Each command integer is then
re-emitted correctly (MoveTo->9, LineTo(c)->(c<<3)|2, ClosePath->15) and every
parameter is left untouched.

Note: the buggy encoding is not always uniquely decodable (a LineTo count>=16 is
genuinely ambiguous), so for real fixtures the canonical regeneration is the
harness (test/regenerate_mvt_expected.sh), which runs the freshly built plugin.
This tool is therefore most useful in --check mode: it decodes a fixture with the
correct MVT semantics and asserts the command stream is spec-legal (all commands
MoveTo/LineTo/ClosePath, polygon rings closed). Run it on the regenerated fixtures
as an independent gate; running it on the pre-fix fixtures fails, demonstrating
the bug. The in-place rewrite is a safe best-effort: it aborts on any feature
without a unique parse rather than guessing.

Usage:
    rewrite_mvt_expected.py --check  FILE.get ...        # validate (no writes)
    rewrite_mvt_expected.py --dry-run FILE.get ...       # report rewrite, no writes
    rewrite_mvt_expected.py FILE.get [FILE.get ...]      # rewrite in place
"""

import re
import sys

MOVETO, LINETO, CLOSEPATH = 1, 2, 7
BUGGY_MOVETO = (MOVETO << 3) | 1      # 9   (also correct: id==count==1)
BUGGY_CLOSEPATH = (CLOSEPATH << 3) | 1  # 57  (correct would be 15)
CORRECT_MOVETO = (1 << 3) | MOVETO    # 9
CORRECT_CLOSEPATH = (1 << 3) | CLOSEPATH  # 15


def lineto_counts(value):
    """Candidate LineTo counts c such that the buggy encoding 16|c == value."""
    base = (LINETO << 3)  # 16
    return sorted({c for c in (value, value ^ base) if c >= 1 and (base | c) == value})


def rewrite_geometry(g, kind):
    """Return the spec-correct command stream for one feature.

    g     -- list[int], the buggy geometry integers
    kind  -- 'POINT' | 'LINESTRING' | 'POLYGON'
    Raises ValueError if the feature does not parse uniquely.
    """
    n = len(g)
    memo = {}

    def nparse(i):
        """Number of valid ways to parse g[i:] into whole parts ending at n."""
        if i == n:
            return 1
        if i in memo:
            return memo[i]
        total = 0
        # Every part starts with a MoveTo, encoded as 9, followed by 2 params.
        if g[i] == BUGGY_MOVETO and i + 3 <= n:
            j = i + 3
            if kind == "POINT":
                total += nparse(j)  # next point/part or end
            else:
                if kind == "POLYGON" and j < n and g[j] == BUGGY_CLOSEPATH:
                    total += nparse(j + 1)  # degenerate ring: MoveTo + ClosePath
                if j < n:
                    for c in lineto_counts(g[j]):
                        end = j + 1 + 2 * c
                        if kind == "POLYGON":
                            if end < n and g[end] == BUGGY_CLOSEPATH:
                                total += nparse(end + 1)
                        else:  # LINESTRING
                            if end <= n:
                                total += nparse(end)
        memo[i] = total
        return total

    total = nparse(0)
    if total != 1:
        raise ValueError(f"{kind} feature has {total} valid parses (need exactly 1)")

    # Reconstruct the unique parse. With nparse(0)==1, exactly one branch out of
    # each reached node has nparse(next)>=1, so the walk is deterministic.
    out, cmd_pos, i = [], [], 0
    while i < n:
        cmd_pos.append(len(out))
        out += [CORRECT_MOVETO, g[i + 1], g[i + 2]]  # MoveTo + 2 params (params kept)
        if kind == "POINT":
            i += 3
            continue
        j = i + 3
        branches = []  # (emit_closepath_only, count, next_i)
        if kind == "POLYGON" and j < n and g[j] == BUGGY_CLOSEPATH:
            branches.append((True, None, j + 1))
        if j < n:
            for c in lineto_counts(g[j]):
                end = j + 1 + 2 * c
                if kind == "POLYGON":
                    if end < n and g[end] == BUGGY_CLOSEPATH:
                        branches.append((False, c, end + 1))
                elif end <= n:
                    branches.append((False, c, end))
        live = [b for b in branches if nparse(b[2]) >= 1]
        if len(live) != 1:
            raise ValueError(f"{kind} reconstruction ambiguity at index {i}")
        close_only, c, nxt = live[0]
        if close_only:
            cmd_pos.append(len(out))
            out.append(CORRECT_CLOSEPATH)
        else:
            cmd_pos.append(len(out))
            out.append((c << 3) | LINETO)
            out += g[j + 1: j + 1 + 2 * c]  # LineTo params (kept)
            if kind == "POLYGON":
                cmd_pos.append(len(out))
                out.append(CORRECT_CLOSEPATH)
        i = nxt

    _validate(g, out, set(cmd_pos), kind)
    return out


def validate_correct(stream, kind):
    """Decode a stream with correct MVT semantics; assert it is spec-legal.

    Raises AssertionError on any illegal command, bad count, unclosed polygon
    ring, or a closed linestring.
    """
    i, n, parts = 0, len(stream), []
    while i < n:
        cmdint = stream[i]
        cid, count = cmdint & 0x7, cmdint >> 3
        i += 1
        if cid == MOVETO:
            if count != 1:
                raise AssertionError("MoveTo count != 1")
            i += 2
            parts.append([False])
        elif cid == LINETO:
            if count < 1:
                raise AssertionError("LineTo count < 1")
            i += 2 * count
        elif cid == CLOSEPATH:
            if count != 1 or not parts:
                raise AssertionError("bad ClosePath")
            parts[-1][0] = True
        else:
            raise AssertionError(f"illegal command id {cid}")
    if i != n:
        raise AssertionError("decode overran / truncated parameters")
    if kind == "POLYGON" and not all(p[0] for p in parts):
        raise AssertionError("polygon ring not closed")
    if kind == "LINESTRING" and any(p[0] for p in parts):
        raise AssertionError("linestring should not be closed")


def _validate(g, out, cmd_pos, kind):
    """Defensive cross-checks on the rewrite."""
    if len(out) != len(g):
        raise AssertionError("length changed")
    # Only command positions may differ; every parameter must be untouched.
    for k, (a, b) in enumerate(zip(g, out)):
        if a != b and k not in cmd_pos:
            raise AssertionError(f"parameter changed at index {k}: {a} -> {b}")
    validate_correct(out, kind)


# ----------------------------------------------------------------------
# TextFormat .get file handling
# ----------------------------------------------------------------------

GEOM_RE = re.compile(r"^(\s*geometry:\s*)(-?\d+)(\s*)$")
TYPE_RE = re.compile(r"^\s*type:\s*(\w+)")


def rewrite_file(path, dry_run=False):
    with open(path) as f:
        lines = f.readlines()

    changed = 0
    for geom_idx, ftype in list(iter_features(lines)):
        if not geom_idx:
            continue
        kind = {"POINT": "POINT", "LINESTRING": "LINESTRING",
                "POLYGON": "POLYGON"}.get(ftype)
        if kind is None:
            raise ValueError(f"{path}: unsupported feature type {ftype!r}")
        g = [int(GEOM_RE.match(lines[i]).group(2)) for i in geom_idx]
        out = rewrite_geometry(g, kind)
        for i, val in zip(geom_idx, out):
            m = GEOM_RE.match(lines[i])
            new = f"{m.group(1)}{val}{m.group(3)}"
            if new != lines[i]:
                lines[i] = new
                changed += 1

    if changed and not dry_run:
        with open(path, "w") as f:
            f.writelines(lines)
    return changed


def iter_features(lines):
    """Yield (geom_line_indices, feature_type) for each `features { … }` block."""
    depth = 0
    stack = []
    for idx, line in enumerate(lines):
        stripped = line.strip()
        if stripped.endswith("{"):
            depth += 1
            if stripped[:-1].strip() == "features":
                stack.append({"geom": [], "type": None, "d": depth})
            continue
        if stripped == "}":
            if stack and stack[-1]["d"] == depth:
                fs = stack.pop()
                yield fs["geom"], fs["type"]
            depth -= 1
            continue
        if stack:
            m = TYPE_RE.match(line)
            if m:
                stack[-1]["type"] = m.group(1)
            elif GEOM_RE.match(line):
                stack[-1]["geom"].append(idx)


def check_file(path):
    """Validate every feature in a fixture as spec-legal MVT. Returns #features."""
    with open(path) as f:
        lines = f.readlines()
    count = 0
    for k, (geom_idx, ftype) in enumerate(iter_features(lines)):
        if not geom_idx:
            continue
        g = [int(GEOM_RE.match(lines[i]).group(2)) for i in geom_idx]
        try:
            validate_correct(g, ftype)
        except AssertionError as e:
            raise AssertionError(f"feature #{k} ({ftype}): {e}")
        count += 1
    return count


def self_test():
    # Unit square (POLYGON) as the buggy encoder emits it -> spec-correct stream:
    # LineTo(3) 19->26, ClosePath 57->15, params untouched.
    assert rewrite_geometry(
        [9, 0, 8192, 19, 20, 0, 0, 19, 19, 0, 57],
        "POLYGON") == [9, 0, 8192, 26, 20, 0, 0, 19, 19, 0, 15]
    # Open 3-point line: MoveTo + LineTo(2). Buggy LineTo(2) == 16|2 == 18 -> 18.
    assert rewrite_geometry(
        [9, 10, 10, 18, 4, 4, 6, 6],
        "LINESTRING") == [9, 10, 10, (2 << 3) | LINETO, 4, 4, 6, 6]
    # Single point: MoveTo only, unchanged.
    assert rewrite_geometry([9, 24, 68], "POINT") == [9, 24, 68]


def main(argv):
    check = "--check" in argv
    dry = "--dry-run" in argv
    args = [a for a in argv if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 1
    self_test()
    for path in args:
        try:
            if check:
                n = check_file(path)
                print(f"{path}: OK, {n} feature(s) spec-legal")
            else:
                n = rewrite_file(path, dry_run=dry)
                verb = "would change" if dry else "changed"
                print(f"{path}: {verb} {n} command integer(s)")
        except (ValueError, AssertionError) as e:
            print(f"FAIL {path}: {e}", file=sys.stderr)
            return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
