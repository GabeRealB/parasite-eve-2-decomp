#!/usr/bin/env python3
"""Lexical macro inventory across project sources, including inactive branches.

Detailed preprocessing records crash the project's libclang build. This scanner
therefore reports potential uses, not resolved preprocessor expansions. Identity
is definition-file/name; conditional redefinitions in one file are one review.
Include reachability limits candidates, but does not prove which branch is live.
Ambiguous uses and token construction must be inspected before renaming.
"""

from __future__ import annotations

import argparse
from bisect import bisect_right
from collections import defaultdict
from dataclasses import dataclass
import json
from pathlib import Path
import re
import tomllib

IDENT = r"[A-Za-z_]\w*"
UPPER = re.compile(r"[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*\Z")
GTE = re.compile(r"gte_[A-Za-z0-9_]+\Z")
LITERALS = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
DIRECTIVE = re.compile(r"^[ \t]*#[ \t]*(\w+)([^\n]*)", re.M)
DEFINE = re.compile(r"[ \t]+(" + IDENT + r")(\([^\n]*?\))?(.*)\Z", re.S)
# Common semantic helpers remain in scope; compiler/assembly machinery does not.
COMMON_HELPERS = {"ARRAY_SIZE", "OFFSET_OF", "PARENT_OF", "PLAYSTATION_SCRATCHPAD_BASE", "PLAYSTATION_SCRATCHPAD_ADDRESS"}


def conventional(name):
    """PsyQ-style GTE wrappers retain their established mixed-case spelling."""
    return bool(UPPER.fullmatch(name) or GTE.fullmatch(name))


def configuration_names(text):
    names = set()

    def walk(value):
        if isinstance(value, dict):
            names.update(value.get("defines", {}))
            for child in value.values():
                walk(child)
        elif isinstance(value, list):
            for child in value:
                walk(child)

    walk(tomllib.loads(text))
    return names


def definition_exists(root, spec):
    """Cheap dispatch before asking the C resolver about a preprocessor name."""
    file, sep, name = spec.rpartition("/")
    if not sep or not re.fullmatch(IDENT, name):
        return False
    path = Path(root) / file
    if not path.is_file():
        return False
    if file == "configs/USA/overlays.toml":
        return name in configuration_names(path.read_text())
    logical = re.sub(r"\\\r?\n", lambda m: " " * len(m[0]), path.read_text())
    text = LITERALS.sub(lambda m: re.sub(r"[^\n]", " ", m[0]), logical)
    return bool(re.search(r"(?m)^\s*#\s*define\s+" + re.escape(name) + r"\b", text))


@dataclass(frozen=True)
class Site:
    file: str
    start: int
    end: int
    line: int
    name: str
    use: str
    owner: str = ""


class Inventory:
    def __init__(self, root):
        self.root = Path(root)
        self.text = {}
        self.code = {}
        self.definitions = defaultdict(list)
        self.sites = defaultdict(list)
        self.includes = defaultdict(set)
        self.metadata = {}
        paths = sorted(p for base in ("src", "include") for p in (self.root / base).rglob("*")
                       if p.is_file() and p.suffix in (".c", ".h") and "psyq" not in p.parts)
        for path in paths:
            self._scan(path.relative_to(self.root).as_posix(), path.read_text())
        self._manifest()
        self.by_name = defaultdict(set)
        for key, definitions in self.definitions.items():
            self.by_name[definitions[0].name].add(key)
        # All branches of literal includes are retained. Generated/SDK includes
        # are external to this inventory and have no project macro definitions.
        self.closures = {}
        self.carriers = defaultdict(set)
        for file in self.text:
            closure, todo = set(), [file]
            while todo:
                current = todo.pop()
                if current in closure:
                    continue
                closure.add(current)
                todo.extend(self.includes[current] - closure)
            self.closures[file] = closure
            if file.endswith(".c") and not file.endswith(".inc.c"):
                for member in closure:
                    self.carriers[member].add(file)

    def _scan(self, file, text):
        self.text[file] = text
        # Keep offsets stable through line splicing and comment/string removal.
        logical = re.sub(r"\\\r?\n", lambda m: " " * len(m[0]), text)
        code = LITERALS.sub(lambda m: re.sub(r"[^\n]", " ", m[0]), logical)
        self.code[file] = code
        newlines = [m.start() for m in re.finditer("\n", text)]
        line = lambda offset: bisect_right(newlines, offset) + 1
        directives = list(DIRECTIVE.finditer(code))
        ranges = []
        guard = None
        # A guard surrounds a whole header. A leading conditional feature
        # definition in a source file is still a semantic configuration item.
        depth, closing = 0, None
        for directive in directives:
            if directive[1] in ("if", "ifdef", "ifndef"):
                depth += 1
            elif directive[1] == "endif":
                depth -= 1
                if depth == 0:
                    closing = directive
                    break
        if (file.endswith(".h") and directives and closing == directives[-1]
                and not code[:directives[0].start()].strip()
                and not code[closing.end():].strip()):
            first = directives[0]
            if first[1] == "ifndef":
                guard = first[2].strip()
            elif first[1] == "if":
                match = re.fullmatch(r"\s*!\s*defined\s*\(?\s*(" + IDENT + r")\s*\)?\s*", first[2])
                guard = match[1] if match else None
        # A conditional whose whole extent holds nothing but directives selects
        # no declaration: `#ifndef NAME / #define NAME default / #endif`, the way
        # a shared source takes a parameter its carrier may have set already.
        defaults, stack = set(), []
        for i, directive in enumerate(directives):
            if directive[1] in ("if", "ifdef", "ifndef"):
                stack.append([i])
            elif directive[1] in ("elif", "else") and stack:
                stack[-1].append(i)
            elif directive[1] == "endif" and stack:
                members = stack.pop()
                inside = code[directives[members[0]].end():directive.start()]
                if not DIRECTIVE.sub("", inside).strip():
                    defaults.update(directives[m].start() for m in members)
        for directive in directives:
            kind, tail = directive[1], directive[2]
            if kind == "include":
                original = logical[directive.start():directive.end()]
                match = re.search(r'[<"]([^>"]+)[>"]', original)
                if match:
                    for base in (Path(file).parent, Path("include"), Path("include/decomp"), Path("src")):
                        path = (self.root / base / match[1]).resolve()
                        try:
                            rel = path.relative_to(self.root.resolve()).as_posix()
                        except ValueError:
                            continue
                        if path.is_file():
                            self.includes[file].add(rel)
                            break
            params, key, body_start = set(), "", directive.end()
            if kind == "define":
                match = DEFINE.fullmatch(tail)
                if match:
                    name = match[1]
                    key = f"{file}/{name}"
                    start = directive.start(2) + match.start(1)
                    site = Site(file, start, start + len(name), line(start), name, "definition", key)
                    self.definitions[key].append(site)
                    body_start = directive.start(2) + match.start(3)
                    params = set(re.findall(IDENT, match[2] or ""))
                    # A guard is the empty first definition following its test.
                    is_guard = name == guard and not match[2] and not match[3].strip() and len(directives) > 1 and directive == directives[1]
                    excluded = is_guard or file == "include/include_asm.h" or (
                        file.startswith("include/decomp/") and not (file == "include/decomp/common.h" and name in COMMON_HELPERS))
                    self.metadata.setdefault(key, {"file": file, "line": site.line, "spelling": name,
                                                   "excluded": excluded, "configuration": False})
                    self.metadata[key]["excluded"] &= excluded
            ranges.append((directive.start(), directive.end(), kind, key, params, body_start))
        ri = 0
        for token in re.finditer(IDENT, code):
            offset, name = token.start(), token[0]
            while ri < len(ranges) and offset >= ranges[ri][1]:
                ri += 1
            use, owner = "potential use", ""
            if ri < len(ranges) and ranges[ri][0] <= offset:
                start, end, kind, owner, params, body_start = ranges[ri]
                if kind == "define":
                    if offset < body_start or name in params:
                        continue
                    use = "replacement token"
                elif kind in ("if", "elif", "ifdef", "ifndef", "undef"):
                    if name in (kind, "defined"):
                        continue
                    use = ("undefinition" if kind == "undef" else
                           "default test" if start in defaults else "conditional test")
                else:
                    continue
            self.sites[name].append(Site(file, offset, token.end(), line(offset), name, use, owner))

    def _manifest(self):
        path = self.root / "configs/USA/overlays.toml"
        if not path.exists():
            return
        file, text = path.relative_to(self.root).as_posix(), path.read_text()
        self.text[file] = text
        for name in sorted(configuration_names(text)):
            key = f"{file}/{name}"
            # These keys are build configuration, not C declarations. Keep every
            # value visible and require explicit config maintenance on rename.
            for match in re.finditer(r"\b(" + re.escape(name) + r")\b[\"']?(?=\s*=)", text):
                line = text.count("\n", 0, match.start(1)) + 1
                self.definitions[key].append(Site(file, match.start(1), match.end(1), line, name, "configuration definition", key))
            if self.definitions[key]:
                self.metadata[key] = {"file": file, "line": self.definitions[key][0].line,
                                      "spelling": name, "excluded": False, "configuration": True}
            else:
                del self.definitions[key]

    def candidates(self, site):
        candidates = set()
        for key in self.by_name.get(site.name, ()):
            file = self.metadata[key]["file"]
            if self.metadata[key]["configuration"] or file in self.closures.get(site.file, {site.file}) or (
                self.carriers[file] & self.carriers[site.file]
            ):
                candidates.add(key)
        return candidates

    def references(self, key):
        name = self.metadata[key]["spelling"]
        return [(site, keys) for site in self.sites[name]
                if key in (keys := self.candidates(site))]

    def report(self, key, summary=False):
        meta = self.metadata[key]
        print(f"Macro {key}: {'configuration' if meta['configuration'] else 'project'}; all conditional definitions included")
        print("Uses are lexical candidates, not proven active expansions. Review include order, branches and token construction.")
        for site in self.definitions[key]:
            text = self.text[site.file]
            if meta["configuration"]:
                body = text.splitlines()[site.line - 1]
            else:
                match = next(d for d in DIRECTIVE.finditer(self.code[site.file]) if d.start() <= site.start < d.end())
                body = text[match.start():match.end()]
            print(f"\n{site.file}:{site.line}: {body.strip()}")
        refs = self.references(key)
        print(f"\n{len(refs)} potential use(s); {sum(len(keys) > 1 for _, keys in refs)} ambiguous")
        if not summary:
            for site, keys in refs:
                context = self.text[site.file].splitlines()[site.line - 1].strip()
                print(f"{site.file}:{site.line}: {site.use}: {context}" + (f"; candidates: {', '.join(sorted(keys))}" if len(keys) > 1 else ""))
        peers = self.by_name[meta["spelling"]] - {key}
        if peers:
            print("Other definitions with this spelling: " + ", ".join(sorted(peers)))
        if meta["configuration"]:
            print("Build-defined macro: review manifest values and every variant; update configuration keys when renaming.")

    def rename(self, key, new_name, *, reviewed=False, dry_run=False, ledger="local/renames.tsv"):
        if not conventional(new_name):
            raise ValueError("macro names must be UPPER_SNAKE_CASE, except established gte_* wrappers")
        if new_name in self.by_name:
            raise ValueError(f"macro {new_name} already exists; reconcile its definitions first")
        meta = self.metadata[key]
        if meta["excluded"] or meta["configuration"]:
            raise ValueError("infrastructure/configuration macro: inspect and update the defining source/configuration explicitly")
        refs = self.references(key)
        ambiguous = [site for site, keys in refs if len(keys) != 1]
        if ambiguous:
            raise ValueError("ambiguous definitions reach a use; inspect the references and edit the intended definitions/uses explicitly")
        # Token pasting can manufacture a name with no whole-token occurrence.
        # Scoping or conditional selection also requires human review; never
        # present a lexical candidate list as a parser-proven automatic rename.
        if not reviewed and not dry_run:
            raise ValueError("inspect --dry-run and references, then pass --macro-reviewed after checking branches, SDK collisions and #/## construction")
        edits = defaultdict(set)
        for site in self.definitions[key] + [s for s, _ in refs]:
            edits[site.file].add((site.start, site.end))
        for file, positions in sorted(edits.items()):
            print(f"  {len(positions):5} {file}")
        print("Lexical rename: comments, string contents, generated tokens and build flags require separate review.")
        if dry_run:
            print("dry run: nothing written")
            return
        staged = {}
        for file, positions in edits.items():
            text = self.text[file]
            for start, end in sorted(positions, reverse=True):
                if text[start:end] != meta["spelling"]:
                    raise ValueError(f"{file}: stale token position")
                text = text[:start] + new_name + text[end:]
            staged[file] = text
        for file, text in staged.items():
            (self.root / file).write_text(text)
        from rename_item import record_rename
        record_rename(str(self.root), ledger, "macro", key,
                      f"{meta['file']}/{new_name}", meta["file"], sum(map(len, edits.values())))


def add_graph(root, nodes, edges):
    """Add conservative preprocessor dependencies without detailed clang records.

    Candidate macros used by one declaration precede that declaration. Names in
    macro bodies also depend on reachable C declarations. Ambiguous bindings at
    a shared include are reviewed together, not confused with unrelated macros.
    """
    inv = Inventory(root)
    active = {key for key, meta in inv.metadata.items() if not meta["excluded"]}
    c_names, owners, file_nodes = defaultdict(set), defaultdict(set), defaultdict(set)
    for usr, meta in nodes.items():
        c_names[meta["name"]].add(usr)
        file = meta.get("file")
        if file:
            file_nodes[file].add(usr)
            for line in range(meta.get("start_line", meta.get("line", 0)),
                              meta.get("end_line", meta.get("line", 0)) + 1):
                owners[file, line].add(usr)
    for key in sorted(active):
        nodes["macro:" + key] = dict(inv.metadata[key], name=key, kind="macro")
        edges["macro:" + key] = set()
    body_names = defaultdict(set)
    for name, sites in inv.sites.items():
        if name not in inv.by_name and name not in c_names:
            continue
        for site in sites:
            candidates = inv.candidates(site) & active
            users = ({"macro:" + site.owner} if site.owner in active
                     else owners.get((site.file, site.line), set()))
            if candidates and site.use == "conditional test" and not users:
                # A file-level condition can select whole declarations, which
                # have no enclosing cursor at the directive. Overapproximate
                # its users to the file and its carriers, including variants.
                users = set(file_nodes[site.file])
                for carrier in inv.carriers[site.file]:
                    users.update(file_nodes[carrier])
            deps = {"macro:" + key for key in candidates}
            for user in users:
                edges.setdefault(user, set()).update(deps - {user})
            # A shared source use may be supplied by several carriers. Keep
            # that binding contract in one step while retaining separate IDs.
            for dep in deps:
                edges[dep].update(deps - {dep})
            if site.owner not in active:
                continue
            body_names[site.owner].add(name)
            for dep in c_names.get(name, ()):
                file = nodes[dep].get("file")
                if file and (file in inv.closures.get(site.file, ()) or
                             inv.carriers[file] & inv.carriers[site.file]):
                    edges["macro:" + site.owner].add(dep)
    # Captured globals/functions may be defined outside the including TU. The
    # AST's resolved dependencies of actual expansion users supply their IDs.
    macro_users = defaultdict(set)
    for usr, deps in edges.items():
        for dep in deps:
            if dep.startswith("macro:"):
                macro_users[dep].add(usr)
    for key in active:
        macro = "macro:" + key
        seen, todo, expanded_deps = set(), [macro], set()
        while todo:
            dep = todo.pop()
            if dep in seen:
                continue
            seen.add(dep)
            for user in macro_users[dep]:
                if user.startswith("macro:"):
                    todo.append(user)
                else:
                    expanded_deps.update(edges.get(user, ()))
        for name in body_names[key]:
            edges[macro].update(c_names[name] & expanded_deps)
    print(f"  inventoried {len(active)} project macros (including inactive branches)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("spec", nargs="?")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--summary", action="store_true")
    parser.add_argument("--exists", action="store_true", help="test the exact definition-file/name without scanning uses")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if args.exists:
        parser.exit(0 if args.spec and definition_exists(args.root, args.spec) else 1)
    inv = Inventory(args.root)
    if args.spec:
        if args.spec not in inv.definitions:
            parser.error(f"no macro definition: {args.spec}")
        inv.report(args.spec, args.summary)
    else:
        result = {key: meta for key, meta in inv.metadata.items() if not meta["excluded"]}
        if args.out:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(json.dumps(result, indent=2) + "\n")
        print(f"{len(result)} project macros (definition-file/name; guards and infrastructure excluded)")


if __name__ == "__main__":
    main()
