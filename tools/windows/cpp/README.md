# GNU C preprocessor

GCC 14.2.0's `cc1`, run as the C preprocessor (`cc1 -E`) of the C sources the build compiles. The compiler and
assembler that make the game's bytes stay GCC 2.8.1 and maspsx; this one turns sources into the tokens they read, and any
modern GNU preprocessor turns these sources into the same tokens. It is run directly, without the `gcc` or `cpp` driver,
so it takes `cc1`'s own forms: `-E -quiet`, and a depfile as `-MMD <file> -MF <file>`.

From the xPack GNU Compiler Collection 14.2.0-1 (win32-x64), unchanged:
<https://github.com/xpack-dev-tools/gcc-xpack/releases/tag/v14.2.0-1>, `xpack-gcc-14.2.0-1-win32-x64.zip`, sha256 `5c94de916ff50447f74f8d2bf13dd98b92f079f73ae064a02c47598c0109d5c8`.
Only `cc1` and the libraries it loads are kept, in the archive's layout below `libexec/`, but for the folder named for the
host's triple (`x86_64-w64-mingw32`), here `host`, so that every platform's `cc1` is at the same path. A DLL is found beside the program.

- `libexec/gcc/host/14.2.0/cc1.exe`
- `libexec/gcc/host/14.2.0/libgmp-10.dll`
- `libexec/gcc/host/14.2.0/libmpc-3.dll`
- `libexec/gcc/host/14.2.0/libmpfr-6.dll`
- `libexec/gcc/host/14.2.0/libzstd.dll`

GCC is free software under the GNU General Public License, version 3 (`licenses/gcc/COPYING3`); each library's license
is in `licenses/`. The source of GCC 14.2.0 is at <https://gcc.gnu.org/releases.html>, and how xPack built it at the
release's tag of <https://github.com/xpack-dev-tools/gcc-xpack>.
