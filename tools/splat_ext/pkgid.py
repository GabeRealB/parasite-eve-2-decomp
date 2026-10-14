"""A package's id: the first word of every image, written by the link.

All 448 packages begin with a distinct id - a u16 zero-extended into a u32 -
and nothing in any package reads or names it. The working guess is that the
packaging tool wrote it after the build, so it is written here by the linker
script rather than compiled from a source: `LONG(<id>)` at offset 0 of the
image's read-only data. The value comes from the overlay manifest (or a
hand-written config's `id` key), never from the package, so the checksum still
checks it.

    - {start: 0x0, type: pkgid, name: packageid, id: 285}
"""

from typing import List, Optional

from splat.segtypes.common.rodata import CommonSegRodata
from splat.segtypes.linker_entry import LinkerEntry


class PackageIdLinkerEntry(LinkerEntry):
    def __init__(self, segment, package_id: int):
        super().__init__(segment, [], segment.dir / f"{segment.name}.id", ".rodata", ".rodata")
        self.object_path = None  # nothing is compiled or assembled
        self.package_id = package_id

    def emit_entry(self, linker_writer) -> None:
        linker_writer._writeln(f"LONG(0x{self.package_id:08X}); /* package id */")


class PSXSegPkgid(CommonSegRodata):
    def package_id(self) -> int:
        assert isinstance(self.yaml, dict) and "id" in self.yaml, f"{self.name}: a pkgid subsegment needs an `id`"
        value = int(self.yaml["id"])
        assert 0 <= value <= 0xFFFF, f"{self.name}: package id {value} is not a u16"
        return value

    def out_path(self):
        # No file: nothing is split, compiled or auto-linked for the id.
        return None

    def should_scan(self) -> bool:
        return False

    def should_self_split(self) -> bool:
        return False

    def split(self, rom_bytes: bytes):
        pass

    def get_linker_entries(self) -> List[LinkerEntry]:
        return [PackageIdLinkerEntry(self, self.package_id())]
