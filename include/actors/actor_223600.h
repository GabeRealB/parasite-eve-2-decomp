#ifndef ACTOR_223600_H
#define ACTOR_223600_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

#include <psyq/libgte.h>

/// The actor's per-instance work block, reached through `Task::work`. Only
/// the fields the decompiled message handlers touch are modelled so far: the
/// state word at 0x0 that every handler drives, the animation pair at
/// 0x4A/0x58 the state dispatchers read, the motion state at 0x174, the three
/// bytes at 0x180 that a handler copies out of the event packet and the flag
/// at 0x20C the tick latches while the model is parked.
typedef struct Actor223600Work {
    /* 0x000 */ s16        field_0; ///< state
    /* 0x002 */ s16        field_2; ///< state at the previous dispatch
    /* 0x004 */ s16        field_4; ///< set when `field_0` moved away from `field_2`
    /* 0x006 */ s16        field_6; ///< frames spent in the approach state
    /* 0x008 */ s16        field_8;
    /* 0x00A */ byte       pad_A[0x2];
    /* 0x00C */ GpAnimCtx  anim;     ///< `func_800B3F84` arg0
    /* 0x020 */ GpAnimSlot slots[1]; ///< slots 1.. continue past here, overlapping the fields below
    /* 0x048 */ byte       pad_48[0x2];
    /* 0x04A */ u16        field_4A; ///< low ten bits: current animation id
    /* 0x04C */ byte       pad_4C[0xC];
    /* 0x058 */ u16        field_58;
    /* 0x05A */ byte       pad_5A[0xB6];
    /* 0x110 */ byte       poses[0x60]; ///< `func_800B3F84` arg3
    /* 0x170 */ s16        field_170;
    /* 0x172 */ s16        field_172;
    /* 0x174 */ s16        field_174; ///< motion state
    /* 0x176 */ u16        field_176;
    /* 0x178 */ s16        field_178;
    /* 0x17A */ byte       pad_17A[0x4];
    /* 0x17E */ s16        field_17E;
    /* 0x180 */ u8         field_180;
    /* 0x181 */ u8         field_181;
    /* 0x182 */ u8         field_182;
    /* 0x183 */ byte       pad_183[0x1];
    /* 0x184 */ u16        field_184;
    /* 0x186 */ u16        field_186;
    /* 0x188 */ byte       pad_188[0xC];
    /// World X/Y/Z of the model's coordinate, narrowed to 16 bits as the spawn
    /// handler samples them through `Actor223600CoordPos`.
    /* 0x194 */ u16  field_194;
    /* 0x196 */ u16  field_196;
    /* 0x198 */ u16  field_198;
    /* 0x19A */ byte pad_19A[0x2];
    /// Target the approach state steers towards: world X in `field_19C` and
    /// world Z in `field_1A0`, both seeded from the spawn point.
    /* 0x19C */ u16    field_19C;
    /* 0x19E */ s16    field_19E;
    /* 0x1A0 */ u16    field_1A0;
    /* 0x1A2 */ byte   pad_1A2[0x6];
    /* 0x1A8 */ MATRIX field_1A8; ///< installed at `TmdObject.field_1C`
    /* 0x1C8 */ MATRIX field_1C8; ///< installed at `TmdObject.field_20`
    /* 0x1E8 */ byte   pad_1E8[0x20];
    /* 0x208 */ u16    field_208; ///< animation id that last raised the reaction
    /* 0x20A */ byte   pad_20A[0x2];
    /* 0x20C */ s8     field_20C; ///< 1 while the model's coordinate is zeroed
    /* 0x20D */ byte   pad_20D[0x5];
    /// Per-frame height step the parked state adds to the model's world Y,
    /// seeded by the motion the tick enters and retuned as it advances.
    /* 0x212 */ s16 field_212;
} Actor223600Work;
STATIC_ASSERT_SIZEOF(Actor223600Work, 0x214);

/// `GsCOORDINATE2.coord.t[]` seen as three unsigned halfwords, so
/// `func_actor_223600_8014B540` samples each world coordinate with `lhu`.
typedef struct Actor223600CoordPos {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  x;
    /* 0x1A */ byte pad_1A[2];
    /* 0x1C */ u16  y;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ u16  z;
    /* 0x22 */ byte pad_22[2];
} Actor223600CoordPos;
STATIC_ASSERT_SIZEOF(Actor223600CoordPos, 0x24);

/// 0xC-byte scratch taken from `0x1F8003FC` by the approach state: the XZ
/// offset from the model to its target, and the yaw step derived from it.
typedef struct Actor223600Turn {
    /* 0x0 */ s16  dx;
    /* 0x2 */ s16  dy;
    /* 0x4 */ s16  dz;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s16  yaw;
    /* 0xA */ byte pad_A[0x2];
} Actor223600Turn;
STATIC_ASSERT_SIZEOF(Actor223600Turn, 0xC);

/// Overlay-wide record the spawn handler points at the instance's coordinate,
/// tagging it with a 0x100 weight and a mode of 1.
typedef struct Actor223600Anchor {
    /* 0x0 */ GsCOORDINATE2* coord;
    /* 0x4 */ s16            field_4;
    /* 0x6 */ s16            field_6;
} Actor223600Anchor;
STATIC_ASSERT_SIZEOF(Actor223600Anchor, 0x8);

extern Actor223600Anchor D_actor_223600_80150B5C;

/// Pair source the spawn handler installs at `GpEnemy::field_50`.
extern GpPairSrcE D_actor_223600_8014CFCC;

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_223600_801509C0[];

/// Message table the spawn handler publishes as `Task::field_24`.
extern u8 D_actor_223600_80150B28[];

/// Event packet handed to this actor's message handlers. Its first three bytes
/// are copied into the work block, and its first four are then re-read as two
/// little-endian `u16` words: a command word and a sub-command.
typedef union Actor223600Event {
    /* 0x0 */ u8  bytes[4];
    /* 0x0 */ u16 words[2];
} Actor223600Event;
STATIC_ASSERT_SIZEOF(Actor223600Event, 0x4);

/// Game mode word the tick switches on: 0 and 1 drive the model's `field_C`
/// from the work block, 2 forces 0x80.
extern u8 D_801153F4;

/// Set to 1 while world motion is frozen; the approach state skips its forward
/// step for that frame.
extern u8 D_80072729;

/// The three state handlers `func_actor_223600_8014CA00` copies onto its stack
/// before the indirect call, in the order the dispatcher indexes them: entry 0
/// is the shared idle handler, 1 the show handler and 2 the hide handler. The
/// entries are `GpEnemyTaskFunc`s, so the tick is an `ActorsShared80135df4`
/// handler alongside `func_actor_223600_8014B540` and `Gp_DestroyEnemy` in
/// `ActorsShared80135df4Table`.
extern const GpEnemyTaskFuncTable3 D_actor_223600_80149E4C;

/// Restarts the model's motion for the work block's current state. Declared
/// without a prototype because its two call sites pass different argument
/// counts: the spawn handler hands it the HP it has just installed, while the
/// approach state calls it with the task alone.
void func_actor_223600_8014B2F4();
s32  func_actor_223600_8014B464(Actor223600Work* arg0);
void func_actor_223600_8014B540(GpEnemy* enemy, Task* task);
void func_actor_223600_8014B840(GpEnemy* enemy, Task* task);
void func_actor_223600_8014BBF4(GpEnemy* enemy, Task* task);
void func_actor_223600_8014CA00(GpEnemy* enemy, Task* task);
s32  func_actor_223600_8014CC04(Task* task, s32 arg1, s32 arg2);
s32  func_actor_223600_8014CCD4(Task* task, s32 arg1, Actor223600Event* event);

#endif // ACTOR_223600_H
