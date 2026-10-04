#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"
#include "types.h"
#include "version.h"

#define PAD_RODATA()

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

#define ALIGN(x, a) (((u32)(x) + ((a) - 1)) & ~((a) - 1))

#define SECTION(x) __attribute__((section(x)))

#define STATIC_ASSERT(cond, msg) \
    typedef char static_assertion_##msg[(cond) ? 1 : -1]

#define STATIC_ASSERT_SIZEOF(type, size) \
    typedef char static_assertion_sizeof_##type[(sizeof(type) == (size)) ? 1 : -1]

/*
 * Matching helpers: empty GNU C statement-asm that emit no MIPS.
 *
 * VOLATILE variants are scheduling fences (delay slots, insn motion).
 * SOFT_ variants omit explicit volatile. GCC 2.8.1 makes no-output asm
 * implicitly volatile, including SOFT_USE_REG; basic empty asm is also a
 * boundary. Read/write SOFT_TOUCH_REG avoids that rule but still changes
 * dependencies and may move. Inspect the RTL (CODEGEN_MODEL.md section 11).
 *
 * Expansions are plain statement-asm. Do not wrap them in do/while or
 * extra braces: that changes stack and scheduling. GCC 2.8.1 has no
 * variadic macros; use the numbered forms for multiple operands.
 *
 * register T x asm("v0") creates hard-register RTL; only top-level register
 * declarations globally reserve the register. Local fp pins can be invalid.
 * Instruction-emitting asm (lui/lo, sll, move) stays written out.
 */
#define SCHED_BARRIER() __asm__ volatile("")
#define SOFT_BARRIER()  __asm__("")

/*
 * MATCHING CARRIER, not reconstructed source. Clears a dead flag and leaves an
 * empty loop behind it; a later guard then tests the flag alongside its real
 * condition. The first common-subexpression pass stops scanning at a loop end,
 * so only the second can fold that test, and folding it there limits how far
 * that pass carries an equivalence through a run of identical guards: each
 * guard past the second keeps the constant its predecessor loaded instead of
 * sharing the first. Neither the flag nor the loop leaves an instruction. Use
 * it only where that per-guard constant grouping is what the target shows; the
 * spelling that originally produced it is unknown.
 */
#define CSE_STEER(flag) \
    flag = 0;           \
    do {                \
    } while (0)

#define COMPILER_BARRIER()      __asm__ volatile("" ::: "memory")
#define SOFT_COMPILER_BARRIER() __asm__("" ::: "memory")

#define TOUCH_REG(x)                  __asm__ volatile("" : "+r"(x))
#define TOUCH_REG2(a, b)              __asm__ volatile("" : "+r"(a), "+r"(b))
#define TOUCH_REG3(a, b, c)           __asm__ volatile("" : "+r"(a), "+r"(b), "+r"(c))
#define TOUCH_REG4(a, b, c, d)        __asm__ volatile("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d))
#define TOUCH_REG5(a, b, c, d, e)     __asm__ volatile("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d), "+r"(e))
#define TOUCH_REG_MEM(x)              __asm__ volatile("" : "+r"(x) :: "memory")
#define TOUCH_REG2_MEM(a, b)          __asm__ volatile("" : "+r"(a), "+r"(b) :: "memory")
#define TOUCH_REG_USE(x, y)           __asm__ volatile("" : "+r"(x) : "r"(y))
#define TOUCH_REG_USE2(x, y, z)       __asm__ volatile("" : "+r"(x) : "r"(y), "r"(z))
#define TOUCH_REG2_USE(a, b, c)       __asm__ volatile("" : "+r"(a), "+r"(b) : "r"(c))

#define SOFT_TOUCH_REG(x)             __asm__("" : "+r"(x))
#define SOFT_TOUCH_REG2(a, b)         __asm__("" : "+r"(a), "+r"(b))
#define SOFT_TOUCH_REG3(a, b, c)      __asm__("" : "+r"(a), "+r"(b), "+r"(c))
#define SOFT_TOUCH_REG4(a, b, c, d)   __asm__("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d))
#define SOFT_TOUCH_REG5(a, b, c, d, e) \
    __asm__("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d), "+r"(e))
#define SOFT_TOUCH_REG_USE(x, y) __asm__("" : "+r"(x) : "r"(y))
#define SOFT_TOUCH_REG_USE2(x, y, z) __asm__("" : "+r"(x) : "r"(y), "r"(z))
#define SOFT_TOUCH_REG2_USE(a, b, c) __asm__("" : "+r"(a), "+r"(b) : "r"(c))

/* Output-only: gives `x` a definition that emits no MIPS. It is the third
 * form alongside TOUCH_REG ("+r") and USE_REG ("r"): the value `x` held
 * before is dead from here on, so the allocator and the scheduler stop
 * treating the variable as one range. */
#define DEF_REG(x)              __asm__ volatile("" : "=r"(x))
#define SOFT_DEF_REG(x)         __asm__("" : "=r"(x))

#define USE_REG(x)              __asm__ volatile("" :: "r"(x))
#define USE_REG2(a, b)          __asm__ volatile("" :: "r"(a), "r"(b))
#define USE_REG3(a, b, c)       __asm__ volatile("" :: "r"(a), "r"(b), "r"(c))
#define USE_REG4(a, b, c, d)    __asm__ volatile("" :: "r"(a), "r"(b), "r"(c), "r"(d))
#define USE_REG5(a, b, c, d, e) __asm__ volatile("" :: "r"(a), "r"(b), "r"(c), "r"(d), "r"(e))
#define SOFT_USE_REG(x)         __asm__("" :: "r"(x))
#define SOFT_USE_REG2(a, b)     __asm__("" :: "r"(a), "r"(b))

#define CLOBBER_REG(reg) __asm__ volatile("" ::: #reg)

#define TOUCH_MEM(x) __asm__("" : : "m"(x))

#define MOVE_ZERO(x)       __asm__ volatile("" : "=r"(x) : "0"(0))
/* Schedulable variant: the `move` may be placed anywhere in its block. */
#define SOFT_MOVE_ZERO(x)  __asm__("" : "=r"(x) : "0"(0))
#define COPY_REG(dst, src) __asm__ volatile("" : "=r"(dst) : "r"(src))
/* `+&r` / `"r"` cannot overlap, so GCC emits `move` and frees src. */
#define COPY_REG_EC(dst, src) __asm__ volatile("" : "+&r"(dst) : "r"(src))

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
