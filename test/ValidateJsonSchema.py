#!/usr/bin/env python3
"""Validate JSON files against an OGC OpenAPI schema (YAML).

Usage: ValidateJsonSchema.py SCHEMA.yaml FILE.json [FILE.json ...]

The OGC schemas reference each other with relative-path $ref values
("collectionInfo.yaml", "../common-core/link.yaml"). Those are inlined
here before validation so that any jsonschema version works, including
the 2.6 shipped with RHEL8. Schemas are OpenAPI 3.0 dialect, i.e. a
JSON Schema draft 4 subset plus keywords such as "nullable" and
"example" that a draft 4 validator ignores.

Exit status is 0 when every file validates, 1 otherwise, 2 on usage or
missing dependencies. Errors are printed one per line with the JSON path.
"""

import json
import os
import sys

try:
    import yaml
    import jsonschema
except ImportError as e:  # pragma: no cover
    sys.stderr.write(
        f"ValidateJsonSchema.py: missing python module: {e.name}\n"
        "Install python3-jsonschema and python3-pyyaml.\n")
    sys.exit(2)


def load_schema(path):
    """Load a YAML schema and inline every relative-path $ref recursively."""
    with open(path) as f:
        schema = yaml.safe_load(f)
    return inline_refs(schema, os.path.dirname(os.path.abspath(path)))


def inline_refs(node, base_dir):
    if isinstance(node, dict):
        ref = node.get("$ref")
        if isinstance(ref, str) and not ref.startswith("#"):
            return load_schema(os.path.normpath(os.path.join(base_dir, ref)))
        return {k: inline_refs(v, base_dir) for k, v in node.items()}
    if isinstance(node, list):
        return [inline_refs(v, base_dir) for v in node]
    return node


def json_path(error):
    path = "$"
    for p in error.absolute_path:
        path += f"[{p}]" if isinstance(p, int) else f".{p}"
    return path


def main(argv):
    if len(argv) < 3:
        sys.stderr.write(__doc__)
        return 2

    schema = load_schema(argv[1])
    validator = jsonschema.Draft4Validator(schema, format_checker=jsonschema.FormatChecker())

    failed = 0
    for path in argv[2:]:
        try:
            with open(path) as f:
                instance = json.load(f)
        except (OSError, ValueError) as e:
            print(f"{path}: cannot read as JSON: {e}")
            failed += 1
            continue

        errors = sorted(validator.iter_errors(instance), key=lambda e: list(e.absolute_path))
        if errors:
            failed += 1
            print(f"{path}: FAIL: {len(errors)} schema violation(s) against {argv[1]}")
            for e in errors[:20]:
                print(f"    {json_path(e)}: {e.message}")
            if len(errors) > 20:
                print(f"    ... and {len(errors) - 20} more")
        else:
            print(f"{path}: OK ({os.path.basename(argv[1])})")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
