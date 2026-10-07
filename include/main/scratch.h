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

/// Writable `type*` view of the shared downward-growing scratch-stack cursor.
///
/// `type` is a pointee type valid in a `type**` cast; use `void` for an
/// untyped pointer or `u8` for byte displacements. A value read loads the
/// cursor once from `SCRATCH_STACK_CURSOR_SLOT`; assignment replaces it once,
/// and a compound update reads and writes it once. Element arithmetic requires
/// a complete object type and moves by `sizeof(type)` bytes per element.
/// Taking this lvalue's address gives the slot as `type**` without reading it.
/// The type argument occurs once and supplies no value expression; the macro
/// captures no caller variables.
///
/// Reading the cursor reserves nothing. It can address the current block or
/// the empty-stack slot, so the selected type alone does not establish a live
/// object or its extent. Callers initialize the shared cursor and reserve
/// aligned storage below it before using a block. Assigning a lower address
/// reserves bytes; restoring a saved cursor releases intervening reservations
/// in reverse order. Keep reservations below the slot and clear of other live
/// scratchpad storage. No bounds or alignment checks or block clearing occur;
/// released storage can be reused, and resetting the cursor ends all current
/// reservations.
#define SCRATCH_STACK_CURSOR(type) (*(type**)SCRATCH_STACK_CURSOR_SLOT)

/// Reserves one uninitialized block on the downward-growing scratch stack.
///
/// `blockType` is a complete, fixed-size object type. The shared cursor in
/// `SCRATCH_STACK_CURSOR_SLOT` moves down by `sizeof(blockType)` bytes, and
/// the expression returns the new block's address as `blockType*`. No data is
/// copied onto the stack, cleared, or read from the reserved block.
///
/// Initialize the cursor before use. The entire block must fit below the
/// cursor slot, clear of other live scratchpad storage, with an address aligned
/// for `blockType`. No bounds or alignment checks or rounding are performed.
/// Release reservations in reverse order with `SCRATCH_STACK_RELEASE_BLOCK`
/// or restore the saved cursor; released storage is available for reuse.
///
/// The cursor slot is read and written once. Both occurrences of `blockType`
/// supply a type, with no value argument to evaluate and no caller variables
/// captured. The result is a pointer value, not a writable cursor lvalue.
#define SCRATCH_STACK_RESERVE_BLOCK(blockType) ((blockType*)(*SCRATCH_STACK_CURSOR_SLOT = (u8*)*SCRATCH_STACK_CURSOR_SLOT - sizeof(blockType)))

/// Releases one block from the downward-growing scratch stack.
///
/// `blockType` is a complete, fixed-size object type whose size equals the
/// reservation being released. The shared cursor in `SCRATCH_STACK_CURSOR_SLOT`
/// advances by `sizeof(blockType)` bytes; reservations must be released in
/// reverse order, including those made by directly assigning `SCRATCH_STACK_CURSOR`.
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

/// Reserves `byteCount` uninitialized bytes on the downward-growing scratch stack.
///
/// `byteCount` is the length of the reservation in bytes. The shared cursor in
/// `SCRATCH_STACK_CURSOR_SLOT` moves down by that many bytes, and the
/// expression returns the new cursor as `void*`. That pointer addresses the
/// reserved block. No data is copied, cleared, or read from those bytes.
/// The length is not required to equal `sizeof` of the caller's view;
/// `SCRATCH_STACK_RESERVE_BLOCK` reserves one complete object instead.
///
/// Initialize the cursor before use. The reserved bytes must fit below the
/// cursor slot, clear of other live scratchpad storage, and the new cursor
/// must be aligned for the view that reads them. No bounds or alignment
/// checks or rounding are performed.
///
/// Release the reservation in reverse order with `SCRATCH_STACK_RELEASE_BYTES`
/// of the same length, with `SCRATCH_STACK_RELEASE_BLOCK` when that object's
/// size is the reservation, or by restoring a saved cursor. The slot is read
/// and written once. `byteCount` is evaluated once. This expression captures
/// no caller variables. The result is a pointer value, not a writable cursor
/// lvalue.
#define SCRATCH_STACK_RESERVE_BYTES(byteCount) (*SCRATCH_STACK_CURSOR_SLOT = (u8*)*SCRATCH_STACK_CURSOR_SLOT - (byteCount))

/// Releases `byteCount` bytes from the downward-growing scratch stack.
///
/// `byteCount` is the byte length of the reservation being released. The
/// shared cursor in `SCRATCH_STACK_CURSOR_SLOT` advances by that many bytes.
/// Reservations must be released in reverse order, whichever helper reserved
/// them. The cursor must be initialized, and the update must stay within the
/// stack storage, at or below its empty position at the cursor slot. No bounds
/// or alignment checks are performed.
///
/// Returns the updated cursor as `void*`, rather than the released bytes.
/// The result may point to an enclosing reservation or to the empty-stack
/// slot. It is a pointer value, not a writable cursor lvalue. The slot is
/// read and written once. Released bytes are untouched, but become available
/// for reuse by subsequent reservations. `byteCount` is evaluated once. This
/// expression captures no caller variables.
#define SCRATCH_STACK_RELEASE_BYTES(byteCount) (*SCRATCH_STACK_CURSOR_SLOT = (u8*)*SCRATCH_STACK_CURSOR_SLOT + (byteCount))

/// The address of the stack pointer, for a function that keeps it in a local
/// (`head = SCRATCH_HEAD_ADDR;`) and works through that.
#define SCRATCH_HEAD_ADDR (SCRATCH_STACK_CURSOR_SLOT)

/// `SCRATCH_STACK_CURSOR`, `SCRATCH_STACK_RESERVE_BLOCK` and `SCRATCH_STACK_RELEASE_BLOCK` through such a local.
#define SCRATCH_HEAD_AT(head, type) (*(type**)(head))

#define SCRATCH_PUSH_AT(head, type) (*(type**)(head) -= 1)

#define SCRATCH_POP_AT(head, type) (*(type**)(head) += 1)

/// `SCRATCH_STACK_RELEASE_BYTES` through such a local.
#define SCRATCH_POP_BYTES_AT(head, n) (*(void**)(head) = (u8*)*(void**)(head) + (n))

#endif // MAIN_SCRATCH_H
