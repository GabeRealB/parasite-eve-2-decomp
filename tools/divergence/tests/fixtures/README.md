# Replay inputs

These are small hand-written assembly/RTL parser fixtures, not captured GCC
outputs or evidence that a proposed C edit works. The manifests pin the target
and candidate assembly hashes. Every input is stored here; replay needs neither
a matching scratch directory nor a fetched GCC source tree.

* `allocation`: a register mismatch with a pseudo's final home and initial
  conflict set in the allocation dumps.
* `extension`: signed versus unsigned halfword loads, with an explicit sign
  extension in the initial RTL.

The separate `../compiler_checks.json` embeds C inputs and mutations for
`../smoke.py`, which compiles and scores fresh temporary objects.
