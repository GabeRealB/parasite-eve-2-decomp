"""Small reviewable C edit proposals; deliberately no automatic semantic claims."""
import re


def masked(text):
    """Hide strings/comments/directives while retaining offsets and newlines."""
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|^[ \t]*\#(?:\\\r?\n|[^\n])*'
    return re.sub(pattern, lambda m: re.sub(r"[^\n]", " ", m[0]), text, flags=re.S | re.M)


def candidates(source, allowed, locations=(), limit=16):
    """Propose only entire simple statements, never arbitrary substring swaps.

    Types/volatile qualifiers and cross-statement dependencies still require
    review. The returned plan makes that prerequisite explicit.
    """
    clean = masked(source)
    atom = r"(?:[A-Za-z_]\w*|0[xX][0-9a-fA-F]+|\d+)"
    pattern = re.compile(r"(?m)^[ \t]*(?:[A-Za-z_]\w*[ \t]*=[ \t]*|return[ \t]+)(?P<left>" + atom + r")[ \t]*(?P<op>[+*&|^])[ \t]*(?P<right>" + atom + r")[ \t]*;")
    result = []
    if "operand_order" in allowed:
        for match in pattern.finditer(clean):
            line = source.count("\n", 0, match.start()) + 1
            if locations and min(abs(line - n) for n in locations) > 8:
                continue
            if match["left"] == match["right"]:
                continue
            original = source[match.start():match.end()]
            left, right = match.span("left"), match.span("right")
            edited = (source[match.start():left[0]] + source[right[0]:right[1]] + source[left[1]:right[0]]
                      + source[left[0]:left[1]] + source[right[1]:match.end()])
            # Include enough line context to make the replacement unique.
            if source.count(original) != 1:
                continue
            result.append({"id": f"operand_order_line_{line}", "line": line,
                           "hypothesis": "Commuting these simple operands changes tying/preference while preserving the intended value.",
                           "requires_review": "Verify integer operand types and absence of volatile reads; inspect the ordinary compiled result.",
                           "edits": [{"before": original, "after": edited}]})
            if len(result) >= limit:
                break
    if "statement_order" in allowed and len(result) < limit:
        # Two adjacent constant stores to distinct fields on the same base.
        # Avoid pointer expressions/calls, blocks and arbitrary dependency
        # inference. Volatile, union/bitfield layout and aliasing need review.
        store = re.compile(r"(?m)^[ \t]*(?P<base>[A-Za-z_]\w*)->(?P<field>[A-Za-z_]\w*)[ \t]*=[ \t]*(?:0[xX][0-9a-fA-F]+|-?\d+)[ \t]*;")
        statements = list(store.finditer(clean))
        for left, right in zip(statements, statements[1:]):
            gap = source[left.end():right.start()]
            if not gap.isspace() or gap.count("\n") != 1:
                continue
            if left['base'] != right['base'] or left['field'] == right['field']:
                continue
            line = source.count("\n", 0, left.start()) + 1
            if locations and min(abs(line - n) for n in locations) > 8:
                continue
            before = source[left.start():right.end()]
            if source.count(before) != 1:
                continue
            after = source[right.start():right.end()] + gap + source[left.start():left.end()]
            result.append({"id": f"statement_order_line_{line}", "line": line,
                           "hypothesis": "Reordering these field stores changes scheduling/liveness while retaining their final values.",
                           "requires_review": "Confirm nonvolatile ordinary memory, disjoint fields (including union/bitfield layout), and unchanged base pointer.",
                           "edits": [{"before": before, "after": after}]})
            if len(result) >= limit:
                break
    return result
