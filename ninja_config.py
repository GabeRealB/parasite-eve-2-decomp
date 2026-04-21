# This file has been adapted from the silent-hill-decomp project.

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import spimdisasm
import splat
import splat.scripts.split as split
from ninja import ninja_syntax


# For multi-version support
@dataclass
class GameVersionMetadata:
    name_disk1: str
    name_disk2: str
    version_dir: str
    exe_disk1: str
    exe_disk2: str
    extended_exe_size: int


@dataclass
class GameVersionInfo:
    idx: int
    version_name: str
    metadata: GameVersionMetadata


GAME_VERSIONS = [
    GameVersionInfo(
        0,
        "USA",
        GameVersionMetadata(
            "SLUS-01042",
            "SLUS-01055",
            "USA",
            "SLUS_010.42",
            "SLUS_010.55",
            0x72947,
        ),
    ),
    GameVersionInfo(
        1,
        "JAP",
        GameVersionMetadata(
            "SLPS-02480",
            "SLPS-02481",
            "JAP",
            "SLPS_024.80",
            "SLPS_024.81",
            0x72947,  # TODO
        ),
    ),
]


@dataclass
class YamlInfo:
    split_entries: list[str]
    split_basename: str
    split_linker: str
    split_undef_fun: str
    split_undef_sym: str


YAML_EXECUTABLE = ["main.yaml"]

# Directories
ASSETS_DIR = "assets"
ASM_DIR = "asm"
LINKER_DIR = "linkers"
EXPECTED_DIR = "expected"
CONFIG_DIR = "configs"
TOOLS_DIR = "tools"
ROM_DIR = "rom"
BUILD_DIR = "build"
PERMUTER_DIR = "permuter"

# Sub-directories
if sys.platform == "win32":
    IMAGE_DIR = f"{ROM_DIR}\\image"
    OBJDIFF_DIR = f"{TOOLS_DIR}\\objdiff"
    MKPSXISO_DIR = f"{TOOLS_DIR}\\mkpsxiso"
else:
    IMAGE_DIR = f"{ROM_DIR}/image"
    OBJDIFF_DIR = f"{TOOLS_DIR}/objdiff"
    MKPSXISO_DIR = f"{TOOLS_DIR}/mkpsxiso"

# Tooling Paths
if sys.platform == "win32":
    PYTHON = "python"
else:
    PYTHON = "python3"
MASPSX = f"{PYTHON} tools/maspsx/maspsx.py"
if sys.platform == "win32":
    CROSS = f"{TOOLS_DIR}/win-build/binutils/mips-linux-gnu"
    AS = f"{CROSS}-as.exe"
    LD = f"{CROSS}-ld.exe"
    OBJCOPY = f"{CROSS}-objcopy.exe"
    OBJDUMP = f"{CROSS}-objdump.exe"
    CPP = f"{TOOLS_DIR}/win-build/mcpp/mcpp.exe"
    CC = f"{TOOLS_DIR}/win-build/gcc-psx/cc1psx.exe"
    CC272 = f"{TOOLS_DIR}/win-build/gcc-2.7.2-win/cc1psx.exe"
    OBJDIFF = f"{OBJDIFF_DIR}\\objdiff.exe"
    OBJDIFF_GENSCRIPT = f"{OBJDIFF_DIR}\\objdiff_generate.py"
    POSTBUILD = f"{PYTHON} {TOOLS_DIR}\\postbuild.py"
    DUMPSXISO = f"{MKPSXISO_DIR}\\dumpsxiso.exe"
    ICONV = f"{TOOLS_DIR}\\win-build\\iconv\\iconv.bat"
else:
    CROSS = "mips-linux-gnu"
    AS = f"{CROSS}-as"
    LD = f"{CROSS}-ld"
    OBJCOPY = f"{CROSS}-objcopy"
    OBJDUMP = f"{CROSS}-objdump"
    CPP = f"{CROSS}-cpp"
    CC = f"{TOOLS_DIR}/gcc-2.8.1-psx/cc1"
    CC272 = f"{TOOLS_DIR}/gcc-2.7.2-cdk/cc1"
    OBJDIFF = f"{OBJDIFF_DIR}/objdiff"
    OBJDIFF_GENSCRIPT = f"{OBJDIFF_DIR}/objdiff_generate.py"
    POSTBUILD = f"{PYTHON} {TOOLS_DIR}/postbuild.py"
    DUMPSXISO = f"{MKPSXISO_DIR}/dumpsxiso"
    ICONV = "iconv"

# Compilation flags (General)
INCLUDE_FLAGS = f"-Iinclude -I {BUILD_DIR} -Iinclude/psyq -Iinclude/decomp"
OPT_FLAGS = "-O2"
ENDIAN = "-EL"
DL_EXE_FLAGS = "-G8"
DL_OVL_FLAGS = "-G0"

# Compilation flags (Tool specific)
if sys.platform == "win32":
    COMPILE_COMMANDS_FLAGS = f"{INCLUDE_FLAGS} -D_LANGUAGE_C -DUSE_INCLUDE_ASM -Wall"
    CPP_FLAGS = (
        f"{INCLUDE_FLAGS} -D_LANGUAGE_C -DUSE_INCLUDE_ASM -P -MMD -MP -N -Wall -I-"
    )
    MASPSX_FLAGS = f"--gnu-as-path {AS} --run-assembler"
    ICONV_FLAGS = "$in $out"
else:
    COMPILE_COMMANDS_FLAGS = f"{INCLUDE_FLAGS} -D_LANGUAGE_C -DUSE_INCLUDE_ASM -undef -Wall -lang-c -nostdinc"
    CPP_FLAGS = f"{INCLUDE_FLAGS} -D_LANGUAGE_C -DUSE_INCLUDE_ASM -P -MMD -MP -undef -Wall -lang-c -nostdinc"
    MASPSX_FLAGS = "--aspsx-version=2.77 --run-assembler"
    ICONV_FLAGS = "-f UTF-8 -t SHIFT-JIS $in -o $out"
CC_FLAGS = f"{OPT_FLAGS} -mips1 -mcpu=3000 -w -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return -fcommon -fverbose-asm -msoft-float -mgas -fgnu-linker -quiet"
AS_FLAGS = (
    f"{ENDIAN} {INCLUDE_FLAGS} {OPT_FLAGS} -march=r3000 -mtune=r3000 -no-pad-sections"
)
OBJDUMP_FLAGS = "--disassemble-all --reloc --disassemble-zeroes -Mreg-names=32"

TARGETS_POSTBUILD = ["main"]


def compile_commands_entry(
    target_path: str,
    source_path: str,
    non_matching_enabled,
    game_version_idx: int,
):
    non_matching = ""

    if non_matching_enabled:
        non_matching = "-DNON_MATCHING"

    if sys.platform == "win32":
        source_path = re.sub(r"\\", r"/", source_path)
        target_path = re.sub(r"\\", r"/", target_path)

    return {
        "directory": os.getcwd(),
        "file": source_path,
        "command": f"clang {COMPILE_COMMANDS_FLAGS} -DVER_{GAME_VERSIONS[game_version_idx].version_name} {non_matching} -m32 -o {target_path}.i {source_path}",
    }


def ninja_setup_list_add_source(
    target_path: str,
    source_path: str,
    ninja_file,
    objdiff_file,
    non_matching_enabled,
    game_version_idx: int,
):
    skip_asm = ""
    non_matching = ""

    if objdiff_file is not None:
        skip_asm = "-DSKIP_ASM"
        non_matching = "-DNON_MATCHING"

        if sys.platform == "win32":
            source_target_path = re.sub(
                r"^src",
                rf"asm\\{GAME_VERSIONS[game_version_idx].version_name}",
                source_path,
            )
        else:
            source_target_path = re.sub(
                r"^src",
                rf"asm/{GAME_VERSIONS[game_version_idx].version_name}",
                source_path,
            )
        source_target_path = re.sub(r".c$", r".s", source_target_path)
        if sys.platform == "win32":
            expected_path = re.sub(
                rf"^build\\{GAME_VERSIONS[game_version_idx].version_name}\\src",
                rf"expected\\{GAME_VERSIONS[game_version_idx].version_name}",
                target_path,
            )
        else:
            expected_path = re.sub(
                rf"^build/{GAME_VERSIONS[game_version_idx].version_name}/src",
                rf"expected/{GAME_VERSIONS[game_version_idx].version_name}",
                target_path,
            )

        if os.path.exists(source_target_path):
            if re.search("^asm.(USA|JAP).main.*", source_path):
                objdiff_file.build(
                    outputs=f"{expected_path}.o",
                    rule="as",
                    inputs=source_target_path,
                    variables={"DLFLAG": DL_EXE_FLAGS},
                )
            else:
                objdiff_file.build(
                    outputs=f"{expected_path}.s.o",
                    rule="as",
                    inputs=source_target_path,
                    variables={"DLFLAG": DL_OVL_FLAGS},
                )
        else:
            return

    if non_matching_enabled:
        non_matching = "-DNON_MATCHING"

    if sys.platform == "win32":
        source_path = re.sub(r"\\", r"/", source_path)
        target_path = re.sub(r"\\", r"/", target_path)

    if False:
        pass
    else:
        ninja_file.build(
            outputs=f"{target_path}.i",
            rule="cpp",
            inputs=source_path,
            variables={
                "VERSION": f"-DVER_{GAME_VERSIONS[game_version_idx].version_name}",
                "SKIPASMFLAG": skip_asm,
                "NONMATCHINGFLAG": non_matching,
            },
        )

    if re.search("^src.main.*", source_path):
        ninja_file.build(
            outputs=f"{target_path}.c.s",
            rule="cc",
            inputs=f"{target_path}.i",
            variables={"DLFLAG": DL_EXE_FLAGS},
        )
    else:
        ninja_file.build(
            outputs=f"{target_path}.c.s",
            rule="cc",
            inputs=f"{target_path}.i",
            variables={"DLFLAG": DL_OVL_FLAGS},
        )

    maspxVersion = "2.77"
    if re.search("^src.main.*", source_path):
        ninja_file.build(
            outputs=f"{target_path}.c.o",
            rule="maspsx",
            inputs=f"{target_path}.c.s",
            variables={
                "EXPANDIVFLAG": "",
                "DLFLAG": DL_EXE_FLAGS,
                "MASPSXVER": maspxVersion,
            },
        )
    else:
        ninja_file.build(
            outputs=f"{target_path}.c.o",
            rule="maspsx",
            inputs=f"{target_path}.c.s",
            variables={
                "EXPANDIVFLAG": "",
                "DLFLAG": DL_OVL_FLAGS,
                "MASPSXVER": maspxVersion,
            },
        )

    if objdiff_file is not None:
        return f"{expected_path}.s.o"


def ninja_build(
    split_entries: list[YamlInfo],
    game_version_idx: int,
    objdiff_mode: bool,
    skip_checksum: bool,
    non_matching: bool,
):
    if objdiff_mode:
        ninja_file = ninja_syntax.Writer(
            open("matching.ninja", "w", encoding="utf-8"), width=9999
        )
        ninja_diff_file = ninja_syntax.Writer(
            open("objdiff.ninja", "w", encoding="utf-8"), width=9999
        )
        ninja_diff_file.include("rules.ninja")
        ninja_nonmatching_file = ninja_syntax.Writer(
            open("build.ninja", "w", encoding="utf-8"), width=9999
        )
        ninja_nonmatching_file.include("rules.ninja")
    else:
        ninja_file = ninja_syntax.Writer(
            open("build.ninja", "w", encoding="utf-8"), width=9999
        )

    ninja_file.include("rules.ninja")

    ninja_rules_file = ninja_syntax.Writer(
        open("rules.ninja", "w", encoding="utf-8"), width=9999
    )
    ninja_rules_file.rule(
        "as",
        description="as $in",
        command=f"{AS} {AS_FLAGS} $DLFLAG -o $out $in",
    )
    ninja_rules_file.rule(
        "cc",
        description="cc $in",
        command=f"{CC} {CC_FLAGS} $DLFLAG -o $out $in",
    )
    ninja_rules_file.rule(
        "cc272",
        description="cc272 $in",
        command=f"{CC272} {CC_FLAGS} $DLFLAG -o $out $in",
    )
    ninja_rules_file.rule(
        "cpp",
        description="cpp $in",
        command=f"{CPP} -P -MMD -MP -MT $out -MF $out.d {CPP_FLAGS} $VERSION $SKIPASMFLAG $NONMATCHINGFLAG -o $out $in",
        depfile="$out.d",
    )
    ninja_rules_file.rule(
        "iconv",
        description="iconv $in",
        command=f"{ICONV} {ICONV_FLAGS}",
    )
    ninja_rules_file.rule(
        "objdump",
        description="objdump $in",
        command=f"{OBJDUMP} {OBJDUMP_FLAGS} $in > $out",
    )
    ninja_rules_file.rule(
        "maspsx",
        description="maspsx $in",
        command=f"{MASPSX} {MASPSX_FLAGS} --aspsx-version=$MASPSXVER $EXPANDIVFLAG {AS_FLAGS} $DLFLAG -o $out $in",
    )
    ninja_rules_file.rule(
        "ld",
        description="link $out",
        command=f"{LD} {ENDIAN} {OPT_FLAGS} -nostdlib --no-check-sections -r -b binary -o $out $in",
    )
    ninja_rules_file.rule(
        "elf",
        description="elf $out",
        command=f"{LD} {ENDIAN} {OPT_FLAGS} -nostdlib --no-check-sections -Map $out.map -T $in -T $undef_sym_path -T $undef_fun_path -o $out",
    )
    ninja_rules_file.rule(
        "objcopy",
        description="objcopy $out",
        command=f"{OBJCOPY} -O binary $in $out",
    )

    if sys.platform == "win32":
        ninja_rules_file.rule(
            "sha256sum",
            description="checksum",
            command=f"cmd.exe /c {TOOLS_DIR}\\sha256sum.bat $in",
        )
    else:
        ninja_rules_file.rule(
            "sha256sum",
            description="checksum",
            command="sha256sum --ignore-missing --check $in",
        )

    ninja_rules_file.rule(
        "postbuild",
        description="postbuild script $in",
        command=f"{POSTBUILD} $in",
    )
    ninja_rules_file.rule(
        "objdiff-config",
        description="objdiff-config",
        command=f"{PYTHON} {OBJDIFF_GENSCRIPT} --ninja $in $version",
    )

    cc_entries = []
    elf_build_requirements = []
    checksum_build_requirements = []
    objdiff_config_requirements = []

    # Build all the objects
    for split_config in split_entries:
        for split_entry in split_config.split_entries:
            for entry in split_entry:
                seg = entry.segment

                if entry.object_path is None:
                    continue

                if seg.type[0] == ".":
                    continue

                source_path = str(entry.src_paths[0])
                target_path = str(entry.object_path)

                match seg.type:
                    case (
                        "asm" | "data" | "sdata" | "bss" | "sbss" | "rodata" | "header"
                    ):
                        if re.search("^asm.(USA|JAP).main.*", source_path):
                            ninja_file.build(
                                outputs=target_path,
                                rule="as",
                                inputs=source_path,
                                variables={"DLFLAG": DL_EXE_FLAGS},
                            )
                        else:
                            ninja_file.build(
                                outputs=target_path,
                                rule="as",
                                inputs=source_path,
                                variables={"DLFLAG": DL_OVL_FLAGS},
                            )
                    case "c":
                        cc_entries.append(
                            compile_commands_entry(
                                target_path.removesuffix(".c.o"),
                                source_path,
                                non_matching,
                                game_version_idx,
                            )
                        )

                        ninja_setup_list_add_source(
                            target_path.removesuffix(".c.o"),
                            source_path,
                            ninja_file,
                            None,
                            non_matching,
                            game_version_idx,
                        )

                        if objdiff_mode:
                            if sys.platform == "win32":
                                expected_path = re.sub(
                                    rf"^build\\{GAME_VERSIONS[game_version_idx].version_name}\\src",
                                    rf"expected\\{GAME_VERSIONS[game_version_idx].version_name}\\asm",
                                    f"{target_path}.s.o",
                                )
                            else:
                                expected_path = re.sub(
                                    rf"^build/{GAME_VERSIONS[game_version_idx].version_name}/src",
                                    rf"expected/{GAME_VERSIONS[game_version_idx].version_name}/asm",
                                    f"{target_path}.s.o",
                                )

                            temp_var = [
                                ninja_setup_list_add_source(
                                    target_path.removesuffix(".c.o"),
                                    source_path,
                                    ninja_nonmatching_file,
                                    ninja_diff_file,
                                    True,
                                    game_version_idx,
                                )
                            ]
                            if temp_var != [None]:
                                objdiff_config_requirements += temp_var
                    case "bin":
                        ninja_file.build(
                            outputs=target_path, rule="ld", inputs=source_path
                        )
                    case "lib":
                        print(f"ERROR: Unsupported build segment type {seg.type}")
                        sys.exit(1)

                if not isinstance(seg, splat.segtypes.common.lib.CommonSegLib):
                    elf_build_requirements += [str(s) for s in [entry.object_path]]

        if split_config.split_basename == "main":
            output = f"{BUILD_DIR}/{GAME_VERSIONS[game_version_idx].metadata.version_dir}/out/{GAME_VERSIONS[game_version_idx].metadata.exe_disk1}"
        else:
            output = f"{BUILD_DIR}/{GAME_VERSIONS[game_version_idx].metadata.version_dir}/out/{split_config.split_basename}"

        ninja_file.build(
            outputs=f"{output}.elf",
            rule="elf",
            inputs=split_config.split_linker,
            variables={
                "undef_sym_path": split_config.split_undef_sym,
                "undef_fun_path": split_config.split_undef_fun,
            },
            implicit=elf_build_requirements,
        )
        ninja_file.build(
            outputs=output,
            rule="objcopy",
            inputs=f"{output}.elf",
            implicit=f"{output}.elf",
        )

        if split_config.split_basename in TARGETS_POSTBUILD:
            ninja_file.build(
                outputs=f"{output}.fix",
                rule="postbuild",
                inputs=output,
                implicit=output,
            )

        checksum_build_requirements += [str(s) for s in [output]]

    if objdiff_mode:
        if sys.platform == "win32":
            ninja_diff_file.build(
                outputs=f"{EXPECTED_DIR}\\{GAME_VERSIONS[game_version_idx].version_name}\\objdiff.ok",
                rule="objdiff-config",
                inputs=f"{OBJDIFF_DIR}\\config-retail.yaml",
                variables={
                    "version": GAME_VERSIONS[game_version_idx].metadata.version_dir
                },
                implicit=objdiff_config_requirements,
            )
        else:
            ninja_diff_file.build(
                outputs=f"{EXPECTED_DIR}/{GAME_VERSIONS[game_version_idx].version_name}/objdiff.ok",
                rule="objdiff-config",
                inputs=f"{OBJDIFF_DIR}/config-retail.yaml",
                variables={
                    "version": GAME_VERSIONS[game_version_idx].metadata.version_dir
                },
                implicit=objdiff_config_requirements,
            )

    if not skip_checksum:
        if sys.platform == "win32":
            checksum_target = (
                f"{CONFIG_DIR}/{GAME_VERSIONS[game_version_idx].metadata.version_dir}"
            )
        else:
            checksum_target = f"{CONFIG_DIR}/{GAME_VERSIONS[game_version_idx].metadata.version_dir}/checksum.sha"

        ninja_file.build(
            outputs=f"{BUILD_DIR}/{GAME_VERSIONS[game_version_idx].metadata.version_dir}/out/checksum.ok",
            rule="sha256sum",
            inputs=checksum_target,
            implicit=checksum_build_requirements,
        )

    with open("compile_commands.json", "w") as cc_file:
        json.dump(cc_entries, cc_file, indent=2)


def clean_working_files(clean_build_files: bool, clean_target_files: bool):
    shutil.rmtree(BUILD_DIR, ignore_errors=True)
    shutil.rmtree(PERMUTER_DIR, ignore_errors=True)
    if os.path.exists(".splache"):
        os.remove(".splache")

    if clean_build_files:
        if os.path.exists("build.ninja"):
            os.remove("build.ninja")
        if os.path.exists("matching.ninja"):
            os.remove("matching.ninja")
        if os.path.exists("objdiff.ninja"):
            os.remove("objdiff.ninja")
        if os.path.exists("rules.ninja"):
            os.remove("rules.ninja")
        if os.path.exists(".ninja_log"):
            os.remove(".ninja_log")
        shutil.rmtree(ASM_DIR, ignore_errors=True)
        shutil.rmtree(LINKER_DIR, ignore_errors=True)

    if clean_target_files:
        shutil.rmtree(EXPECTED_DIR, ignore_errors=True)


def extract_files(version: int):
    print(f"Extracting files for version {GAME_VERSIONS[version].version_name}")

    target_assets = f"{ASSETS_DIR}/{GAME_VERSIONS[version].metadata.version_dir}"
    target_rom = f"{ROM_DIR}/{GAME_VERSIONS[version].metadata.version_dir}"

    shutil.rmtree(target_assets, ignore_errors=True)
    shutil.rmtree(target_rom, ignore_errors=True)

    dumpsxiso_flags_disk1 = f"-x {target_rom}/disk1 -s {target_rom}/disk1/layout.xml {IMAGE_DIR}/{GAME_VERSIONS[version].metadata.name_disk1}.bin"
    dumpsxiso_flags_disk2 = f"-x {target_rom}/disk2 -s {target_rom}/disk2/layout.xml {IMAGE_DIR}/{GAME_VERSIONS[version].metadata.name_disk2}.bin"

    os.system(f"{DUMPSXISO} {dumpsxiso_flags_disk1}")
    os.system(f"{DUMPSXISO} {dumpsxiso_flags_disk2}")
    subprocess.call(
        [
            PYTHON,
            f"{TOOLS_DIR}/peassets/extract.py",
            "-exe_d1",
            f"{target_rom}/disk1/{GAME_VERSIONS[version].metadata.exe_disk1}",
            "-exe_d2",
            f"{target_rom}/disk2/{GAME_VERSIONS[version].metadata.exe_disk2}",
            "-s0_hdr",
            f"{target_rom}/disk1/STAGE0.HED",
            "-s0_dat",
            f"{target_rom}/disk1/STAGE0.CDF",
            "-s1",
            f"{target_rom}/disk1/STAGE1.CDF",
            "-s2",
            f"{target_rom}/disk1/STAGE2.CDF",
            "-s3",
            f"{target_rom}/disk1/STAGE3.CDF",
            "-s4",
            f"{target_rom}/disk2/STAGE4.CDF",
            "-s5",
            f"{target_rom}/disk2/STAGE5.CDF",
            "-m_sz",
            f"{GAME_VERSIONS[version].metadata.extended_exe_size}",
            "-o",
            target_assets,
        ]
    )


def main():
    parser = argparse.ArgumentParser(description="Configure the project")
    parser.add_argument(
        "-c",
        "--clean",
        help="Clean build and permuter files",
        action="store_true",
    )
    parser.add_argument(
        "-iso_e",
        "--iso_extract",
        help="Extract game files",
        action="store_true",
    )
    parser.add_argument(
        "-sc",
        "--skip_checksum",
        help="Skip checksum",
        action="store_true",
    )
    parser.add_argument(
        "-nm",
        "--non_matching",
        help="Compile functions labeled as Non Matching",
        action="store_true",
    )
    parser.add_argument(
        "-obj",
        "--objdiff_config",
        help="Builds setup for Objdiff",
        action="store_true",
    )
    parser.add_argument(
        "-ver",
        "--game_version",
        help="Extract and work under a specific version of the game",
        type=str,
    )
    args = parser.parse_args()

    clean_compilation_files = (args.clean) or False
    skip_checksum_option = (args.skip_checksum) or False
    non_matching_option = (args.non_matching) or False
    objdiff_config_option = (args.objdiff_config) or False
    game_version_option = 0  # USA by default
    yamls_paths = []
    splits_yaml_info = []

    if args.game_version is not None:
        for info in GAME_VERSIONS:
            if args.game_version.upper() == info.version_name:
                print(f"Version selected: {info.version_name}")
                game_version_option = info.idx
                break
        else:
            print("Version not supported.")
            sys.exit(1)

    if clean_compilation_files:
        print("Cleaning compilation files")
        clean_working_files(False, False)
        return

    if args.iso_extract:
        extract_files(game_version_option)
        return

    yamls_paths.extend(YAML_EXECUTABLE)
    clean_working_files(True, objdiff_config_option)

    for yaml in yamls_paths:
        splat.util.symbols.spim_context = spimdisasm.common.Context()
        splat.util.symbols.reset_symbols()
        split.main(
            [
                Path(
                    f"{CONFIG_DIR}/{GAME_VERSIONS[game_version_option].metadata.version_dir}/{yaml}"
                )
            ],
            modes="all",
            use_cache=False,
            verbose=False,
            disassemble_all=True,
            make_full_disasm_for_code=objdiff_config_option,
        )
        splits_yaml_info.append(
            YamlInfo(
                [split.linker_writer.entries],
                split.config["options"]["basename"],
                split.config["options"]["ld_script_path"],
                split.config["options"]["undefined_funcs_auto_path"],
                split.config["options"]["undefined_syms_auto_path"],
            )
        )

    ninja_build(
        splits_yaml_info,
        game_version_option,
        objdiff_config_option,
        skip_checksum_option,
        non_matching_option,
    )

    if objdiff_config_option:
        subprocess.call([PYTHON, "-m", "ninja", "-f", "objdiff.ninja"])


if __name__ == "__main__":
    main()
