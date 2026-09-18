#ifndef ACTOR_310600_H
#define ACTOR_310600_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// 0x538-byte work block `func_actor_310600_80161E64` allocates with
/// `Mem_Calloc` and hangs off `Task::work`. The display node at `obj` is
/// linked by `Gp_LinkObj` at spawn (its `field_C` points at `rec`, the
/// `GpRec18` table `Gp_InitRec18Table` fills) and unlinked again by the
/// exit callback `func_actor_310600_80162A24`.
///
/// `light` / `color` are the actor's own lighting and colour matrices;
/// `func_actor_310600_80162A58` republishes them onto the model's
/// `TmdObject::field_1C` / `field_20` in place of the shared defaults
/// `Gp_BindDefaultMtx` installs.
///
/// The block is fronted by the animation context `func_actor_310600_8016246C`
/// drives, laid out exactly as `Actor335800MainWork` is: the `GpAnimCtx` the
/// block itself is handed to as (`func_800B3F84` takes the block address), the
/// twenty `GpAnimSlot`s immediately above it, and the 0x140-byte table
/// `func_800B3F84` also takes at 0x334. `field_474` is the once-only latch the
/// slots are started through, and `field_476` / `field_475` are the animation
/// bank index and the animation id, latched on change and re-read from the
/// block by the loops below them.
typedef struct Actor310600Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140];
    /* 0x474 */ s8         field_474;
    /* 0x475 */ s8         field_475;
    /* 0x476 */ s8         field_476;
    /* 0x477 */ s8         field_477;
    /* 0x478 */ s16        field_478;
    /* 0x47A */ s16        field_47A;
    /* 0x47C */ s16        field_47C;
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ MATRIX     light;
    /* 0x4A0 */ MATRIX     color;
    /* 0x4C0 */ GpObj      obj;
    /* 0x4E0 */ GpRec18    rec;
    /* 0x4F8 */ s32        field_4F8;
    /* 0x4FC */ s32        field_4FC;
    /* 0x500 */ s32        field_500;
    /* 0x504 */ byte       pad_504[0x4];
    /* 0x508 */ VECTOR3    step; // local-space offset `ApplyMatrixLV` rotates into world space
    /* 0x514 */ byte       pad_514[0x4];
    /* 0x518 */ s32        field_518;
    /* 0x51C */ s32        field_51C;
    /* 0x520 */ s32        field_520;
    /* 0x524 */ byte       pad_524[0x4];
    /* 0x528 */ SVECTOR    limit; // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x530 */ byte       pad_530[0x8];
} Actor310600Work;
STATIC_ASSERT_SIZEOF(Actor310600Work, 0x538);

/// Overlay of the `GsCOORDINATE2` at `TmdObject::field_8`, the actor's root
/// part. Offset 0x44 (libgs `param`, `super` at 0x48) holds the Euler angles
/// `func_actor_310600_80162AD8` writes and then hands straight to `RotMatrix`,
/// the same reuse `ActorsShared80132860Coord` documents for its own overlay.
typedef struct Actor310600Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor310600Coord;
STATIC_ASSERT_SIZEOF(Actor310600Coord, 0x4C);

/// The 0x14-byte command block the actor's state handlers build on the stack
/// and hand to `func_actor_310600_8016246C`, which reads it as
/// `{animId, state, path, param}`.
typedef struct Actor310600Cmd {
    /* 0x00 */ s32 animId;
    /* 0x04 */ s32 state;
    /* 0x08 */ s32 path;
    /* 0x0C */ s32 param;
    /* 0x10 */ s32 unk10;
} Actor310600Cmd;

/// The constant local-space offset `func_actor_310600_80162B98` rotates,
/// `{ 0, 0, 0x200000, 0 }` -- straight ahead along the part's own +Z, the same
/// offset body `ActorsShared80132920` / `func_actor_335800_80163CA0` use, which
/// is why that handler's body appears again here. The overlay keeps its own copy
/// in `.rodata` (`actor_310600.c` carries the `INCLUDE_RODATA`), so the address
/// comes from the per-overlay symbol map.
extern VECTOR D_actor_310600_80161E54;

/// Spawn table entry 1 is this actor's `Task::state` dispatcher; the type-1
/// setup entry it is spawned from is `func_actor_310600_80161E64`.
extern TaskDesc D_actor_310600_801796A4[];

/// The overlay's `GpMsgEntry` table, parked in `Task::field_24`.
extern GpMsgEntry D_actor_310600_801796BC[];

void func_actor_310600_80162A24(Task* arg0);

void func_actor_310600_80162A58(Task* arg0);

/// Entry 0 of the second state table dispatch in `func_actor_310600_80161FA0`:
/// the idle handler, which does nothing. Entry 1 is
/// `func_actor_310600_80162A7C`, and the tick handler calls both through a
/// two-entry stack array that passes no argument.
void func_actor_310600_80162A74(void);

void func_actor_310600_80162A7C(Task* task);

s32 func_actor_310600_8016246C(Task* task, s32 arg1, Actor310600Cmd* cmd, s32 arg3);

s32 func_actor_310600_801625F0(Task* task, s32 arg1, s32 arg2, s32 arg3);

#endif
