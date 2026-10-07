#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"
#include "types.h"
#include "version.h"

/// Byte-addressable base of the PlayStation's 1 KiB CPU scratchpad RAM.
///
/// Storage is shared; callers manage the lifetime and alignment of their data.
/// This address does not reserve a block or read the scratch-stack cursor.
#define PLAYSTATION_SCRATCHPAD_BASE ((u8*)0x1F800000)

/// Returns a `void*` at a byte offset into PlayStation scratchpad RAM.
///
/// `byteOffset` is an integer in [0, 0x400], evaluated once and added to
/// `PLAYSTATION_SCRATCHPAD_BASE` in bytes; 0x400 is the one-past-end address.
/// Callers select a typed view whose accesses fit within the RAM and satisfy
/// that type's alignment. Storage is shared, and callers manage reservations
/// and data lifetime. Apart from evaluating `byteOffset`, the macro performs
/// no memory access, reservation or scratch-stack cursor update.
#define PLAYSTATION_SCRATCHPAD_ADDRESS(byteOffset) ((void*)(PLAYSTATION_SCRATCHPAD_BASE + (byteOffset)))

/// Constant element count of a fixed-size array, as `s32`.
///
/// `arr` must have a complete, non-variable-length array type whose element
/// count fits in `s32`. Pointers, including function parameters declared as
/// arrays, do not retain an array bound and must not be passed.
/// Neither occurrence of `arr` is evaluated: its storage is not read and
/// side effects in the argument do not run. Nested arrays count outer elements.
#define ARRAY_SIZE(arr) ((s32)(sizeof(arr) / sizeof((arr)[0])))

/// Byte offset of an embedded member from the start of its aggregate, as `size_t`.
///
/// `type` must be a complete struct or union type; `member` must be addressable.
/// Nested dot paths and array subscripts are supported; subscripts must be in
/// bounds. Bit-fields and paths through stored pointers are not supported.
/// Constant subscripts give a layout constant usable in `STATIC_ASSERT`; each
/// nonconstant subscript is evaluated once. The member's storage is not read.
#define OFFSET_OF(type, member) ((size_t)&(((type*)0)->member))

/// Recovers a containing aggregate's `type*` from its embedded `member` address.
///
/// `type` must be a complete struct or union type. A non-NULL `ptr` must
/// address the start of the named subobject in a live instance of that type,
/// rather than a standalone object or a stored pointer's pointee. Nested dot
/// paths and in-bounds array subscripts follow `OFFSET_OF`'s requirements.
/// The displacement is in bytes, regardless of `ptr`'s pointee type; the
/// member's storage is not read and its owner's lifetime is not extended.
///
/// Pointee types are not checked. Result qualifiers come from `type`, not
/// `ptr`; supply a const-qualified `type` for a read-only containing object.
/// `ptr` and each nonconstant member subscript are evaluated once, with no
/// evaluation order guaranteed between them.
///
/// There is no NULL check. On the target, a zero-offset member preserves NULL
/// for intrusive-list termination; NULL at a nonzero offset is invalid.
#define PARENT_OF(ptr, type, member) ((type*)((u8*)(ptr) - OFFSET_OF(type, member)))

#define SECTION(x) __attribute__((section(x)))

#define STATIC_ASSERT(cond, msg) \
    typedef char static_assertion_##msg[(cond) ? 1 : -1]

#define STATIC_ASSERT_SIZEOF(type, size) \
    typedef char static_assertion_sizeof_##type[(sizeof(type) == (size)) ? 1 : -1]

/*
 * Matching helpers: empty GNU C statement-asm that emit no MIPS. Each use is a
 * matching carrier, not reconstructed source.
 *
 * Only the two forms that still have a user are defined. The rest of the
 * family (TOUCH_REG*, USE_REG*, DEF_REG, the barriers, MOVE_ZERO, COPY_REG,
 * ...) was deleted as its last users were rewritten. Do not add a steering
 * macro, here or in a source file, to keep a change matching (NAMING.md,
 * "Never add a matching hack to keep a cleanup"); tools/check_hack_sites.py
 * still counts the deleted names.
 *
 * SOFT_TOUCH_REG reads and writes `x`; SOFT_USE_REG only reads it. GCC 2.8.1
 * makes an asm with no output implicitly volatile, so SOFT_USE_REG is a
 * scheduling fence despite its spelling. The read/write form avoids that rule
 * but still changes dependencies and may move. Inspect the RTL
 * (CODEGEN_MODEL.md section 11).
 *
 * Expansions are plain statement-asm. Do not wrap them in do/while or
 * extra braces: that changes stack and scheduling.
 */
#define SOFT_TOUCH_REG(x) __asm__("" : "+r"(x))
#define SOFT_USE_REG(x)   __asm__("" :: "r"(x))

/// Exports `orig`, defined in this file, under a second name `alias`: one more
/// global symbol at the same address, adding no bytes. The compiler has no
/// alias attribute for this target, so the assembler makes it, and it has to be
/// in the object that defines `orig`.
#define DEFINE_ALIAS(orig, alias)                                                                  \
    extern __typeof__(orig) alias;                                                                 \
    __asm__(".globl " #alias "\n" #alias " = " #orig)

/// The aliases the overlay manifest declares for the package being built
/// (`aliases` on its slot), as `DEFINE_ALIAS` statements. A source several
/// packages are built from ends with this, so each package exports the shared
/// definitions under the names the resident images call that package's by.
#ifndef PACKAGE_ALIASES
#define PACKAGE_ALIASES
#endif

#endif // COMMON_H
