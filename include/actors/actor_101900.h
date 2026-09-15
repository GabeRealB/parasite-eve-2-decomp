#ifndef ACTOR_101900_H
#define ACTOR_101900_H

#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Private work block of the actor 01900 task, hanging off `Task::idMap`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` list nodes unlinked by the
/// destroy callback and the two child tasks it kills. `Actor01900_Fn0A764`
/// masks `field_A08.flags` and `field_B48.flags`, which is what fixes those
/// two offsets as `GpObj` rather than opaque padding.
typedef struct Actor01900Work {
    /* 0x000 */ s16     field_0;
    /* 0x002 */ s16     field_2;
    /* 0x004 */ s16     field_4;
    /* 0x006 */ s16     field_6;
    /* 0x008 */ byte    pad_8[0x52];
    /* 0x05A */ u16     field_5A;
    /* 0x05C */ byte    pad_5C[0xC];
    /* 0x068 */ u16     field_68;
    /* 0x06A */ byte    pad_6A[0x82E];
    /* 0x898 */ s16     field_898;
    /* 0x89A */ s16     field_89A;
    /* 0x89C */ byte    pad_89C[2];
    /* 0x89E */ s16     field_89E;
    /* 0x8A0 */ byte    pad_8A0[2];
    /* 0x8A2 */ s16     field_8A2;
    /* 0x8A4 */ s16     field_8A4;
    /* 0x8A6 */ byte    pad_8A6[8];
    /* 0x8AE */ s16     field_8AE;
    /* 0x8B0 */ s16     field_8B0;
    /* 0x8B2 */ byte    pad_8B2[2];
    /* 0x8B4 */ s32     field_8B4;
    /* 0x8B8 */ byte    pad_8B8[0x10];
    /* 0x8C8 */ GpObj   field_8C8;
    /* 0x8E8 */ GpRec18 field_8E8;
    /* 0x900 */ byte    pad_900[0x108];
    /* 0xA08 */ GpObj   field_A08;
    /* 0xA28 */ GpRec18 field_A28;
    /* 0xA40 */ byte    pad_A40[0x108];
    /* 0xB48 */ GpObj   field_B48;
    /* 0xB68 */ GpRec18 field_B68;
    /* 0xB80 */ byte    pad_B80[0x90];
    /* 0xC10 */ s16     field_C10;
    /* 0xC12 */ byte    pad_C12[0x12];
    /* 0xC24 */ s16     field_C24;
    /* 0xC26 */ byte    pad_C26[0xE];
    /* 0xC34 */ u8      field_C34[3];
    /* 0xC37 */ byte    pad_C37;
    /* 0xC38 */ Task*   field_C38;
    /* 0xC3C */ Task*   field_C3C;
    /* 0xC40 */ byte    pad_C40[8];
    /// Ring of the last seven view-space positions `Actor01900_Fn09D3C`
    /// records, one per step; `field_C98` is the write cursor.
    /* 0xC48 */ SVECTOR field_C48[7];
    /* 0xC80 */ byte    pad_C80[0x18];
    /* 0xC98 */ s16     field_C98;
} Actor01900Work;

/// Animation view of the same task work block. The arrays cover slot indices
/// used by Actor01900_Fn01950; the remaining bytes are not yet described.
typedef struct Actor01900AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[19];
    /* 0x328 */ byte       pad_328[0x130];
    /* 0x458 */ GpAnimCtx  blendAnim;
    /* 0x46C */ GpAnimSlot blendSlots[19];
    /* 0x764 */ byte       pad_764[0x13E];
    /* 0x8A2 */ s16        field_8A2;
    /* 0x8A4 */ byte       pad_8A4[6];
    /* 0x8AA */ s16        field_8AA;
    /* 0x8AC */ s16        field_8AC;
} Actor01900AnimWork;

/// Per-task actor context handed to the overlay's callbacks: `field_1C` is the
/// work block above (the same pointer `Task::idMap` holds) and `field_2C` is
/// the actor's `TmdObject`. Same shape as the other actor overlays' contexts.
typedef struct Actor01900 {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Actor01900Work* field_1C;
    /* 0x20 */ GpEnemy*        field_20;
    /* 0x24 */ byte            pad_24[8];
    /* 0x2C */ TmdObject*      field_2C;
} Actor01900;

/// Per-room clamp applied to the accumulated height offset of the actor's
/// motion scratch. `field_0` / `field_2` are matched against
/// `GameSession.field_7` / `field_6`, and when a row matches the offset is
/// clamped to [`lo`, `hi`]. `Actor01900_D172CC` holds two live rows plus a
/// zero terminator row that the two-iteration scan never reaches.
typedef struct Actor01900HeightClamp {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ s16  lo;
    /* 0x6 */ s16  hi;
    /* 0x8 */ byte pad_8[8];
} Actor01900HeightClamp;
STATIC_ASSERT_SIZEOF(Actor01900HeightClamp, 0x10);

/// 0x34-byte scratch `Actor01900_Fn06904` takes from `G_SCRATCH_HEAD`: a
/// `MATRIX` plus the `VECTOR` handed to `ScaleMatrix` and the yaw stored
/// before `Gfx_RotMatrixY`. Same layout as `ActorShared80135a60Scratch`.
typedef struct Actor01900RotScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor01900RotScratch;
STATIC_ASSERT_SIZEOF(Actor01900RotScratch, 0x34);

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `Actor01900_Fn03C98`.
/// The first 0x10 bytes are the `GpDeltaScratch` filled by `func_800E0C10`;
/// `step` is the integer part of those deltas (scaled down to length 0xC0
/// when longer), `len` its XZ length, and `moved` the return value: set when
/// the X or Z delta is nonzero.
typedef struct Actor01900Delta {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ SVECTOR        step;
    /* 0x18 */ s32            len;
    /* 0x1C */ s32            moved;
} Actor01900Delta;
STATIC_ASSERT_SIZEOF(Actor01900Delta, 0x20);

/// Payload of the `0x7D3` message the overlay's `Actor01900_D1728C` handler
/// table dispatches to `Actor01900_Fn0A31C`. Senders build the record in their
/// own data (`D_actor_146300_80137AAC` and friends); `field_4` selects which
/// animation id the actor switches to.
typedef struct Actor01900Msg7D3 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor01900Msg7D3;

/// The actor's state handlers, indexed by `Actor01900Work::field_0`.
/// `Actor01900_Fn09D3C` copies the table to its frame before dispatching.
typedef struct Actor01900StateTable {
    void (*fn[32])(Actor01900*);
} Actor01900StateTable;
STATIC_ASSERT_SIZEOF(Actor01900StateTable, 0x80);

/// 0x18-byte scratch `Actor01900_Fn09D3C` takes from `G_SCRATCH_HEAD`;
/// `pos` receives the model's third coordinate transformed to view space.
typedef struct Actor01900ViewScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR pos;
} Actor01900ViewScratch;
STATIC_ASSERT_SIZEOF(Actor01900ViewScratch, 0x18);

extern Actor01900StateTable  Actor01900_D001BC;
extern Actor01900HeightClamp Actor01900_D172CC[];
extern char                  Actor01900_D10B68;
extern void*                 D_80114B78[1];
extern u8                    D_801153F2[2];
extern u8                    D_801153F4;

s32  Actor01900_Fn00E00(GsCOORDINATE2* arg0, void* arg1, s32 arg2);
void Actor01900_Fn02A50(Actor01900* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void Actor01900_Fn01C94(Actor01900* arg0);
void Actor01900_Fn03FF8(Actor01900* arg0, void* arg1, s32 arg2);
void Actor01900_Fn08724(Actor01900* arg0);
void Actor01900_Fn0A7C0(Actor01900* arg0);
void Actor01900_Fn03C04(GameSessionFrom4* session, GsCOORDINATE2* coord);
s32  Actor01900_Fn0A31C(Actor01900* arg0, s32 arg1, Actor01900Msg7D3* arg2);
s32  Actor01900_Fn0A5A4(Actor01900* arg0, s32 arg1, u16* arg2);
s32  Actor01900_Fn0A38C(Actor01900* arg0, s32 arg1, s32 arg2);

#endif
