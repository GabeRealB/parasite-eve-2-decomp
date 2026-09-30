#ifndef MAIN_SCRATCH_H
#define MAIN_SCRATCH_H

#include "common.h"

/// Byte offset from `PLAYSTATION_SCRATCHPAD_BASE` to the scratch-stack cursor slot.
///
/// The final four bytes of scratchpad RAM hold an absolute 32-bit pointer,
/// not an offset or a scratch block. An empty stack points to this slot itself.
/// Reservations move the pointer down; their data must stay below the slot and
/// clear of other live scratchpad storage. On the little-endian target, the
/// pointer's low halfword is its byte offset from the scratchpad base, which
/// other scratchpad users read to check the space below the active stack.
enum { SCRATCH_STACK_HEAD_BYTE_OFFSET = 0x3FC };

/// Address of the writable cursor slot for the downward-growing scratch stack.
///
/// Expands to a constant `void**` addressing the final four bytes of the
/// PlayStation's 1 KiB scratchpad RAM. The slot stores an absolute pointer to
/// the current top block; an empty stack points to the slot itself. Evaluating
/// this macro performs no memory access or reservation. Dereference it to read
/// or replace the cursor; cast the slot to `type**` for an element-sized update.
///
/// The slot and its stack are shared across overlays. Initialize the cursor
/// before use, keep it aligned for each reserved block, and keep all blocks
/// below the slot and clear of other live scratchpad storage. Reservations
/// move the cursor down and releases move it up in reverse order. No bounds or
/// lifetime checks are provided; released blocks must not remain in use.
#define SCRATCH_STACK_CURSOR_SLOT ((void**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET))

/// The stack pointer seen as a `type*`: reads the top block, or, assigned,
/// moves the top to a block the caller computed.
#define SCRATCH_HEAD(type) (*(type**)SCRATCH_STACK_CURSOR_SLOT)

/// Takes one `type` off the stack; the block is then `SCRATCH_HEAD(type)`.
#define SCRATCH_PUSH(type) (*(type**)SCRATCH_STACK_CURSOR_SLOT -= 1)

/// Releases one block from the downward-growing scratch stack.
///
/// `blockType` is a complete, fixed-size object type whose size equals the
/// reservation being released. The shared cursor in `SCRATCH_STACK_CURSOR_SLOT`
/// advances by `sizeof(blockType)` bytes; reservations must be released in
/// reverse order, including those made by directly assigning `SCRATCH_HEAD`.
/// The cursor must be initialized, and the update must stay within the stack
/// storage, at or below its empty position at the cursor slot. No bounds or
/// alignment checks are performed.
///
/// Returns the updated cursor as `blockType*`, rather than the released block.
/// The result may point to an enclosing reservation of a different type or to
/// the empty-stack slot; it is not necessarily a dereferenceable `blockType`.
/// The slot is read and written once. Released bytes are untouched, but become
/// available for reuse by subsequent reservations. This expression captures no
/// caller variables and evaluates no value argument.
#define SCRATCH_STACK_RELEASE_BLOCK(blockType) ((blockType*)(*SCRATCH_STACK_CURSOR_SLOT = (u8*)*SCRATCH_STACK_CURSOR_SLOT + sizeof(blockType)))

/// Takes `n` bytes off the stack.
#define SCRATCH_PUSH_BYTES(n) (*SCRATCH_STACK_CURSOR_SLOT = (u8*)*SCRATCH_STACK_CURSOR_SLOT - (n))

/// Gives `n` bytes back to the stack.
#define SCRATCH_POP_BYTES(n) (*SCRATCH_STACK_CURSOR_SLOT = (u8*)*SCRATCH_STACK_CURSOR_SLOT + (n))

/// The address of the stack pointer, for a function that keeps it in a local
/// (`head = SCRATCH_HEAD_ADDR;`) and works through that.
#define SCRATCH_HEAD_ADDR (SCRATCH_STACK_CURSOR_SLOT)

/// `SCRATCH_HEAD`, `SCRATCH_PUSH` and `SCRATCH_STACK_RELEASE_BLOCK` through such a local.
#define SCRATCH_HEAD_AT(head, type) (*(type**)(head))

#define SCRATCH_PUSH_AT(head, type) (*(type**)(head) -= 1)

#define SCRATCH_POP_AT(head, type) (*(type**)(head) += 1)

/// `SCRATCH_PUSH_BYTES` / `SCRATCH_POP_BYTES` through such a local.
#define SCRATCH_PUSH_BYTES_AT(head, n) (*(void**)(head) = (u8*)*(void**)(head) - (n))

#define SCRATCH_POP_BYTES_AT(head, n) (*(void**)(head) = (u8*)*(void**)(head) + (n))

#endif // MAIN_SCRATCH_H
