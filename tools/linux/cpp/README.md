# GNU C preprocessor

GCC 14.2.0's `cc1`, run as the C preprocessor (`cc1 -E`) of the C sources the build compiles. The compiler and
assembler that make the game's bytes stay GCC 2.8.1 and maspsx; this one turns sources into the tokens they read, and any
modern GNU preprocessor turns these sources into the same tokens. It is run directly, without the `gcc` or `cpp` driver,
so it takes `cc1`'s own forms: `-E -quiet`, and a depfile as `-MMD <file> -MF <file>`.

From the xPack GNU Compiler Collection 14.2.0-1 (linux-x64), unchanged:
<https://github.com/xpack-dev-tools/gcc-xpack/releases/tag/v14.2.0-1>, `xpack-gcc-14.2.0-1-linux-x64.tar.gz`, sha256 `f1a5db554874b27812e6286d242200430d9c5fb62bc04f4e59d6fe47d498af5a`.
Only `cc1` and the libraries it loads are kept, in the archive's layout below `libexec/`, but for the folder named for the
host's triple (`x86_64-pc-linux-gnu`), here `host`, so that every platform's `cc1` is at the same path. The program finds its libraries three folders up, by its own relative rpath.

- `libexec/gcc/host/14.2.0/cc1`
- `libexec/libgmp.so.10`
- `libexec/libiconv.so.2`
- `libexec/libisl.so.23`
- `libexec/libmpc.so.3`
- `libexec/libmpfr.so.6`
- `libexec/libz.so.1`
- `libexec/libzstd.so.1`

GCC is free software under the GNU General Public License, version 3 (`licenses/gcc/COPYING3`); each library's license
is in `licenses/`. The source of GCC 14.2.0 is at <https://gcc.gnu.org/releases.html>, and how xPack built it at the
release's tag of <https://github.com/xpack-dev-tools/gcc-xpack>.
