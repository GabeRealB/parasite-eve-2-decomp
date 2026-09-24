#ifndef ROOMS_NEO_ARK_WOODLAND_PATH_H
#define ROOMS_NEO_ARK_WOODLAND_PATH_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_woodland_path_8017E9B0`.
extern const TaskFuncTable3 D_neo_ark_woodland_path_8017D614;

void func_neo_ark_woodland_path_8017F154(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);
void func_neo_ark_woodland_path_8017F5F4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_neo_ark_woodland_path_8017FDE4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_neo_ark_woodland_path_801801D0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);

/// The room's frame countdown at `D_neo_ark_woodland_path_8018498E`. Signed,
/// although the arithmetic reads compile as `lhu` (`func_...8018154C` adds to
/// it, `func_...80180DDC` counts it down): a load whose result is truncated by
/// the following `sh` only has to supply the low half, so GCC picks the
/// unsigned form by itself, while the `lh` comparisons and the -1 the 0x7DB
/// handler stores need the signed declaration. A union offering both views
/// compiles the same instructions but marks every access `in_struct`, and that
/// flag decides the scheduler's dependence analysis - it pinned a load after a
/// store in `func_neo_ark_woodland_path_80180C6C`. See
/// `DECOMPILATION_LEARNINGS.md`, "A union that only names a view costs
/// `in_struct`".
extern s16 D_neo_ark_woodland_path_8018498E;

/// Payload the 0x7DB handlers take as `Gp_DispatchMsg`'s `arg2`: the two-byte
/// sender id `0xB05` followed by the command the switch dispatches on.
typedef struct NeoArkWoodlandPathMsg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} NeoArkWoodlandPathMsg;
STATIC_ASSERT_SIZEOF(NeoArkWoodlandPathMsg, 0x4);

/// The same four bytes as this room builds them for its own 0x7DB send:
/// two id bytes (0x05, 0x0B, which the receiver reads as the halfword 0xB05)
/// followed by the spawn parameter.
typedef struct NeoArkWoodlandPathMsg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} NeoArkWoodlandPathMsg7DB;
STATIC_ASSERT_SIZEOF(NeoArkWoodlandPathMsg7DB, 0x4);

/// The object `Task::spawnArg2` holds for this room: the pending spawn
/// parameter handed over by `D_neo_ark_woodland_path_80184A60`, and the state
/// byte the handler clears once it has been taken. Trailing pad keeps pointer
/// alignment; the full object size is not known yet.
///
/// The halfword at 0x40 is reached through a cast rather than as a member,
/// because the original compiles it as a *scalar* reference: `MEM_IN_STRUCT_P`
/// (the `/s` on a MEM) feeds the scheduler's dependence analysis, and as a
/// member access the neighbouring `D_neo_ark_woodland_path_80184A60` store is
/// scheduled differently - see `DECOMPILATION_LEARNINGS.md`, "Scalar memory
/// references".
typedef struct NeoArkWoodlandPathObj {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ u16  field_40;
    /* 0x42 */ byte pad_42[0xA];
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[3];
} NeoArkWoodlandPathObj;

extern NeoArkWoodlandPathMsg7DB D_neo_ark_woodland_path_80184A5C;

/// The room's five spawn slots: `func_neo_ark_woodland_path_8018046C` fills the
/// first free one with a countdown and `func_neo_ark_woodland_path_80180B18`
/// hands slot 0 to the spawn it triggers and clears it. Read as `lhu` by the
/// handler and as `lh` by the slot filler, so each site names the view it uses
/// (`[0]` here, an `s16*` cast there).
extern u16 D_neo_ark_woodland_path_80184A60[5];

/// Ceiling `func_neo_ark_woodland_path_8018046C` clamps a spawn slot to
/// (0x1A4, 420 frames). Only the first halfword is this unit's; the run
/// continues into the room's parameter block, so the extent is splat's.
extern u16 D_neo_ark_woodland_path_8018494C[18];

/// The same run reached through its leading label, which is how
/// `func_neo_ark_woodland_path_80180C6C` reads the ceiling: element 2 is
/// `D_...8494C[0]`, 420 frames. splat names both addresses because the compiled
/// code names both, and the two are different code - an index keeps this
/// symbol in a register and takes the offset as the load's displacement, while
/// naming `D_...8494C` addresses it directly.
extern u16 D_neo_ark_woodland_path_80184948[];

/// The room's arming count, packed into game flag 0x10A as a nibble:
/// `func_neo_ark_woodland_path_80180C6C` adds the slot's spawn count to it and
/// then caps it at 5, the number of slots `D_...84A60` has. Signed, though the
/// add reads it as `lhu` - the result is truncated by the following `sh`, so
/// only the low half matters and GCC picks the unsigned load by itself.
extern s16 D_neo_ark_woodland_path_80184990;

/// How many spawns each slot arms, indexed by `gGameSession->at4.loc.place` (the
/// slot the session is in): the byte `func_...80180C6C` adds to
/// `D_...80184990`, and the gate `func_...80180DDC` tests against zero.
extern u8 D_neo_ark_woodland_path_80184970[];

/// The room's 0x7DB message-handler table - id/handler pairs, 0x7FFFFFFF
/// terminated - which `func_neo_ark_woodland_path_80180C6C` parks in
/// `Task::msgTable` for the task's message dispatch to walk. Same shape, and
/// the same three handlers, as the table in `D_...84998` next to it.
extern s32 D_neo_ark_woodland_path_801849F4[];

/// The same gate for the arm-state one step earlier: `func_...80180568` tests
/// it against zero and `func_...801806D8` reads the slot's count from it. One
/// byte per session slot, indexed by `gGameSession->at4.loc.place`, like
/// `D_...84970` above.
extern u8 D_neo_ark_woodland_path_80184980[];

/// `func_...80180568`'s own 0x7DB handler table, parked in `Task::msgTable`
/// exactly as `D_...849F4` is by `func_...80180C6C`.
extern s32 D_neo_ark_woodland_path_80184998[];

/// Set once a spawn slot has been armed, read by the room's other states.
extern s16 D_neo_ark_woodland_path_80184996;

/// Spawn point requested by the room's 0x7DB handlers (the message's third
/// byte, taken only while `D_...8498E` has run out). One-based index into
/// `D_...849B8`; zero means no request, and `func_...801806D8` clears it every
/// frame whether or not it placed a spawn.
extern s16 D_neo_ark_woodland_path_80184992;

/// A placement `func_...801806D8` puts a spawned task at: the x and z it writes
/// into the task's coordinate translation (y is always zero) and the Y
/// rotation it hands `Gfx_RotMatrixY`. The halfword after `x` is not read.
typedef struct NeoArkWoodlandPathSpawnPos {
    s16 x;
    s16 pad_2;
    s16 z;
    s16 rotY;
} NeoArkWoodlandPathSpawnPos;

/// The room's spawn placements, indexed by `D_...80184992 - 1`.
extern NeoArkWoodlandPathSpawnPos D_neo_ark_woodland_path_801849B8[];

/// The second arming sequence's spawn placements, which
/// `func_...80180DDC` picks from by `D_...80184992 - 1`. Five of them; any
/// request past the fourth takes the last.
extern NeoArkWoodlandPathSpawnPos D_neo_ark_woodland_path_80184A14[5];

/// Debug format `func_...80180DDC` prints the flag-0x10A count minus the
/// pending spawn slots with. Its bytes sit in the previous unit's `.rodata`.
extern char D_neo_ark_woodland_path_8017D648[];

/// `Gp_StateF0.field_6` as `func_...801806D8` saw it on the previous frame, so
/// that it can tell the reference count was non-zero before the frame began.
extern s16 D_neo_ark_woodland_path_801849F0;

/// The object `Task::spawnArg2` holds for the task that runs
/// `func_neo_ark_woodland_path_8017EA08`. Only the halfword at 0x26 is known:
/// the chance, out of 0x200, of spawning an effect this frame. It is recomputed
/// from how far the tracked model parts moved. Nothing yet shows whether this
/// is the same object as `NeoArkWoodlandPathObj`.
typedef struct NeoArkWoodlandPathTrailObj {
    /* 0x00 */ byte pad_0[0x26];
    /* 0x26 */ s16  chance;
} NeoArkWoodlandPathTrailObj;

/// World position (`workm.t`) of model parts 15 and 18 of the slot-3 task, as
/// `func_neo_ark_woodland_path_8017EA08` last saw them. Each frame it compares
/// the parts against these entries to measure how far they moved.
extern SVECTOR D_neo_ark_woodland_path_80181684[2];

s32 func_neo_ark_woodland_path_80180B18(Task* task, s32 arg1, NeoArkWoodlandPathMsg* msg);

#endif
