#ifndef ROOMS_DRYFIELD_JUNK_YARD_H
#define ROOMS_DRYFIELD_JUNK_YARD_H

#include "common.h"

#include "main/session.h"
#include "main/task.h"

/// Block `func_dryfield_junk_yard_8017D658` carves off the scratch stack
/// (`0x1F8003FC`, one `addiu` of `-0x18`) to hold the model's world position
/// while the ground-effect quad draws it, and hands to `Gp_DrawEffGroundQuad`.
/// Only `pos` is written; the alloc size is 0x18 rather than the `VECTOR3`'s
/// 0xC, so the tail is padding here.
///
/// Same shape as `Actor01600GroundScratch`, which is the same 0x18 with its
/// `VECTOR3 pos` at 0x00.
typedef struct {
    /* 0x00 */ VECTOR3 pos;
    /* 0x0C */ byte    pad_C[0xC];
} DjyGroundQuadScratch;
STATIC_ASSERT_SIZEOF(DjyGroundQuadScratch, 0x18);

/// Resident routine at the fixed `0x8072xxxx` address the room overlays import
/// for the game's named-sequence start (`0x80724608`, listed as
/// `absolute:True` in `configs/USA/sym/rooms.imports.txt`). Its first argument
/// is the game pointer the sequence is hung on, the second and third are the
/// two `s16` parameters that vary per call site, and the fourth is the name
/// word below.
void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

/// The `"DOG"` name word the yard hands to `func_80724608`, and the only
/// caller of it in this room. It is the 4 bytes between the room's shared
/// three-state table and its jump table at the head of the leading rodata
/// (`0x8017D5D0`, split out as its own `D_` symbol), which is why the overlay
/// names it after this room even though the string itself is data: the
/// declaration is `s32` because the 4-byte run is exactly one word,
/// little-endian `0x00474F44`.
extern s32 D_dryfield_junk_yard_8017D5D0;

/// State 1 of the yard's room task, whose table is
/// `RoomsShared8017d878Table` (entry `0x8017D5C8`, between
/// `func_dryfield_junk_yard_8017D708` and `Task_Kill`).
///
/// It waits for a stream file to be open (`D_8007107A`, the main-executable
/// halfword the room overlays gate streamed scenes on) and for the slot-0xA
/// game pointer the state-0 task also dispatches to, then starts the named
/// sequence on that pointer. The state never advances, so it repeats every
/// frame until the table's kill entry runs.
void func_dryfield_junk_yard_8017DC60(void);

#endif // ROOMS_DRYFIELD_JUNK_YARD_H
