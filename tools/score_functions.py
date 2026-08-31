#!/usr/bin/env python3
"""
Score assembly functions by complexity and find the simplest one to decompile.

Usage:
    python3 tools/score_functions.py <folder_path> [folder_path ...]
    python3 tools/score_functions.py asm/USA/main/nonmatchings
    python3 tools/score_functions.py asm/USA/main/nonmatchings asm/USA/title/nonmatchings
    python3 tools/score_functions.py --exhaustive asm/USA/main/nonmatchings
    python3 tools/score_functions.py --score-func func_800B6544_1E35F4 asm/USA/main/nonmatchings
    python3 tools/score_functions.py --min-score 0.4 --max-score 0.8 asm/USA/main/nonmatchings
"""

import sys
import os
import re
import argparse
import numpy as np
from pathlib import Path
from dataclasses import dataclass
from typing import List, Sequence


# --- fitted model (tools/fit_difficulty_model.py) ---
# Logistic regression on 1980 observations mined from this repo:
# every `matched <fn> <attempts>` commit plus every give-up in
# tools/difficult_functions, joined to that function's assembly metrics.
# Target: P(the function does not match on the first attempt); a give-up is a
# positive whatever its attempt count. 5-fold CV AUC 0.774. Adding stack
# size, jump-table, float/GTE, callee or memory-op counts moves that by less
# than the fold noise: the difficulty a static count cannot see is the
# difficulty that is left.
#
# Counts are log1p'd. Calls genuinely predict *easier* (glue code), so `jumps`
# fits negative, and on raw counts that extrapolates to nonsense - a
# 214-instruction, 46-call room function scored below a 2-instruction leaf
# stub. The log holds the effect down: raw counts leave 58 functions of >=100
# instructions ranked easier than that stub, log1p leaves none, for ~0.01 AUC.
LOG_FEATURES = True
MEANS = np.array(
    [
        np.float64(3.9218325996327468),
        np.float64(1.3759320095809051),
        np.float64(1.3575412473882016),
        np.float64(1.3751432420111622),
    ]
)
STDS = np.array(
    [
        np.float64(1.0888815743018432),
        np.float64(1.013443437091216),
        np.float64(1.0858708965957478),
        np.float64(0.9960265517236578),
    ]
)
COEFFICIENTS = np.array(
    [
        np.float64(1.0146368439695734),
        np.float64(-0.1513536285729376),
        np.float64(-0.7147786128762945),
        np.float64(0.7836282711027525),
    ]
)
INTERCEPT = 0.049479124193788436
# --- end fitted model ---


def difficulty_logit(instructions, branches, jumps, labels):
    """Raw log-odds behind the 0..1 difficulty score.

    Rank on this, not on the score: the sigmoid saturates, and once a few
    hundred functions all read 1.000 the score can no longer order the ones
    that are left.
    """
    features = np.array([instructions, branches, jumps, labels], dtype=np.float64)
    if LOG_FEATURES:
        features = np.log1p(features)
    return float(np.dot((features - MEANS) / STDS, COEFFICIENTS) + INTERCEPT)


def decompilation_difficulty_score(instructions, branches, jumps, labels):
    """Probability the function will not match on the first attempt (0=easy)."""
    return 1 / (1 + np.exp(-difficulty_logit(instructions, branches, jumps, labels)))


@dataclass
class FunctionScore:
    """Stores complexity metrics for a function."""

    name: str
    file_path: str
    instruction_count: int = 0
    branch_count: int = 0
    jump_count: int = 0
    label_count: int = 0
    stack_size: int = 0

    @property
    def total_score(self) -> float:
        """Calculate decompilation difficulty score using ML model (0=easy, 1=hard)."""
        return decompilation_difficulty_score(
            self.instruction_count, self.branch_count, self.jump_count, self.label_count
        )

    @property
    def difficulty(self) -> float:
        """Same ordering as `total_score`, without the sigmoid's saturation."""
        return difficulty_logit(
            self.instruction_count, self.branch_count, self.jump_count, self.label_count
        )

    def __str__(self) -> str:
        return (
            f"{self.name:50s} | Difficulty: {self.total_score:5.3f} | "
            f"Instructions: {self.instruction_count:3d} | "
            f"Branches: {self.branch_count:2d} | "
            f"Jumps: {self.jump_count:2d} | "
            f"Labels: {self.label_count:2d} | "
            f"Stack: {self.stack_size:4d}"
        )

    def to_simple_format(self) -> str:
        """Return a simple parseable format without column names."""
        return (
            f"{self.name} | {self.total_score:.3f} | "
            f"{self.instruction_count} | {self.branch_count} | "
            f"{self.jump_count} | {self.label_count} | {self.stack_size}"
        )


def parse_multi_function_file(file_path: str) -> List[FunctionScore]:
    """Parse a file containing multiple functions and return a list of scores."""
    scores = []

    try:
        with open(file_path, "r") as f:
            content = f.read()

        # Find all function blocks using glabel/endlabel markers
        # Pattern: glabel <function_name> ... endlabel <function_name>
        glabel_pattern = r"glabel\s+(\S+)"

        # Find all glabel occurrences
        glabels = list(re.finditer(glabel_pattern, content))

        if len(glabels) == 0:
            # No glabels found, use standard analysis (extracts name from filename)
            score = analyze_function(file_path)
            return [score] if score is not None else []

        # One or more functions found - parse each one
        for i, match in enumerate(glabels):
            func_name = match.group(1)
            start_pos = match.start()

            # Find the end of this function (next glabel or end of file)
            if i + 1 < len(glabels):
                end_pos = glabels[i + 1].start()
            else:
                end_pos = len(content)

            # Extract function content
            func_content = content[start_pos:end_pos]

            # Analyze this function
            score = analyze_function_content(func_name, func_content, file_path)
            if score is not None:
                scores.append(score)

    except Exception as e:
        print(f"Error parsing multi-function file {file_path}: {e}", file=sys.stderr)

    return scores


def analyze_function_content(
    func_name: str, content: str, file_path: str
) -> FunctionScore:
    """Analyze function content and return complexity metrics."""

    # Branch instructions (conditional)
    branch_patterns = [
        r"\b(beq|bne|bnez|beqz|blez|bgtz|bltz|bgez|blt|bgt|ble|bge|bltzal|bgezal)\b"
    ]

    # Jump instructions
    jump_patterns = [
        r"\bjal\b",  # Function calls
        r"\bj\b",  # Unconditional jumps (not jr - that's return)
    ]

    # Local label pattern. `symbol_name_format` prefixes an overlay's labels
    # with the segment (.Lactor_301500_801637F0:, .Lshelter_b6_nursery_8018008C:),
    # so a hex-only pattern counts zero labels in every generated overlay.
    label_pattern = r"^\s*\.L\w+:"

    # Instruction pattern (lines with actual assembly)
    instruction_pattern = r"/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s*\*/"

    # Stack allocation pattern (addiu $sp, $sp, -0xNN)
    stack_pattern = r"addiu\s+\$sp,\s*\$sp,\s*-0x([0-9A-Fa-f]+)"

    # Jump table / data patterns. `symbol_name_format` prefixes an overlay's
    # symbols with the segment, so the name is not hex-only:
    # jtbl_neo_ark_bridge_8017D620, D_actor_301500_80161E20. A [0-9A-Fa-f_]
    # class matches neither (`t`, `o`, `r`, `k`, `1` aside), which silently
    # turned both filters off for every generated overlay.
    jumptable_pattern = r"^jtbl_\w*[0-9A-Fa-f]{6,}$"

    data_pattern = r"^D_\w*[0-9A-Fa-f]{6,}$"

    # Interior labels of the shared actor text units (Actor01900_L0A354). The
    # naming there is authoritative, not a guess: `Fn<hex>` symbols are `jal`
    # targets (589 call sites) and are what the header `.word` table names,
    # while `L<hex>` symbols are the target of no call anywhere in the tree and
    # appear only inside the `Jt<hex>` jump tables. They are switch arms and
    # shared epilogues, so there is no standalone C function to write - match
    # the enclosing `Fn*` and the whole span comes with it.
    fragment_pattern = r"^Actor\d+_L[0-9A-F]+$"

    # Skip non-code sections (data, bss, rodata, header)
    # These typically have .data, .bss, .rodata suffixes or are named "header"
    if (
        re.match(jumptable_pattern, func_name)
        or re.match(data_pattern, func_name)
        or re.match(fragment_pattern, func_name)
        or func_name.endswith(".data")
        or func_name.endswith(".bss")
        or func_name.endswith(".rodata")
        or func_name == "header"
    ):
        return None

    # Skip files that only contain rodata (no actual code)
    if ".section .rodata" in content and "glabel" not in content:
        return None

    score = FunctionScore(name=func_name, file_path=file_path)

    for line in content.split("\n"):
        line = line.strip()

        # Skip empty lines, glabel, endlabel, and comments
        if (
            not line
            or line.startswith("glabel")
            or line.startswith("endlabel")
            or line.startswith("nonmatching")
        ):
            continue

        # Count local labels
        if re.match(label_pattern, line):
            score.label_count += 1
            continue

        # Count instructions
        if re.search(instruction_pattern, line):
            score.instruction_count += 1

            # Check for branches
            for pattern in branch_patterns:
                if re.search(pattern, line):
                    score.branch_count += 1
                    break

            # Check for jumps
            for pattern in jump_patterns:
                if re.search(pattern, line):
                    score.jump_count += 1
                    break

            # Check for stack allocation
            stack_match = re.search(stack_pattern, line)
            if stack_match:
                score.stack_size = int(stack_match.group(1), 16)
    return score


def analyze_function(file_path: str) -> FunctionScore:
    """Analyze an assembly file and return complexity metrics."""

    # Branch instructions (conditional)
    branch_patterns = [
        r"\b(beq|bne|bnez|beqz|blez|bgtz|bltz|bgez|blt|bgt|ble|bge|bltzal|bgezal)\b"
    ]

    # Jump instructions
    jump_patterns = [
        r"\bjal\b",  # Function calls
        r"\bj\b",  # Unconditional jumps (not jr - that's return)
    ]

    # Local label pattern. `symbol_name_format` prefixes an overlay's labels
    # with the segment (.Lactor_301500_801637F0:, .Lshelter_b6_nursery_8018008C:),
    # so a hex-only pattern counts zero labels in every generated overlay.
    label_pattern = r"^\s*\.L\w+:"

    # Instruction pattern (lines with actual assembly)
    instruction_pattern = r"/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+\*/"

    # Stack allocation pattern (addiu $sp, $sp, -0xNN)
    stack_pattern = r"addiu\s+\$sp,\s*\$sp,\s*-0x([0-9A-Fa-f]+)"

    # Extract function name from file
    filename = os.path.basename(file_path)
    func_name = filename.replace(".s", "")

    # Jump table / data patterns. `symbol_name_format` prefixes an overlay's
    # symbols with the segment, so the name is not hex-only:
    # jtbl_neo_ark_bridge_8017D620, D_actor_301500_80161E20. A [0-9A-Fa-f_]
    # class matches neither (`t`, `o`, `r`, `k`, `1` aside), which silently
    # turned both filters off for every generated overlay.
    jumptable_pattern = r"^jtbl_\w*[0-9A-Fa-f]{6,}$"

    data_pattern = r"^D_\w*[0-9A-Fa-f]{6,}$"

    # Interior labels of the shared actor text units (Actor01900_L0A354). The
    # naming there is authoritative, not a guess: `Fn<hex>` symbols are `jal`
    # targets (589 call sites) and are what the header `.word` table names,
    # while `L<hex>` symbols are the target of no call anywhere in the tree and
    # appear only inside the `Jt<hex>` jump tables. They are switch arms and
    # shared epilogues, so there is no standalone C function to write - match
    # the enclosing `Fn*` and the whole span comes with it.
    fragment_pattern = r"^Actor\d+_L[0-9A-F]+$"

    # Skip non-code sections (data, bss, rodata, header)
    # These typically have .data, .bss, .rodata suffixes or are named "header"
    if (
        re.match(jumptable_pattern, func_name)
        or re.match(data_pattern, func_name)
        or re.match(fragment_pattern, func_name)
        or func_name.endswith(".data")
        or func_name.endswith(".bss")
        or func_name.endswith(".rodata")
        or func_name == "header"
    ):
        return None

    score = FunctionScore(name=func_name, file_path=file_path)

    try:
        with open(file_path, "r") as f:
            content = f.read()

            # Skip files that only contain rodata (no actual code)
            if ".section .rodata" in content and "glabel" not in content:
                return None

            for line in content.splitlines():
                line = line.strip()

                # Skip empty lines, glabel, endlabel, and comments
                if (
                    not line
                    or line.startswith("glabel")
                    or line.startswith("endlabel")
                    or line.startswith("nonmatching")
                ):
                    continue

                # Count local labels
                if re.match(label_pattern, line):
                    score.label_count += 1
                    continue

                # Count instructions
                if re.search(instruction_pattern, line):
                    score.instruction_count += 1

                    # Check for branches
                    for pattern in branch_patterns:
                        if re.search(pattern, line):
                            score.branch_count += 1
                            break

                    # Check for jumps
                    for pattern in jump_patterns:
                        if re.search(pattern, line):
                            score.jump_count += 1
                            break

                    # Check for stack allocation
                    stack_match = re.search(stack_pattern, line)
                    if stack_match:
                        score.stack_size = int(stack_match.group(1), 16)

    except Exception as e:
        print(f"Error analyzing {file_path}: {e}", file=sys.stderr)

    return score


def score_folder(folder_path: str, exhaustive: bool = False) -> List[FunctionScore]:
    """Score all assembly functions in a folder and its subdirectories.

    Args:
        folder_path: Path to the folder to scan
        exhaustive: If True, parse multi-function .s files individually
    """
    return score_folders([folder_path], exhaustive=exhaustive)


def score_folders(
    folder_paths: Sequence[str], exhaustive: bool = False
) -> List[FunctionScore]:
    """Score assembly functions across one or more folders (and subdirectories).

    Args:
        folder_paths: Paths to scan (e.g. all overlay nonmatchings dirs)
        exhaustive: If True, parse multi-function .s files individually
    """
    scores: List[FunctionScore] = []
    found_any_asm = False

    for folder_path in folder_paths:
        if not os.path.isdir(folder_path):
            print(f"Error: '{folder_path}' is not a valid directory", file=sys.stderr)
            sys.exit(1)

        asm_files = list(Path(folder_path).glob("**/*.s"))
        if not asm_files:
            print(
                f"Warning: No .s files found in '{folder_path}'",
                file=sys.stderr,
            )
            continue

        found_any_asm = True
        for asm_file in asm_files:
            if exhaustive:
                file_scores = parse_multi_function_file(str(asm_file))
                scores.extend(file_scores)
            else:
                score = analyze_function(str(asm_file))
                if score is not None:  # Skip jump tables / data
                    scores.append(score)

    if not found_any_asm:
        paths = ", ".join(f"'{p}'" for p in folder_paths)
        print(f"Error: No .s files found in {paths}", file=sys.stderr)
        sys.exit(1)

    # Sort by complexity (lowest first). Rank on the logit: past ~0.999 the
    # score rounds to 1.000 for hundreds of functions at once and stops
    # ordering them, which is exactly the pool the vacuum has left to pick from.
    scores.sort(key=lambda s: s.difficulty)

    return scores


def load_difficult_functions(difficult_file: str) -> set:
    """Load the list of difficult functions to exclude."""
    difficult_functions = set()

    if os.path.exists(difficult_file):
        try:
            with open(difficult_file, "r") as f:
                for line in f:
                    line = line.strip()
                    if line:  # Skip empty lines
                        # Extract only the first value (function name)
                        # Some lines may have additional data like attempt counts
                        func_name = line.split()[0]
                        difficult_functions.add(func_name)
        except Exception as e:
            print(f"Warning: Could not read {difficult_file}: {e}", file=sys.stderr)

    return difficult_functions


def main():
    parser = argparse.ArgumentParser(
        description="Score assembly functions by complexity and find the simplest one to decompile.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 tools/score_functions.py asm/USA/main/nonmatchings
  python3 tools/score_functions.py asm/USA/main/nonmatchings asm/USA/title/nonmatchings
  python3 tools/score_functions.py --exhaustive asm/USA/main/nonmatchings
  python3 tools/score_functions.py --score-func func_800B6544_1E35F4 asm/USA/main/nonmatchings
  python3 tools/score_functions.py --min-score 0.4 --max-score 0.8 asm/USA/main/nonmatchings
  python3 tools/score_functions.py --exhaustive --min-score 0.5 asm/USA/main/nonmatchings
        """,
    )
    parser.add_argument(
        "folder_paths",
        nargs="+",
        help="Path(s) to folder(s) containing .s assembly files (e.g. all overlay nonmatchings)",
    )
    parser.add_argument(
        "--exhaustive",
        action="store_true",
        help="Parse multi-function .s files (e.g., asm/1DCF60.s) and score each function individually. Ignores difficult_functions file.",
    )
    parser.add_argument(
        "--score-func",
        metavar="FUNCTION_NAME",
        help="Search for and score a specific function by name (e.g., func_800B6544_1E35F4)",
    )
    parser.add_argument(
        "--min-score",
        type=float,
        metavar="MIN",
        help="Only show functions with difficulty score >= MIN (0..1)",
    )
    parser.add_argument(
        "--max-score",
        type=float,
        metavar="MAX",
        help="Only show functions with difficulty score <= MAX (0..1)",
    )
    parser.add_argument(
        "--ranked",
        action="store_true",
        help="Print remaining function names, easiest first (one per line).",
    )
    parser.add_argument(
        "--exclude-file",
        action="append",
        default=[],
        metavar="PATH",
        help="Extra names to skip (same format as tools/difficult_functions). Repeatable.",
    )
    parser.add_argument(
        "--only-difficult",
        action="store_true",
        help="Only score functions listed in tools/difficult_functions (minus --exclude-file).",
    )

    args = parser.parse_args()
    folder_paths = args.folder_paths
    paths_display = ", ".join(f"'{p}'" for p in folder_paths)

    # If --score-func is specified, search for that specific function
    if args.score_func:
        scores = score_folders(
            folder_paths, exhaustive=True
        )  # Always use exhaustive mode for specific function search

        # Find the matching function
        matching_scores = [s for s in scores if s.name == args.score_func]

        if not matching_scores:
            print(
                f"Error: Function '{args.score_func}' not found in {paths_display}",
                file=sys.stderr,
            )
            if re.match(r"^Actor\d+_L[0-9A-F]+$", args.score_func):
                print(
                    f"Hint: '{args.score_func}' is an interior label, not a function - "
                    "it is a switch arm or shared epilogue of the Fn* symbol above it "
                    "in the unit. Score and decompile that parent instead; see "
                    'DECOMPILATION_LEARNINGS.md, "Splat L-labels with no prologue are '
                    'the parent function".',
                    file=sys.stderr,
                )
            sys.exit(1)

        if len(matching_scores) > 1:
            print(
                f"Warning: Found {len(matching_scores)} occurrences of '{args.score_func}'",
                file=sys.stderr,
            )
            sys.exit(1)

        print(matching_scores[0].total_score)

        sys.exit(0)

    scores = score_folders(folder_paths, exhaustive=args.exhaustive)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    difficult_file = os.path.join(script_dir, "difficult_functions")
    listed = set()
    extra_skip = set()
    for extra in args.exclude_file:
        extra_skip |= load_difficult_functions(extra)
    # Exhaustive listing ignores the give-up file unless --only-difficult.
    if args.only_difficult or not args.exhaustive:
        listed = load_difficult_functions(difficult_file)

    if args.only_difficult and not listed:
        print("Error: tools/difficult_functions is empty", file=sys.stderr)
        sys.exit(1)

    filtered_scores = []
    for score in scores:
        if score.instruction_count <= 0:
            continue
        if score.name in extra_skip:
            continue
        if args.only_difficult:
            if score.name not in listed:
                continue
        elif score.name in listed:
            continue
        filtered_scores.append(score)

    if args.min_score is not None:
        filtered_scores = [
            s for s in filtered_scores if s.total_score >= args.min_score
        ]
    if args.max_score is not None:
        filtered_scores = [
            s for s in filtered_scores if s.total_score <= args.max_score
        ]

    if not filtered_scores:
        if args.only_difficult:
            print("Error: No remaining difficult functions", file=sys.stderr)
        else:
            print(
                "Error: All functions are marked as difficult or are data sections!",
                file=sys.stderr,
            )
        sys.exit(1)

    if args.ranked:
        for score in filtered_scores:
            print(score.name)
        return

    # In exhaustive mode, list all functions
    if args.exhaustive:
        print("ALL FUNCTIONS (sorted by complexity):\n")
        for score in filtered_scores:
            print(score.to_simple_format())
        print(f"\n{'=' * 80}\n")

    simplest = filtered_scores[0]

    # Simple mode: just print the function name if no special flags
    if not args.exhaustive and args.min_score is None and args.max_score is None:
        print(simplest.name)
    else:
        print(f"SIMPLEST FUNCTION: {simplest.name}")
        print(f"{simplest}")


if __name__ == "__main__":
    main()
