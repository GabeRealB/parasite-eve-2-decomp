"""A C translation unit from the shared library, `src/lib/`.

A library unit is one source for the whole game: every package that carries its
bytes links the same object, whichever family the package belongs to, so the
source cannot live under a family's `src_path`. Only the source moves. The split
still writes the unit's disassembly under the family's own `nonmatchings/` and
`matchings/`, because each family links it at its own addresses and its asm has
to stay where that family's tooling looks for it.

The unit is named `lib/<unit>`, which puts the asm in `.../nonmatchings/lib/<unit>`
and, resolved from the parent of `src_path`, the source in `src/lib/<unit>.c`.
The unit's `.rodata` and `.data` subsegments take their object from this one, so
they follow it without a type of their own.
"""

from pathlib import Path
from typing import Optional

from splat.segtypes.common.c import CommonSegC
from splat.util import options


class PSXSegLibsrc(CommonSegC):
    def out_path(self) -> Optional[Path]:
        return options.opts.src_path.parent / self.dir / f"{self.name}.{self.file_extension}"
