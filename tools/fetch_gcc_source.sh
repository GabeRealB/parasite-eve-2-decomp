#!/usr/bin/env bash
#
# Put the *patched* GCC 2.8.1 source under local/gcc/, so matching agents read
# the compiler this project actually builds with.
#
# Why bother: an agent working a function stuck in the high 90s reaches for the
# compiler source to check what a pass really does, and left to itself it
# guesses at URLs. One sweep produced nine fetches over four URLs, four of them
# pulling 2.95.3 - seven years newer, different scheduler and CSE. Nothing in
# the matching loop catches a conclusion drawn from the wrong compiler.
#
# And stock 2.8.1 is not what we build with either: tools/linux/gcc-2.8.1-psx
# comes from decompals/old-gcc, which patches the tree before building. This
# applies the same patches, so what you read is what compiled the object.
# It does NOT build anything - source only.
#
# Layout:
#   local/gcc/patches/          the decompals patch set
#   local/gcc/gcc-2.8.1-psx/    stock 2.8.1 with those patches applied
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/local/gcc"
SRC="$DEST/gcc-2.8.1-psx"
PATCHES="$DEST/patches"
VERSION=2.8.1
TARBALL="gcc-${VERSION}.tar.gz"
URL="https://ftp.gnu.org/gnu/gcc/${TARBALL}"
REPO="https://github.com/decompals/old-gcc"

FORCE=false
[[ "${1:-}" == "--force" ]] && FORCE=true

if [[ -d "$SRC" && $FORCE == false ]]; then
    echo "$SRC already exists; --force to redo it."
    exit 0
fi

mkdir -p "$DEST"
cd "$DEST"

# 1. the patch set. Shallow clone into a temp dir - only patches/ is wanted, and
#    the repo also carries build machinery and other compiler versions.
echo "==> patches from $REPO"
rm -rf .old-gcc "$PATCHES"
git clone --depth 1 --quiet "$REPO" .old-gcc
[[ -d .old-gcc/patches ]] || { echo "no patches/ in $REPO - layout changed" >&2; exit 1; }
cp -r .old-gcc/patches "$PATCHES"
printf '%s\n' "$(git -C .old-gcc rev-parse HEAD)" > "$PATCHES/.commit"
rm -rf .old-gcc
echo "    $(ls "$PATCHES" | grep -c . ) file(s), pinned at $(cut -c1-12 <"$PATCHES/.commit")"

# 2. stock source
echo "==> $URL"
[[ -f "$TARBALL" ]] || curl -fL --retry 3 -o "$TARBALL" "$URL"
rm -rf "$SRC" "gcc-${VERSION}"
tar xzf "$TARBALL"
mv "gcc-${VERSION}" "$SRC"

# 3. the same transformations decompals applies before building. Kept in the
#    Dockerfile's order; psx.patch is applied with -s because it is expected to
#    touch files that do not all exist.
echo "==> patching"
cd "$SRC"
sed -i -- 's/include <varargs.h>/include <stdarg.h>/g' ./*.c
patch -u -p1 obstack.h        -i "$PATCHES/obstack-${VERSION}.h.patch"
patch -u -p1 config/mips/mips.h -i "$PATCHES/mips.patch"
patch -su -p1 < "$PATCHES/psx.patch"

# 4. Prove the patches landed, so a silently-stock tree cannot pass. Assert on
#    what each patch *adds*, not on a cc1 command-line option: the Dockerfile
#    greps `./cc1 -version` for -msplit-addresses, which only exists once the
#    thing is built, and the source spells the switch without the leading m.
grep -q 'ASM_OUTPUT_SECTION_NAME' config/mips/mips.h \
    || { echo "mips.patch did not apply" >&2; exit 1; }
grep -q 'split-addresses' config/mips/mips.h \
    || { echo "mips.patch applied but the target switches are missing" >&2; exit 1; }
[[ -f config/mips/psx.h && -f config/mips/xm-psx.h ]] \
    || { echo "psx.patch did not apply - no config/mips/psx.h" >&2; exit 1; }

cat > "$DEST/README.md" <<'NOTE'
# GCC 2.8.1 source (patched, as built)

Regenerate with `tools/fetch_gcc_source.sh`. Gitignored via `local/`; GCC is
GPL and nothing here is redistributed by this repository.

* `gcc-2.8.1-psx/` - stock 2.8.1 plus the decompals patches, matching
  `tools/linux/gcc-2.8.1-psx/cc1`.
* `patches/` - the patch set, with the upstream commit in `.commit`.

Read **only** this tree. If a question needs a different GCC to answer, the
answer does not apply here - the 2.95.3 scheduler and CSE are not ours.

The passes behind the leftovers that actually block a match:

| file | what it explains |
|---|---|
| `local-alloc.c`, `global.c` | which pseudo gets which hard register |
| `reload1.c` | spills, and why a pin does not do what you expect |
| `sched.c` | sched1 ordering and its tie-breaking on source order |
| `combine.c` | why two insns fold, or refuse to |
| `cse.c` | reassociation, and constant/symbol operand order |

A finding worth keeping goes in `DECOMPILATION_LEARNINGS.md` as a rule about the
generated code, not as a pointer into compiler source.
NOTE

echo "==> done: $SRC"
