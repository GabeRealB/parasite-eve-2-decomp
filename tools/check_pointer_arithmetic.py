#!/usr/bin/env python3
"""
Detect pointer arithmetic with casts in C code.

This linter detects patterns like:
    (u8*)ptr + offset
    (SomeStruct*)ptr + index
    *(s32*)((char*)ptr + 0x10)

These should be replaced with proper struct field access.
"""

import re
import sys
import os
import argparse
import subprocess
from datetime import datetime
from pycparser import c_parser, c_ast, parse_file
from pycparser.plyparser import ParseError


class PointerArithmeticDetector(c_ast.NodeVisitor):
    """AST visitor that detects pointer arithmetic with casts."""

    def __init__(self, filename):
        self.filename = filename
        self.violations = []

    def visit_BinaryOp(self, node):
        """Check binary operations for cast + pointer arithmetic pattern."""
        # Check if this is addition or subtraction
        if node.op in ["+", "-"]:
            # Check if left operand is a cast
            if isinstance(node.left, c_ast.Cast):
                # Check if the cast is to a pointer type
                if self._is_pointer_cast(node.left):
                    # This is a violation: (Type*)expr + offset
                    self.violations.append(
                        {
                            # The node's own file: a header's inline body is
                            # reported under the header, not every includer.
                            "file": os.path.normpath(node.coord.file),
                            "line": node.coord.line,
                            "column": node.coord.column,
                            "type": self._get_cast_type(node.left),
                            "coord": node.coord,
                        }
                    )

        # Continue traversing the tree
        self.generic_visit(node)

    def _is_pointer_cast(self, cast_node):
        """Check if a cast node is casting to a pointer type."""
        type_node = cast_node.to_type

        # Navigate through type declarations to find PtrDecl
        while type_node:
            if isinstance(type_node, c_ast.PtrDecl):
                return True
            if isinstance(type_node, c_ast.TypeDecl):
                return False
            if isinstance(type_node, c_ast.Typename):
                type_node = type_node.type
            else:
                break

        return False

    def _get_cast_type(self, cast_node):
        """Extract the type name from a cast for reporting."""
        type_node = cast_node.to_type

        # Try to build a readable type string
        if isinstance(type_node, c_ast.Typename):
            return self._typename_to_string(type_node)

        return "unknown type"

    def _typename_to_string(self, typename):
        """Convert a typename AST node to a string."""
        parts = []

        # Get the base type names
        if typename.type:
            parts.extend(self._get_type_names(typename.type))

        # Add pointer indicator
        if self._has_pointer(typename.type):
            parts.append("*")

        return "".join(parts) if parts else "unknown"

    def _get_type_names(self, node):
        """Recursively extract type name strings."""
        names = []

        if isinstance(node, c_ast.IdentifierType):
            names.extend(node.names)
        elif isinstance(node, c_ast.TypeDecl):
            if node.type:
                names.extend(self._get_type_names(node.type))
        elif isinstance(node, c_ast.PtrDecl):
            if node.type:
                names.extend(self._get_type_names(node.type))
        elif isinstance(node, c_ast.Struct):
            if node.name:
                names.append(f"struct {node.name}")
            else:
                names.append("struct")

        return names

    def _has_pointer(self, node):
        """Check if type includes a pointer."""
        if isinstance(node, c_ast.PtrDecl):
            return True
        if isinstance(node, c_ast.TypeDecl):
            return False
        if hasattr(node, "type"):
            return self._has_pointer(node.type)
        return False


ASM_START = re.compile(r"\b(?:__asm__|asm)\b\s*(?:(?:volatile|__volatile__)\s*)?\(")


def strip_asm(text):
    """Remove every GNU asm construct, parentheses balanced."""
    out, i = [], 0
    while True:
        m = ASM_START.search(text, i)
        if not m:
            out.append(text[i:])
            return "".join(out)
        out.append(text[i:m.start()])
        depth, j = 1, m.end()
        while depth and j < len(text):
            if text[j] == '"':
                j += 1
                while j < len(text) and text[j] != '"':
                    j += 2 if text[j] == "\\" else 1
            elif text[j] == "(":
                depth += 1
            elif text[j] == ")":
                depth -= 1
            j += 1
        # Keep the removed span's newlines so reported line numbers stay right.
        out.append("\n" * text.count("\n", m.start(), j))
        i = j


def preprocess_file(filename, cpp_path="gcc", cpp_args=""):
    """
    Preprocess a C file for parsing.

    Uses gcc preprocessor to handle includes and macros.
    """
    # Match the Makefile's include paths (IINC)
    include_paths = [
        "include",
        "include/psyq",
        "include/decomp",
        "src",
    ]

    # Match the Makefile's macros (MACROS)
    cpp_args_list = [
        "-E",  # Preprocess only
        "-D_LANGUAGE_C",
        "-D_MIPS_SZLONG=32",
        "-D_MIPS_SZINT=32",
        "-D__USE_ISOC99",
        "-DF3DEX_GBI_2",
        "-DNDEBUG",
        "-D_FINALROM",
        # pycparser compatibility macros
        "-D__attribute__(x)=",
        "-D__extension__=",
        "-D__inline__=",
        "-D__restrict=",
        "-D__volatile__=volatile",
        "-D__inline=",
        "-D__const=const",
    ]

    # Add include paths
    for path in include_paths:
        if os.path.exists(path):
            cpp_args_list.append(f"-I{path}")

    # pycparser does not know GNU asm: statements with operand lists,
    # `asm volatile`, and `asm("name")` labels on declarations. The
    # preprocessor cannot remove `__asm__ volatile (...)` (a function-like
    # macro needs its `(` next), so strip them from the preprocessed text.
    try:
        text = subprocess.run(
            [cpp_path, *cpp_args_list, filename], capture_output=True, text=True, check=True
        ).stdout
        return c_parser.CParser().parse(strip_asm(text), filename)
    except subprocess.CalledProcessError as e:
        print(f"Warning: Could not preprocess {filename}: {e.stderr.strip()[:200]}", file=sys.stderr)
        return None
    except ParseError as e:
        print(f"Warning: Could not parse {filename}: {e}", file=sys.stderr)
        return None


# Macros whose expansion is pointer arithmetic by design: the scratch-pad
# stack and its fixed address (main/scratch.h, PSX_SCRATCH_ADDR) and the
# link-node-to-enemy step (gameplay/1BC.h).
# The check sees preprocessed code, so it tests the source line instead.
SANCTIONED = re.compile(r"\b(?:SCRATCH_[A-Z_]+|GP_NODE_ENEMY|PSX_SCRATCH_ADDR)\s*\(|\b(?:G_SCRATCH_HEAD|SCRATCH_HEAD_ADDR)\b")
_LINES: dict = {}


def sanctioned(violation):
    path = violation["file"]
    if path not in _LINES:
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                _LINES[path] = f.read().split("\n")
        except OSError:
            _LINES[path] = []
    lines = _LINES[path]
    n = violation["line"]
    return 0 < n <= len(lines) and bool(SANCTIONED.search(lines[n - 1]))


def check_file(filename, verbose=False):
    """Check a single C file for pointer arithmetic violations."""
    if verbose:
        print(f"Checking {filename}...", file=sys.stderr)

    try:
        ast = preprocess_file(filename)
        if ast is None:
            return []

        detector = PointerArithmeticDetector(filename)
        detector.visit(ast)

        return [v for v in detector.violations if not sanctioned(v)]

    except Exception as e:
        if verbose:
            print(f"Error processing {filename}: {e}", file=sys.stderr)
        return []


def check_directory(directory, exclude_dirs=None, verbose=False):
    """Recursively check all C files in a directory."""
    if exclude_dirs is None:
        exclude_dirs = {"build", "tools", "lib", "expected", ".git"}

    paths = []
    for root, dirs, files in os.walk(directory):
        # Remove excluded directories from traversal
        dirs[:] = [d for d in dirs if d not in exclude_dirs]
        paths += [os.path.join(root, f) for f in files if f.endswith(".c")]

    # pycparser is pure Python, about a second per file, and files are
    # independent: parse them across every core.
    from concurrent.futures import ProcessPoolExecutor

    with ProcessPoolExecutor() as pool:
        results = pool.map(check_file, sorted(paths), [verbose] * len(paths), chunksize=4)
    grouped: dict = {}
    seen = set()
    for vs in results:
        for v in vs or []:
            key = (v["file"], v["line"], v["column"])
            if key not in seen:
                seen.add(key)
                grouped.setdefault(v["file"], []).append(v)
    return grouped


_BLAME: dict = {}


def get_git_blame_timestamp(filename, line_number):
    """Get the timestamp when a specific line was introduced using git blame.

    Each file is blamed once, for every line, and cached: one `git blame`
    per violation made a full-tree run take minutes.
    """
    if filename not in _BLAME:
        times = {}
        try:
            out = subprocess.run(
                ["git", "blame", "--line-porcelain", filename],
                capture_output=True, text=True, timeout=60,
            ).stdout
            line, stamp = 0, None
            for row in out.split("\n"):
                parts = row.split(" ")
                if len(parts) >= 3 and len(parts[0]) == 40 and parts[1].isdigit():
                    line = int(parts[2])
                elif row.startswith("author-time "):
                    stamp = int(parts[1])
                elif row.startswith("\t") and line:
                    times[line] = datetime.fromtimestamp(stamp) if stamp else None
        except (subprocess.TimeoutExpired, subprocess.SubprocessError, ValueError):
            pass
        _BLAME[filename] = times
    return _BLAME[filename].get(line_number)

def format_violation(filename, violation):
    """Format a violation for display."""
    timestamp_str = ""
    if "timestamp" in violation and violation["timestamp"]:
        timestamp_str = f" [{violation['timestamp'].strftime('%Y-%m-%d')}]"
    return (
        f"{filename}:{violation['line']}:{violation.get('column', 0)}:{timestamp_str} "
        f"pointer arithmetic with cast: ({violation['type']})ptr + offset"
    )


def main():
    parser = argparse.ArgumentParser(
        description="Check for pointer arithmetic with casts in C code",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Check a single file
  %(prog)s src/main.c

  # Check all C files in src directory
  %(prog)s src/

  # Check with verbose output
  %(prog)s src/ --verbose

  # Fail on any violations
  %(prog)s src/ --strict
""",
    )

    parser.add_argument("path", help="File or directory to check")
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Show detailed progress"
    )
    parser.add_argument(
        "--strict", action="store_true", help="Exit with error code if violations found"
    )
    parser.add_argument(
        "--exclude",
        action="append",
        default=[],
        help="Additional directories to exclude (can be used multiple times)",
    )

    args = parser.parse_args()

    # Determine if we're checking a file or directory
    if os.path.isfile(args.path):
        violations = check_file(args.path, verbose=args.verbose)
        all_violations = {args.path: violations} if violations else {}
    elif os.path.isdir(args.path):
        exclude_dirs = {"build", "tools", "lib", "expected", ".git"} | set(args.exclude)
        all_violations = check_directory(
            args.path, exclude_dirs=exclude_dirs, verbose=args.verbose
        )
    else:
        print(f"Error: {args.path} is not a file or directory", file=sys.stderr)
        return 1

    # Report results
    total_violations = sum(len(v) for v in all_violations.values())

    if total_violations == 0:
        print(f"\n{'=' * 70}")
        print("Pointer Arithmetic Check")
        print(f"{'=' * 70}")
        print("✅ No violations found!")
        return 0

    # Add git blame timestamps to violations
    if args.verbose:
        print("Fetching git blame timestamps...", file=sys.stderr)

    all_violations_flat = []
    for filepath, violations in all_violations.items():
        for violation in violations:
            violation["timestamp"] = get_git_blame_timestamp(
                filepath, violation["line"]
            )
            all_violations_flat.append((filepath, violation))

    # Sort by timestamp (oldest first), violations without timestamps go last
    def sort_key(item):
        filepath, violation = item
        ts = violation.get("timestamp")
        if ts is None:
            return (1, datetime.max, filepath, violation["line"])
        return (0, ts, filepath, violation["line"])

    all_violations_flat.sort(key=sort_key)

    # Print violations
    print(f"\n{'=' * 70}")
    print("Pointer Arithmetic Check")
    print(f"{'=' * 70}")
    print(f"Found {total_violations} violation(s) in {len(all_violations)} file(s):")
    print("(sorted by when introduced, oldest first)\n")

    for filepath, violation in all_violations_flat:
        print(format_violation(filepath, violation))

    print(f"\n{'=' * 70}")
    print("These patterns should be replaced with proper struct field access.")
    print("See CLAUDE.md for examples of correct struct usage.")
    print(f"{'=' * 70}\n")

    if args.strict:
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
