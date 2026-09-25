#ifndef SCRATCH_H
#define SCRATCH_H

#include "common.h"

/// The scratch-pad stack: temporary blocks carved off the top of the
/// PlayStation's 1 KB scratch pad, which grows downward from the pointer kept
/// in its last word. A function takes a block by moving that pointer down by
/// the block's size and gives it back by moving it up again, in reverse order.
///
/// These are macros, not inline functions: the stack pointer's address is a
/// constant, and GCC keeps it in a register across the load and the store only
/// when the expression is written in the function itself. Inlined from a
/// function body, the same update is addressed twice and scheduled differently.
#define G_SCRATCH_HEAD         PSX_SCRATCH_ADDR(0x3FC)
#define GameResetScratchHead() *(void**)G_SCRATCH_HEAD = G_SCRATCH_HEAD

/// The stack pointer seen as a `type*`: reads the top block, or, assigned,
/// moves the top to a block the caller computed.
#define SCRATCH_HEAD(type) (*(type**)G_SCRATCH_HEAD)

/// Takes one `type` off the stack; the block is then `SCRATCH_HEAD(type)`.
#define SCRATCH_PUSH(type) (*(type**)G_SCRATCH_HEAD -= 1)

/// Gives one `type` back to the stack.
#define SCRATCH_POP(type) (*(type**)G_SCRATCH_HEAD += 1)

/// Takes `n` bytes off the stack.
#define SCRATCH_PUSH_BYTES(n) (*(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - (n))

/// Gives `n` bytes back to the stack.
#define SCRATCH_POP_BYTES(n) (*(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + (n))

/// `SCRATCH_PUSH_BYTES` / `SCRATCH_POP_BYTES` through a pointer to the stack
/// pointer that the function keeps in a local (`head = (void**)G_SCRATCH_HEAD`).
#define SCRATCH_PUSH_BYTES_AT(head, n) (*(void**)(head) = (u8*)*(void**)(head) - (n))
#define SCRATCH_POP_BYTES_AT(head, n)  (*(void**)(head) = (u8*)*(void**)(head) + (n))

#endif
