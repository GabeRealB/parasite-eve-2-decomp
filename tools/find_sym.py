import argparse
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection


def inspect_obj(path: Path, sym: str, imp: list[Path], exp: list[Path]):
    with open(path, "rb") as f:
        elffile = ELFFile(f)
        symtab = elffile.get_section_by_name(".symtab")
        if symtab is None:
            return
        assert isinstance(symtab, SymbolTableSection)

        symbols = symtab.get_symbol_by_name(sym)
        if symbols is None:
            return
        assert isinstance(symbols, list)

        is_export = False
        is_import = False
        for symbol in symbols:
            st_shndx: str = symbol["st_shndx"]
            if st_shndx == "SHN_UNDEF":
                is_import = True
            else:
                is_export = True

        if is_import:
            imp.append(path)
        if is_export:
            exp.append(path)


def iterate_dir(dir: Path, sym: str, imp: list[Path], exp: list[Path]):
    for entry in dir.iterdir():
        if entry.is_dir():
            iterate_dir(entry, sym, imp, exp)
        elif entry.suffix == ".o":
            inspect_obj(entry, sym, imp, exp)


def main():
    parser = argparse.ArgumentParser(
        description="Find elf objects importing/exporting symbols"
    )
    parser.add_argument("dir", type=Path)
    parser.add_argument("sym", type=str)
    args = parser.parse_args()

    dir: Path = args.dir
    sym: str = args.sym
    import_list: list[Path] = []
    export_list: list[Path] = []
    iterate_dir(dir.absolute(), sym, import_list, export_list)

    print("Exported by:")
    for entry in export_list:
        print(f"\t{entry}")

    print("\nImported by:")
    for entry in import_list:
        print(f"\t{entry}")


if __name__ == "__main__":
    main()
