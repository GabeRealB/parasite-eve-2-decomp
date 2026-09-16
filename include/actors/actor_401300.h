#ifndef ACTOR_401300_H
#define ACTOR_401300_H

#include "common.h"

#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

/// Private work block of the actor 401300 task, hanging off `Task::idMap`.
///
/// Only the fields the matched code touches are named so far: `yaw` at 0x18
/// (the heading `func_actor_401300_80141614` reads back from the root
/// coordinate, one halfword later than `ActorsShared80169f74Work::yaw`), the
/// three `GpObj` display nodes `func_actor_401300_80141758` hands back to
/// `Gp_UnlinkObj`, the two child tasks it kills, and the halfword the
/// teardown-ish `func_actor_401300_80141EF8` tests before it stamps the enemy's
/// `field_40` with the -999 sentinel. The block is a good deal larger - sibling
/// `func_actor_401300_80141C88` reads animation state at 0x89C..0xC0E of the
/// same pointer - so the struct stays open-ended.
///
/// The display nodes do *not* sit at the same addresses as the same-shaped
/// teardown of actor 01900/401800, which keeps its three at 0x8C8/0xA08/0xB48.
/// Animation halfwords sit 4 bytes later than `Actor01900Work` (0x89C vs
/// 0x898); the three `GpObj` nodes sit 0xA8 later (0x970/0xAB0/0xBF0).
typedef struct Actor401300Work {
    /* 0x000 */ s16      field_0;
    /* 0x002 */ s16      field_2;
    /* 0x004 */ s16      field_4;
    /* 0x006 */ s16      field_6;
    /* 0x008 */ byte     pad_8[0x10];
    /* 0x018 */ s16      yaw;
    /* 0x01A */ byte     pad_1A[0x44];
    /* 0x05E */ u16      field_5E;
    /* 0x060 */ byte     pad_60[0xC];
    /* 0x06C */ u16      field_6C;
    /* 0x06E */ byte     pad_6E[0x82A];
    /* 0x898 */ s32      field_898;
    /* 0x89C */ s16      field_89C;
    /* 0x89E */ s16      field_89E;
    /* 0x8A0 */ byte     pad_8A0[2];
    /* 0x8A2 */ s16      field_8A2;
    /* 0x8A4 */ byte     pad_8A4[2];
    /* 0x8A6 */ s16      field_8A6;
    /* 0x8A8 */ s16      field_8A8;
    /* 0x8AA */ byte     pad_8AA[8];
    /* 0x8B2 */ s16      field_8B2;
    /* 0x8B4 */ s16      field_8B4;
    /* 0x8B6 */ s16      field_8B6;
    /* 0x8B8 */ s16      field_8B8;
    /* 0x8BA */ s16      field_8BA;
    /* 0x8BC */ s32      field_8BC;
    /* 0x8C0 */ byte     pad_8C0[0x50];
    /* 0x910 */ GpEffArg field_910;
    /* 0x918 */ byte     pad_918[0x58];
    /* 0x970 */ GpObj    field_970;
    /* 0x990 */ byte     field_990[0x120];
    /* 0xAB0 */ GpObj    field_AB0;
    /* 0xAD0 */ byte     field_AD0[0x120];
    /* 0xBF0 */ GpObj    field_BF0;
    /* 0xC10 */ byte     pad_C10[0x38];
    /// Saved at 0xC48 and copied over 0xC68 when
    /// `func_actor_401300_80139520` enters its state.
    /* 0xC48 */ MATRIX field_C48;
    /* 0xC68 */ MATRIX field_C68;
    /* 0xC88 */ byte   pad_C88[2];
    /* 0xC8A */ s16    field_C8A;
    /* 0xC8C */ byte   pad_C8C[0xC];
    /* 0xC98 */ s16    field_C98;
    /* 0xC9A */ byte   pad_C9A[6];
    /* 0xCA0 */ u16    field_CA0;
    /* 0xCA2 */ byte   pad_CA2[6];
    /// Copy of the first three bytes of the last event
    /// `func_actor_401300_80132554` handled.
    /* 0xCA8 */ u8   field_CA8[3];
    /* 0xCAB */ byte pad_CAB;
    /* 0xCAC */ s32  field_CAC;
    /* 0xCB0 */ s32  field_CB0;
    /* 0xCB4 */ byte pad_CB4[0x20];
    /// Player position and facing sent with message 0x3E9 by
    /// `func_actor_401300_80138800`.
    /* 0xCD4 */ VECTOR  field_CD4;
    /* 0xCE4 */ SVECTOR field_CE4;
    /* 0xCEC */ byte    pad_CEC[0x20];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xD0C */ Task* field_D0C;
    /* 0xD10 */ Task* field_D10;
    /* 0xD14 */ byte  pad_D14[0xC];
    /* 0xD20 */ s16   field_D20;
    /* 0xD22 */ s16   field_D22;
} Actor401300Work;

/// Animation view of the same work block, as `func_actor_401300_80133324`
/// reads it: the `Actor01900AnimWork` layout shifted 4 bytes later, like the
/// rest of this overlay's animation fields.
typedef struct Actor401300AnimWork {
    /* 0x000 */ byte       pad_0[0x20];
    /* 0x020 */ GpAnimCtx  anim;
    /* 0x034 */ GpAnimSlot slots[19];
    /* 0x32C */ byte       pad_32C[0x130];
    /* 0x45C */ GpAnimCtx  blendAnim;
    /* 0x470 */ GpAnimSlot blendSlots[19];
    /* 0x768 */ byte       pad_768[0x138];
    /* 0x8A0 */ s16        field_8A0;
    /* 0x8A2 */ s16        field_8A2;
    /* 0x8A4 */ byte       pad_8A4[2];
    /* 0x8A6 */ s16        field_8A6;
    /* 0x8A8 */ byte       pad_8A8[6];
    /* 0x8AE */ s16        field_8AE;
    /* 0x8B0 */ s16        field_8B0;
} Actor401300AnimWork;

/// Per-task actor context: `field_1C` is the work block (`Task::idMap`) and
/// `field_2C` is the actor's `TmdObject`. Same shape as `Actor01900`.
typedef struct Actor401300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor401300Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ byte             pad_30[6];
    /// High halfword of `Task::spawnArg1`; `func_actor_401300_8014192C` only
    /// stamps the -999 sentinel when it is 2.
    /* 0x36 */ s16 field_36;
} Actor401300;

/// Payload of the message `func_actor_401300_80141494` handles; `field_4`
/// selects the animation id written to `Actor401300Work::field_8A2`. Same
/// shape as `Actor01900Msg7D3`.
typedef struct Actor401300Msg {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u32 field_4;
} Actor401300Msg;

/// Event record `func_actor_401300_80132554` dispatches on: `w[0]` is the
/// event kind (0x301, 0xB05, 0x1D05) and `w[1]` its sub-code, and the first
/// three bytes are also copied raw into `Actor401300Work::field_CA8`.
typedef union Actor401300Event {
    u8  b[3];
    u16 w[2];
} Actor401300Event;

/// Height-clamp row `func_actor_401300_80132BE4` scans: `field_0` / `field_2`
/// are matched against `GameSessionFrom4::field_3` / `field_2`, and when a row
/// matches the coordinate's Y is clamped to [`lo`, `hi`]. Same shape as
/// `Actor01900HeightClamp`.
typedef struct Actor401300HeightClamp {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ s16  lo;
    /* 0x6 */ s16  hi;
    /* 0x8 */ byte pad_8[8];
} Actor401300HeightClamp;
STATIC_ASSERT_SIZEOF(Actor401300HeightClamp, 0x10);

extern Actor401300HeightClamp D_actor_401300_801589C8[];

/// Halfword table in the overlay's data; element 0 is the value the 0xB05/0xC
/// event writes into `GpEnemy::field_40`. Declared as an array: a scalar lets
/// the scheduler hoist its load above the preceding store.
extern u16 D_actor_401300_80141FA4[];

/// Per-animation reset argument for `func_800B4114`, indexed by the previous
/// and the new animation id (`field_8A0`, `field_8A2`).
extern s8 D_actor_401300_8015804C[][45];

/// Two rest/target rotation pairs `func_actor_401300_80133834` blends by
/// `0x200 - t` (in 1/512ths) into coord 7 and coord 8.
extern SVECTOR D_actor_401300_801589F8[2];
extern SVECTOR D_actor_401300_80158A08[2];

/// 0xC-byte `G_SCRATCH_HEAD` block `func_actor_401300_801397F8` borrows for
/// its player-in-radius test: the X/Z offset to the camera target and the
/// radius, each squared in place before `dx + dz < r`. Same shape as
/// `Actor01900RangeScratch`.
typedef struct Actor401300RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor401300RangeScratch;

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_401300_8013AAE8` takes: the
/// offset from the actor to the player, then the clamped turn applied to the
/// root coordinate. Same shape as `Actor01900AimScratch`.
typedef struct Actor401300AimScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     angle;
    /* 0xE */ s16     pad_E;
} Actor401300AimScratch;
STATIC_ASSERT_SIZEOF(Actor401300AimScratch, 0x10);

/// 0xC-byte `G_SCRATCH_HEAD` block `func_actor_401300_8013A208` takes: the
/// offset from the actor to the player, then the clamped turn. Same shape as
/// `Actor01900TurnScratch`.
typedef struct Actor401300TurnScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad_A;
} Actor401300TurnScratch;
STATIC_ASSERT_SIZEOF(Actor401300TurnScratch, 0xC);

/// 0x34-byte `G_SCRATCH_HEAD` block `func_actor_401300_80132910` takes to push
/// the root coordinate away from kind 0x10000 / 0x30000 records: `pos` is the
/// world translation, `offset` the push (clamped to length 0x140), `i` the
/// record cursor and `hit` the return value. Same shape as `Actor01900PushScratch`.
typedef struct Actor401300PushScratch {
    /* 0x00 */ SVECTOR offset;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ s32     kind;
    /* 0x14 */ s32     len;
    /* 0x18 */ s16     i;
    /* 0x1A */ s16     hit;
    /* 0x1C */ s16     dist[12];
} Actor401300PushScratch;
STATIC_ASSERT_SIZEOF(Actor401300PushScratch, 0x34);

/// 0x34-byte `G_SCRATCH_HEAD` block `Actor401300_RescaleYaw` builds its scaled
/// Y rotation in. Same shape as `Actor01900RotScratch`.
typedef struct Actor401300RotScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor401300RotScratch;
STATIC_ASSERT_SIZEOF(Actor401300RotScratch, 0x34);

extern MATRIX* D_80073B8C;

/// Movement freeze flag: `Actor401300_MoveForward` skips its step when it is 1.
extern u8  D_80072729;
extern u32 Gp_LcgState;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401300_8013B6E8`.
extern void* D_80114B78[1];

/// Overlay effect model data `func_actor_401300_8013B6E8` points
/// `D_80114B78` at before spawning.
extern char D_actor_401300_80147894;
extern char D_actor_401300_80148808;
extern char D_actor_401300_80148A14;
extern u8   D_801153F2[2];

/// Overlay-data word `func_actor_401300_801397F8` points
/// `D_actor_401300_80158878` at on entering its state.
extern s32  D_actor_401300_80152BB8;
extern s32* D_actor_401300_80158878;

s32 func_actor_401300_80132554(Actor401300* arg0, s32 arg1, Actor401300Event* arg2);

s32 func_actor_401300_80141494(Actor401300* arg0, s32 arg1, Actor401300Msg* arg2);

s32 func_actor_401300_80141614(Task* task, s32 arg1, ActorShared80169f74Placement* placement);

void func_actor_401300_80133A3C(Actor401300* arg0);

void func_actor_401300_8013B6E8(Actor401300* arg0);

void func_actor_401300_80141758(Task* task);

void func_actor_401300_8014192C(Actor401300* arg0);

void func_actor_401300_801419B8(Actor401300* arg0);

void func_actor_401300_80141A60(Actor401300* arg0);

void func_actor_401300_80141EF8(Task* task);

#endif // ACTOR_401300_H
