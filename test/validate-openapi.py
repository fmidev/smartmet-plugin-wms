#!/usr/bin/env python3
"""
Validate the generated OpenAPI definitions of the EDR plugin and check the test requests
against them.

Usage: validate-openapi.py [--ignore <testignore-file>]
                           <definition>:<input-dir>[:<prefix>[:<output-dir>]] [...]

The optional prefix selects the requests whose file names start with it. The expected
outputs are in <output-dir>, by default <input-dir> with "input" replaced by "output".

Each <definition> is the expected output of a /api test (an optional "Status:" header block
produced with the X-Test-Headers request header is skipped). The definition is validated
against the OpenAPI 3.1 schema in schemas/oas31-schema.json, if the jsonschema module is
available.

Every request in <input-dir> whose expected output is a success (no error status in the
"Status:" header block or in a problem details body) is then checked against the definition
in the same way as clients such as OGC API Browser do:

  - the request must match an operation (the server URL path is stripped first; templates
    may be parts of segments, and of several matching paths the one the request fits best
    wins, then the one with the most literal segments)
  - path and query parameter values must be listed in the enum of the parameter, if any;
    the values of array parameters are split at commas, and case differences are accepted
  - required parameters must be present
  - every query parameter must be declared

Exits with status 1 if any check fails.
"""

import glob
import json
import os
import re
import sys
import urllib.parse

errors = []
HERE = os.path.dirname(os.path.abspath(__file__))
OAS_SCHEMA = os.path.join("openapi", "oas31-schema.json")


def error(where, msg):
    errors.append(f"{where}: {msg}")


def load_output(fname):
    with open(fname, encoding="utf-8", errors="replace") as f:
        text = f.read()
    status = None
    if text.startswith("Status:"):
        header, _, text = text.partition("\n\n")
        status = int(header.split("\n")[0].split()[1])
    return text, status


def resolve(doc, obj):
    while isinstance(obj, dict) and "$ref" in obj:
        node = doc
        for part in obj["$ref"].lstrip("#/").split("/"):
            node = node[part]
        obj = node
    return obj


def validate_schema(fname, doc):
    try:
        import jsonschema
    except ImportError:
        print("jsonschema not available, skipping OpenAPI schema validation")
        return
    # The OpenAPI 3.1 schema is a JSON Schema 2020-12 document (jsonschema 4.0 or newer)
    if not hasattr(jsonschema, "Draft202012Validator"):
        print("jsonschema too old for JSON Schema 2020-12, skipping OpenAPI schema validation")
        return
    with open(os.path.join(HERE, "schemas", OAS_SCHEMA), encoding="utf-8") as f:
        schema = json.load(f)
    validator = jsonschema.Draft202012Validator(schema)
    for e in sorted(validator.iter_errors(doc), key=lambda e: list(e.path))[:20]:
        error(fname, "/".join(map(str, e.path)) + ": " + e.message[:200])


class Operation:
    def __init__(self, doc, path, op):
        self.path = path
        self.segments = [s for s in path.split("/") if s]
        self.literals = sum(1 for s in self.segments if "{" not in s)
        # A template may be a part of a segment, e.g. {TileCol}.{format}
        self.patterns = []
        for segment in self.segments:
            names = re.findall(r"{([^}]+)}", segment)
            regex = "".join(
                "([^/]+?)" if part.startswith("{") else re.escape(part)
                for part in re.split(r"({[^}]+})", segment)
                if part
            )
            self.patterns.append((re.compile("^" + regex + "$"), names))
        params = [resolve(doc, p) for p in op.get("parameters", [])]
        self.params = {(p["in"], p["name"]): p for p in params}

    def match(self, segments):
        if len(segments) != len(self.segments):
            return None
        values = {}
        for (regex, names), value in zip(self.patterns, segments):
            m = regex.match(value)
            if m is None:
                return None
            values.update(zip(names, m.groups()))
        return values


def enum_values(param):
    schema = param.get("schema", {})
    if schema.get("type") == "array":
        return schema.get("items", {}).get("enum"), True
    return schema.get("enum"), False


def allowed(value, enum):
    """Like OGC API Browser: exact, numerically equal or case insensitive matches are fine"""
    if value in enum or any(str(value).lower() == str(e).lower() for e in enum):
        return True
    try:
        return any(float(value) == float(e) for e in enum)
    except (TypeError, ValueError):
        return False


def request_issues(op, path_values, query):
    """The problems of a request matched to an operation"""
    issues = []
    for (where, name), param in op.params.items():
        if where == "path":
            enum, _ = enum_values(param)
            if enum is not None and not allowed(path_values.get(name), enum):
                issues.append(f"{op.path}: {name} '{path_values.get(name)}' is not in the enum")

    names = {k for k, _ in query}
    for (where, name), param in op.params.items():
        if where == "query" and param.get("required") and name not in names:
            issues.append(f"{op.path}: required parameter {name} is missing")

    for name, value in query:
        param = op.params.get(("query", name))
        if param is None:
            issues.append(f"{op.path}: query parameter {name} is not declared")
            continue
        enum, is_array = enum_values(param)
        if enum is None:
            continue
        values = [v.strip() for v in value.split(",")] if is_array else [value]
        for v in values:
            # An empty value is the same as a missing option
            if v and not allowed(v, enum):
                issues.append(f"{op.path}: {name} value '{v}' is not in the enum")
    return issues


def check_request(fname, doc, operations, base_path, url):
    parts = urllib.parse.urlsplit(url)
    path = urllib.parse.unquote(parts.path)
    if base_path and path.startswith(base_path):
        path = path[len(base_path):]
    segments = [s for s in path.split("/") if s]
    query = urllib.parse.parse_qsl(parts.query, keep_blank_values=True)

    # Like a server, pick the operation the request fits best: the fewest problems, then the
    # most literal segments
    best = None
    for op in operations:
        values = op.match(segments)
        if values is None:
            continue
        issues = request_issues(op, values, query)
        key = (len(issues), -op.literals)
        if best is None or key < best[0]:
            best = (key, issues)
    if best is None:
        error(fname, f"no operation describes {parts.path}")
        return
    for issue in best[1]:
        error(fname, issue)


def check(definition, input_dir, ignored, prefix, output_dir=None):
    text, status = load_output(definition)
    doc = json.loads(text)
    validate_schema(definition, doc)

    base_path = urllib.parse.urlsplit(doc["servers"][0]["url"]).path.rstrip("/")
    operations = [
        Operation(doc, path, item["get"]) for path, item in doc["paths"].items() if "get" in item
    ]

    if not output_dir:
        output_dir = input_dir.replace("input", "output")
    count = 0
    for fname in sorted(glob.glob(os.path.join(input_dir, prefix + "*.get"))):
        name = os.path.basename(fname)
        if name in ignored:
            continue
        expected = os.path.join(output_dir, name)
        if not os.path.exists(expected):
            continue
        text, status = load_output(expected)
        if status is None and "<ExceptionReport" in text[:1000]:
            continue  # an OWS exception
        if status is None:
            # Without a status header, a problem details body is an error response
            try:
                body = json.loads(text)
                if isinstance(body, dict) and isinstance(body.get("status"), int):
                    status = body["status"]
            except ValueError:
                pass
        if status is not None and status != 200:
            continue
        with open(fname, encoding="utf-8", errors="replace") as f:
            request = f.readline().split()
        if len(request) < 2 or request[0] != "GET":
            continue
        # Server resources outside the API, such as the API catalog of the server
        path = urllib.parse.urlsplit(request[1]).path
        if base_path and not (path == base_path or path.startswith(base_path + "/")):
            continue
        check_request(fname, doc, operations, base_path, request[1])
        count += 1
    print(f"{definition}: checked {count} requests in {input_dir}")


def main(argv):
    ignored = set()
    args = []
    i = 0
    while i < len(argv):
        if argv[i] == "--ignore":
            with open(argv[i + 1], encoding="utf-8") as f:
                ignored |= {line.strip() for line in f if line.strip() and not line.startswith("#")}
            i += 2
        else:
            args.append(argv[i])
            i += 1
    if not args:
        print(__doc__)
        return 2
    for arg in args:
        definition, _, inputs = arg.partition(":")
        input_dir, _, rest = inputs.partition(":")
        prefix, _, output_dir = rest.partition(":")
        check(definition, input_dir, ignored, prefix, output_dir)
    for e in errors:
        print(e)
    print(f"{len(errors)} errors")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
