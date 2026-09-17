#ifndef ROOMS_NEO_ARK_WOODLAND_PATH_H
#define ROOMS_NEO_ARK_WOODLAND_PATH_H

#include "common.h"

#include "main/task.h"

/// The room's frame countdown at `D_neo_ark_woodland_path_8018498E`, seen
/// with both signs: `func_neo_ark_woodland_path_8018154C` adds to it as an
/// unsigned halfword (`lhu`) while the 0x7DB handler clears it to -1 as a
/// signed one (`addiu $v0,$zero,-1`). The two reads compile differently, so
/// each site names the view it uses.
typedef union NeoArkWoodlandPathTimer {
    /* 0x0 */ u16 u;
    /* 0x0 */ s16 s;
} NeoArkWoodlandPathTimer;

extern NeoArkWoodlandPathTimer D_neo_ark_woodland_path_8018498E;

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

/// Set once a spawn slot has been armed, read by the room's other states.
extern s16 D_neo_ark_woodland_path_80184996;

s32 func_neo_ark_woodland_path_80180B18(Task* task, s32 arg1, NeoArkWoodlandPathMsg* msg);

#endif
