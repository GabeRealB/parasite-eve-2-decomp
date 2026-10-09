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
