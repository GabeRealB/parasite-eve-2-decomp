#ifndef ACTOR_403000_H
#define ACTOR_403000_H

#include "common.h"

#include "actors/actors_shared_80164954.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// One of the four display nodes the spawn handler `func_actor_403000_801343B8`
/// links in a row from 0xB50 of the work block: the `GpObj` list node `Gp_LinkObj`
/// appends and the task's exit callback `func_actor_403000_8013D4F4` hands back
/// to `Gp_UnlinkObj`, followed by the five-entry `GpRec18` table the node's
/// `field_C` points at (+0x20). `Gp_InitRec18Table(_, 5, 0)` on that table is
/// what fixes the 0x98 stride the four nodes are spaced by.
typedef struct Actor403000Obj {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[5];
} Actor403000Obj;
STATIC_ASSERT_SIZEOF(Actor403000Obj, 0x98);

/// Per-actor work block for the `actor_403000` overlay.
///
/// `func_actor_403000_801343B8` allocates it with `memCalloc(0xFDC, 0)` and
/// parks it in the `Task::work` slot (0x1C) -- the same slot `Actor00100Work`
/// and `Actor403100Work` use for the same job, so the overlay reaches its state
/// through the task and not through the actor. `field_0` is the animation state
/// the per-frame handler switches on and `field_2` its frame counter, both
/// restarted together by the message handlers; `yaw` is the heading
/// `func_actor_403000_8013D364` reads back from the root coordinate (0xC
/// rather than `ActorShared80164af0Work::field_16`); `field_AC6` is the
/// requested animation id, written by the handlers and read back by the
/// handlers that tick the current state.
///
/// `field_AC0` is the playback state `func_actor_403000_80133AF8` switches on
/// (1 is the running state, whose per-frame branch compares `field_AC4`, the
/// animation in progress, against `field_AC6`) and `field_AC2` the frame
/// counter it advances; `field_ACA` is the clip id that tick copies into each
/// display node, so `func_actor_403000_8013D850` starting the actor rewrites
/// `field_AC6` and `field_ACA` together. `field_6` is the per-frame tick that
/// same function bumps, and bit 0x100 of `field_60` is what it watches to tell
/// that the animation it asked for has arrived.
typedef struct Actor403000Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ s16  field_2;
    /* 0x004 */ s16  field_4;
    /* 0x006 */ u16  field_6;
    /* 0x008 */ s16  field_8;
    /* 0x00A */ byte pad_A[0x2];
    /* 0x00C */ s16  yaw;
    /* 0x00E */ byte pad_E[0x52];
    /* 0x060 */ union {
        u16 half;
        s32 word;
    } field_60;
    /* 0x064 */ byte           pad_64[0xA5C];
    /* 0xAC0 */ s16            field_AC0;
    /* 0xAC2 */ s16            field_AC2;
    /* 0xAC4 */ s16            field_AC4;
    /* 0xAC6 */ s16            field_AC6;
    /* 0xAC8 */ byte           pad_AC8[2];
    /* 0xACA */ s16            field_ACA;
    /* 0xACC */ s16            field_ACC;
    /* 0xACE */ s16            field_ACE;
    /* 0xAD0 */ s16            field_AD0;
    /* 0xAD2 */ byte           pad_AD2[0x4];
    /* 0xAD6 */ s16            field_AD6;
    /* 0xAD8 */ s16            field_AD8;
    /* 0xADA */ s16            field_ADA;
    /* 0xADC */ s16            field_ADC;
    /* 0xADE */ byte           pad_ADE[0x2];
    /* 0xAE0 */ s16            field_AE0;
    /* 0xAE2 */ s16            field_AE2;
    /* 0xAE4 */ s16            field_AE4;
    /* 0xAE6 */ s16            field_AE6;
    /* 0xAE8 */ u8             field_AE8;
    /* 0xAE9 */ u8             field_AE9;
    /* 0xAEA */ u8             field_AEA;
    /* 0xAEB */ u8             field_AEB;
    /* 0xAEC */ byte           pad_AEC[0x64];
    /* 0xB50 */ Actor403000Obj objB50;
    /* 0xBE8 */ Actor403000Obj objBE8;
    /* 0xC80 */ Actor403000Obj objC80;
    /* 0xD18 */ Actor403000Obj objD18;
    /* 0xDB0 */ GpObj          objDB0;
    /* 0xDD0 */ GpActorD4Rec   recDD0;
    /// Record table `func_actor_403000_8013D48C` scans: the same five-entry
    /// `GpRec18` run the display nodes carry at +0x20, here standing on its own
    /// after the four nodes. `func_actor_403000_8013C864` hands it back to
    /// `Gp_ClearRec18Occupied` twice, and the scan reads a record's `key`
    /// the way the shared hit-record walkers do -- 0 means the run has ended,
    /// high half 0x10 is the kind that counts as present.
    /* 0xDE8 */ GpRec18      records[5];
    /* 0xE60 */ GpObj        objE60;
    /* 0xE80 */ GpActorD4Rec recE80;
    /* 0xE98 */ GpRec18      recordsE98[5];
    /* 0xF10 */ MATRIX       field_F10;
    /// The second of the two default matrices the spawn handler binds to the
    /// display object -- `&work->field_F10` and this one are what it writes to
    /// `TmdObject::lightMtx` / `field_20` -- so it is a `MATRIX` whether or not
    /// the animation that drives it is running. `func_actor_403000_8013D72C`
    /// zeroes this one field by field, last element first.
    /* 0xF30 */ MATRIX field_F30;
    /* 0xF50 */ byte   pad_F50[0x20];
    /// Frames left before the next hit is taken (`Gp_GetIdParam2` of the last
    /// hit id), counted down by `func_actor_403000_80134F44`.
    /* 0xF70 */ s16  field_F70;
    /* 0xF72 */ byte pad_F72[0x2];
    /// Horizontal target the display object is eased toward while it sits
    /// below the player (`func_actor_403000_8013BDE0`).
    /* 0xF74 */ s16  field_F74;
    /* 0xF76 */ s16  field_F76;
    /* 0xF78 */ s16  field_F78;
    /* 0xF7A */ byte pad_F7A[0x2];
    /* 0xF7C */ s16  field_F7C;
    /* 0xF7E */ s16  field_F7E;
    /* 0xF80 */ s16  field_F80;
    /* 0xF82 */ byte pad_F82[0x2];
    /* 0xF84 */ s16  field_F84;
    /* 0xF86 */ s16  field_F86;
    /// Bitmask of the four trigger points in `D_actor_403000_80158D64` last
    /// latched from `GameFlag_GetNibble(0xE2)` by `func_actor_403000_80134E00`.
    /* 0xF88 */ s16      field_F88;
    /* 0xF8A */ byte     pad_F8A[0x2];
    /* 0xF8C */ s16      field_F8C;
    /* 0xF8E */ byte     pad_F8E[0x2];
    /* 0xF90 */ void*    field_F90;
    /* 0xF94 */ s32      field_F94;
    /* 0xF98 */ s32      field_F98;
    /* 0xF9C */ s32      field_F9C;
    /* 0xFA0 */ s32      field_FA0;
    /* 0xFA4 */ u8       field_FA4;
    /* 0xFA5 */ u8       field_FA5;
    /* 0xFA6 */ u8       field_FA6;
    /* 0xFA7 */ byte     pad_FA7[0x1];
    /* 0xFA8 */ GpEffArg field_FA8; // `func_800FDB18` argument record
    /* 0xFB0 */ SVECTOR  field_FB0;
    /* 0xFB8 */ s16      field_FB8;
    /* 0xFBA */ s16      field_FBA;
    /* 0xFBC */ s16      field_FBC;
    /* 0xFBE */ byte     pad_FBE[0x2];
    /* 0xFC0 */ s16      field_FC0;
    /* 0xFC2 */ s16      field_FC2;
    /* 0xFC4 */ byte     pad_FC4[0x4];
    /* 0xFC8 */ s16      field_FC8;
    /* 0xFCA */ s16      field_FCA;
    /* 0xFCC */ u16      field_FCC;
    /* 0xFCE */ byte     pad_FCE[0x3];
    /* 0xFD1 */ s8       field_FD1;
    /* 0xFD2 */ s8       field_FD2;
    /* 0xFD3 */ s8       field_FD3;
    /* 0xFD4 */ s8       field_FD4;
    /* 0xFD5 */ s8       field_FD5;
    /* 0xFD6 */ u8       field_FD6;
    /* 0xFD7 */ s8       field_FD7;
    /* 0xFD8 */ s8       field_FD8;
    /* 0xFD9 */ u8       field_FD9;
    /* 0xFDA */ u8       field_FDA;
    /* 0xFDB */ byte     pad_FDB[0x1];
} Actor403000Work;
STATIC_ASSERT_SIZEOF(Actor403000Work, 0xFDC);

/// Animation view of the same work block, as `func_actor_403000_801336B4`
/// reads it: the `Actor01900AnimWork` layout 8 bytes earlier, with 24 slots
/// per context. `field_AD4` is the blend weight, `field_AD2` the clip written
/// to the blend slots and `field_ACA` the clip id (see `Actor403000Work`).
typedef struct Actor403000AnimWork {
    /* 0x000 */ byte       pad_0[0x14];
    /* 0x014 */ GpAnimCtx  anim;
    /* 0x028 */ GpAnimSlot slots[24];
    /* 0x3E8 */ byte       pad_3E8[0x180];
    /* 0x568 */ GpAnimCtx  blendAnim;
    /* 0x57C */ GpAnimSlot blendSlots[24];
    /* 0x93C */ byte       pad_93C[0x184];
    /* 0xAC0 */ s16        field_AC0;
    /* 0xAC2 */ s16        field_AC2;
    /* 0xAC4 */ s16        field_AC4;
    /* 0xAC6 */ s16        field_AC6;
    /* 0xAC8 */ u16        field_AC8;
    /* 0xACA */ s16        field_ACA;
    /* 0xACC */ byte       pad_ACC[2];
    /* 0xACE */ s16        field_ACE;
    /* 0xAD0 */ s16        field_AD0;
    /* 0xAD2 */ s16        field_AD2;
    /* 0xAD4 */ s16        field_AD4;
    /* 0xAD6 */ u16        field_AD6;
    /* 0xAD8 */ u16        field_AD8;
    /* 0xADA */ byte       pad_ADA[4];
    /* 0xADE */ s16        field_ADE;
    /* 0xAE0 */ u16        field_AE0;
    /* 0xAE2 */ byte       pad_AE2[6];
    /* 0xAE8 */ s8         field_AE8;
    /* 0xAE9 */ s8         field_AE9;
    /* 0xAEA */ s8         field_AEA;
    /* 0xAEB */ s8         field_AEB;
    /* 0xAEC */ byte       pad_AEC[4];
    /* 0xAF0 */ u32        field_AF0;
} Actor403000AnimWork;

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object.
typedef struct Actor403000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor403000Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor403000;

/// Payload the sender of the animation message passes as `Gp_DispatchMsg`'s
/// `arg2`; only the animation id at 0x4 is read.
typedef struct Actor403000Msg {
    /* 0x0 */ byte pad_0[0x4];
    /* 0x4 */ u16  field_4;
} Actor403000Msg;

/// Event record `func_actor_403000_801324EC` dispatches on: `w[0]` is the
/// event kind (only 0x204 is handled) and `w[1]` its sub-code, and the first
/// three bytes are also copied raw into `Actor403000Work::field_FA4`..`field_FA6`.
/// Same shape as `Actor401300Event`.
typedef union Actor403000Event {
    u8  b[3];
    u16 w[2];
} Actor403000Event;

/// Payload of message 0x3FE, the push the actor asks the player to take:
/// `x`/`z` are the displacement, `field_10`/`field_12` the kind and count.
/// `func_actor_403000_801384E8` clears the vector once the player accepts it.
typedef struct Actor403000Msg3FE {
    /* 0x00 */ s32  x;
    /* 0x04 */ s32  y;
    /* 0x08 */ s32  z;
    /* 0x0C */ byte pad_C[0x4];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s8   field_12;
    /* 0x13 */ byte pad_13[0x1];
} Actor403000Msg3FE;
STATIC_ASSERT_SIZEOF(Actor403000Msg3FE, 0x14);

/// 0x34-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_actor_403000_80134F44`: `pos` and `id` are the first damage record
/// found on the four hit tables, `d`/`dist` the player's offset from the model
/// and its length, `rel` the hit position relative to the model, `damage` the
/// amount taken and `angle` the wrapped heading of the hit.
typedef struct Actor403000DamageScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ SVECTOR rel;
    /* 0x18 */ SVECTOR pos;
    /* 0x20 */ s32     id;
    /* 0x24 */ u32     damage;
    /* 0x28 */ s32     dist;
    /* 0x2C */ s16     angle;
    /* 0x2E */ byte    pad_2E[0x6];
} Actor403000DamageScratch;
STATIC_ASSERT_SIZEOF(Actor403000DamageScratch, 0x34);

/// Payload of message 0x7DA `func_actor_403000_80134F44` hands the slot-4 task
/// when the actor is killed.
typedef struct Actor403000Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor403000Msg7DA;

/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_actor_403000_801384E8`: `dir` holds the display object's first
/// matrix column, normalised and scaled down into the push vector.
typedef struct Actor403000PushScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ byte    pad_18[0x10];
} Actor403000PushScratch;
STATIC_ASSERT_SIZEOF(Actor403000PushScratch, 0x28);

/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_actor_403000_801386E8`: `d` is the player's offset from the model and
/// `dist` its length, `target` the camera target relative to the model,
/// `angle` the clamped turn and `ret` the reply to message 0x3F9.
typedef struct Actor403000LungeScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ SVECTOR target;
    /* 0x18 */ s32     dist;
    /* 0x1C */ byte    pad_1C[0x4];
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     ret;
    /* 0x24 */ byte    pad_24[0x4];
} Actor403000LungeScratch;
STATIC_ASSERT_SIZEOF(Actor403000LungeScratch, 0x28);

/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_actor_403000_80137084`: `d` is the camera target's offset from the
/// model and `dist` its length, `target` the same offset for the heading,
/// `angle` the clamped turn, `cell` / `playerCell` the waypoint-grid cells.
typedef struct Actor403000ChaseScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ SVECTOR target;
    /* 0x18 */ s32     dist;
    /* 0x1C */ byte    pad_1C[0x4];
    /* 0x20 */ s16     angle;
    /* 0x22 */ byte    pad_22[0x2];
    /* 0x24 */ s8      cell;
    /* 0x25 */ s8      playerCell;
    /* 0x26 */ byte    pad_26[0x2];
} Actor403000ChaseScratch;
STATIC_ASSERT_SIZEOF(Actor403000ChaseScratch, 0x28);

/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_actor_403000_801377C8`: `d`/`dist` the player's offset from the model
/// and its length, `target` the camera target relative to the model,
/// `playerYaw`/`aimYaw` the player's facing and the reversed heading to the
/// camera target, `angle` the wrapped turn, `ret` the reply to message 0x3F9
/// and `cell`/`playerCell` the waypoint-grid cells.
typedef struct Actor403000GrabScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ SVECTOR target;
    /* 0x18 */ s32     dist;
    /* 0x1C */ s16     playerYaw;
    /* 0x1E */ s16     aimYaw;
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     ret;
    /* 0x24 */ s8      cell;
    /* 0x25 */ s8      playerCell;
    /* 0x26 */ byte    pad_26[0x2];
} Actor403000GrabScratch;
STATIC_ASSERT_SIZEOF(Actor403000GrabScratch, 0x28);

/// Payload of message 0x3E9 `func_actor_403000_801386E8` sends the player:
/// the player's position and a heading of the model's facing minus 0x400.
typedef struct Actor403000Msg3E9 {
    /* 0x00 */ s32  x;
    /* 0x04 */ s32  y;
    /* 0x08 */ s32  z;
    /* 0x0C */ byte pad_C[0x4];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ byte pad_16[0x2];
} Actor403000Msg3E9;
STATIC_ASSERT_SIZEOF(Actor403000Msg3E9, 0x18);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_actor_403000_8013B238`: `vec` is the waypoint relative to the
/// coordinate and later the scaled matrix columns, `index` the waypoint picked
/// from `base` plus `field_FD5`, `angle` the wrapped heading error.
typedef struct Actor403000AimScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s16     index;
    /* 0x0A */ byte    pad_A[0x2];
    /* 0x0C */ s16     angle;
    /* 0x0E */ s16     base;
    /* 0x10 */ byte    pad_10[0x4];
} Actor403000AimScratch;
STATIC_ASSERT_SIZEOF(Actor403000AimScratch, 0x14);

/// Scratch frame used by `func_actor_403000_8013B74C` for the drop target,
/// heading error, player-message result and waypoint-grid cell.
typedef struct Actor403000DropScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR target;
    /* 0x18 */ byte    pad_18[0x8];
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     ret;
    /* 0x24 */ byte    pad_24;
    /* 0x25 */ s8      base;
    /* 0x26 */ byte    pad_26[0x2];
} Actor403000DropScratch;
STATIC_ASSERT_SIZEOF(Actor403000DropScratch, 0x28);

/// Squared horizontal components and radius used by `Actor403000_Outside`.
typedef struct Actor403000RadiusScratch {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
} Actor403000RadiusScratch;
STATIC_ASSERT_SIZEOF(Actor403000RadiusScratch, 0xC);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_actor_403000_8013C2D4`: `facing` and `base` are the player's and the
/// actor's waypoint-grid cells, `index` the waypoint picked from `base` plus
/// `field_FD1`, `vec` it relative to the coordinate and `angle` the clamped turn.
typedef struct Actor403000SeekScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s16     index;
    /* 0x0A */ byte    pad_A[0x2];
    /* 0x0C */ s16     angle;
    /* 0x0E */ s16     base;
    /* 0x10 */ s16     facing;
    /* 0x12 */ byte    pad_12[0x2];
} Actor403000SeekScratch;
STATIC_ASSERT_SIZEOF(Actor403000SeekScratch, 0x14);

/// 0xC-byte scratch from `G_SCRATCH_HEAD` used by
/// `func_actor_403000_80134204`: `index` picks the next waypoint out of
/// `D_actor_403000_80158CE0`, `target` is it relative to the coordinate and
/// `turn` the +1/-1 steering result.
typedef struct Actor403000TurnScratch {
    /* 0x00 */ SVECTOR target;
    /* 0x08 */ s8      index;
    /* 0x09 */ s8      turn;
    /* 0x0A */ byte    pad_A[0x2];
} Actor403000TurnScratch;
STATIC_ASSERT_SIZEOF(Actor403000TurnScratch, 0xC);

/// 0xC-byte `G_SCRATCH_HEAD` block `func_actor_403000_80133FC0` takes: the
/// player's position relative to the model, then the wrapped facing error.
typedef struct Actor403000FacingScratch {
    /* 0x00 */ SVECTOR target;
    /* 0x08 */ s16     angle;
    /* 0x0A */ byte    pad_A[0x2];
} Actor403000FacingScratch;
STATIC_ASSERT_SIZEOF(Actor403000FacingScratch, 0xC);

/// 0x34-byte `G_SCRATCH_HEAD` block `func_actor_403000_801365D0` takes to
/// rebuild the model's rotation: a yaw-only matrix from the current facing
/// (`angle`), scaled by `scale`, copied back into the coordinate.
typedef struct Actor403000ScaleScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ byte   pad_32[0x2];
} Actor403000ScaleScratch;
STATIC_ASSERT_SIZEOF(Actor403000ScaleScratch, 0x34);

/// 0xB4-byte `G_SCRATCH_HEAD` block `func_actor_403000_801330D4` takes (and
/// never returns): a coordinate parented to the caller's, and `pos`, its
/// origin walked up the parent chain into view space.
typedef struct Actor403000TrailScratch {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ byte          pad_50[0x18];
    /* 0x68 */ SVECTOR       pos;
    /* 0x70 */ byte          pad_70[0x44];
} Actor403000TrailScratch;
STATIC_ASSERT_SIZEOF(Actor403000TrailScratch, 0xB4);

/// 0x38-byte `G_SCRATCH_HEAD` block `func_actor_403000_8013C864` takes each
/// frame: `d` and `dist` are the player's offset from the model and its length
/// (even frames), `to`/`from` the two world positions handed to `func_800E0308`
/// as the line-of-sight segment (odd frames), `ofs` the flare offset passed to
/// `func_actor_403000_801327B0`.
typedef struct Actor403000UpdateScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ byte    pad_10[0x8];
    /* 0x18 */ SVECTOR to;
    /* 0x20 */ SVECTOR from;
    /* 0x28 */ SVECTOR ofs;
    /* 0x30 */ s32     dist;
    /* 0x34 */ byte    pad_34[0x4];
} Actor403000UpdateScratch;
STATIC_ASSERT_SIZEOF(Actor403000UpdateScratch, 0x38);

/// The animation-state handlers `func_actor_403000_8013C864` copies onto its
/// stack and calls through, indexed by `Actor403000Work::field_0`.
typedef struct Actor403000StateTable {
    void (*funcs[35])(Actor403000*);
} Actor403000StateTable;
extern const Actor403000StateTable D_actor_403000_80131F44;

/// Trail history `func_actor_403000_801330D4` shifts down one slot per call,
/// storing the newest position in slot 0.
extern SVECTOR D_actor_403000_80158DF0[18];

/// Waypoint grid for `func_actor_403000_80134204`: two rows of five indices
/// (row by `coord.t[2]`, column by `coord.t[0]` band), each one less than the
/// `D_actor_403000_80158CE0` entry it selects.
extern u8 D_actor_403000_80158D48[];

/// The push message `func_actor_403000_801384E8` keeps resending to the
/// player.
extern Actor403000Msg3FE D_actor_403000_80158DB0;

/// The grab message `func_actor_403000_801386E8` sends as 0x3E9.
extern Actor403000Msg3E9 D_actor_403000_80158D90;

/// Pairs of hit-effect vectors `func_actor_403000_80134910` picks from by
/// turn magnitude; `pad` indexes the display object's coordinate parts.
extern SVECTOR D_actor_403000_80158C48[];

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id -- `func_actor_403000_8013ACBC` scales a byte id by 8 into it
/// the same way -- so a record's `vx`/`vy`/`vz` are the vector an actor's
/// handlers copy out of it. Lives in the overlay's trailing data region.
extern SVECTOR D_actor_403000_80158CE0[];

/// Four trigger points (`vx`/`vz` used) `func_actor_403000_80134E00` measures
/// the display object against, one per bit of `GameFlag_GetNibble(0xE2)`.
extern SVECTOR D_actor_403000_80158D64[];

/// The game's shared 32-bit LCG state: a draw is
/// `Gp_LcgState = Gp_LcgState * 5 + 0x71357911`, read back from the global,
/// with the caller taking the bits it wants out of the high half.
extern u32 Gp_LcgState;

/// Tick the work block's animation playback: state `field_AC0` 1 advances
/// `field_AC2` until it catches up with the requested `field_AC6`, copying
/// `field_ACA` into the four display nodes' clip slot as it goes.
void func_actor_403000_80133AF8(Actor403000* arg0);

s32 func_actor_403000_80134204(GsCOORDINATE2* coord);

s32 func_actor_403000_80132348(GsCOORDINATE2* coord, GpRec18* recs, s32 count);

/// Copy `placement` onto the actor's root coordinate (Y then X then Z) and
/// cache the resulting heading in `Actor403000Work::yaw`.
s32 func_actor_403000_8013D364(Task* task, s32 arg1, ActorShared80164954Placement* placement);

/// Latch the requested animation and restart the animation state machine.
s32 func_actor_403000_8013D464(Task* task, s32 arg1, Actor403000Msg* msg);

/// Report whether the work block's five-entry record run holds a live entry:
/// the walk stops at the first empty `key` and answers 1 if any record it
/// passed carried the 0x10 kind bits.
s16 func_actor_403000_8013D48C(Task* task);

/// `Task::exitCallback` installed by the spawn handler, for the teardown path
/// where the enemy was created: hand the four display nodes back to
/// `Gp_UnlinkObj`, drop the enemy's `field_54` slot, then let `Gp_DestroyEnemy`
/// free the enemy and the task.
void func_actor_403000_8013D4F4(Task* task);

/// Copy the `vx`/`vy`/`vz` of record `arg1` of the pose table into `arg0`.
void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1);

void func_actor_403000_8013D5F8(Actor403000* arg0);

/// Per-frame update for the actor once its work block exists: on the frame
/// `field_4` is set, reinstate the display object's buffers and restart the
/// animation state machine on clip 0x10, then tick `field_6` and the playback
/// state, and when bit 0x100 of `field_60` reports the clip has arrived, raise
/// `field_FD2`/`field_FD3` and move the state machine to state 2.
void func_actor_403000_8013D850(Actor403000* arg0);

/// Per-frame countdown: on the frame `field_4` is set, reload the `field_6`
/// tick from a fresh `Gp_LcgState` draw masked to 0xA..0x19, then decrement
/// it. When the tick underflows and the enemy still has HP left
/// (`GpEnemy::field_40`), the animation state `field_0` is set to 0x13.
void func_actor_403000_8013D910(Actor403000* arg0);

#endif // ACTOR_403000_H
