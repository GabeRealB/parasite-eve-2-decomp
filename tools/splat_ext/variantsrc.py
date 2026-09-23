"""A C translation unit compiled once per package from one shared source.

Some packages are one source built several times with a different build-time
parameter - the MP5A5 and its two upgrades differ only in their weapon index and
item id. Each package needs its own object, since the parameter changes the
code, but there is one source to keep.

The unit is named per package (`mp5a5_p1/mp5a5`), and that name is what splat
derives the object path from, so the package's `.rodata` and `.data`
subsegments, which take their object from their C sibling, follow it to the
per-package object. The source it compiles is named by the subsegment's
`source` key (`mp5a5/mp5a5`), which the split reads to sort functions into
`matchings/` and `nonmatchings/`, and which is never created: a variant is only
declared for a source that already exists. The build passes the package's
declared defines when it compiles the object.
"""

from pathlib import Path
from typing import List, Optional

from splat.segtypes.common.c import CommonSegC
from splat.segtypes.linker_entry import LinkerEntry
from splat.util import options


class PSXSegVariantsrc(CommonSegC):
    _reading_source = False

    def source_path(self) -> Path:
        assert isinstance(self.yaml, dict) and "source" in self.yaml, (
            f"{self.name}: a variantsrc subsegment needs a `source` key"
        )
        return options.opts.src_path / f"{self.yaml['source']}.{self.file_extension}"

    def out_path(self) -> Optional[Path]:
        # The split reads the real source; everything else - the object path,
        # and through it the siblings' - uses the per-package name.
        if self._reading_source:
            return self.source_path()
        return super().out_path()

    def scan(self, rom_bytes: bytes):
        self._reading_source = True
        try:
            super().scan(rom_bytes)
        finally:
            self._reading_source = False

    def split(self, rom_bytes: bytes):
        if not self.source_path().is_file():
            raise FileNotFoundError(f"{self.name}: variant source {self.source_path()} is missing")
        self._reading_source = True
        try:
            super().split(rom_bytes)
        finally:
            self._reading_source = False

    def get_linker_entries(self) -> List[LinkerEntry]:
        return [
            LinkerEntry(
                self,
                [self.source_path()],
                super().out_path(),
                self.get_linker_section_order(),
                self.get_linker_section_linksection(),
                self.is_noload(),
            )
        ]
