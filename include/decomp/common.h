#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"
#include "types.h"
#include "version.h"

#define PAD_RODATA()

#define PSX_SCRATCH ((void*)0x1F800000)

#define PSX_SCRATCH_ADDR(offset) ((void*)(((u8*)PSX_SCRATCH) + (offset)))

/// Computes the size of an array.
///
/// @param arr Array.
/// @return Element count.
#define ARRAY_SIZE(arr) (s32)(sizeof(arr) / sizeof((arr)[0]))

#define OFFSET_OF(st, m) ((size_t)&(((st*)0)->m))

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
 * SOFT_ variants apply the same constraint without volatile; GCC 2.8.1
 * -fschedule-insns may still move surrounding instructions. That
 * distinction is a matching difference (see Gp_DebugPanTask).
 *
 * Expansions are plain statement-asm. Do not wrap them in do/while or
 * extra braces: that changes stack and scheduling. GCC 2.8.1 has no
 * variadic macros; use the numbered forms for multiple operands.
 *
 * register T x asm("v0") is a different tool (function-wide hard pin).
 * Instruction-emitting asm (lui/lo, sll, move) stays written out.
 */
#define SCHED_BARRIER() __asm__ volatile("")
#define SOFT_BARRIER()  __asm__("")

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

#define SOFT_TOUCH_REG(x)             __asm__("" : "+r"(x))
#define SOFT_TOUCH_REG2(a, b)         __asm__("" : "+r"(a), "+r"(b))
#define SOFT_TOUCH_REG3(a, b, c)      __asm__("" : "+r"(a), "+r"(b), "+r"(c))
#define SOFT_TOUCH_REG4(a, b, c, d)   __asm__("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d))
#define SOFT_TOUCH_REG5(a, b, c, d, e) \
    __asm__("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d), "+r"(e))
#define SOFT_TOUCH_REG_USE(x, y) __asm__("" : "+r"(x) : "r"(y))

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
#define COPY_REG(dst, src) __asm__ volatile("" : "=r"(dst) : "r"(src))
/* `+&r` / `"r"` cannot overlap, so GCC emits `move` and frees src. */
#define COPY_REG_EC(dst, src) __asm__ volatile("" : "+&r"(dst) : "r"(src))

#endif // COMMON_H
