#include "actor_510900_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/acropolis_helicopter_landing_pad.h"
#include "../../shared/actor_messages.h"
#include "../../shared/golem_pawn_rook.h"
#include "../../shared/no9_golem.h"

s32 func_actor_510900_801391B8(Task*, s32, s32);

s32 func_actor_510900_8013BD5C(Task*);

s32 func_actor_510900_8013BD84(Task*, s32, AnimationPlayRequest*);

s32 func_actor_510900_8013BE64(Task*, s32, s32);

s16 func_actor_510900_8013BE84(Task*);

// Only the leading view ID is read; retain the following halfwords.
extern u16 D_actor_510900_80167CE4[4];

/// 0x10-byte scratch `func_actor_510900_80138F44` takes from the scratch stack
/// to rebuild the collision face this actor occupies. `center` starts as the
/// fixed local offset of the body's footprint and becomes that offset rotated
/// into world space, translated by the actor coordinate and clamped to the
/// grid; `rotated` receives each `rtv0` result in turn.
typedef struct Actor510900GridScratch {
    /* 0x00 */ SVECTOR center;
    /* 0x08 */ SVECTOR rotated;
} Actor510900GridScratch;
STATIC_ASSERT_SIZEOF(Actor510900GridScratch, 0x10);

/// `Task::work` of the child task `func_actor_510900_8013A85C` drives: an
/// animation context `Gp_AnimTickIndex` ticks slots 1..10 of, with a pair of
/// words past it. Below 2, `field_334` + 0xB is the game-flag nibble index
/// `field_336` is written to; otherwise `field_336` goes to the parent work's
/// `field_5C2`.
typedef struct Actor510900ChildAnim {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ AnimationSlot         slots[11];   ///< `func_800B3F84` arg4, reset 1..10
    /* 0x1CC */ byte                  poses[0xB0]; ///< `func_800B3F84` arg3
    /* 0x27C */ MATRIX                colorMtx;    ///< handed to `TmdObject::colorMtx`
    /* 0x29C */ MATRIX                lightMtx;    ///< handed to `TmdObject::lightMtx`
    /* 0x2BC */ WorldCollisionBody    obj2BC;
    /* 0x2DC */ WorldCollisionContact rec2DC;
    /* 0x2F4 */ WorldCollisionBody    obj2F4;
    /* 0x314 */ WorldCollisionContact rec314;
    /// Task the frame handler releases (state 2) once the camera has cut
    /// away from every view this actor is visible in.
    /* 0x32C */ struct Task* field_32C;
    /// State the frame handler below dispatches on: 0 waits for the grab, 1
    /// runs the `field_332` countdown, 2 is done.
    /* 0x330 */ s16 field_330;
    /* 0x332 */ s16 field_332;
    /* 0x334 */ s16 field_334;
    /* 0x336 */ s16 field_336;
} Actor510900ChildAnim;
STATIC_ASSERT_SIZEOF(Actor510900ChildAnim, 0x338);

/// 0x18 scratch block `func_actor_510900_8013A9BC` takes from the scratch stack
/// for the frame it starts the grab on; only the trailing `SVECTOR` is used,
/// as the spawn argument of both effects.
typedef struct Actor510900GrabScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR rot;
} Actor510900GrabScratch;
STATIC_ASSERT_SIZEOF(Actor510900GrabScratch, 0x18);

/// 0xD0-byte `Task::work` block `func_actor_510900_801397F0` allocates for its
/// child task: the child's colour and light matrices (handed to
/// `TmdObject::colorMtx` / `lightMtx`), two linked `WorldCollisionBody`s with their `WorldCollisionContact`
/// tables, and the timer/state trio `func_actor_510900_8013A100` runs its
/// teardown state machine on.
typedef struct Actor510900ChildFx {
    /* 0x00 */ MATRIX                colorMtx;
    /* 0x20 */ MATRIX                lightMtx;
    /* 0x40 */ WorldCollisionBody    obj40;
    /* 0x60 */ WorldCollisionContact rec60;
    /* 0x78 */ WorldCollisionBody    obj78;
    /* 0x98 */ WorldCollisionCapsule d4rec;
    /* 0xB0 */ WorldCollisionContact recB0;
    /* 0xC8 */ u16                   field_C8; ///< frame counter, reset at every state change
    /* 0xCA */ s16                   field_CA; ///< state: 0 fade in, 1 hold, 2 hit, 3 expire
    /* 0xCC */ s16                   field_CC;
    /* 0xCE */ s16                   field_CE;
} Actor510900ChildFx;
STATIC_ASSERT_SIZEOF(Actor510900ChildFx, 0xD0);

/// 0x28-byte scratch `func_actor_510900_80139C10` takes from the scratch stack
/// every frame the child effect turns: `rot` is the yaw it spins by (and then
/// the offset the trail effect is spawned along), `mtx` the rotation
/// `RotMatrix` builds from it and composes into the coordinate.
typedef struct Actor510900ChildFxTickScratch {
    /* 0x00 */ SVECTOR rot;
    /* 0x08 */ MATRIX  mtx;
} Actor510900ChildFxTickScratch;
STATIC_ASSERT_SIZEOF(Actor510900ChildFxTickScratch, 0x28);

/// 0x2C-byte scratch from the scratch stack used by `func_actor_510900_8013A310`:
/// the 0x3F8 query buffer followed by the `AnimationPlayRequest` it sends as message 0x3FF.
typedef struct Actor510900HitScratch {
    /* 0x00 */ GpDelayArg           query;
    /* 0x18 */ AnimationPlayRequest anim;
} Actor510900HitScratch;
STATIC_ASSERT_SIZEOF(Actor510900HitScratch, 0x2C);

/// Animation-set table handed to the player as the 0x3FF payload's `source.sets`.
extern AnimationSet* D_actor_510900_80167B2C[];

/// Spawn position of the child, indexed by its `Task::spawnArg1`.
extern SVECTOR D_actor_510900_80167CB8[];
/// Spawn rotation about Y, indexed the same way.
extern u16 D_actor_510900_80167CD0[];
/// Animation set table `func_800B3F84` installs in the context above.
extern AnimationSet* D_actor_510900_80167CAC[];

/// `Actor510900ChildFx::field_CE` per 1000 units of distance between the child
/// and the player, clamped to the last entry.
extern u16 D_actor_510900_80167C94[12];

/// 0x7C-byte `Task::work` block `func_actor_510900_8013AD90` allocates: two
/// linked `WorldCollisionBody`s, each with its one-entry `WorldCollisionContact` table.
/// `func_actor_510900_8013C430` unlinks both.
typedef struct Actor510900ChildWork {
    /* 0x00 */ WorldCollisionBody    obj0;
    /* 0x20 */ WorldCollisionContact rec20;
    /* 0x38 */ WorldCollisionBody    obj38;
    /* 0x58 */ WorldCollisionContact rec58;
    /* 0x70 */ Task*                 field_70; // released (state 3) on a view change
    /* 0x74 */ s16                   field_74; // row of `D_actor_510900_80167CEC`
    /* 0x76 */ s16                   field_76; // countdown, decremented on a view change
    /* 0x78 */ s16                   field_78; // state handed to `field_70` when the grab lands
    /* 0x7A */ byte                  pad_7A[0x2];
} Actor510900ChildWork;
STATIC_ASSERT_SIZEOF(Actor510900ChildWork, 0x7C);

/// Table the state 1 handler below picks `field_59C` from; a 4-bit
/// `gRandomLcgState` draw indexes at least sixteen `u16` entries.
extern u16 D_actor_510900_801679F0[];

/// Table the 0x14-animation state picks `field_59C` from, indexed by a 4-bit
/// `gRandomLcgState` draw the same way `D_actor_510900_801679F0` is.
extern u16 D_actor_510900_801679D0[];

/// Per-cycle threshold the patrol walk in `func_actor_510900_80136B70` compares
/// a 4-bit `gRandomLcgState` draw against, indexed by the `field_59C` cycle counter;
/// a draw above the entry ends the walk.
extern s16 D_actor_510900_80167A10[];

/// Per-animation-id value `func_actor_510900_8013BB20` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 D_actor_510900_80167B38[];

/// One corner of the square `func_actor_510900_80138978` walks the actor
/// around, in world x/z.
typedef struct Actor510900PatrolCorner {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor510900PatrolCorner;
STATIC_ASSERT_SIZEOF(Actor510900PatrolCorner, 0x4);

/// The unit direction the side leaving that corner runs in.
typedef struct Actor510900PatrolStep {
    /* 0x0 */ s8 x;
    /* 0x1 */ s8 z;
} Actor510900PatrolStep;
STATIC_ASSERT_SIZEOF(Actor510900PatrolStep, 0x2);

/// The four corners of the patrol square, indexed by `Actor510900Work::field_5A8`.
extern Actor510900PatrolCorner D_actor_510900_80167B84[4];

/// The direction of each of its four sides, indexed the same way.
extern Actor510900PatrolStep D_actor_510900_80167B94[4];

/// `func_800FDB18` argument record the state-3 handler refreshes every sixth
/// frame from the player's model coordinates.
extern EffectSpawnArg D_actor_510900_80167B7C;

/// The yaws `func_actor_510900_801387F4` turns the actor's coordinate
/// towards, indexed by `Actor510900Work::field_5A8` (one entry further on
/// while `field_5AE` is below 0x3E8).
extern u16 D_actor_510900_80167B9C[];

/// One of the four world-space x/z boxes `func_actor_510900_8013864C` tests the
/// player against; the index of the box containing them is latched into
/// `Actor510900Work::field_5AA`.
typedef struct Actor510900PatrolBox {
    /* 0x0 */ s16 minX;
    /* 0x2 */ s16 maxX;
    /* 0x4 */ s16 minZ;
    /* 0x6 */ s16 maxZ;
} Actor510900PatrolBox;
STATIC_ASSERT_SIZEOF(Actor510900PatrolBox, 0x8);

/// The four boxes, in the same order as the patrol square's corners.
extern Actor510900PatrolBox D_actor_510900_80167BA4[4];

/// The three face normals `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->normals`, restoring the collision grid this actor edited.
extern SVECTOR D_actor_510900_80167BC4[3];

/// The twelve face corners `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->vertices`.
extern SVECTOR D_actor_510900_80167BDC[12];

/// The three `WorldCollisionGridFace` records `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->faces`.
extern WorldCollisionGridFace D_actor_510900_80167C3C[3];

/// The extra face normal `func_actor_510900_8013B424` installs as
/// `Gp_GridParams->normals[3]` while the actor's own face is in the grid.
extern SVECTOR D_actor_510900_80167C60;

/// The four face corners that face uses, copied into
/// `Gp_GridParams->vertices[12..15]`.
extern SVECTOR D_actor_510900_80167C68[4];

/// The `WorldCollisionGridFace` record for that face, copied into
/// `Gp_GridParams->faces[3]`.
extern WorldCollisionGridFace D_actor_510900_80167C88;

static s32  func_actor_510900_8013691C(Task* arg0);
static void func_actor_510900_8013864C(Task* arg0);
static void func_actor_510900_801387F4(Task* arg0);
static void func_actor_510900_80138978(Task* arg0);
static void func_actor_510900_80138A9C(Task* arg0);
static void func_actor_510900_80138D38(Task* arg0);
static void func_actor_510900_80138F44(Task* arg0);
static void func_actor_510900_8013B6A0(Enemy* arg0, Task* arg1);
static void func_actor_510900_8013B804(Task* arg0);
static void func_actor_510900_8013B870(Task* arg0);
static void func_actor_510900_8013B988(Task* arg0);
static void func_actor_510900_8013BA58(Task* arg0);
static void func_actor_510900_8013BB20(Task* arg0);
static void func_actor_510900_8013BC80(Task* arg0);

static void func_actor_510900_8013C380(Task* arg0);
static void func_actor_510900_8013C430(Task* arg0);

/// The script block pair `Gp_SpawnScript18` is handed at blend 0x58; both live
/// in the room overlay, not here.

/// `field_59C` reload tables, indexed by four bits of `gRandomLcgState`.
extern s16 D_actor_510900_80167990[];
extern s16 D_actor_510900_801679B0[];

void func_actor_510900_8013B3D0(Task*);
void func_actor_510900_8013BE98(Task*);
void func_actor_510900_8013BF90(Task*);
void func_actor_510900_8013C090(Task*);
void func_actor_510900_8013C190(Task*);
void func_actor_510900_8013C1EC(Task*);
void func_actor_510900_8013C3DC(Task*);

DamageAttack D_actor_510900_8016796C[5] = {
    { 24, 6 },
    { 20, 7 },
    { 999, 6 },
    { 0, 2 },
    { 1, 6 },
};

EnemyParams D_actor_510900_80167980 = { &D_actor_510900_80167968, 1600, 500, 800, 30, 50, 3, 0, 0 };

s16 D_actor_510900_80167990[16] = {
    2000,
    2100,
    2200,
    2300,
    2400,
    2500,
    2600,
    2700,
    2800,
    2900,
    3000,
    3200,
    3400,
    3600,
    3800,
    4000,
};

s16 D_actor_510900_801679B0[16] = {
    14,
    16,
    18,
    20,
    22,
    24,
    26,
    28,
    30,
    32,
    35,
    40,
    45,
    50,
    55,
    60,
};

u16 D_actor_510900_801679D0[16] = {
    5,
    5,
    5,
    5,
    10,
    10,
    10,
    10,
    15,
    15,
    15,
    15,
    20,
    20,
    30,
    30,
};

u16 D_actor_510900_801679F0[16] = {
    15,
    15,
    15,
    15,
    25,
    25,
    25,
    25,
    35,
    35,
    35,
    35,
    45,
    45,
    45,
    45,
};

s16 D_actor_510900_80167A10[4] = {
    14,
    12,
    10,
    0,
};

TaskDesc D_actor_510900_80167A18[7] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013B3D0, { .model = &D_actor_510900_80141B64 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013BE98, { .model = &D_actor_510900_80142124 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013BF90, { .model = &D_actor_510900_80141E08 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013C090, { .model = &D_actor_510900_80142480 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013C190, { .model = &D_actor_510900_801427AC } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013C1EC, { .model = &D_actor_510900_8014313C } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_510900_8013C3DC, { .value = 0 } },
};

Actor510900MessageEntry D_actor_510900_80167A6C[7] = {
    { 2014, { .call1 = func_actor_510900_8013BD5C } },
    { 2003, { .call2 = func_actor_510900_8013BD84 } },
    { 2004, { .call3 = actorMsgPlaceRotMatrix } },
    { 2005, { .call4 = func_actor_510900_8013BE64 } },
    { 2007, { .call4 = func_actor_510900_801391B8 } },
    { 2006, { .call0 = func_actor_510900_8013BE84 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

u8 D_actor_510900_80167AA4[136] = {
    0,
    0,
    0,
    0,
    224,
    58,
    20,
    128,
    216,
    61,
    20,
    128,
    208,
    71,
    20,
    128,
    164,
    86,
    20,
    128,
    192,
    96,
    20,
    128,
    108,
    108,
    20,
    128,
    252,
    144,
    20,
    128,
    216,
    154,
    20,
    128,
    0,
    165,
    20,
    128,
    124,
    169,
    20,
    128,
    112,
    192,
    20,
    128,
    180,
    199,
    20,
    128,
    200,
    221,
    20,
    128,
    200,
    233,
    20,
    128,
    136,
    246,
    20,
    128,
    140,
    3,
    21,
    128,
    156,
    13,
    21,
    128,
    124,
    33,
    21,
    128,
    92,
    39,
    21,
    128,
    4,
    50,
    21,
    128,
    196,
    53,
    21,
    128,
    148,
    62,
    21,
    128,
    24,
    66,
    21,
    128,
    72,
    95,
    21,
    128,
    8,
    110,
    21,
    128,
    200,
    133,
    21,
    128,
    216,
    145,
    21,
    128,
    136,
    190,
    21,
    128,
    32,
    214,
    21,
    128,
    252,
    7,
    22,
    128,
    36,
    218,
    21,
    128,
    224,
    66,
    22,
    128,
    204,
    113,
    22,
    128,
};

AnimationSet* D_actor_510900_80167B2C[3] = {
    NULL,
    &D_actor_510900_801597B4,
    &D_actor_510900_80159DFC,
};

s16 D_actor_510900_80167B38[34] = {
    0,
    8,
    8,
    4,
    4,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    4,
    3,
    3,
    3,
    3,
    0,
    8,
    4,
    4,
    8,
    8,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

EffectSpawnArg D_actor_510900_80167B7C = { NULL, 300, 1 };

Actor510900PatrolCorner D_actor_510900_80167B84[4] = {
    { -6600, -6600 },
    { 6600, -6600 },
    { 6600, 6600 },
    { -6600, 6600 },
};

Actor510900PatrolStep D_actor_510900_80167B94[4] = {
    { 1, 0 },
    { 0, 1 },
    { -1, 0 },
    { 0, -1 },
};

u16 D_actor_510900_80167B9C[4] = {
    1024,
    0,
    3072,
    2048,
};

Actor510900PatrolBox D_actor_510900_80167BA4[4] = {
    { -7200, 7200, -7200, -6000 },
    { 6000, 7200, -6000, 7200 },
    { -7200, 6000, 6000, 7200 },
    { -7200, -4608, -6000, 6000 },
};

SVECTOR D_actor_510900_80167BC4[3] = {
#include "assets/actor_510900_collision_35DA4.inc"
};

SVECTOR D_actor_510900_80167BDC[12] = {
#include "assets/actor_510900_collision_35DBC.inc"
};

WorldCollisionGridFace D_actor_510900_80167C3C[3] = {
#include "assets/actor_510900_collision_35E1C.inc"
};

SVECTOR D_actor_510900_80167C60 = { -4096, 0, 0, 0 };

SVECTOR D_actor_510900_80167C68[4] = {
    { -5952, -200, -1248, 0 },
    { -5952, -200, -2500, 0 },
    { -5952, 0, -1248, 0 },
    { -5952, 0, -2500, 0 },
};

WorldCollisionGridFace D_actor_510900_80167C88 = { { 12, 13, 14, 15 }, 3, 0 };

u16 D_actor_510900_80167C94[12] = {
    25,
    20,
    15,
    12,
    10,
    9,
    8,
    7,
    6,
    5,
    5,
    5,
};

AnimationSet* D_actor_510900_80167CAC[3] = {
    NULL,
    &D_actor_510900_80167294,
    &D_actor_510900_80167940,
};

SVECTOR D_actor_510900_80167CB8[3] = {
    { 6458, -2658, 535, 0 },
    { -1336, -2658, 6552, 0 },
    { -6441, -2658, -100, 0 },
};

u16 D_actor_510900_80167CD0[4] = {
    2048,
    1024,
    0,
    0,
};

u16 D_actor_510900_80167CD8[2][3] = {
    { 5, 6, 7 },
    { 7, 8, 9 },
};

u16 D_actor_510900_80167CE4[4] = {
    10,
    10,
    10,
    0,
};

u16 D_actor_510900_80167CEC[7][4] = {
    { 0, 1, 2, 6 },
    { 0, 1, 2, 6 },
    { 0, 4, 4, 6 },
    { 0, 4, 4, 6 },
    { 0, 5, 5, 6 },
    { 0, 5, 5, 6 },
    { 0, 0, 0, 0 },
};

static void func_actor_510900_8013A310(Task* task);

static void func_actor_510900_8013A9BC(Task* task);

static s32 func_actor_510900_8013C240(Task* task);

static void func_actor_510900_8013C338(Task* arg0, GfxCoord* arg1);

static void func_actor_510900_8013B0D8(Task* arg0);

/// View index the child keeps running in; any other view parks it.
/// Game-flag nibble 0xD values, indexed by the child's `field_74` and the
/// parent work's `field_5C2`.
extern u16 D_actor_510900_80167CEC[][4];

static void func_actor_510900_8013B658(Enemy* arg0, Task* arg1);

static void func_actor_510900_8013BEEC(Enemy* enemy, Task* task);

static void func_actor_510900_8013BFE4(Enemy* enemy, Task* task);

static void func_actor_510900_8013C034(Enemy* enemy, Task* task);

static void func_actor_510900_8013C0E4(Enemy* enemy, Task* task);

static void func_actor_510900_8013C134(Enemy* enemy, Task* task);

/// The three views the child is visible in, indexed by its `field_334`.
extern u16 D_actor_510900_80167CD8[][3];

static void func_actor_510900_80135744(Task* arg0);
static void func_actor_510900_80135E90(Task* arg0);
static void func_actor_510900_80136184(Task* arg0);
static void func_actor_510900_80136B70(Task* arg0);
static void func_actor_510900_80137008(Task* arg0);
static void func_actor_510900_801373B8(Task* arg0);
static void func_actor_510900_801375D8(Task* arg0);
static void func_actor_510900_80137868(Task* arg0);
static void func_actor_510900_80137E20(Task* arg0);
static void func_actor_510900_80137FBC(Task* arg0);
static void func_actor_510900_80138250(Task* arg0);
static void func_actor_510900_801384C4(Task* arg0);
static void func_actor_510900_801395AC(Enemy* enemy, Task* task);
static void func_actor_510900_801397F0(Enemy* arg0, Task* arg1);
static void func_actor_510900_80139C10(Enemy* enemy, Task* task);
static void func_actor_510900_8013A100(Enemy* enemy, Task* task);
static void func_actor_510900_8013A5B8(Enemy* enemy, Task* task);
static void func_actor_510900_8013A85C(Enemy* arg0, Task* arg1);
static void func_actor_510900_8013AD90(Enemy* enemy, Task* task);
static void func_actor_510900_8013AF38(Enemy* arg0, Task* arg1);

/// Applies this frame's hits from the three `rec49C` collision records. A
/// type-2 id lands only while the `field_58C` cooldown is clear: its damage
/// is halved for 0x8000 ids, otherwise scaled by the player's distance and
/// doubled/quadrupled by the id's class and `Gp_RollEnemyChance`, and may pick
/// a flinch (`reaction`) that sets the next handler. Type-5 ids apply the
/// `Gp_LookupIdField` table damage directly.
static void func_actor_510900_80135744(Task* arg0)
{
    s32              lastId;
    VECTOR*          d;
    Actor510900Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              reaction;
    s32              param;
    s32              i;
    s16              dmg;
    s16              hit;
    s32              full;
    s16              loss;
    s32              sound;
    s32              pan;
    s16              wait;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(VECTOR));
    d        = SCRATCH_STACK_CURSOR(VECTOR);
    coord    = arg0->extra.tmd->coords;
    work     = arg0->work;
    enemy    = arg0->spawnArg2.pointer;
    reaction = 0;
    lastId   = 0;
    if (work->field_58C != 0) {
        if (--work->field_58C <= 0) {
            work->field_58C = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        switch ((u16)(work->rec49C[i].key.value >> 16)) {
            case 0:
            case 1:
            case 3:
            case 4:
                break;
            case 2:
                if (work->field_58C != 0) {
                    break;
                }
                param = Gp_GetIdParam0(work->rec49C[i].key.value);
                if (work->rec49C[i].key.value & 0x8000) {
                    if ((u8)work->rec49C[i].key.value - 1 < 6U) {
                        reaction = 2;
                    }
                    dmg = (s16)Gp_ComputeDamage(work->rec49C[i].key.value, 0, 0, 0) >> 1;
                    func_800E2C78(arg0->spawnArg2.pointer, work->rec49C[i].key.value, dmg, 0);
                } else {
                    d->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                    d->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                    d->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                    full  = Gp_ComputeDamage(work->rec49C[i].key.value, SquareRoot0(d->vx * d->vx + d->vy * d->vy + d->vz * d->vz), 0, 0);
                    dmg   = full;
                    if ((u16)param == 5) {
                        dmg = full * 2;
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 2, NULL);
                    }
                    if (Gp_RollEnemyChance(enemy, work->rec49C[i].key.value, 0) != 0) {
                        dmg *= 4;
                        if ((u16)param != 5) {
                            Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
                        }
                        if (work->field_5B8 == 0) {
                            reaction = 1;
                        }
                    }
                }
                switch ((u16)param) {
                    case 0:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                    case 1:
                        if (work->field_5B8 == 0) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            if ((gRandomLcgState >> 16) & 1) {
                                reaction = 1;
                            }
                        }
                        break;
                    case 2:
                        if (work->field_5B4 == 0) {
                            Gp_SetObjFlag2(enemy, work->rec49C[i].key.value, 0);
                        }
                        break;
                    case 3:
                        if (work->field_5B8 == 0) {
                            reaction = 2;
                        }
                        break;
                    case 4:
                        dmg = dmg * 75 / 100;
                        break;
                }
                enemy->hp -= dmg;
                func_800DA6E8(&enemy->node, dmg, 0);
                if (enemy->hp <= 0) {
                    if (work->field_5B4 < 2) {
                        work->field_5B4 = 0;
                    } else {
                        enemy->hp = 1;
                    }
                }
                if (reaction != 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780004;
                    } else {
                        sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780005;
                    }
                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                if (work->field_578 != 0) {
                    SndEvt_EnqueueType7(work->field_578, 0);
                    work->field_578 = 0;
                }
                if (work->field_5B4 == 1) {
                    reaction = 0;
                }
                switch (reaction) {
                    case 0: {
                        u32 rng;
                        u32 r;
                        s32 v;
                        s32 r2;
                        s32 v2;

                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        r               = rng >> 16;
                        v               = (r & 0x7F) + 0x40;
                        gRandomLcgState = rng;
                        if (!(r & 1)) {
                            v = -v;
                        }
                        work->field_570.vx = v;
                        r2                 = (s16)r >> 8;
                        v2                 = (r2 & 0x7F) + 0x40;
                        if (!(r2 & 1)) {
                            v2 = -v2;
                        }
                        work->field_570.vy = v2;
                        work->field_584    = 1;
                        if (enemy->hp <= 0) {
                            work->field_58E     = 0xC;
                            work->field_590     = 0;
                            work->field_586     = 0x18;
                            work->obj47C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                    }
                    case 1:
                        work->field_58E     = 8;
                        work->field_590     = 0;
                        work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (enemy->hp <= 0) {
                            work->obj47C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                    case 2:
                        work->field_58E     = 9;
                        work->field_590     = 0;
                        work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (enemy->hp <= 0) {
                            work->obj47C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                }
                if (lastId != work->rec49C[i].key.value) {
                    lastId = work->rec49C[i].key.value;
                    func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, &arg0->extra.tmd->coords[3], NULL, &work->field_53C);
                }
                wait = Gp_GetIdParam2(work->rec49C[i].key.value);
                if (wait > 0) {
                    work->field_58C = wait;
                }
                break;
            case 5:
                hit = 0;
                switch ((u32)(u16)work->rec49C[i].key.value) {
                    case 2:
                        hit             = 1;
                        work->field_58E = 10;
                        work->field_590 = 0;
                        break;
                    case 3:
                        hit             = 1;
                        work->field_58E = 9;
                        work->field_590 = 0;
                        break;
                    case 4:
                        hit             = 1;
                        work->field_58E = 11;
                        work->field_590 = 0;
                        break;
                }
                if (hit) {
                    work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    loss                = Gp_LookupIdField((u16)work->rec49C[i].key.value, 1);
                    enemy->hp          -= loss;
                    func_800DA6E8(&enemy->node, loss, 0);
                    if (enemy->hp <= 0) {
                        if (work->field_5B4 < 2) {
                            work->field_5B4     = 0;
                            work->obj47C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        } else {
                            enemy->hp = 1;
                        }
                    }
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->rec49C);
    if (work->rec524[0].flags & 1) {
        if ((work->rec524[0].key.value & 0xFFFF0000) == 0x10000) {
            work->field_5B2     = 1;
            work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
        Gp_ClearRec18Occupied(work->rec524);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// State 0 (animations 0xA/8/0x19): the idle-to-walk start. Sub-state 0 queues
/// the step cue at blend 0xA and past 0x64 hands sub-state 1 the animation 0xA;
/// sub-state 1 pushes both body objects into their flagged pose with the
/// animation 8 past blend 0x14. Sub-states 2 and 3 are the two walk cycles:
/// each feeds `field_5A2` 0x38 over a 0x13-blend window, mutes it while
/// `field_5B2` is latched, queues the footfall cue, and at the end of the cycle
/// either hands on to the next one or - when `field_5B2` is set - clears both
/// flagged poses and restarts the cycle with the animation 0x19. Sub-state 4
/// leaves for state 3 with the animation 0xF.
static void func_actor_510900_80135E90(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_590) {
        case 0:
            if (work->field_58A == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780006;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x64) {
                work->field_590 = 1;
                work->field_586 = 0xA;
            }
            break;
        case 1:
            if (work->field_58A >= 0x14) {
                work->field_590     = 2;
                work->field_586     = 8;
                work->obj4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj504.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                = Gp_PackPair(&D_actor_510900_80167968, 2);
                work->obj4E4.key    = pair;
                work->obj504.key    = pair;
            }
            break;
        case 2:
            work->field_5A2 = ((u32)((u16)work->field_58A - 9) < 0x13U) ? 0x38 : 0;
            if (work->field_5B2 != 0) {
                work->field_5A2 = 0;
            }
            if (work->field_58A == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000B;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x1D) {
                if (work->field_5B2 != 0) {
                    work->field_58E     = 2;
                    work->field_590     = 3;
                    work->field_586     = 0x19;
                    work->field_5B2     = 0;
                    work->field_598     = 0;
                    work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    work->field_590 = 3;
                    work->field_586 = 9;
                }
            }
            break;
        case 3:
            work->field_5A2 = ((u32)((u16)work->field_58A - 5) < 0x13U) ? 0x38 : 0;
            if (work->field_5B2 != 0) {
                work->field_5A2 = 0;
            }
            if (work->field_58A == 6) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000B;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x27) {
                if (work->field_5B2 != 0) {
                    work->field_58E     = 2;
                    work->field_590     = 3;
                    work->field_586     = 0x19;
                    work->field_5B2     = 0;
                    work->field_598     = 0;
                    work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    work->field_590 = 4;
                }
            }
            break;
        case 4:
            work->field_586 = 0xF;
            work->field_58E = 3;
            work->field_590 = 1;
            break;
    }
}

/// Sub-state machine on `field_590`. Off the corner (`field_5A8` != `field_5AA`)
/// a player range `field_5AE` of 0xBB8 or more forces sub-state 4, except from
/// 8. Sub-state 0 counts `field_59C` down and, unless
/// `func_actor_510900_8013691C` hands over, picks 1 or 5 from that range;
/// 2, 3 and 6 drain `field_59C` by 0x1D a frame (3 also spawns an effect every
/// 0x28 frames through `field_59E`). Each reload of `field_59C` is a 4-bit
/// `gRandomLcgState` draw from `D_actor_510900_80167990` or `D_actor_510900_801679B0`.
static void func_actor_510900_80136184(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    EffectWork*      eff;
    s32              snd;
    s32              rng;
    s16              speed;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_5AA != work->field_5A8 && work->field_590 != 8 && work->field_5AE >= 0xBB8) {
        work->field_590 = 4;
    }
    switch (work->field_590) {
        case 0:
            work->field_5A2 = 0;
            work->field_5B2 = 0;
            if (--work->field_59C <= 0) {
                if (work->field_594 == 2) {
                    work->field_594 = 1;
                }
                if (func_actor_510900_8013691C(arg0) == 0) {
                    if (work->field_5AE < 0x7D0) {
                        work->field_590 = 1;
                        work->field_586 = 2;
                        work->field_59C = 0;
                    } else if (((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.targeted == 1 && (u32)(gPlayerStatus.weaponSlotItem - 0xA) < 3U) {
                        work->field_590 = 5;
                        work->field_586 = 0x15;
                        snd             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780003;
                        SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                            (s8)worldCoordGetOriginAudioDepth(coord));
                    } else {
                        work->field_590 = 1;
                        work->field_586 = 2;
                        work->field_59C = 1;
                    }
                }
            }
            break;
        case 1:
            work->field_5A2 = 0;
            if (work->field_58A >= 0xD) {
                if (work->field_59C == 0) {
                    work->field_590 = 2;
                    work->field_586 = 3;
                } else {
                    work->field_590 = 3;
                    work->field_586 = 4;
                    work->field_59E = 0x1D;
                }
                work->field_59C = D_actor_510900_80167990[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            }
            break;
        case 2:
            work->field_5A2  = 0x1D;
            work->field_59C -= 0x1D;
            if (work->field_59C <= 0 || work->field_5A6 > 0xB478U) {
                work->field_586 = 1;
                work->field_590 = 0;
                work->field_59C = D_actor_510900_801679B0[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            }
            if (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU &&
                work->field_5A6 > 0xB477U) {
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            break;
        case 3:
            work->field_5A2  = 0x1D;
            work->field_59C -= 0x1D;
            if (--work->field_59E == 0) {
                eff = Gp_SpawnEff(0x80060044, &arg0->extra.tmd->coords[8], 0, NULL);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
                work->field_59E = 0x28;
                snd             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000E;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.targeted == 1 && (u32)(gPlayerStatus.weaponSlotItem - 0xA) < 3U) {
                work->field_590 = 6;
                work->field_586 = 0x15;
                snd             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780003;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_59C <= 0 || work->field_5A6 > 0xB478U) {
                work->field_586 = 1;
                work->field_590 = 0;
                work->field_59C = D_actor_510900_801679B0[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            }
            if (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU &&
                work->field_5A6 > 0xB477U) {
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            break;
        case 4:
            speed           = 0;
            work->field_586 = 6;
            if (work->field_58A >= 8) {
                speed = 0xA0;
            }
            work->field_5A2 = speed;
            if (work->field_5AE < 0x5DC) {
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            if (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU &&
                work->field_5A6 > 0xB477U) {
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            break;
        case 5:
            if (work->field_58A >= 0xF) {
                work->field_590 = 6;
                work->field_586 = 0x16;
                work->field_59C = D_actor_510900_80167990[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            }
            break;
        case 6:
            work->field_5A2  = 0x1D;
            work->field_59C -= 0x1D;
            if (work->field_59C <= 0 || work->field_5A6 > 0xB478U ||
                (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU)) {
                work->field_590 = 7;
                work->field_586 = 0x17;
            }
            break;
        case 7:
            work->field_5A2 = 0;
            if (work->field_58A >= 0xD) {
                work->field_590 = 0;
                work->field_586 = 1;
                if (work->field_5AE >= 0x7D0 && work->field_5AC < 0x3E8 && work->field_5A6 <= 0xAE9CU &&
                    work->field_5A6 > 0xB477U) {
                    work->field_59C = 0;
                } else {
                    work->field_59C = D_actor_510900_801679B0[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                }
            }
            break;
        case 8:
            if (work->field_5B0 < 0xC8 || work->field_5AA != work->field_5A8 || work->field_58A < 0xB ||
                work->field_58A >= 0x18) {
                work->field_5A2 = 0;
            } else {
                work->field_5A2 = -0xA7;
            }
            if (work->field_58A == 0x19) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780008;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x32) {
                work->field_58E = 1;
                work->field_590 = 0;
                work->field_586 = 1;
                work->field_59C = 0;
            }
            break;
    }
}

/// Picks the handler the patrol hands over to, from the distance `field_5A6`
/// walked around the patrol square and the player's facing. Two spots on the
/// lap - 0x4B00 and 0x7F00, each with a 0x120 window - fire once apiece through
/// the `field_5BE` latch and send the actor into state 2; past 0xB477 the lap is
/// over and state 6 takes it. Otherwise, only on the frame the corner has been
/// reached (`field_5A8` == `field_5AA`), the player's range `field_5AC` and
/// whether they face the actor choose between states 2..5, each with the
/// animation the handler starts on. Returns 1 when a handler was selected.
static s32 func_actor_510900_8013691C(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              diff;
    s32              ret;
    s16              dist;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_5BE == 0) {
        diff = work->field_5A6 - 0x4B00;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < 0x120) {
            work->field_5BE = 1;
            work->field_58E = 2;
            work->field_590 = 0;
            work->field_586 = 0xA;
            return 1;
        }
        diff = work->field_5A6 - 0x7F00;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < 0x120) {
            work->field_5BE = 2;
            work->field_58E = 2;
            work->field_590 = 0;
            work->field_586 = 0xA;
            return 1;
        }
    }
    if (work->field_5A6 > 0xB477U) {
        work->field_58E = 6;
        work->field_590 = 0;
        work->field_586 = 0xE;
        return 1;
    }
    if (work->field_5A6 > 0xAE9CU) {
        return 0;
    }
    if (work->field_5A8 != work->field_5AA) {
        return 0;
    }
    if (gPlayerStatus.coordMtx->m[0][2] * coord->coord.m[0][2] +
            gPlayerStatus.coordMtx->m[2][2] * coord->coord.m[2][2] <
        0) {
        dist = work->field_5AC;
        if (dist < 0xAF0) {
            ret             = 1;
            work->field_58E = 2;
            work->field_590 = 0;
            work->field_586 = 0xA;
            goto done;
        }
        if (dist < 0xED8) {
            ret             = 1;
            work->field_58E = 3;
            work->field_590 = 0;
            work->field_586 = 0xE;
            goto done;
        }
        if (dist < 0x12C0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 0x10) & 1) {
                work->field_58E = 3;
                work->field_590 = 0;
                work->field_586 = 0xE;
            } else {
                work->field_58E = 4;
                work->field_590 = 0;
                work->field_586 = 0x1B;
            }
        } else {
            ret = 0;
            if (work->field_5BA == 0) {
                ret             = 1;
                work->field_58E = 4;
                work->field_590 = 0;
                work->field_586 = 0x1B;
            }
            goto done;
        }
    } else {
        if (work->field_5AC < 0xAF0) {
            work->field_58E = 2;
            work->field_590 = 0;
            work->field_586 = 0xA;
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 0x10) & 0xF) < 0xAU) {
                work->field_58E = 4;
                work->field_590 = 0;
                work->field_586 = 0x1B;
            } else {
                work->field_58E = 5;
                work->field_590 = 0;
                work->field_586 = 0xB;
            }
        }
    }
    ret = 1;
done:
    return ret;
}

/// State 2 (sub-states 0..3): the patrol walk. Sub-state 0 stops the actor and
/// hands sub-state 1 the animation 8 past blend 0x14. Sub-states 1 and 2 are
/// the two walk cycles: each feeds `field_5A2` 0x38 over a 0x13-blend window,
/// mutes it while `field_5B2` is latched and queues the footfall cue; sub-state
/// 1 also arms `field_5C0` when a grab candidate is pending and pushes both
/// body objects into their flagged pose at blend 9. At the end of a cycle a
/// 4-bit `gRandomLcgState` draw against `D_actor_510900_80167A10[field_59C]`
/// decides whether to walk another cycle - bumping `field_59C`, which sub-state
/// 2 caps at three - or to leave for state 1 with a fresh `field_59C` from
/// `D_actor_510900_801679D0`. A latched `field_5B2` instead sends sub-state 3,
/// the turn, with the animation 0x19; it queues its cue at blend 0x2D and past
/// 0x5A leaves the same way.
static void func_actor_510900_80136B70(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_590) {
        case 0:
            work->field_5A2 = 0;
            if (work->field_58A >= 0x14) {
                work->field_590 = 1;
                work->field_586 = 8;
                work->field_59C = 0;
            }
            break;
        case 1:
            work->field_5A2 = ((u32)((u16)work->field_58A - 9) < 0x13U) ? 0x38 : 0;
            if (work->field_5B2 != 0) {
                work->field_5A2 = 0;
            }
            if (work->field_58A == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000B;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((work->field_5BE > 0) && (work->field_58A == 8)) {
                work->field_5C0 = 1;
            }
            if (work->field_58A == 9) {
                work->obj4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj504.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                = Gp_PackPair(&D_actor_510900_80167968, 2);
                work->obj4E4.key    = pair;
                work->obj504.key    = pair;
            }
            if (work->field_58A >= 0x1D) {
                if (work->field_5B2 != 0) {
                    work->field_590     = 3;
                    work->field_586     = 0x19;
                    work->field_5B2     = 0;
                    work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (D_actor_510900_80167A10[work->field_59C] < (s32)((gRandomLcgState >> 0x10) & 0xF)) {
                        u16* tbl            = D_actor_510900_801679D0;
                        work->field_58E     = 1;
                        work->field_586     = 1;
                        work->field_590     = 0;
                        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->field_59C     = tbl[(gRandomLcgState >> 0x10) & 0xF];
                        work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    } else {
                        work->field_590 = 2;
                        work->field_586 = 9;
                        work->field_59C++;
                    }
                }
            }
            break;
        case 2:
            work->field_5A2 = ((u32)((u16)work->field_58A - 5) < 0x13U) ? 0x38 : 0;
            if (work->field_5B2 != 0) {
                work->field_5A2 = 0;
            }
            if (work->field_58A == 6) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000B;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x27) {
                if (work->field_5B2 != 0) {
                    work->field_590     = 3;
                    work->field_586     = 0x19;
                    work->field_5B2     = 0;
                    work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    if ((D_actor_510900_80167A10[work->field_59C] <
                         (s32)(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF)) ||
                        (work->field_59C >= 3)) {
                        u16* tbl            = D_actor_510900_801679D0;
                        work->field_58E     = 1;
                        work->field_590     = 0;
                        work->field_586     = 1;
                        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->field_59C     = tbl[(gRandomLcgState >> 0x10) & 0xF];
                        work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    } else {
                        work->field_59C++;
                        work->field_590 = 1;
                        work->field_586 = 8;
                    }
                }
            }
            break;
        case 3:
            if (work->field_58A == 0x2D) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000A;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x5A) {
                u16* tbl        = D_actor_510900_801679D0;
                work->field_58E = 1;
                work->field_590 = 0;
                work->field_586 = 1;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_59C = tbl[(gRandomLcgState >> 0x10) & 0xF];
            }
            break;
    }
}

/// State 3 (sub-state 0/1): the wind-up. Sub-state 0 queues the two-part
/// charge sound at blend 0x39 - setting `field_594`/`field_598` and pushing
/// both body objects into their flagged pose - a second cue at 0x41, and past
/// 0x5A hands sub-state 1 the animation 0xF. Sub-state 1 converts `field_5AC`
/// into the `field_59C` budget at blend 0xE, feeds `field_5A2` from it over
/// blends 0x17..0x2F, queues two more cues, and past 0x46 leaves for either
/// state 8 (when `field_5B2` is set) or state 1 with a fresh `field_59C`,
/// clearing both flagged poses on the way out.
static void func_actor_510900_80137008(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;
    s32              val;
    s32              cur;
    u32              rng;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_590) {
        case 0:
            work->field_5A2 = 0;
            if (work->field_58A == 0x39) {
                work->field_594 = 1;
                work->field_598 = 0x55;
                snd             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000E;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_57C = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000D;
                SndEvt_EnqueueType6(work->field_57C, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
                work->obj4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj504.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                = Gp_PackPair(&D_actor_510900_80167968, 5);
                work->obj4E4.key    = pair;
                work->obj504.key    = pair;
            }
            if (work->field_58A == 0x41) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780006;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x5A) {
                work->field_586 = 0xF;
                work->field_590 = 1;
            }
            break;
        case 1:
            if (work->field_58A == 0xE) {
                cur = work->field_5AC;
                val = 0x1388;
                if (cur < 0x1389) {
                    val = cur;
                }
                work->field_59C = (val - 0x4B0) / 24;
            }
            work->field_5A2 = ((u32)((u16)work->field_58A - 0x17) < 0x19U) ? work->field_59C : 0;
            if (work->field_58A == 0x19) {
                work->obj4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj504.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                = Gp_PackPair(&D_actor_510900_80167968, 1);
                work->obj4E4.key    = pair;
                work->obj504.key    = pair;
                snd                 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780007;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A == 0x2F) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780008;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x46) {
                if (work->field_5B2 != 0) {
                    work->field_58E = 1;
                    work->field_590 = 8;
                    work->field_5B2 = 0;
                    work->field_586 = 5;
                } else {
                    work->field_58E = 1;
                    work->field_586 = 1;
                    work->field_590 = 0;
                    work->field_59C = D_actor_510900_801679D0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
                    gRandomLcgState = rng;
                }
                work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
    }
}

/// Handler that spawns the actor's companion enemy once the 0x14 animation has
/// blended in (`field_58A` == 0x14): the spawned task's model takes its texture
/// page and CLUT row from the current area record, indexed by the enemy's own
/// bank nibble, and a sound is queued from the actor's attach coordinate. Past
/// blend 0x46 the handler leaves for either state 5 (animation 0xB) or, on a
/// failed `gRandomLcgState` roll, state 1 with a fresh `field_59C`.
static void func_actor_510900_801373B8(Task* arg0)
{
    Actor510900Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    GameLocationKey* sessionKey;
    TmdObject*       model;
    GpAreaVariant*   rec;
    AreaPlacement*   entry;
    GameLocationKey  key;
    s32              idx;
    s32              snd;
    s32              pan;
    s32              roll;
    u32              rng;

    roll  = 0;
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;

    work->field_5A2 = 0;
    if (work->field_58A == 0x14) {
        work->field_5BA = 1;
        model           = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 4, roll, enemy)->task->extra.tmd;
        idx             = (u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey      = &gGameSession->location.loc;
        key.stage       = sessionKey->stage;
        key.area        = sessionKey->area;
        key.room        = sessionKey->room;
        key.view        = gGameSession->location.loc.view;
        areaSyncLocationVariant(&key);
        rec = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry                    = gpAreaPlaceAt(rec->field_0, idx);
        model->texturePageOffset = entry->texturePageOffset;
        model->clutRowOffset     = entry->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
        snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780012;
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    if (work->field_58A >= 0x46) {
        if (work->field_5AC >= 0xEA7) {
            /* Reading the global back is what keeps the store ahead of the
               shift: written as a local, the store sinks into the branch
               delay slot and the draw lands in a different register. */
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 0x10) & 0xF) < 0xCU) {
                roll = 1;
            }
        }
        if (roll == 0) {
            work->field_58E = 1;
            work->field_586 = 1;
            work->field_590 = 0;
            work->field_59C = D_actor_510900_801679D0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
            gRandomLcgState = rng;
            return;
        }
        work->field_58E = 5;
        work->field_590 = 0;
        work->field_586 = 0xB;
    }
}

/// State 5 (animation 0xB): the actor rears up, holds, then either drops back
/// to state 1 or commits. Sub-state 0 ramps `field_5A2` at blend 0x47, queues
/// the rear-up sound at 0x4A and hands sub-state 1 the animation 0xC at 0x52.
/// Sub-state 1 pushes both body objects into their flagged pose while
/// `field_5AC` is short; otherwise it bleeds `field_59C` down by 0x84 a frame
/// and, once that runs out (or `field_5AE` drops below 0x384), returns to
/// state 1 with a fresh `gRandomLcgState` draw. Sub-state 2 queues the landing
/// sound at blend 0xE and leaves for state 8 past 0x3B.
static void func_actor_510900_801375D8(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;
    s32              val;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_590) {
        case 0:
            work->field_5A2 = (work->field_58A < 0x47) ? 0 : 0x84;
            if (work->field_58A == 0x4A) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000F;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x52) {
                work->field_586 = 0xC;
                work->field_590 = 1;
                work->field_59C = 0xEA6;
            }
            break;
        case 1:
            work->field_5A2 = 0x84;
            if (work->field_5AC < 0x514) {
                work->field_590     = 2;
                work->field_586     = 0xD;
                work->obj4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj504.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                = Gp_PackPair(&D_actor_510900_80167968, 0);
                work->obj4E4.key    = pair;
                work->obj504.key    = pair;
            } else {
                work->field_59C -= 0x84;
                if (work->field_59C < 0 || work->field_5AE < 0x384) {
                    work->field_58E     = 1;
                    work->field_590     = 0;
                    work->field_586     = 1;
                    gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    val                 = D_actor_510900_801679D0[(gRandomLcgState >> 16) & 0xF];
                    work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->field_59C     = val;
                }
            }
            break;
        case 2:
            work->field_5A2 = 0;
            if (work->field_58A == 0xE) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000C;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x3B) {
                work->field_590     = 8;
                work->field_586     = 5;
                work->field_58E     = 1;
                work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
    }
}

/// Handler for the scripted sequence the actor plays out through `field_590`:
/// state 0 opens with the two 0x4078 cues at blend 0x39 and hands over at 0x5A,
/// state 1 plays one more cue at blend 0xA and hands over at 0x64, state 2 runs
/// the long beat - a pair swap and a screen fade at 0x50, a cue at 0x53 whose id
/// depends on `field_594`, a script spawn at 0x58, the pair blanked at 0x60 -
/// and either enters state 3 when `field_5B2` is latched or, past blend 0xB3,
/// drops back to `field_58E` state 1 with a fresh `field_59C`. State 3 keeps the
/// walk speed inside its two blend windows and, every sixth frame, restarts the
/// effect on the player's model; its `field_5B6` sub-state queues file 0x1E and
/// plays the arrival cue once the drive goes idle.
///
/// `blend` is one temp on purpose: the `lh` of `field_586` leaves its high bits
/// unknown to combine, which is what keeps the `andi 0xFFFF` on the second
/// window test of each chain.
static void func_actor_510900_80137868(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;
    s32              blend;
    u32              rng;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    SCRATCH_STACK_RESERVE_BYTES(0x10);

    switch (work->field_590) {
        case 0:
            work->field_5B4 = 1;
            work->field_5A2 = 0;
            if (work->field_58A == 0x39) {
                work->field_594 = 1;
                work->field_598 = 0x10E;
                snd             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000E;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_57C = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000D;
                SndEvt_EnqueueType6(work->field_57C, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
                work->obj4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj504.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                = Gp_PackPair(&D_actor_510900_80167968, 5);
                work->obj4E4.key    = pair;
                work->obj504.key    = pair;
            }
            if (work->field_58A >= 0x5A) {
                work->field_586 = 0x1A;
                work->field_590 = 1;
            }
            break;
        case 1:
            if (work->field_58A == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780006;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_58A >= 0x64) {
                work->field_586 = 7;
                work->field_590 = 2;
            }
            break;
        case 2:
            if (work->field_58A == 0x50) {
                work->field_5B4     = 2;
                work->obj4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj504.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                = Gp_PackPair(&D_actor_510900_80167968, 3);
                work->obj4E4.key    = pair;
                work->obj504.key    = pair;

                gGameSession->deathRestartDelay   = 0x80;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
            }
            if (work->field_58A == 0x53) {
                if (work->field_594 == 1) {
                    snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780010;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                        (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000C;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                        (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            if (work->field_58A == 0x58) {
                Gp_SpawnScript18(D_acropolis_helicopter_landing_pad_80187D34, &D_acropolis_helicopter_landing_pad_80187D3C);
            }
            if (work->field_58A == 0x60) {
                work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            blend = (u16)work->field_58A;
            if (blend - 0x4B < 0x10U) {
                work->field_5A2 = 0x3C;
            } else if (((blend - 0x8F) & 0xFFFF) < 0xFU) {
                work->field_5A2 = -0x40;
            } else {
                work->field_5A2 = 0;
            }
            if (work->field_58A >= 0x51) {
                if (work->field_5B2 == 1) {
                    work->field_590  = 3;
                    work->field_5B2  = 0;
                    work->field_5B6  = 1;
                    gPlayerStatus.hp = 0;
                    Gp_PulseState1C80();
                }
            } else {
                work->field_5B2 = 0;
            }
            if (work->field_58A >= 0xB3) {
                work->field_58E = 1;
                work->field_586 = 1;
                work->field_590 = 0;
                work->field_59C = D_actor_510900_801679D0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
                gRandomLcgState = rng;
            }
            break;
        case 3:
            blend = work->field_586;
            if (blend == 7) {
                blend = (u16)work->field_58A;
                if (blend - 0x4B < 0x10U) {
                    work->field_5A2 = 0x3C;
                } else if (((blend - 0x8F) & 0xFFFF) < 0xFU) {
                    work->field_5A2 = -0x10;
                } else {
                    work->field_5A2 = 0;
                }
                if (work->field_58A >= 0xAA) {
                    work->field_586 = 0x19;
                }
            }
            work->field_59C++;
            if (work->field_59C >= 6) {
                work->field_59C               = 0;
                D_actor_510900_80167B7C.coord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                func_800FDB18(5, &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[4], NULL,
                              &D_actor_510900_80167B7C);
            }
            switch (work->field_5B6) {
                case 0:
                    break;
                case 1:
                    CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                    work->field_5B6 = 2;
                    break;
                case 2:
                    if (CdCmd_IsIdle() == 1) {
                        coord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                        SndEvt_EnqueueType6(0x70010001, (s8)worldCoordGetOriginAudioPan(coord),
                                            (s8)worldCoordGetOriginAudioDepth(coord));
                        work->field_5B6 = 0;
                    }
                    break;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_510900_80137E20(Task* arg0)
{
    Actor510900Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              snd;
    s32              pan;
    s32              rng;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    switch (work->field_590) {
        case 0:
            work->field_586 = 0x10;
            work->field_5A2 = 0;
            work->field_590 = 1;
            snd             = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780005;
            pan             = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            if (work->field_5B0 >= 0xC8 && work->field_5AA == work->field_5A8) {
                if ((u16)work->field_58A >= 3 && (u16)work->field_58A < 13) {
                    work->field_5A2 = -0x2C;
                } else if ((u16)work->field_58A >= 0x12 && (u16)work->field_58A < 0x27) {
                    work->field_5A2 = -0x1E;
                } else {
                    work->field_5A2 = 0;
                }
            } else {
                work->field_5A2 = 0;
            }
            if (work->field_58A >= 0x53) {
                if (enemy->hp <= 0) {
                    work->field_58E = 0xC;
                    work->field_590 = 0;
                    work->field_586 = 0x18;
                } else {
                    work->field_58E = 1;
                    work->field_586 = 1;
                    work->field_590 = 0;
                    work->field_59C =
                        D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                    gRandomLcgState = rng;
                }
            }
            break;
    }
}

static void func_actor_510900_80137FBC(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    Enemy*           enemy;
    SVECTOR*         rot;
    SVECTOR*         head;
    s32              startPan;
    s32              loopPan;
    s16              step;
    u32              rng;

    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) = head - 1;
    work                          = arg0->work;
    enemy                         = arg0->spawnArg2.pointer;
    coord                         = arg0->extra.tmd->coords;
    switch (work->field_590) {
        case 0:
            work->field_586 = 0x12;
            work->field_5A2 = 0;
            work->field_590 = 1;
            work->field_59C = 0;
            work->field_578 = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780009;
            startPan        = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(work->field_578, startPan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            if (work->field_58A & 1) {
                rot             = head - 1;
                rot->vx         = 0;
                rot->vz         = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                rot->vy         = ((gRandomLcgState >> 0x10) & 0x2FF) - 0x680;
                Gp_SpawnEff(0x600E0, coord, 0x100, rot);
            }
            work->field_59C++;
            if (work->field_59C >= 3) {
                loopPan = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueTypeA(work->field_578, loopPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_59C = 0;
            }
            if ((work->field_5B0 < 0xC8) || (work->field_5AA != work->field_5A8)) {
                work->field_5A2 = 0;
            } else {
                step = 0;
                if ((u32)((u16)work->field_58A - 0x47) < 7) {
                    step = -0x53;
                }
                work->field_5A2 = step;
            }
            if (work->field_58A >= 0x51) {
                if (work->field_578 != 0) {
                    SndEvt_EnqueueType7(work->field_578, 0);
                    work->field_578 = 0;
                }
                if (enemy->hp <= 0) {
                    work->field_58E = 0xC;
                    work->field_590 = 0;
                    work->field_586 = 0x18;
                } else {
                    work->field_58E = 1;
                    work->field_586 = 1;
                    work->field_590 = 0;
                    work->field_59C =
                        D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
                    gRandomLcgState = rng;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

static void func_actor_510900_80138250(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    Enemy*           enemy;
    SVECTOR*         rot;
    SVECTOR*         head;
    s32              startPan;
    s32              loopPan;
    u32              rng;

    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) = head - 1;
    work                          = arg0->work;
    enemy                         = arg0->spawnArg2.pointer;
    coord                         = arg0->extra.tmd->coords;
    switch (work->field_590) {
        case 0:
            work->field_5A2 = 0;
            work->field_586 = 0x13;
            work->field_590 = 1;
            work->field_59C = 0;
            work->field_578 = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780009;
            startPan        = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(work->field_578, startPan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            if (work->field_58A & 1) {
                rot             = head - 1;
                rot->vx         = 0;
                rot->vz         = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                rot->vy         = ((gRandomLcgState >> 0x10) & 0x2FF) - 0x680;
                Gp_SpawnEff(0x600E0, coord, 0x100, rot);
            }
            work->field_59C++;
            if (work->field_59C >= 3) {
                loopPan = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueTypeA(work->field_578, loopPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_59C = 0;
            }
            if (work->field_58A >= 0x1E) {
                if (work->field_578 != 0) {
                    SndEvt_EnqueueType7(work->field_578, 0);
                    work->field_578 = 0;
                }
                work->field_586 = 0x14;
                work->field_590 = 2;
            }
            break;
        case 2:
            if (work->field_58A >= 0x3B) {
                if (enemy->hp <= 0) {
                    work->field_58E = 0xC;
                    work->field_590 = 0;
                    work->field_586 = 0x18;
                } else {
                    work->field_58E = 1;
                    work->field_586 = 1;
                    work->field_590 = 0;
                    work->field_59C =
                        D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
                    gRandomLcgState = rng;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

static void func_actor_510900_801384C4(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pan;
    u32              rng;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (gPlayerStatus.hp <= 0) {
        ((Enemy*)arg0->spawnArg2.pointer)->hp = 1;
        work->field_58E                       = 1;
        work->field_586                       = 1;
        work->field_590                       = 0;
        work->field_59C                       = D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
        gRandomLcgState                       = rng;
        return;
    }
    if (work->field_594 != 0) {
        work->field_594 = 3;
    }
    work->field_598 = 0;
    if (work->field_578 != 0) {
        SndEvt_EnqueueType7(work->field_578, 0);
        work->field_578 = 0;
    }
    if (work->field_57C != 0) {
        SndEvt_EnqueueType7(work->field_57C, 0);
        work->field_57C = 0;
    }
    if (work->field_580 != 0) {
        SndEvt_EnqueueType7(work->field_580, 0);
        work->field_580 = 0;
    }
    ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->field_58A == 0x70) {
        snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780007;
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    Gp_StateC08.field_6 &= 1;
    work->field_592      = 0;
}

/// Latches which of the four `D_actor_510900_80167BA4` boxes the player stands
/// in into `field_5AA`, records how far `field_5A6` is from the near and far
/// ends of the current patrol side, and measures the straight-line distance
/// from the actor's attach coordinate to the player into `field_5AC`.
static void func_actor_510900_8013864C(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    VECTOR*          delta;
    VECTOR*          head;
    s32              i;
    s32              dx;
    s32              dz;

    work                         = arg0->work;
    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    coord                        = arg0->extra.tmd->coords;
    SCRATCH_STACK_CURSOR(VECTOR) = head - 1;
    delta                        = head - 1;

    for (i = 0; i < 4; i++) {
        if (D_actor_510900_80167BA4[i].minX < gPlayerStatus.coordMtx->t[0] &&
            gPlayerStatus.coordMtx->t[0] < D_actor_510900_80167BA4[i].maxX &&
            D_actor_510900_80167BA4[i].minZ < gPlayerStatus.coordMtx->t[2] &&
            gPlayerStatus.coordMtx->t[2] < D_actor_510900_80167BA4[i].maxZ) {
            work->field_5AA = i;
            break;
        }
    }

    work->field_5B0 = __builtin_abs(work->field_5A8 * 13200 - work->field_5A6);
    work->field_5AE = __builtin_abs((work->field_5A8 + 1) * 13200 - work->field_5A6);

    dx              = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    delta->vx       = dx;
    delta->vy       = 0;
    dz              = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    delta->vz       = dz;
    work->field_5AC = SquareRoot0(dx * dx + dz * dz);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

static void func_actor_510900_801387F4(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    SVECTOR*         rot;
    SVECTOR*         head;
    u16              target;
    u16              cur;
    s32              d;
    s16              diff;
    s32              sdiff;
    s32              mag;
    s32              prev;
    s32              prev2;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_5A2 != 0) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        SCRATCH_STACK_CURSOR(SVECTOR) = head - 1;
        rot                           = head - 1;
        if (work->field_5AE < 0x3E8) {
            target = D_actor_510900_80167B9C[work->field_5A8 + 1];
        } else {
            target = D_actor_510900_80167B9C[work->field_5A8];
        }
        cur             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
        d               = target - cur;
        diff            = d;
        sdiff           = (s16)d;
        mag             = __builtin_abs(sdiff);
        work->field_5A0 = cur;
        if (mag < 0x800) {
            if (mag < 0x1F) {
                work->field_5A0 = target;
            } else {
                prev = work->field_5A0;
                if (sdiff > 0) {
                    work->field_5A0 = prev + 0x1E;
                } else {
                    work->field_5A0 = prev - 0x1E;
                }
            }
        } else if (sdiff > 0 ? (0x1000 - sdiff) < 0x1F : (sdiff + 0x1000) < 0x1F) {
            work->field_5A0 = target;
        } else {
            prev2 = work->field_5A0;
            if (diff > 0) {
                work->field_5A0 = prev2 - 0x1E;
            } else {
                work->field_5A0 = prev2 + 0x1E;
            }
        }
        rot->vx = 0;
        rot->vy = work->field_5A0;
        rot->vz = 0;
        RotMatrix(rot, &coord->coord);
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Walks the actor once around a fixed square patrol path: `field_5A6` is the
/// distance travelled, advanced by `field_5A2` and clamped, and its quotient by
/// the side length selects the corner (also latched into `field_5A8` for the
/// turn handler) while the remainder is the offset along that side.
static void func_actor_510900_80138978(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s16              side;
    s16              along;
    u16              dist;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;

    dist            = work->field_5A6 + (u16)work->field_5A2;
    work->field_5A6 = dist;
    if (dist < 0xC8) {
        work->field_5A6 = 0xC8;
    } else if (dist > 0xB66C) {
        work->field_5A6 = 0xB66C;
    }

    side            = work->field_5A6 / 13200;
    work->field_5A8 = side;
    along           = work->field_5A6 % 13200;

    coord->coord.t[0] = D_actor_510900_80167B84[side].x + (along * D_actor_510900_80167B94[side].x);
    coord->coord.t[1] = 0;
    coord->coord.t[2] =
        D_actor_510900_80167B84[work->field_5A8].z + (along * D_actor_510900_80167B94[work->field_5A8].z);
}

/// Fires the actor's step sounds: while the current animation record carries
/// `flags` cue bit 0x20 or 0x10, a sound is queued on the frame that bit has just
/// dropped from `Actor510900Work::field_59A`, panned and depth-attenuated from
/// the actor's attach coordinate. The record's two bits are latched for the
/// next frame at the end.
static void func_actor_510900_80138A9C(Task* arg0)
{
    s32                    snd;
    s32                    pan;
    s32                    pan2;
    Actor510900Work*       work;
    GfxCoord*              coord;
    const AnimationRecord* rec;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    rec   = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec != NULL) {
        if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->field_59A & ANIMATION_RECORD_CUE_2)) {
            snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780001;
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->field_59A & ANIMATION_RECORD_CUE_1)) {
            snd  = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780002;
            pan2 = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(snd, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->field_59A = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

#include "../../shared/no9_golem_aim_head.inc.c"

/// Yaws the head coordinate (`coords[3]`) by the residual rotation in
/// `Actor510900Work::field_570` and then walks that residual 0x20 back towards
/// zero on each axis, snapping to zero inside the last step. `field_584` is
/// cleared on the frame both axes have come to rest.
static void func_actor_510900_80138D38(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    matrix = SCRATCH_STACK_CURSOR(MATRIX);
    active = 0;
    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(&work->field_570, matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&matrix->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&matrix->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);

    angleX = work->field_570.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_570.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_570.vx = nextX;
            active             = 1;
        }
    }

    angleY = work->field_570.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_570.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_570.vy = nextY;
            active             = 1;
        }
    }

    if (active == 0) {
        work->field_584 = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Rebuilds the three collision faces this actor occupies in the grid, at the
/// body's current position and facing. `center` is the fixed local footprint
/// offset rotated into world space, translated by the coordinate and clamped to
/// the grid extent; the twelve corners in `D_actor_510900_80167BDC` are rotated
/// and offset from it into `Gp_GridParams->vertices`, and the three face normals
/// in `D_actor_510900_80167BC4` are rotated in place into `field_4`.
static void func_actor_510900_80138F44(Task* arg0)
{
    Actor510900GridScratch* scratch;
    GfxCoord*               coord;
    SVECTOR*                normals;
    SVECTOR*                corners;
    s32                     i;

    scratch = (Actor510900GridScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor510900GridScratch));
    coord   = arg0->extra.tmd->coords;
    normals = Gp_GridParams->normals;
    corners = Gp_GridParams->vertices;

    scratch->center.vx = -0x258;
    scratch->center.vy = 0;
    scratch->center.vz = 0x280;

    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->center);
    gte_rtv0();
    gte_stsv(&scratch->rotated);

    scratch->center.vx = coord->coord.t[0] + scratch->rotated.vx;
    scratch->center.vy = coord->coord.t[1] + scratch->rotated.vy;
    scratch->center.vz = coord->coord.t[2] + scratch->rotated.vz;

    if (scratch->center.vx > 0x1770) {
        scratch->center.vx = 0x1770;
    } else if (scratch->center.vx < -0x1770) {
        scratch->center.vx = -0x1770;
    }
    if (scratch->center.vz > 0x1770) {
        scratch->center.vz = 0x1770;
    } else if (scratch->center.vz < -0x1770) {
        scratch->center.vz = -0x1770;
    }

    for (i = 0; i < 12; i++) {
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&D_actor_510900_80167BDC[i]);
        gte_rtv0();
        gte_stsv(&scratch->rotated);
        corners[i].vx = scratch->rotated.vx + scratch->center.vx;
        corners[i].vy = scratch->rotated.vy + scratch->center.vy;
        corners[i].vz = scratch->rotated.vz + scratch->center.vz;
    }

    for (i = 0; i < 3; i++) {
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&D_actor_510900_80167BC4[i]);
        gte_rtv0();
        gte_stsv(&normals[i]);
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor510900GridScratch));
}

/// Message 0x7D7 handler (entry in `D_actor_510900_80167A6C`). `arg2` picks
/// between three visibility/liveness states of the boss and its two companion
/// enemies, whose models are reached through `field_568` / `field_56C`.
///
/// 0 parks the actor: both models and its own drop to blend 0 and `field_5A4`
/// becomes 1, after taking a `gSceneCombatState` reference (release id 0x1B).
/// 1 wakes it: the same blend reset, then the patrol is restarted from
/// distance 0x11F8 with animation 0x1A and yaw 0x400, the head coordinate is
/// rebuilt from that yaw with its translation cleared, and slots 1..0x12 are
/// reseeded. The first wake (`field_594` still 0) also pushes both body objects
/// into their flagged pose and queues the charge cue.
/// 2 puts it away: blend 0x80 everywhere, the linked task is handed state 4 and
/// dropped, the flagged poses are cleared, and the collision grid this actor
/// edited is restored - the three faces `func_actor_510900_8013B524` copies back
/// plus the fourth `func_actor_510900_8013B424` zeroes, both written out inline.
///
/// `vec` is one `SVECTOR*` serving two unrelated roles - the scratch rotation
/// state 1 builds the head matrix from, and the extra face normal state 2
/// clears - which is what puts it in `$a0` in both. The `do`/`while (0)` cuts
/// the basic block so the second `Gp_GridParams->normals` read is scheduled on
/// its own; without it the corner pointer and its walking copy coalesce.
s32 func_actor_510900_801391B8(Task* arg0, s32 arg1, s32 arg2)
{
    Actor510900Work*        work;
    TmdObject*              obj;
    GfxCoord*               coord;
    Enemy*                  enemy;
    SVECTOR*                rot;
    void*                   head;
    SVECTOR*                normals;
    SVECTOR*                verts;
    WorldCollisionGridFace* faces;
    s32                     pair;
    s32                     i;
    s32                     j;
    s32                     k;
    SVECTOR*                vec;
    SVECTOR*                corners;

    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(SVECTOR);
    rot                        = SCRATCH_STACK_CURSOR(SVECTOR);
    obj                        = arg0->extra.tmd;
    work                       = arg0->work;
    enemy                      = arg0->spawnArg2.pointer;
    coord                      = obj->coords;

    switch (arg2) {
        case 0:
            (Gp_IncStateF0Ref)(0x1B);
            obj->flags                        = 0;
            work->field_568->extra.tmd->flags = 0;
            work->field_56C->extra.tmd->flags = 0;
            work->field_5A4                   = 1;
            break;
        case 1:
            obj->flags                        = 0;
            work->field_568->extra.tmd->flags = 0;
            work->field_56C->extra.tmd->flags = 0;
            work->field_5A4                   = 2;
            work->field_5A6                   = 0x11F8;
            vec                               = rot;
            work->field_586                   = 0x1A;
            work->field_588                   = 0x1A;
            work->field_5A0                   = 0x400;
            work->field_58E                   = 0;
            work->field_590                   = 0;
            work->field_5A2                   = 0;
            work->field_58A                   = 0;
            work->obj47C.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            enemy->node.state.parts.flags     = WORLD_TARGET_HIDE_HP;
            vec->vx                           = 0;
            vec->vy                           = work->field_5A0;
            vec->vz                           = 0;
            RotMatrix(vec, &coord->coord);
            coord->coord.t[0] = 0;
            coord->coord.t[1] = 0;
            coord->coord.t[2] = 0;
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->field_586);
            }
            if (work->field_594 == 0) {
                work->field_594 = 1;
                if (work->field_564 != NULL) {
                    work->field_564[0xD] = 1;
                }
                work->field_598     = 0xF0;
                work->obj4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj504.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                = Gp_PackPair(&D_actor_510900_80167968, 5);
                work->obj4E4.key    = pair;
                work->obj504.key    = pair;
                work->field_57C     = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000D;
                SndEvt_EnqueueType6(work->field_57C, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 2:
            obj->flags                        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_568->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_56C->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_5A4                   = 0;
            if (work->field_564 != NULL) {
                work->field_564[0xD] = 4;
            }
            work->field_564               = NULL;
            work->obj47C.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj4E4.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj504.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;

            normals = Gp_GridParams->normals;
            verts   = Gp_GridParams->vertices;
            faces   = Gp_GridParams->faces;
            for (j = 0; j < 12; j++) {
                verts[j] = D_actor_510900_80167BDC[j];
            }
            for (j = 0; j < 3; j++) {
                normals[j] = D_actor_510900_80167BC4[j];
                faces[j]   = D_actor_510900_80167C3C[j];
            }

            do {
                vec = Gp_GridParams->normals;
            } while (0);
            corners   = Gp_GridParams->vertices;
            vec[3].vx = 0;
            vec[3].vy = 0;
            vec[3].vz = 0;
            for (k = 0; k < 4; k++) {
                corners[12 + k].vx = 0;
                corners[12 + k].vy = 0;
                corners[12 + k].vz = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
    return 0;
}

/// Frame handler of the muzzle-flash child task, dispatched from
/// `func_actor_510900_8013BE98` once the spawn has run. The parent's animation
/// id (`field_586`) selects the behaviour: below 0x1C the model is hidden,
/// 0x1C/0x1D hold, 0x1E unhides it once the blend weight reaches 0xC8, and 0x1F
/// drives the coordinate. There the weight's position within its 40-frame cycle
/// picks between an identity frame parented to the body's coordinate 12 (with
/// frame 0xE re-deriving the stashed local matrix from the view) and the stashed
/// matrix parented to the view, lifted along y by `3*(n - 0xC)^2 - 0x1B0`.
/// Weight 0x52 restores the parented identity frame.
static void func_actor_510900_801395AC(Enemy* enemy, Task* task)
{
    TmdObject*        obj;
    Actor510900Work*  work;
    GfxCoord*         coord;
    GfxCoord*         parentCoord;
    GfxRotationWords* mat;
    GfxRotationWords* mat2;
    s16               blend;
    s16               r;
    s32               dy;

    work  = (Actor510900Work*)task->parent->work;
    obj   = task->extra.tmd;
    coord = obj->coords;
    if (work->field_586 < 0x1C) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    SCRATCH_STACK_RESERVE_BYTES(0x20);
    switch (work->field_586) {
        case 0x1C:
        case 0x1D:
            break;

        case 0x1E:
            if (work->field_58A == 0xC8) {
                obj->flags = 0;
            }
            break;

        case 0x1F:
            parentCoord = &task->parent->extra.tmd->coords[12];
            blend       = work->field_58A;
            if (blend < 0x50) {
                r = blend % 40;
                if (r < 0xF) {
                    mat               = (GfxRotationWords*)&coord->coord;
                    mat->m00M01       = ONE;
                    mat->m11M12       = ONE;
                    mat->m22          = ONE;
                    mat->m02M10       = 0;
                    mat->m20M21       = 0;
                    coord->coord.t[0] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = 0;
                    coord->parent     = parentCoord;
                    if (r == 0xE) {
                        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parentCoord->workm, &work->field_544);
                    }
                } else {
                    r                  = r - 0xF;
                    dy                 = ((r - 0xC) * (r - 0xC) * 3) - 0x1B0;
                    coord->coord       = work->field_544;
                    coord->parent      = &gGfxViewCoord;
                    coord->coord.t[1] += dy;
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
            } else if (blend == 0x52) {
                mat2                = (GfxRotationWords*)&coord->coord;
                mat2->m00M01        = ONE;
                mat2->m02M10        = 0;
                mat2->m11M12        = ONE;
                mat2->m20M21        = 0;
                mat2->m22           = ONE;
                coord->coord.t[0]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[2]   = 0;
                coord->parent       = parentCoord;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Spawn state of the child effect task: allocates its `Actor510900ChildFx`
/// work block, places the child on the parent's fourth coordinate offset by a
/// fixed local vector and yawed -0x160, and links its two collision objects.
/// `field_CE` comes from `D_actor_510900_80167C94` indexed by the horizontal
/// distance to the player in units of 1000, clamped to the last entry.
static void func_actor_510900_801397F0(Enemy* arg0, Task* arg1)
{
    Actor510900ChildFx*        work;
    GolemPawnRookPlaceScratch* scratch;
    TmdObject*                 tmd;
    GfxCoord*                  coord;
    GfxCoord*                  parentCoords;
    GfxCoord*                  parentCoord;
    s32                        dx;
    s32                        dz;
    s32                        idx;

    tmd          = arg1->extra.tmd;
    coord        = tmd->coords;
    parentCoords = arg1->parent->extra.tmd->coords;
    parentCoord  = &parentCoords[3];
    work         = memCalloc(sizeof(Actor510900ChildFx), false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work    = work;
    tmd->flags    = 0;
    scratch       = (GolemPawnRookPlaceScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(GolemPawnRookPlaceScratch));
    tmd->lightMtx = &work->lightMtx;
    tmd->colorMtx = &work->colorMtx;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&gGfxViewCoord);
    parentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(parentCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->rot.vx = -0xA5;
    scratch->rot.vy = -0x235;
    scratch->rot.vz = 0xA0;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->rot);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    coord->parent      = &gGfxViewCoord;
    coord->coord.t[0] += scratch->pos.vx;
    coord->coord.t[1] += scratch->pos.vy;
    coord->coord.t[2] += scratch->pos.vz;

    scratch->rot.vx = -0x160;
    scratch->rot.vy = 0;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &scratch->mtx);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->mtx);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->mtx.m[0][1]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->mtx.m[0][2]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][2]);

    dx              = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->pos.vy = 0;
    scratch->pos.vx = dx;
    dz              = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    scratch->pos.vz = dz;
    idx             = SquareRoot0((dx * dx) + (dz * dz)) / 1000;
    if (idx >= 0xC) {
        idx = 0xB;
    }

    work->field_CE               = D_actor_510900_80167C94[idx];
    work->obj40.coord            = coord;
    work->obj40.context.contacts = &work->rec60;
    work->obj40.pos.vx           = 0;
    work->obj40.pos.vy           = 0;
    work->obj40.pos.vz           = 0;
    work->obj40.key              = 0;
    work->obj40.radius           = 0xC8;
    work->obj40.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj40);
    Gp_InitRec18Table(&work->rec60, 1, 0);

    work->d4rec.ends[0].vx      = 0;
    work->d4rec.ends[0].vy      = 0;
    work->d4rec.ends[0].vz      = 0;
    work->d4rec.ends[1].vx      = 0;
    work->d4rec.ends[1].vy      = 0x1F4;
    work->d4rec.ends[1].vz      = 0;
    work->d4rec.end0Radius      = 1;
    work->d4rec.end1Radius      = 1;
    work->d4rec.contacts        = &work->recB0;
    work->obj78.context.capsule = &work->d4rec;
    work->obj78.coord           = coord;
    work->obj78.pos.vx          = 0;
    work->obj78.pos.vy          = 0;
    work->obj78.pos.vz          = 0;
    work->obj78.key             = 0;
    work->obj78.radius          = 0;
    work->obj78.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->obj40.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(3, &work->obj78);
    Gp_InitRec18Table(&work->recB0, 1, 0);
    work->obj78.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    arg1->state = 1;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemPawnRookPlaceScratch));
}

/// Per-frame handler of the effect child while it is alive: spins the object by
/// `field_CE` about X, drags it 150 units down its own Y axis and drips a trail
/// effect every third frame. Once it has fallen past -0x514 and come back up,
/// or either `WorldCollisionContact` table reports a hit, it fires the impact effects,
/// reparents the task under the spawned one and hands the actor to state 2.
/// A parent that has stopped (`field_592` == 0) tears the object down the same
/// way. `gSceneCombatState.actorControl` 1 only refreshes the colour and 2 only hides the model.
static void func_actor_510900_80139C10(Enemy* enemy, Task* task)
{
    VECTOR                         pos;
    EffectWork*                    eff;
    GfxCoord*                      coord;
    u8*                            head;
    Actor510900ChildFxTickScratch* scratch;
    TmdObject*                     tmd;
    Actor510900ChildFx*            work;
    Actor510900Work*               parent;
    s32                            angle;
    s32                            snd;
    s32                            done;
    u16                            tick;

    tmd    = task->extra.tmd;
    work   = (Actor510900ChildFx*)task->work;
    coord  = tmd->coords;
    parent = (Actor510900Work*)task->parent->work;
    done   = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(task->spawnArg2.pointer, &pos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    head                     = SCRATCH_STACK_CURSOR(u8);
    scratch                  = (Actor510900ChildFxTickScratch*)(head - sizeof(Actor510900ChildFxTickScratch));
    SCRATCH_STACK_CURSOR(u8) = (u8*)scratch;
    angle                    = -work->field_CE;
    scratch->rot.vx          = angle;
    scratch->rot.vy          = 0;
    scratch->rot.vz          = 0;
    RotMatrix(&scratch->rot, &scratch->mtx);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->mtx);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->mtx.m[0][1]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->mtx.m[0][2]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][2]);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[0]  += -(coord->coord.m[0][1] * 0x96) >> 12;
    coord->coord.t[1]  += -(coord->coord.m[1][1] * 0x96) >> 12;
    coord->coord.t[2]  += -(coord->coord.m[2][1] * 0x96) >> 12;
    tick                = work->field_C8 + 1;
    work->field_C8      = tick;
    if (tick >= 3) {
        scratch->rot.vx = 0;
        scratch->rot.vy = 0x64;
        scratch->rot.vz = 0;
        Gp_SpawnEff(0x60070, coord, 0x01001600, &scratch->rot);
        work->field_C8 = 0;
    }
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2.pointer, &pos, 0, 0);
    if (coord->coord.t[1] < -0x514) {
        work->field_CA = 1;
    }
    if (work->field_CA != 0 && coord->coord.t[1] >= -0x513) {
        done = 1;
    }
    if (done != 0 || (work->rec60.key.value & 0xFFFF0000) == 0x10000 || work->recB0.key.value != 0) {
        Gp_SpawnEff(0x6005C, coord, 0x10002200, NULL);
        Gp_SpawnEff(0x60070, coord, 0xC1001200, NULL);
        eff = Gp_SpawnEff(0x80060185, coord, 0, NULL);
        if (eff != NULL) {
            Task_Reparent(task, eff->task);
        }
        if (work->rec60.key.value != 0) {
            work->field_CA = 2;
        } else {
            work->field_CA = 0;
        }
        work->obj40.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(&work->rec60);
        Gp_UnlinkObj(&work->obj78);
        work->field_C8         = 0;
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        snd                    = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51100009;
        SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        task->state = 2;
    }
    Gp_ClearRec18Occupied(&work->rec60);
    if (parent->field_592 == 0) {
        work->obj40.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_UnlinkObj(&work->obj78);
        work->field_C8         = 0;
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->state            = 2;
        work->field_CA         = 3;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor510900ChildFxTickScratch));
}

/// Frame handler of the effect child task: state 0 fades the object in over
/// 0x10 frames, state 1 holds it until its `WorldCollisionContact` reports a hit or 0x1F
/// frames pass, state 2 runs the hit handler, and state 3 unlinks the object
/// and destroys the enemy.
static void func_actor_510900_8013A100(Enemy* enemy, Task* task)
{
    Actor510900ChildFx* work;
    Actor510900Work*    parent;
    u16                 tick;

    work   = (Actor510900ChildFx*)task->work;
    parent = (Actor510900Work*)task->parent->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        switch (work->field_CA) {
            case 0:
                tick           = work->field_C8 + 1;
                work->field_C8 = tick;
                if (tick >= 0x10) {
                    work->obj40.radius = 0x258;
                    work->field_CA     = 1;
                    work->field_C8     = 0;
                    work->obj40.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    return;
                }
                if (parent->field_592 == 0) {
                    work->obj40.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_UnlinkObj(&work->obj78);
                    work->field_C8         = 0;
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    task->state            = 2;
                    work->field_CA         = 3;
                    return;
                }
                break;
            case 1:
                if ((work->rec60.key.value & 0xFFFF0000) == 0x10000) {
                    work->field_CA     = 2;
                    work->field_CC     = 0;
                    work->obj40.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    tick           = work->field_C8 + 1;
                    work->field_C8 = tick;
                    if (tick >= 0x1F) {
                        work->field_CA = 3;
                        work->field_C8 = 0;
                    } else if (parent->field_592 == 0) {
                        work->obj40.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        Gp_UnlinkObj(&work->obj78);
                        work->field_C8         = 0;
                        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        task->state            = 2;
                        work->field_CA         = 3;
                    }
                }
                Gp_ClearRec18Occupied(&work->rec60);
                return;
            case 2:
                func_actor_510900_8013A310(task);
                return;
            case 3:
                tick           = work->field_C8 + 1;
                work->field_C8 = tick;
                if (tick >= 0x1F) {
                    parent->field_5BA = 0;
                    Gp_UnlinkObj(&work->obj40);
                    Gp_DestroyEnemy(enemy, task);
                }
                break;
        }
    }
}

/// Runs the player-hold sequence the effect's state 2 drives, on a 0x2C-byte
/// scratch block: `field_CC` 0 asks the player for the hold (message 0x3F8) and
/// on success starts the grab animation and its sound, 1 holds until the parent
/// reports the hit or 0x3C frames pass and then switches to the second
/// animation, and 2 waits 0x14 frames before releasing the player. Any refused
/// message leaves the effect in state 3 so the caller tears it down.
static void func_actor_510900_8013A310(Task* task)
{
    Actor510900ChildFx*    work;
    Actor510900Work*       parent;
    Task*                  player;
    void*                  head;
    Actor510900HitScratch* scratch;
    GfxCoord*              obj;
    u16                    tick;
    s32                    snd;
    s32                    pan;

    work                       = (Actor510900ChildFx*)task->work;
    parent                     = (Actor510900Work*)task->parent->work;
    player                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(Actor510900HitScratch);
    scratch                    = SCRATCH_STACK_CURSOR(Actor510900HitScratch);

    switch (work->field_CC) {
        case 0:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
                scratch->query.field_14 = 0xC;
                if (TASK_MESSAGE_DISPATCH_POINTER(player, 0x3F8, scratch, 0) != 0) {
                    work->field_CA = 3;
                    break;
                }
                Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_510900_80167968, 4), 0);
                scratch->anim.source.sets          = D_actor_510900_80167B2C;
                scratch->anim.animationId          = 1;
                scratch->anim.blend                = ANIMATION_BLEND_RESET;
                scratch->anim.blendFrames          = 0;
                scratch->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->anim, 0);
                work->field_CC = 1;
                work->field_CE = 0;
                obj            = player->extra.tmd->coords;
                snd            = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x5110000A;
                pan            = (s8)worldCoordGetOriginAudioPan(obj);
                SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(obj));
            }
            break;
        case 1:
            tick           = work->field_CE + 1;
            work->field_CE = tick;
            if ((s16)tick < 0x3D && parent->field_5BC != 1 && parent->field_592 != 0) {
                break;
            }
            parent->field_5BC                  = 0;
            scratch->anim.source.sets          = D_actor_510900_80167B2C;
            scratch->anim.animationId          = 2;
            scratch->anim.blend                = ANIMATION_BLEND_RESET;
            scratch->anim.blendFrames          = 0;
            scratch->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->anim, 0);
            work->field_CC = 2;
            work->field_CE = 0;
            break;
        case 2:
            tick           = work->field_CE + 1;
            work->field_CE = tick;
            if ((s16)tick >= 0x15) {
                if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                    Gp_DispatchMsg(player, 0x3F1, 0, 0);
                    work->field_CA = 3;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor510900HitScratch));
}

/// Spawn handler of the child task: allocates the animation work block, seeds
/// the model's root coordinate from the spawn-index tables, resets animation
/// slots 1..10 and links the two render objects.
static void func_actor_510900_8013A5B8(Enemy* enemy, Task* task)
{
    TmdObject*            tmd;
    GfxCoord*             coords;
    Actor510900ChildAnim* work;
    GfxCoord*             coord;
    SVECTOR*              rot;
    void*                 head;
    s32                   i;

    tmd    = task->extra.tmd;
    coords = tmd->coords;
    work   = memCalloc(sizeof(Actor510900ChildAnim), 0);
    coord  = &coords[10];
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work                 = work;
    tmd->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coords->composeStamp       = GRAPHICS_COORD_DIRTY;
    tmd->lightMtx              = &work->lightMtx;
    tmd->colorMtx              = &work->colorMtx;
    head                       = SCRATCH_STACK_CURSOR(void);
    enemy->field_4             = &coords->coord;
    rot                        = (SVECTOR*)(head - 8);
    SCRATCH_STACK_CURSOR(void) = head - 8;
    enemy->field_48            = 0;
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->coord                  = coord;
    enemy->bodyPos.vx             = -0xC8;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    work->field_334               = task->spawnArg1.value;
    ((SVECTOR*)(head - 8))->vx    = 0;
    rot->vy                       = D_actor_510900_80167CD0[work->field_334];
    rot->vz                       = 0;
    RotMatrix(rot, &coords->coord);
    i                  = 1;
    coords->coord.t[0] = D_actor_510900_80167CB8[work->field_334].vx;
    coords->coord.t[1] = D_actor_510900_80167CB8[work->field_334].vy;
    coords->coord.t[2] = D_actor_510900_80167CB8[work->field_334].vz;
    coords->parent     = &gGfxViewCoord;
    func_800B3F84(&work->anim, D_actor_510900_80167CAC, tmd, work->poses, work->slots);
    do {
        Gp_AnimResetSlot(&work->anim, i, 1);
        i++;
    } while (i < 0xB);
    work->obj2BC.pos.vx           = -0xC8;
    work->obj2BC.coord            = coord;
    work->obj2BC.pos.vy           = 0;
    work->obj2BC.pos.vz           = 0;
    work->obj2BC.context.contacts = &work->rec2DC;
    work->obj2BC.key              = 0;
    work->obj2BC.radius           = 0xC8;
    work->obj2BC.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj2BC);
    Gp_InitRec18Table(&work->rec2DC, 1, 0);
    work->obj2F4.key              = 0x50002;
    work->obj2F4.coord            = coord;
    work->obj2F4.pos.vx           = 0;
    work->obj2F4.pos.vy           = 0;
    work->obj2F4.pos.vz           = 0;
    work->obj2F4.context.contacts = &work->rec314;
    work->obj2F4.radius           = 0x15E;
    work->obj2F4.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->obj2BC.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(8, &work->obj2F4);
    Gp_InitRec18Table(&work->rec314, 1, 0);
    work->obj2F4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback  = func_actor_510900_8013C380;
    task->state         = 1;
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Frame handler (state 1) of the child task. Mode 1 of `gSceneCombatState.actorControl` only
/// redraws, mode 2 hides the model and flags the context, and mode 0 ticks the
/// animation until `func_actor_510900_8013C240` reports ready before falling
/// into the normal body.
static void func_actor_510900_8013A85C(Enemy* arg0, Task* arg1)
{
    TmdObject*            obj;
    Actor510900ChildAnim* work;
    GfxCoord*             coord;
    Actor510900Work*      parent;
    s32                   mode;
    s32                   i;
    s32                   one;

    obj    = arg1->extra.tmd;
    work   = (Actor510900ChildAnim*)arg1->work;
    coord  = obj->coords;
    parent = (Actor510900Work*)arg1->parent->work;
    mode   = gSceneCombatState.actorControl;
    one    = 1;
    if (mode == one) {
        goto case1;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto case0;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto case2;
    }
    goto body;
case0:
    if (func_actor_510900_8013C240(arg1) == 0) {
        i = 1;
        do {
            Gp_AnimTickIndex(&work->anim, i);
            i++;
        } while (i < 0xB);
        return;
    }
    arg1->extra.tmd->flags       = 0;
    arg0->node.state.parts.flags = one;
    goto body;
case2:
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
body:
    func_actor_510900_8013A9BC(arg1);
    if (work->field_334 < 2) {
        GameFlag_SetNibble(work->field_334 + 0xB, work->field_336);
    } else {
        parent->field_5C2 = work->field_336;
    }
    i = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0xB);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
case1:
    func_actor_510900_8013C338(arg1, coord);
}

/// Grab state machine of the child task, run from the frame handler above.
/// State 0 waits for the grab: the player has to be inside the `rec2DC` node
/// (and survive `Gp_ComputeDamage`) or the parent has to request this child by
/// number through `field_5BE`; on a hit it resets the animation slots, spawns
/// the grab effect and its sound and starts the `field_332` countdown. State 1
/// runs that countdown, keeping the four trailing part coordinates updated, and
/// hands the held effect task its exit state once the timer runs out or the
/// camera cuts away. State 2 only releases the held task. `field_334` 2 mirrors
/// the state back to the parent's `field_5C4`.
static void func_actor_510900_8013A9BC(Task* task)
{
    Actor510900ChildAnim*   work;
    Actor510900Work*        parent;
    Enemy*                  ctx;
    Actor510900GrabScratch* scratch;
    Actor510900GrabScratch* head;
    GfxCoord*               coord;
    EffectWork*             eff;
    Task*                   spawned;
    s16                     next;
    s32                     grabbed;
    s32                     i;
    s32                     one;
    s32                     snd;
    s32                     pan;
    s32                     dmg;
    s16                     state;

    grabbed                                      = 0;
    head                                         = SCRATCH_STACK_CURSOR(Actor510900GrabScratch);
    coord                                        = &task->extra.tmd->coords[10];
    SCRATCH_STACK_CURSOR(Actor510900GrabScratch) = head - 1;
    scratch                                      = head - 1;
    work                                         = (Actor510900ChildAnim*)task->work;
    ctx                                          = task->spawnArg2.pointer;
    state                                        = work->field_330;
    parent                                       = (Actor510900Work*)task->parent->work;
    one                                          = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto end;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto end;
case0:
    if (parent->field_592 == 0) {
        work->field_336 = 3;
        work->field_330 = 2;
        goto end;
    }
    ctx->node.state.parts.flags = gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
    dmg                         = work->rec2DC.key.value;
    work->obj2BC.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    if ((dmg & 0xFFFF8000) == 0x20000 && ctx->node.state.parts.targeted == one &&
        Gp_ComputeDamage(dmg, 0x3E8, 0, 0) != 0) {
        grabbed = 1;
    }
    if (parent->field_5BE == work->field_334 + 1 && parent->field_5C0 == 1) {
        grabbed           = 1;
        parent->field_5BE = -1;
    }
    if (grabbed == 1) {
        work->field_336 = grabbed;
        work->field_330 = grabbed;
        i               = 1;
        do {
            func_800B4114(&work->anim, i, 2, 0, 0);
            i++;
        } while (i < 0xB);
        scratch->rot.vx = 0;
        scratch->rot.vy = 0x80;
        scratch->rot.vz = 0;
        eff             = Gp_SpawnEff(0x8006005B, coord, 0, &scratch->rot);
        if (eff != NULL) {
            spawned         = eff->task;
            work->field_32C = spawned;
            Task_Reparent(task, spawned);
        }
        Gp_SpawnEff(0x6005C, coord, 0x200, &scratch->rot);
        work->field_332     = 0x78;
        work->obj2BC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj2F4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        snd                 = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51100004;
        pan                 = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    Gp_ClearRec18Occupied(&work->rec2DC);
    goto end;
case1:
    if (Gp_FindRec18(&work->rec314, 0) != 0) {
        work->obj2F4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    Gp_ClearRec18Occupied(&work->rec314);
    func_acropolis_helicopter_landing_pad_80180A64(&task->extra.tmd->coords[9]);
    func_acropolis_helicopter_landing_pad_80180A64(&task->extra.tmd->coords[8]);
    func_acropolis_helicopter_landing_pad_80180A64(&task->extra.tmd->coords[7]);
    func_acropolis_helicopter_landing_pad_80180A64(&task->extra.tmd->coords[6]);
    work->field_332--;
    next = 2;
    if (work->field_332 <= 0) {
        Task* held;

        work->field_336     = next;
        work->obj2F4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        held                = work->field_32C;
        if (held != NULL) {
            held->state = state;
        }
    } else {
        Task* held;

        if (parent->field_592 != 0) {
            goto end;
        }
        work->field_336     = next;
        work->obj2F4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        held                = work->field_32C;
        if (held != NULL) {
            held->state     = 2;
            work->field_32C = NULL;
        }
    }
    work->field_330 = next;
    goto end;
case2:
    if (parent->field_592 == 0) {
        Task* held;

        held = work->field_32C;
        if (held != NULL) {
            held->state     = state;
            work->field_32C = NULL;
        }
    }
end:
    if (work->field_334 == 2) {
        parent->field_5C4 = work->field_330;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor510900GrabScratch);
}

static void func_actor_510900_8013AD90(Enemy* enemy, Task* task)
{
    GfxCoord*             coord;
    GfxRotationWords*     mat;
    Actor510900ChildWork* work;

    coord = task->extra.tmd->coords;
    work  = memCalloc(sizeof(Actor510900ChildWork), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    mat                 = (GfxRotationWords*)&coord->coord;
    task->work          = work;
    mat->m00M01         = ONE;
    mat->m11M12         = ONE;
    mat->m22            = ONE;
    mat->m02M10         = 0;
    mat->m20M21         = 0;
    coord->coord.t[0]   = -0x17D4;
    coord->coord.t[1]   = -0x456;
    coord->coord.t[2]   = 0x17C;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    work->obj0.coord              = coord;
    work->obj0.pos.vx             = 0;
    work->obj0.pos.vy             = 0;
    work->obj0.pos.vz             = 0;
    work->obj0.context.contacts   = &work->rec20;
    work->obj0.key                = 0;
    work->obj0.radius             = 0x12C;
    work->obj0.flags              = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->obj38.pos.vy           = -0x200;
    work->obj38.coord            = coord;
    work->obj38.pos.vx           = 0;
    work->obj38.pos.vz           = 0;
    work->obj38.context.contacts = &work->rec58;
    work->obj38.key              = 0;
    work->obj38.radius           = 0x200;
    work->obj38.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->obj0.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(8, &work->obj38);
    Gp_InitRec18Table(&work->rec58, 1, 0);
    work->obj38.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback = func_actor_510900_8013C430;
    task->state        = 1;
}

static void func_actor_510900_8013AF38(Enemy* arg0, Task* arg1)
{
    Actor510900ChildWork* work;
    Actor510900Work*      parent;
    s32                   mode;
    s32                   one;
    u32                   random;
    Task*                 child;

    work   = (Actor510900ChildWork*)arg1->work;
    parent = (Actor510900Work*)arg1->parent->work;
    mode   = gSceneCombatState.actorControl;
    one    = 1;
    if (mode == one) {
        return;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto case0;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto case2;
    }
    goto body;
case0:
    if ((Gp_GetViewIndex() & 0xFF) != D_actor_510900_80167CE4[0]) {
        arg0->node.state.parts.flags = one;
        work->obj0.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj38.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        if (work->field_76 != 0) {
            work->field_76--;
        }
        child = work->field_70;
        if (child != NULL) {
            child->state   = 3;
            work->field_70 = NULL;
        }
        return;
    }
    arg0->node.state.parts.flags = one;
    goto body;
case2:
    arg0->node.state.parts.flags = one;
    return;
body:
    func_actor_510900_8013B0D8(arg1);
    GameFlag_SetNibble(0xD, D_actor_510900_80167CEC[work->field_74][parent->field_5C2]);
    random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = random;
    if ((u16)((random >> 16) % 3) == 0) {
        Gp_SpawnEff(0x6005A, arg1->extra.tmd->coords, 0, NULL);
    }
}

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - indexed by `Task::state`.
static const GpEnemyTaskFuncTable3 D_actor_510900_80131ECC = {
    { func_actor_510900_801397F0, func_actor_510900_80139C10, func_actor_510900_8013A100 },
};

/// `func_actor_510900_8013AF38`.
/// State 1 watches the parent's `field_5C4` phase and its own collision record
/// for a 0x20000 hit; landing one spawns the grab effects, reparents this task
/// under the effect's task and hands the victim the state in `field_78`. The
/// timer in `field_76` then runs the hold out (state 3), and states 4 and 5
/// finish or release the victim.
static void func_actor_510900_8013B0D8(Task* arg0)
{
    Actor510900ChildWork* work;
    GfxCoord*             coord;
    Actor510900Work*      parent;
    Enemy*                ctx;
    EffectWork*           eff;
    Task*                 held;
    Task*                 ending;
    Task*                 dropped;
    Task*                 released;
    Task*                 spawned;
    s32                   hit;
    s32                   tag;
    s32                   snd;
    s32                   pan;
    s32                   next;
    u16                   left;

    work   = (Actor510900ChildWork*)arg0->work;
    coord  = arg0->extra.tmd->coords;
    parent = (Actor510900Work*)arg0->parent->work;
    ctx    = arg0->spawnArg2.pointer;

    switch (work->field_74) {
        case 0:
            next = parent->field_5C4;
            if (next == 1) {
                work->field_74 = next;
            }
            break;
        case 1:
            ctx->node.state.parts.flags = gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
            hit                         = work->rec20.key.value;
            work->obj0.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            if ((hit & ~0x7FFF) == 0x20000) {
                tag = ctx->node.state.parts.targeted;
                if (tag == 1 && Gp_ComputeDamage(hit, 0x3E8, 0, 0) != 0) {
                    work->field_74 = 2;
                    work->field_76 = 0x3C;
                    Gp_SpawnEff(0x6003B, coord, 0, NULL);
                    eff = Gp_SpawnEff(0x8006005F, coord, 0, NULL);
                    if (eff != NULL) {
                        spawned        = eff->task;
                        work->field_70 = spawned;
                        Task_Reparent(arg0, spawned);
                    }
                    work->obj0.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->obj38.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    if (parent->field_5C4 == 2) {
                        work->field_78  = tag;
                        work->obj38.key = 0x50003;
                    } else {
                        work->field_78  = 0;
                        work->obj38.key = 0x50004;
                    }
                    snd = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51100002;
                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            Gp_ClearRec18Occupied(&work->rec20);
            break;
        case 2:
            held           = work->field_70;
            work->field_74 = 3;
            if (held != NULL) {
                held->state = work->field_78;
            }
            if (Gp_FindRec18(&work->rec58, 0) != 0) {
                work->obj38.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            Gp_ClearRec18Occupied(&work->rec58);
            break;
        case 3:
            if (Gp_FindRec18(&work->rec58, 0) != 0) {
                work->obj38.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            Gp_ClearRec18Occupied(&work->rec58);
            left           = work->field_76 - 1;
            work->field_76 = left;
            if ((s16)left <= 0) {
                work->field_74     = 4;
                work->obj38.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                ending             = work->field_70;
                if (ending != NULL) {
                    ending->state = 2;
                }
                break;
            }
            if (parent->field_592 == 0) {
                work->field_74     = 5;
                work->obj38.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                dropped            = work->field_70;
                if (dropped != NULL) {
                    dropped->state = 3;
                    work->field_70 = NULL;
                }
            }
            break;
        case 4:
            if (parent->field_592 == 0) {
                work->field_74 = 5;
                released       = work->field_70;
                if (released != NULL) {
                    released->state = 3;
                    work->field_70  = NULL;
                }
            }
            break;
        case 5:
            break;
    }
}

void func_actor_510900_8013B3D0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = { func_actor_510900_801350F8, func_actor_510900_8013B658 };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Adds (arg0 == 1) or clears the extra collision-grid face this actor edits
/// in, extending the three faces `func_actor_510900_8013B524` restores with a
/// fourth. Clearing zeroes only the vertices and normal; the face record stays.
void func_actor_510900_8013B424(s32 arg0)
{
    s32                     i;
    SVECTOR*                normals = Gp_GridParams->normals;
    SVECTOR*                verts   = Gp_GridParams->vertices;
    WorldCollisionGridFace* faces   = Gp_GridParams->faces;

    if (arg0 == 1) {
        for (i = 0; i < 4; i++) {
            verts[12 + i] = D_actor_510900_80167C68[i];
        }
        normals[3] = D_actor_510900_80167C60;
        faces[3]   = D_actor_510900_80167C88;
    } else {
        normals[3].vx = 0;
        normals[3].vy = 0;
        normals[3].vz = 0;
        for (i = 0; i < 4; i++) {
            verts[12 + i].vx = 0;
            verts[12 + i].vy = 0;
            verts[12 + i].vz = 0;
        }
    }
}

/// Restores the collision-grid faces this actor edited. The spawn handler
/// passes its task, which this never reads; the parameter is kept because the
/// call site materialises it.
void func_actor_510900_8013B524(Task* arg0)
{
    s32                     i;
    SVECTOR*                normals = Gp_GridParams->normals;
    SVECTOR*                verts   = Gp_GridParams->vertices;
    WorldCollisionGridFace* faces   = Gp_GridParams->faces;

    for (i = 0; i < 12; i++) {
        verts[i] = D_actor_510900_80167BDC[i];
    }

    for (i = 0; i < 3; i++) {
        normals[i] = D_actor_510900_80167BC4[i];
        faces[i]   = D_actor_510900_80167C3C[i];
    }
}

void func_actor_510900_8013B608(Task* arg0)
{
    Actor510900Work* work = arg0->work;

    Gp_UnlinkObj(&work->obj47C);
    Gp_UnlinkObj(&work->obj4E4);
    Gp_UnlinkObj(&work->obj504);
    Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
}

static void func_actor_510900_8013B658(Enemy* arg0, Task* arg1)
{
    if (gGameSession->eventState != 0) {
        func_actor_510900_801355B4(arg0, arg1);
        return;
    }
    func_actor_510900_8013B6A0(arg0, arg1);
}

static void func_actor_510900_8013B6A0(Enemy* arg0, Task* arg1)
{
    GfxCoord*        temp_s1;
    TmdObject*       temp_a1;
    Actor510900Work* temp_s2;
    s32              one;
    s32              sp;

    temp_s2 = arg1->work;
    temp_a1 = arg1->extra.tmd;
    temp_s1 = temp_a1->coords;
    if (temp_s2->field_5A4 != 0) {
        sp  = gSceneCombatState.actorControl;
        one = 1;
        if (sp == one) {
            goto case1;
        }
        if (sp >= 2) {
            goto ge2;
        }
        if (sp == 0) {
            goto case0;
        }
        goto default_body;
    ge2:
        if (sp == 2) {
            goto case2;
        }
        goto default_body;
    case0:
        temp_a1->flags               = 0;
        arg0->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        goto default_body;
    case1:
        no9GolemDrawShadow(arg1);
        func_actor_510900_8013BC38(arg1, temp_s1);
        return;
    case2:
        temp_a1->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        arg0->node.state.parts.flags = one;
        return;
    default_body:
        if (arg0->reactionFlags != 0) {
            func_actor_510900_8013B804(arg1);
        }
        func_actor_510900_80135744(arg1);
        func_actor_510900_8013864C(arg1);
        func_actor_510900_8013B870(arg1);
        func_actor_510900_801387F4(arg1);
        func_actor_510900_80138978(arg1);
        func_actor_510900_80138A9C(arg1);
        func_actor_510900_8013BB20(arg1);
        no9GolemAimHead(arg1);
        if (temp_s2->field_584 != 0) {
            func_actor_510900_80138D38(arg1);
        }
        temp_s1->composeStamp                   = GRAPHICS_COORD_DIRTY;
        arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(temp_s1);
        no9GolemDrawShadow(arg1);
        func_actor_510900_8013BC38(arg1, temp_s1);
        func_actor_510900_80138F44(arg1);
        func_actor_510900_8013BC80(arg1);
    }
}

static void func_actor_510900_8013B804(Task* arg0)
{
    Actor510900Work* work;
    Enemy*           enemy;
    u8               flags;

    enemy = arg0->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = arg0->work;
    if (flags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->field_58E       = 7;
        work->field_590       = 0;
        work->field_5B8       = 1;
    }
    flags = enemy->reactionFlags;
    if (flags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags = flags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
    }
}

static void func_actor_510900_8013B870(Task* arg0)
{
    s16 temp_v1;

    temp_v1 = ((Actor510900Work*)arg0->work)->field_58E;
    switch (temp_v1) {
        case 0:
            func_actor_510900_80135E90(arg0);
            return;
        case 1:
            func_actor_510900_80136184(arg0);
            return;
        case 2:
            func_actor_510900_80136B70(arg0);
            return;
        case 3:
            func_actor_510900_80137008(arg0);
            return;
        case 4:
            func_actor_510900_801373B8(arg0);
            return;
        case 5:
            func_actor_510900_801375D8(arg0);
            return;
        case 6:
            func_actor_510900_80137868(arg0);
            return;
        case 7:
            func_actor_510900_8013B988(arg0);
            return;
        case 8:
            func_actor_510900_8013BA58(arg0);
            return;
        case 9:
            func_actor_510900_80137E20(arg0);
            return;
        case 10:
            func_actor_510900_80137FBC(arg0);
            return;
        case 11:
            func_actor_510900_80138250(arg0);
            return;
        case 12:
            func_actor_510900_801384C4(arg0);
        default:
            return;
    }
}

static void func_actor_510900_8013B988(Task* arg0)
{
    Actor510900Work* work;
    s32              state;
    s32              rng;

    work  = arg0->work;
    state = work->field_590;
    switch (state) {
        case 0:
            work->field_5A2 = 0;
            work->field_586 = 0x13;
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->field_586 = 0x14;
                work->field_590 = 1;
                work->field_5B8 = 0;
            }
            break;
        case 1:
            if (work->field_58A >= 0x3B) {
                work->field_58E = state;
                work->field_590 = 0;
                work->field_586 = state;
                work->field_59C =
                    D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                gRandomLcgState = rng;
            }
            break;
    }
}

static void func_actor_510900_8013BA58(Task* arg0)
{
    Actor510900Work* work;
    s32              state;
    Enemy*           enemy;
    s32              rng;

    work  = arg0->work;
    state = work->field_590;
    enemy = arg0->spawnArg2.pointer;
    switch (state) {
        case 0:
            work->field_586 = 0x11;
            work->field_5A2 = 0;
            work->field_590 = 1;
            break;
        case 1:
            if (work->field_58A >= 0x50) {
                if (enemy->hp <= 0) {
                    work->field_58E = 0xC;
                    work->field_590 = 0;
                    work->field_586 = 0x18;
                } else {
                    work->field_58E = state;
                    work->field_590 = 0;
                    work->field_586 = state;
                    work->field_59C =
                        D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                    gRandomLcgState = rng;
                }
            }
            break;
    }
}

static void func_actor_510900_8013BB20(Task* arg0)
{
    Actor510900Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    if (work->field_586 != work->field_588) {
        work->field_588 = work->field_586;
        work->field_58A = 0;
        value           = D_actor_510900_80167B38[work->field_586];
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->rig.anim, i, work->field_586, 0, value);
        }
    } else {
        work->field_58A++;
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
}

#include "../../shared/no9_golem_draw_shadow.inc.c"

void func_actor_510900_8013BC38(Task* arg0, GfxCoord* arg1)
{
    VECTOR pos;

    pos.vx = arg1->workm.t[0];
    pos.vy = arg1->workm.t[1];
    pos.vz = arg1->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &pos, 0, 0);
}

static void func_actor_510900_8013BC80(Task* arg0)
{
    Actor510900Work* work = arg0->work;

    if ((u16)(work->field_594 - 1) < 2) {
        if (--work->field_598 <= 0) {
            work->field_594 = 3;
            if (work->field_57C != 0) {
                SndEvt_EnqueueType7(work->field_57C, 0);
                work->field_57C = 0;
            }
            if (work->field_580 != 0) {
                SndEvt_EnqueueType7(work->field_580, 0);
                work->field_580 = 0;
            }
            work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    if (work->field_594 != work->field_596) {
        if (work->field_564 != NULL) {
            work->field_564[0xD] = work->field_594;
        }
        work->field_596 = work->field_594;
    }
}

s32 func_actor_510900_8013BD5C(Task* arg0)
{
    if (gPlayerStatus.hp > 0) {
        ((Actor510900Work*)arg0->work)->field_5BC = 1;
    }
    return 0;
}

s32 func_actor_510900_8013BD84(Task* arg0, s32 arg1, AnimationPlayRequest* arg2)
{
    Actor510900Work* work;
    s32              blend;
    s32              i;

    blend           = (arg2->blend != ANIMATION_BLEND_RESET) * 8;
    work            = arg0->work;
    work->field_586 = arg2->animationId + 0x1B;
    for (i = 1; i < 0x13; i++) {
        func_800B4114(&work->rig.anim, i, work->field_586, 0, blend);
    }
    work->field_58A = 0;
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"

/// Message 0x7D5 handler: switches the model's 0x80 flag: set when `arg2` is 0,
/// cleared for any other value. The message id itself is unused.
s32 func_actor_510900_8013BE64(Task* task, s32 msgId, s32 arg2)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    if (arg2 == 0) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags = 0;
    }
    return 0;
}

s16 func_actor_510900_8013BE84(Task* arg0)
{
    return ((Actor510900Work*)arg0->work)->field_592;
}

void func_actor_510900_8013BE98(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_510900_8013BEEC, func_actor_510900_801395AC };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_510900_8013BEEC(Enemy* enemy, Task* task)
{
    TmdObject*       obj;
    Actor510900Work* work;
    GfxCoord*        coord;

    obj                 = task->extra.tmd;
    work                = (Actor510900Work*)task->parent->work;
    coord               = obj->coords;
    obj->clutRowOffset += 2;
    tmdProcessStream(obj);
    tmdProcessStream(obj);
    coord->parent       = &task->parent->extra.tmd->coords[12];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj->lightMtx       = &work->field_45C;
    obj->colorMtx       = &work->field_43C;
    task->state         = 1;
}

void func_actor_510900_8013BF90(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_510900_8013BFE4, func_actor_510900_8013C034 };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_510900_8013BFE4(Enemy* enemy, Task* task)
{
    TmdObject*       obj;
    Actor510900Work* work;

    obj                 = task->extra.tmd;
    work                = (Actor510900Work*)task->parent->work;
    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj->coords->parent = &task->parent->extra.tmd->coords[8];
    obj->lightMtx       = &work->field_45C;
    obj->colorMtx       = &work->field_43C;
    task->state         = 1;
}

static void func_actor_510900_8013C034(Enemy* enemy, Task* task)
{
    task->extra.tmd->flags                = task->parent->extra.tmd->flags;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(task->extra.tmd->coords);
}

void func_actor_510900_8013C090(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_510900_8013C0E4, func_actor_510900_8013C134 };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_510900_8013C0E4(Enemy* enemy, Task* task)
{
    TmdObject*       obj;
    Actor510900Work* work;

    obj                 = task->extra.tmd;
    work                = (Actor510900Work*)task->parent->work;
    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj->coords->parent = &task->parent->extra.tmd->coords[3];
    obj->lightMtx       = &work->field_45C;
    obj->colorMtx       = &work->field_43C;
    task->state         = 1;
}

static void func_actor_510900_8013C134(Enemy* enemy, Task* task)
{
    task->extra.tmd->flags                = task->parent->extra.tmd->flags;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(task->extra.tmd->coords);
}

/// Runs the enemy's current state handler, copying the table onto the stack
/// before the call.
void func_actor_510900_8013C190(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_510900_80131ECC;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

void func_actor_510900_8013C1EC(Task* task)
{
    GpEnemyTaskFunc fns[2] = {
        func_actor_510900_8013A5B8,
        func_actor_510900_8013A85C,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Reports whether the camera has cut away from every view this child runs in.
/// Until then it returns 1 and the caller keeps ticking the animation; on the
/// frame all three views miss it hides the model, releases the task it holds
/// and returns 0.
static s32 func_actor_510900_8013C240(Task* task)
{
    TmdObject*            obj;
    Actor510900ChildAnim* work;
    Enemy*                ctx;
    s32                   i;
    u8                    misses;
    u8                    view;

    obj    = task->extra.tmd;
    work   = (Actor510900ChildAnim*)task->work;
    ctx    = task->spawnArg2.pointer;
    misses = 0;
    view   = Gp_GetViewIndex();
    for (i = 0; i < 3; i++) {
        if (view != D_actor_510900_80167CD8[work->field_334][i]) {
            misses++;
        }
    }

    if (misses != 3) {
        return 1;
    }

    obj->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->obj2BC.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj2F4.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->field_32C != NULL) {
        work->field_32C->state = 2;
        work->field_32C        = NULL;
    }
    if (work->field_332 != 0) {
        work->field_332--;
    }
    return 0;
}

static void func_actor_510900_8013C338(Task* arg0, GfxCoord* arg1)
{
    VECTOR pos;

    pos.vx = arg1->workm.t[0];
    pos.vy = arg1->workm.t[1];
    pos.vz = arg1->workm.t[2];
    func_800D7A9C(arg0->extra.tmd, &pos, 0, 3);
}

static void func_actor_510900_8013C380(Task* arg0)
{
    Enemy*                enemy = arg0->spawnArg2.pointer;
    Actor510900ChildAnim* work  = arg0->work;

    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->obj2BC);
    Gp_UnlinkObj(&work->obj2F4);
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_510900_8013C3DC(Task* task)
{
    GpEnemyTaskFunc fns[2] = {
        func_actor_510900_8013AD90,
        func_actor_510900_8013AF38,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_510900_8013C430(Task* arg0)
{
    Enemy*                enemy = arg0->spawnArg2.pointer;
    Actor510900ChildWork* work  = arg0->work;

    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj38);
    Gp_DestroyEnemy(enemy, arg0);
}
