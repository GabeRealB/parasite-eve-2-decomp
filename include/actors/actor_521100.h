#ifndef ACTOR_521100_H
#define ACTOR_521100_H

#include "common.h"
#include "gameplay/1BC.h"
#include <psyq/libgte.h>

/// The actor's attach coordinate. `Gp_UpdateCoord` reads it as a plain
/// `GsCOORDINATE2`, and the body that walks the actor forward advances
/// `coord.coord.t` by the facing axis `coord.coord.m[0][2]` / `m[2][2]`;
/// the actor keeps one more word after the coordinate, so the layout is
/// spelled out here rather than borrowed from libgs.
typedef struct Actor521100Coord {
    /* 0x00 */ s32    field_0;
    /* 0x04 */ MATRIX coord;
    /* 0x24 */ MATRIX workm;
    /* 0x44 */ byte   pad_44[0xC];
    /* 0x50 */ s32    field_50;
} Actor521100Coord;
STATIC_ASSERT_SIZEOF(Actor521100Coord, 0x54);

typedef struct Actor521100Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor521100Coord* field_8;
    /* 0x0C */ s16               field_C;
} Actor521100Obj2C;

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block below, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object. Same shape as the
/// other actor overlays' contexts.
typedef struct Actor521100 {
    /* 0x00 */ byte                    pad_0[0x1C];
    /* 0x1C */ struct Actor521100Work* field_1C;
    /* 0x20 */ GpEnemy*                field_20;
    /* 0x24 */ byte                    pad_24[8];
    /* 0x2C */ Actor521100Obj2C*       field_2C;
} Actor521100;

/// Per-actor work block. `Mem_Calloc(0x4B4, 0)` in `func_actor_521100_80135DDC`
/// stores the pointer both in `D_actor_521100_8016A3D8` and in the task's
/// 0x1C slot. `anim` sits at 0x40 and the slot array at 0x54, the same layout
/// as `Actor202900Work`; the nineteen slots are the ones
/// `func_actor_521100_80136724` ticks. The step / playing-id / `animId` /
/// clear-flag fields at 0x47C / 0x47E / 0x480 / 0x482 are the same layout
/// `Actor202900Work` uses.
///
/// `yaw` and `travel` are the cache the "walk to" placement opcode writes:
/// the heading it applied to the root coordinate and the remaining distance,
/// scaled by 20. Same offsets as `ActorsShared80133678Work`.
typedef struct Actor521100Work {
    /* 0x000 */ byte         pad_0[0x40];
    /* 0x040 */ GpAnimCtx    anim;
    /* 0x054 */ GpAnimSlot   slots[0x13];
    /* 0x34C */ byte         pad_34C[0x130];
    /* 0x47C */ s16          field_47C; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x47E */ u16          field_47E; // animation id currently playing
    /* 0x480 */ u16          animId;    // animation id the slots are seeded with
    /* 0x482 */ s16          field_482; // cleared when a step body is started
    /* 0x484 */ byte         pad_484[0x2A];
    /* 0x4AE */ u16          yaw;
    /* 0x4B0 */ byte         pad_4B0[0x2];
    /* 0x4B2 */ s16          travel;
    /* 0x4B4 */ byte         pad_4B4[0x198];
    /* 0x64C */ s16          field_64C; // the attach coordinate's translation, snapshotted each frame
    /* 0x64E */ s16          field_64E;
    /* 0x650 */ s16          field_650;
    /* 0x652 */ byte         pad_652[2];
    /* 0x654 */ Actor521100* field_654;
    /* 0x658 */ byte         pad_658[0x28];
    /* 0x680 */ s16          field_680;
    /* 0x682 */ s16          field_682; // non-zero while the tick in func_actor_521100_80135B80 remaps the model's field_C
    /* 0x684 */ byte         pad_684[8];
    /* 0x68C */ s16          field_68C;
    /* 0x68E */ s16          field_68E;
    /* 0x690 */ s16          field_690;
    /* 0x692 */ s16          field_692;
    /* 0x694 */ s16          field_694;
    /// 12-bit angles. The step-1 entry body `func_actor_521100_80135680`
    /// subtracts them, wraps the difference into [-0x800, 0x800] and reads
    /// `field_6AA` when the result is under 0x200.
    /* 0x696 */ u16  field_696;
    /* 0x698 */ u16  field_698;
    /* 0x69A */ s16  field_69A; // forward speed, in 12-bit fixed point
    /* 0x69C */ s16  field_69C; // cleared together with the forward speed
    /* 0x69E */ s16  field_69E;
    /* 0x6A0 */ s16  field_6A0;
    /* 0x6A2 */ byte pad_6A2[6];
    /* 0x6A8 */ s16  field_6A8;
    /* 0x6AA */ s16  field_6AA;
    /* 0x6AC */ byte pad_6AC[4];
    /* 0x6B0 */ s16  field_6B0;
    /* 0x6B2 */ s16  field_6B2;
    /// The animation record's flag nibble (`rec->field_3 & 0x30`) latched for
    /// the next frame by the footstep cue body `func_actor_521100_80134D88`, so
    /// each foot fires on the frame its bit has just dropped.
    /* 0x6B4 */ u16 field_6B4;
} Actor521100Work;

/// 4-byte message record; the handler switches on the halfword at 0x2,
/// the same shape as `Actor361100Msg`.
typedef struct Actor521100Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor521100Msg;
STATIC_ASSERT_SIZEOF(Actor521100Msg, 0x4);

typedef struct Actor521100Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor521100Ctx;

/// Argument block of the "walk to" script opcode: the world position to walk
/// to. Only the horizontal components are read.
typedef struct Actor521100Target {
    /* 0x00 */ VECTOR pos;
} Actor521100Target;
STATIC_ASSERT_SIZEOF(Actor521100Target, 0x10);

/// Argument block of the message handler `func_actor_521100_801369B8`
/// implements: which animation to start. Same 4-byte-id prefix as
/// `Actor202900AnimArgs`, and the same `(u16)` narrowing on the store into the
/// work block's `animId`. The stored id is `args->animId + 1`.
typedef struct Actor521100AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor521100AnimArgs;

extern Actor521100Work* D_actor_521100_8016A3D8;

/// State table the overlay dispatches through, indexed by `Task::state`.
/// `D_actor_521100_80131E68` is its 3 words: create
/// (`func_actor_521100_80135DDC`), update (`func_actor_521100_80136680`)
/// and teardown (`func_actor_521100_801360C4`). Both handlers take the
/// task's 0x20 spawn argument first, like `Actor210600StateFuncTable3`.
typedef void (*Actor521100StateFunc)(void* spawnArg2, Task* task);

typedef struct Actor521100StateFuncTable3 {
    Actor521100StateFunc funcs[3];
} Actor521100StateFuncTable3;
STATIC_ASSERT_SIZEOF(Actor521100StateFuncTable3, 0xC);

extern const Actor521100StateFuncTable3 D_actor_521100_80131E68;

/// Stack copy `func_actor_521100_80136604` makes before the indirect call.
/// The copy itself moves only the 3 words of `D_actor_521100_80131E68`, but
/// the dispatcher's frame is 0x30 with `$ra` at 0x28, which needs 17-24 bytes
/// of locals. The trailing `u8`/`u8`/`u16` at 0x10 are written to 2, 9, 1;
/// `field_C` is unread. Same 20-byte table-plus-context shape as
/// `Actor210600DispatchCtx`.
typedef struct Actor521100DispatchCtx {
    /* 0x00 */ Actor521100StateFuncTable3 table;
    /* 0x0C */ s32                        field_C;
    /* 0x10 */ u8                         field_10;
    /* 0x11 */ u8                         field_11;
    /* 0x12 */ u16                        field_12;
} Actor521100DispatchCtx;
STATIC_ASSERT_SIZEOF(Actor521100DispatchCtx, 0x14);

void func_actor_521100_80135414(Actor521100Ctx* arg0, Actor521100* arg1);
void func_actor_521100_80135478(Actor521100Ctx* arg0, Actor521100* arg1);
void func_actor_521100_801355C8(Actor521100* arg0);
void func_actor_521100_80135F2C(Task* task);
void func_actor_521100_80136724(void);
void func_actor_521100_80136820(void);
s32  func_actor_521100_801369B8(Task* task, s32 arg1, Actor521100AnimArgs* args);
s32  func_actor_521100_80136BE8(Task* task, s32 arg1, Actor521100Target* target);

#endif
