# Asset usage records

One file per overlay: how the code uses each asset the overlay owns, and each
ancestor's asset its code uses. Read out of the sources; every `established`
line carries the code location that settles it.

Tab-separated, eight fields:

    asset  part  user  how  when  evidence  status  note

`status` is `established` or `open`; an `open` line says in its note what is
missing. Lines starting with `#` are remarks about the overlay as a whole.
`tools/peassets/asset_map.py` reads these and shows them with each asset and
overlay.

An evidence line number is as of the sweep date (2026-10-09); the sources are
still being edited, so the function or symbol the line names is the stable
reference. A use is `established` when both the mechanism and its reach are
shown; a mechanism with no path found to it is `open`, with what was checked;
a use the code excludes is `established` with "not reached" in `when`.
