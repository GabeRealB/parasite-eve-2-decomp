#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actor_207200_private.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
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

#include "overlay.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern EnemyParams   D_actor_207200_8014E7D4;
extern AnimationSet* D_actor_207200_80153ED4[13];
/// `field_492` value for frames 20..39 of helper stage 1, indexed by frame - 20.
extern s16 D_actor_207200_80153F20[];
/// Base damage the shatter hit doubles, before a 0..99 roll is added.
/// Effect offsets `func_800FDB18` is handed for the two hit tables.
extern SVECTOR D_actor_207200_80153F08;
extern SVECTOR D_actor_207200_80153F10;

/// Work storage of the large enemy variant, including all five collision bodies.
///
/// Each body owns the adjacent contact array. Both setup and state handlers
/// use this layout; the two six-entry arrays occupy the full space between
/// their body and the following body.
typedef struct {
    /* 0x000 */ byte                  pad_0[0x14];
    /* 0x014 */ byte                  field_14[0x118];
    /* 0x12C */ byte                  field_12C[0x70];
    /* 0x19C */ MATRIX                field_19C;
    /* 0x1BC */ MATRIX                field_1BC;
    /* 0x1DC */ WorldCollisionBody    obj1;
    /* 0x1FC */ WorldCollisionContact rec1[1];
    /* 0x214 */ WorldCollisionBody    obj2;
    /* 0x234 */ WorldCollisionContact rec2[6];
    /* 0x2C4 */ WorldCollisionBody    obj3;
    /* 0x2E4 */ WorldCollisionContact rec3[6];
    /* 0x374 */ WorldCollisionBody    obj4;
    /* 0x394 */ WorldCollisionContact rec4[1];
    /* 0x3AC */ WorldCollisionBody    obj5;
    /* 0x3CC */ WorldCollisionContact rec5[1];
    /* 0x3E4 */ EffectSpawnArg        eff0;
    /* 0x3EC */ EffectSpawnArg        eff1;
    /* 0x3F4 */ EffectSpawnArg        eff2;
    /* 0x3FC */ byte                  pad_3FC[0x50];
    /* 0x44C */ SVECTOR               field_44C;
    /* 0x454 */ s32                   field_454;
    /* 0x458 */ s32                   field_458;
    /* 0x45C */ s32                   field_45C;
    /* 0x460 */ byte                  pad_460[4];
    /* 0x464 */ MATRIX                field_464;
    /* 0x484 */ s16                   field_484;
    /* 0x486 */ s16                   field_486;
    /* 0x488 */ s16                   field_488;
    /* 0x48A */ u16                   field_48A;
    /* 0x48C */ s16                   field_48C;
    /* 0x48E */ u16                   field_48E;
    /* 0x490 */ u16                   field_490;
    /* 0x492 */ s16                   field_492;
    /* 0x494 */ s16                   field_494;
    /* 0x496 */ byte                  pad_496[2];
    /* 0x498 */ s16                   field_498;
    /* 0x49A */ s16                   field_49A;
    /* 0x49C */ s16                   field_49C;
    /* 0x49E */ s16                   field_49E;
    /* 0x4A0 */ s16                   field_4A0;
    /* 0x4A2 */ s16                   field_4A2;
    /* 0x4A4 */ s16                   field_4A4;
    /* 0x4A6 */ s16                   field_4A6;
    /* 0x4A8 */ s16                   field_4A8;
    /* 0x4AA */ s16                   field_4AA;
} _Actor207200LargeWork;
STATIC_ASSERT_SIZEOF(_Actor207200LargeWork, 0x4AC);

/// 0x48-byte block `func_actor_207200_8014BEF4` takes from the scratch stack:
/// `d` receives the `func_800E0C10` push-back, then the offset to the player
/// or to a push record, which `norm` holds normalised.
typedef struct Actor207200DmgScratch {
    /* 0x00 */ byte                pad_0[0x20];
    /* 0x20 */ WorldCollisionDelta d;
    /* 0x30 */ byte                pad_30[8];
    /* 0x38 */ VECTOR              norm;
} Actor207200DmgScratch;
STATIC_ASSERT_SIZEOF(Actor207200DmgScratch, 0x48);

/// The records closing three of the overlay's model streams, handed to the
/// spawned effect as its model through `D_800626EC[5].data.model`.
static TmdSource _gActor207200CreepingStrangerBurstLeg;
static TmdSource _gActor207200CreepingStrangerBurstArm;
static TmdSource _gActor207200CreepingStrangerBurstHead;
extern SVECTOR   D_actor_207200_80153F18;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_207200_8014B278(Enemy* arg0, Task* arg1);
static void func_actor_207200_8014C870(Task* arg0, s32 arg1);
static s32  func_actor_207200_8014CE20(GfxCoord* arg0, u32* arg1);
static void func_actor_207200_8014CA84(Enemy* arg0, Task* arg1);
static void func_actor_207200_8014D2DC(Enemy* arg0, Task* arg1);
static void func_actor_207200_8014CFEC(Task* arg0);
static void func_actor_207200_8014D128(Task* arg0);
static void func_actor_207200_8014D41C(Task* arg0);
static void func_actor_207200_8014D49C(Task* arg0);
static void func_actor_207200_8014D5C4(Task* arg0);
static void func_actor_207200_8014D65C(Task* arg0);
static void func_actor_207200_8014D70C(Enemy* arg0, Task* task);
static void func_actor_207200_8014D77C(Task* task);
static void func_actor_207200_8014D7E8(Task* arg0);
static void func_actor_207200_8014D8DC(Task* arg0);
static void func_actor_207200_8014D97C(Task* arg0, GfxCoord* arg1);
static void func_actor_207200_8014DAF8(Task* dst, Task* src);
static void func_actor_207200_8014DB4C(Task* arg0);

/// The large enemy's state handlers - spawn, live tick and teardown tick -
/// which `func_actor_207200_8014D280` dispatches through by task state.
static const EnemyTaskFuncTable3 D_actor_207200_80149E30 = {
    { func_actor_207200_8014B278, func_actor_207200_8014D2DC, func_actor_207200_8014CA84 }
};

void func_actor_207200_8014D280(Task*);

EnemyParams D_actor_207200_8014E7D4 = { D_actor_207200_8014E7CC, 250, 15, 48, 1, 50, 10, 0, 0 };

static TmdBone _gActor207200CreepingStrangerBodySkeleton[7] = {
#include "assets/creeping_stranger_body_skeleton.inc"
};

static u32 _gActor207200CreepingStrangerBodyPartVerts[7] = {
#include "assets/creeping_stranger_body_partVerts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBodyVerts[125] = {
#include "assets/creeping_stranger_body_verts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBodyNormals[136] = {
#include "assets/creeping_stranger_body_normals.inc"
};

static u32 _gActor207200CreepingStrangerBodyStream[1592] = {
#include "assets/creeping_stranger_body_stream.inc"
};

static TmdSource _gActor207200CreepingStrangerBody = {
    0,
    8356,
    2504,
    7,
    _gActor207200CreepingStrangerBodyPartVerts,
    _gActor207200CreepingStrangerBodyVerts,
    _gActor207200CreepingStrangerBodyNormals,
    _gActor207200CreepingStrangerBodySkeleton,
    _gActor207200CreepingStrangerBodyStream,
};

static TmdBone _gActor207200CreepingStrangerBurstLegSkeleton[1] = {
#include "assets/creeping_stranger_burst_leg_skeleton.inc"
};

static u32 _gActor207200CreepingStrangerBurstLegPartVerts[1] = {
#include "assets/creeping_stranger_burst_leg_partVerts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstLegVerts[8] = {
#include "assets/creeping_stranger_burst_leg_verts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstLegNormals[9] = {
#include "assets/creeping_stranger_burst_leg_normals.inc"
};

static u32 _gActor207200CreepingStrangerBurstLegStream[61] = {
#include "assets/creeping_stranger_burst_leg_stream.inc"
};

static TmdSource _gActor207200CreepingStrangerBurstLeg = {
    0,
    368,
    0,
    1,
    _gActor207200CreepingStrangerBurstLegPartVerts,
    _gActor207200CreepingStrangerBurstLegVerts,
    _gActor207200CreepingStrangerBurstLegNormals,
    _gActor207200CreepingStrangerBurstLegSkeleton,
    _gActor207200CreepingStrangerBurstLegStream,
};

static TmdBone _gActor207200CreepingStrangerBurstArmSkeleton[1] = {
#include "assets/creeping_stranger_burst_arm_skeleton.inc"
};

static u32 _gActor207200CreepingStrangerBurstArmPartVerts[1] = {
#include "assets/creeping_stranger_burst_arm_partVerts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstArmVerts[20] = {
#include "assets/creeping_stranger_burst_arm_verts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstArmNormals[22] = {
#include "assets/creeping_stranger_burst_arm_normals.inc"
};

static u32 _gActor207200CreepingStrangerBurstArmStream[195] = {
#include "assets/creeping_stranger_burst_arm_stream.inc"
};

static TmdSource _gActor207200CreepingStrangerBurstArm = {
    0,
    1272,
    0,
    1,
    _gActor207200CreepingStrangerBurstArmPartVerts,
    _gActor207200CreepingStrangerBurstArmVerts,
    _gActor207200CreepingStrangerBurstArmNormals,
    _gActor207200CreepingStrangerBurstArmSkeleton,
    _gActor207200CreepingStrangerBurstArmStream,
};

static TmdBone _gActor207200CreepingStrangerBurstHeadSkeleton[1] = {
#include "assets/creeping_stranger_burst_head_skeleton.inc"
};

static u32 _gActor207200CreepingStrangerBurstHeadPartVerts[1] = {
#include "assets/creeping_stranger_burst_head_partVerts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstHeadVerts[33] = {
#include "assets/creeping_stranger_burst_head_verts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstHeadNormals[40] = {
#include "assets/creeping_stranger_burst_head_normals.inc"
};

static u32 _gActor207200CreepingStrangerBurstHeadStream[316] = {
#include "assets/creeping_stranger_burst_head_stream.inc"
};

static TmdSource _gActor207200CreepingStrangerBurstHead = {
    0,
    2116,
    0,
    1,
    _gActor207200CreepingStrangerBurstHeadPartVerts,
    _gActor207200CreepingStrangerBurstHeadVerts,
    _gActor207200CreepingStrangerBurstHeadNormals,
    _gActor207200CreepingStrangerBurstHeadSkeleton,
    _gActor207200CreepingStrangerBurstHeadStream,
};

static AnimationPackedPose _gActor207200Animation07BF8Bank1[8] = {
#include "assets/actor_207200_animation_07BF8_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation07BF8Bank4[27] = {
#include "assets/actor_207200_animation_07BF8_bank4.inc"
};

static AnimationRecord _gActor207200Animation07BF8Records[72] = {
#include "assets/actor_207200_animation_07BF8_records.inc"
};

static u16 _gActor207200Animation07BF8Indices[8] = {
#include "assets/actor_207200_animation_07BF8_indices.inc"
};

static AnimationSet _gActor207200Animation07BF8 = {
    _gActor207200Animation07BF8Records,
    _gActor207200Animation07BF8Indices,
    { NULL, _gActor207200Animation07BF8Bank1, NULL, NULL, _gActor207200Animation07BF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation07F34Bank1[16] = {
#include "assets/actor_207200_animation_07F34_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation07F34Bank4[55] = {
#include "assets/actor_207200_animation_07F34_bank4.inc"
};

static AnimationRecord _gActor207200Animation07F34Records[90] = {
#include "assets/actor_207200_animation_07F34_records.inc"
};

static u16 _gActor207200Animation07F34Indices[8] = {
#include "assets/actor_207200_animation_07F34_indices.inc"
};

static AnimationSet _gActor207200Animation07F34 = {
    _gActor207200Animation07F34Records,
    _gActor207200Animation07F34Indices,
    { NULL, _gActor207200Animation07F34Bank1, NULL, NULL, _gActor207200Animation07F34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation08248Bank1[14] = {
#include "assets/actor_207200_animation_08248_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation08248Bank4[55] = {
#include "assets/actor_207200_animation_08248_bank4.inc"
};

static AnimationRecord _gActor207200Animation08248Records[86] = {
#include "assets/actor_207200_animation_08248_records.inc"
};

static u16 _gActor207200Animation08248Indices[8] = {
#include "assets/actor_207200_animation_08248_indices.inc"
};

static AnimationSet _gActor207200Animation08248 = {
    _gActor207200Animation08248Records,
    _gActor207200Animation08248Indices,
    { NULL, _gActor207200Animation08248Bank1, NULL, NULL, _gActor207200Animation08248Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation08540Bank1[14] = {
#include "assets/actor_207200_animation_08540_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation08540Bank4[51] = {
#include "assets/actor_207200_animation_08540_bank4.inc"
};

static AnimationRecord _gActor207200Animation08540Records[83] = {
#include "assets/actor_207200_animation_08540_records.inc"
};

static u16 _gActor207200Animation08540Indices[8] = {
#include "assets/actor_207200_animation_08540_indices.inc"
};

static AnimationSet _gActor207200Animation08540 = {
    _gActor207200Animation08540Records,
    _gActor207200Animation08540Indices,
    { NULL, _gActor207200Animation08540Bank1, NULL, NULL, _gActor207200Animation08540Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation08B98Bank1[31] = {
#include "assets/actor_207200_animation_08B98_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation08B98Bank4[121] = {
#include "assets/actor_207200_animation_08B98_bank4.inc"
};

static AnimationRecord _gActor207200Animation08B98Records[178] = {
#include "assets/actor_207200_animation_08B98_records.inc"
};

static u16 _gActor207200Animation08B98Indices[8] = {
#include "assets/actor_207200_animation_08B98_indices.inc"
};

static AnimationSet _gActor207200Animation08B98 = {
    _gActor207200Animation08B98Records,
    _gActor207200Animation08B98Indices,
    { NULL, _gActor207200Animation08B98Bank1, NULL, NULL, _gActor207200Animation08B98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation09018Bank1[21] = {
#include "assets/actor_207200_animation_09018_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation09018Bank4[90] = {
#include "assets/actor_207200_animation_09018_bank4.inc"
};

static AnimationRecord _gActor207200Animation09018Records[121] = {
#include "assets/actor_207200_animation_09018_records.inc"
};

static u16 _gActor207200Animation09018Indices[8] = {
#include "assets/actor_207200_animation_09018_indices.inc"
};

static AnimationSet _gActor207200Animation09018 = {
    _gActor207200Animation09018Records,
    _gActor207200Animation09018Indices,
    { NULL, _gActor207200Animation09018Bank1, NULL, NULL, _gActor207200Animation09018Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation093ECBank1[19] = {
#include "assets/actor_207200_animation_093EC_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation093ECBank4[72] = {
#include "assets/actor_207200_animation_093EC_bank4.inc"
};

static AnimationRecord _gActor207200Animation093ECRecords[102] = {
#include "assets/actor_207200_animation_093EC_records.inc"
};

static u16 _gActor207200Animation093ECIndices[8] = {
#include "assets/actor_207200_animation_093EC_indices.inc"
};

static AnimationSet _gActor207200Animation093EC = {
    _gActor207200Animation093ECRecords,
    _gActor207200Animation093ECIndices,
    { NULL, _gActor207200Animation093ECBank1, NULL, NULL, _gActor207200Animation093ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation09704Bank1[14] = {
#include "assets/actor_207200_animation_09704_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation09704Bank4[59] = {
#include "assets/actor_207200_animation_09704_bank4.inc"
};

static AnimationRecord _gActor207200Animation09704Records[83] = {
#include "assets/actor_207200_animation_09704_records.inc"
};

static u16 _gActor207200Animation09704Indices[8] = {
#include "assets/actor_207200_animation_09704_indices.inc"
};

static AnimationSet _gActor207200Animation09704 = {
    _gActor207200Animation09704Records,
    _gActor207200Animation09704Indices,
    { NULL, _gActor207200Animation09704Bank1, NULL, NULL, _gActor207200Animation09704Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation099F4Bank1[14] = {
#include "assets/actor_207200_animation_099F4_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation099F4Bank4[54] = {
#include "assets/actor_207200_animation_099F4_bank4.inc"
};

static AnimationRecord _gActor207200Animation099F4Records[78] = {
#include "assets/actor_207200_animation_099F4_records.inc"
};

static u16 _gActor207200Animation099F4Indices[8] = {
#include "assets/actor_207200_animation_099F4_indices.inc"
};

static AnimationSet _gActor207200Animation099F4 = {
    _gActor207200Animation099F4Records,
    _gActor207200Animation099F4Indices,
    { NULL, _gActor207200Animation099F4Bank1, NULL, NULL, _gActor207200Animation099F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation09D08Bank1[13] = {
#include "assets/actor_207200_animation_09D08_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation09D08Bank4[59] = {
#include "assets/actor_207200_animation_09D08_bank4.inc"
};

static AnimationRecord _gActor207200Animation09D08Records[85] = {
#include "assets/actor_207200_animation_09D08_records.inc"
};

static u16 _gActor207200Animation09D08Indices[8] = {
#include "assets/actor_207200_animation_09D08_indices.inc"
};

static AnimationSet _gActor207200Animation09D08 = {
    _gActor207200Animation09D08Records,
    _gActor207200Animation09D08Indices,
    { NULL, _gActor207200Animation09D08Bank1, NULL, NULL, _gActor207200Animation09D08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation09FE4Bank1[15] = {
#include "assets/actor_207200_animation_09FE4_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation09FE4Bank4[48] = {
#include "assets/actor_207200_animation_09FE4_bank4.inc"
};

static AnimationRecord _gActor207200Animation09FE4Records[76] = {
#include "assets/actor_207200_animation_09FE4_records.inc"
};

static u16 _gActor207200Animation09FE4Indices[8] = {
#include "assets/actor_207200_animation_09FE4_indices.inc"
};

static AnimationSet _gActor207200Animation09FE4 = {
    _gActor207200Animation09FE4Records,
    _gActor207200Animation09FE4Indices,
    { NULL, _gActor207200Animation09FE4Bank1, NULL, NULL, _gActor207200Animation09FE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation0A080Bank1[2] = {
#include "assets/actor_207200_animation_0A080_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation0A080Bank4[5] = {
#include "assets/actor_207200_animation_0A080_bank4.inc"
};

static AnimationRecord _gActor207200Animation0A080Records[14] = {
#include "assets/actor_207200_animation_0A080_records.inc"
};

static u16 _gActor207200Animation0A080Indices[8] = {
#include "assets/actor_207200_animation_0A080_indices.inc"
};

static AnimationSet _gActor207200Animation0A080 = {
    _gActor207200Animation0A080Records,
    _gActor207200Animation0A080Indices,
    { NULL, _gActor207200Animation0A080Bank1, NULL, NULL, _gActor207200Animation0A080Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_207200_80153EC8 = { { { TASK_BODY_TMD, 96 } }, func_actor_207200_8014D280, { .model = &_gActor207200CreepingStrangerBody } };

AnimationSet* D_actor_207200_80153ED4[13] = {
    NULL,
    &_gActor207200Animation07BF8,
    &_gActor207200Animation07F34,
    &_gActor207200Animation08248,
    &_gActor207200Animation08540,
    &_gActor207200Animation08B98,
    &_gActor207200Animation09018,
    &_gActor207200Animation093EC,
    &_gActor207200Animation09704,
    &_gActor207200Animation099F4,
    &_gActor207200Animation09D08,
    &_gActor207200Animation09FE4,
    &_gActor207200Animation0A080,
};

SVECTOR D_actor_207200_80153F08 = { 0 };

SVECTOR D_actor_207200_80153F10 = { 0, -100, -150, 0 };

SVECTOR D_actor_207200_80153F18 = { 0 };

s16 D_actor_207200_80153F20[20] = {
    0,
    3,
    6,
    9,
    12,
    15,
    18,
    21,
    24,
    27,
    24,
    21,
    18,
    15,
    12,
    9,
    6,
    3,
    0,
    0,
};

static void            func_actor_207200_8014B628(Task* arg0);
static void            func_actor_207200_8014B87C(Task* arg0);
static void            func_actor_207200_8014BEF4(Task* arg0);
static __inline__ void Actor207200_TickAnim(Task* arg0);
static __inline__ void Actor207200_UpdateColor(Enemy* enemy, Task* actor);

static void func_actor_207200_8014B278(Enemy* arg0, Task* arg1)
{
    _Actor207200LargeWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              part6;
    GfxCoord*              part3;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x4ACU, false);
    part6 = coord + 6;
    part3 = coord + 3;
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_1BC;
    obj->colorMtx       = &work->field_19C;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = coord;
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &D_actor_207200_8014E7D4;
    arg0->recs                   = work->rec3;
    arg0->hp                     = (u16)D_actor_207200_8014E7D4.hpMax;
    work->field_44C.vy           = (coord)->param.rot.vy;
    animationInitContext((AnimationContext*)work, D_actor_207200_80153ED4, obj,
                         (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->field_12C, (AnimationSlot*)work->field_14);
    for (i = 1; i < 7; i++) {
        animationResetSlot((AnimationContext*)work, i, 1);
    }
    (Gp_IncStateF0Ref)(0);

    work->field_48C = 1;
    work->field_48E = 1;
    work->field_4A4 = 0;
    work->field_4A6 = 0;
    work->field_488 = 0;
    work->field_494 = 0;
    work->field_49E = 0;

    work->obj1.coord            = coord;
    work->obj1.context.contacts = work->rec1;
    work->obj1.pos.vx           = 0;
    work->obj1.pos.vy           = 0;
    work->obj1.pos.vz           = 0;
    work->obj1.key              = 0;
    work->obj1.radius           = 0x7D0;
    work->obj1.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, ARRAY_SIZE(work->rec1), 0);

    work->obj2.pos.vy           = -0x12C;
    work->obj2.pos.vz           = -0xB4;
    work->obj2.coord            = coord;
    work->obj2.context.contacts = work->rec2;
    work->obj2.pos.vx           = 0;
    work->obj2.key              = 0x3002B;
    work->obj2.radius           = 0x12C;
    work->obj2.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->obj1.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->rec2, ARRAY_SIZE(work->rec2), 0);

    work->obj3.coord            = part3;
    work->obj3.context.contacts = work->rec3;
    work->obj3.pos.vx           = 0;
    work->obj3.pos.vy           = 0;
    work->obj3.pos.vz           = 0;
    work->obj3.key              = 0x3002B;
    work->obj3.radius           = 0x96;
    work->obj3.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->obj2.flags           |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(2, &work->obj3);
    Gp_InitRec18Table(work->rec3, ARRAY_SIZE(work->rec3), 0);

    work->obj4.coord            = part3;
    work->obj4.context.contacts = work->rec4;
    work->obj4.pos.vx           = 0;
    work->obj4.pos.vy           = 0x50;
    work->obj4.pos.vz           = 0x8C;
    work->obj3.flags           |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj4.key              = Gp_PackPair(D_actor_207200_8014E7CC, 0);
    work->obj4.radius           = 0x12C;
    work->obj4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec4, ARRAY_SIZE(work->rec4), 0);

    work->obj5.coord            = part6;
    work->obj5.context.contacts = work->rec5;
    work->obj5.pos.vx           = 0xFA;
    work->obj5.pos.vy           = 0;
    work->obj5.pos.vz           = 0;
    work->obj4.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj5.key              = Gp_PackPair(D_actor_207200_8014E7CC, 1);
    work->obj5.radius           = 0x12C;
    work->obj5.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj5);
    Gp_InitRec18Table(work->rec5, ARRAY_SIZE(work->rec5), 0);
    work->obj5.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->eff0.coord      = arg1->extra.tmd->coords + 3;
    work->eff0.spawnArgLo = 0x100;
    work->eff0.spawnArgHi = 1;
    work->eff2.coord      = arg1->extra.tmd->coords + 3;
    work->eff2.spawnArgLo = 0x400;
    work->eff2.spawnArgHi = 3;
    work->eff1.coord      = arg1->extra.tmd->coords + 1;
    work->eff1.spawnArgLo = 0x100;
    work->eff1.spawnArgHi = 1;
    work->field_4A8       = 0;
    arg1->exitCallback    = func_actor_207200_8014DB4C;
    arg1->state++;
}

/// Helper-slot state 0 of the enemy: while it is still alive, a hit recorded in
/// the first render node's table (or the global flag `gSceneCombatState.signals.bytes.enemyAlert`) arms the
/// death sequence - helper state 1, a random 0..89 delay in `field_4AA` and
/// `Gp_ArmStateF0(1)`. Then runs the idle cycle in `field_48C`: state 1 waits
/// 0x5B frames and rolls a 30% chance of moving to 9, which plays the
/// room-tagged sound on frame 5 and returns to 1 after 0x2D frames.
static void func_actor_207200_8014B628(Task* arg0)
{
    _Actor207200LargeWork* work;
    GfxCoord*              obj;
    s32                    id;
    s32                    pan;
    u32                    rnd;
    u16                    hi;

    SCRATCH_STACK_RESERVE_BYTES(8);
    work = (_Actor207200LargeWork*)arg0->work;
    obj  = arg0->extra.tmd->coords;
    if (work->field_4A6 == 0) {
        if (Gp_CountRec18Hi(work->rec1, 0x10000) != 0) {
            work->field_4A2 = 1;
        }
        if (work->field_4A2 != 0 || gSceneCombatState.signals.bytes.enemyAlert != 0) {
            rnd               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            hi                = rnd >> 16;
            work->field_49A   = 0;
            work->field_492   = 0;
            work->field_486   = 1;
            gRandomLcgState   = rnd;
            work->obj1.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_4AA   = hi % 90;
            Gp_ArmStateF0(1);
        }
        Gp_ClearRec18Occupied(work->rec1);
    }
    switch (work->field_48C) {
        case 1:
            work->field_498 = 1;
            work->field_492 = 0;
            if ((s16)work->field_490 >= 0x5B) {
                work->field_490 = 0;
                work->field_48E = 0;
                if (work->field_4A6 == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if ((u16)((gRandomLcgState >> 16) % 100) < 30) {
                        work->field_48C = 9;
                    }
                }
            }
            break;
        case 9:
            if ((s16)work->field_490 == 5) {
                id  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480004;
                pan = (s8)worldCoordGetOriginAudioPan(obj);
                SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(obj));
            }
            if ((s16)work->field_490 >= 0x2D) {
                work->field_48C = 1;
                work->field_490 = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Helper-slot state 1 of the enemy, stepped by `field_49A`. Stage 0 waits out
/// the random delay in `field_4AA`; stage 1 moves to stage 4 once the player is
/// within 0x385 and inside +/-0x200 of the facing angle, otherwise picks a turn
/// direction; stages 2/3 turn the model by `field_484` (+/-25) on frames
/// 30..50 and re-check the angle every 60 frames; stages 4-6 play the
/// room-tagged sounds and toggle the display flags of two render nodes, stage 4
/// rolling a 40% chance of stage 6 before returning to stage 1.
static void func_actor_207200_8014B87C(Task* arg0)
{
    _Actor207200LargeWork* work;
    GfxCoord*              coord;
    s32                    angle;
    u32                    dist;
    s32                    id;
    s16                    state;

    work  = (_Actor207200LargeWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_49A) {
        case 0:
            work->field_48C = 1;
            if ((s16)work->field_490 > work->field_4AA) {
                work->field_49A = 1;
            }
            break;
        case 1:
            angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
            if (work->field_4A6 == 0 && dist < 0x385 && ABS(angle) < 0x200) {
                work->field_492   = 0;
                work->field_49A   = 4;
                work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                break;
            }
            work->field_48C   = 2;
            work->field_498   = 0;
            work->obj3.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if ((u32)(work->field_490 - 20) < 20) {
                work->field_492 = D_actor_207200_80153F20[(s16)work->field_490 - 20];
            } else {
                work->field_492 = 0;
            }
            if ((s16)work->field_490 >= 75) {
                work->field_490 = 0;
                if (work->field_4A6 == 0) {
                    if (ABS(angle) > 0x200 || work->field_494 != 0) {
                        if (angle < 0) {
                            work->field_484 = -25;
                            work->field_49A = 3;
                            work->field_48C = 4;
                        } else {
                            work->field_484 = 25;
                            work->field_49A = 2;
                            work->field_48C = 3;
                        }
                    }
                }
            }
            break;
        case 2:
            state           = 3;
            work->field_492 = 0;
            work->field_48C = state;
            if ((u32)(work->field_490 - 30) < 21) {
                work->field_44C.vx  = 0;
                work->field_44C.vz  = 0;
                work->field_44C.vy += work->field_484;
                RotMatrix(&work->field_44C, &coord->coord);
            }
            if ((s16)work->field_490 >= 60) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
                    if (ABS(angle) < 0x200 || work->field_494 != 0) {
                        work->field_49A = 1;
                        work->field_494 = 0;
                        work->field_490 = 0;
                        work->field_48C = 2;
                    } else if (angle < 0) {
                        work->field_484 = -25;
                        work->field_490 = 0;
                        work->field_49A = 3;
                        work->field_48C = 4;
                    } else {
                        work->field_484 = 25;
                        work->field_490 = 0;
                        work->field_49A = 2;
                        work->field_48C = state;
                    }
                }
            }
            break;
        case 3:
            state           = 4;
            work->field_492 = 0;
            work->field_48C = state;
            if ((u32)(work->field_490 - 30) < 21) {
                work->field_44C.vx  = 0;
                work->field_44C.vz  = 0;
                work->field_44C.vy += work->field_484;
                RotMatrix(&work->field_44C, &coord->coord);
            }
            if ((s16)work->field_490 >= 60) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
                    if (ABS(angle) < 0x200 || work->field_494 != 0) {
                        work->field_49A = 1;
                        work->field_494 = 0;
                        work->field_490 = 0;
                        work->field_48C = 2;
                    } else if (angle < 0) {
                        work->field_484 = -25;
                        work->field_490 = 0;
                        work->field_49A = 3;
                        work->field_48C = state;
                    } else {
                        work->field_484 = 25;
                        work->field_49A = 2;
                        work->field_490 = 0;
                        work->field_48C = 3;
                    }
                }
            }
            break;
        case 4:
            work->field_492 = 0;
            if ((s16)work->field_490 == 30) {
                work->field_4A0 = 0;
                id              = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480002;
                SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((s16)work->field_490 == 42) {
                work->obj4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if ((s16)work->field_490 == 45) {
                work->obj4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->field_4A0 != 0 && (s16)work->field_490 == 45) {
                id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480005;
                SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_48C == 8 && (s16)work->field_490 >= 60) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if ((u16)((gRandomLcgState >> 16) % 100) < 40) {
                        work->field_49A = 6;
                        work->field_48C = 10;
                    } else {
                        work->field_49A = 1;
                        work->field_48C = 2;
                    }
                    work->field_490 = 0;
                }
            } else {
                work->field_48C = 8;
            }
            break;
        case 5:
            work->field_492 = 0;
            if ((s16)work->field_490 == 30) {
                work->obj5.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if ((s16)work->field_490 == 60) {
                work->obj5.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if ((s16)work->field_490 >= 90) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    work->field_49A = 1;
                    work->field_490 = 0;
                    work->field_48C = 2;
                }
            }
            break;
        case 6:
            work->field_48C = 10;
            work->field_492 = 0;
            if ((s16)work->field_490 == 10) {
                id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480006;
                SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((s16)work->field_490 >= 90) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    work->field_49A = 1;
                    work->field_490 = 0;
                    work->field_48C = 2;
                }
            }
            break;
    }
}

/// Per-frame collision handling. Each six-record table's `func_800E0C10`
/// result pushes the model back (1) or snaps it to `field_454` (2). Records of
/// the first table then dispatch on their kind: 1 sets the turn state when the
/// player is off-angle and near, 2 applies damage from the player's distance,
/// 3 pushes the model out of the record's radius. Unless `field_4A6` is set,
/// each 0x20000 record of the second table applies damage too, and some ids
/// end the tick through `func_actor_207200_8014D128` / `8014CFEC`. The tables
/// and, when `field_49A` is set, the two part records are cleared last.
static void func_actor_207200_8014BEF4(Task* arg0)
{
    _Actor207200LargeWork* work;
    Actor207200DmgScratch* sc;
    Actor207200DmgScratch* head;
    GfxCoord*              coord;
    u32                    dist;
    Enemy*                 enemy;
    s32                    i;
    s32                    angle;
    s32                    damage;
    s32                    push;
    s32                    param;
    s32                    n;
    s32                    snd;

    work                                        = (_Actor207200LargeWork*)arg0->work;
    head                                        = SCRATCH_STACK_CURSOR(Actor207200DmgScratch);
    SCRATCH_STACK_CURSOR(Actor207200DmgScratch) = head - 1;
    sc                                          = head - 1;
    coord                                       = arg0->extra.tmd->coords;
    enemy                                       = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->rec3, &head[-1].d, 6, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->d.fixed.vx.halves.integer;
            coord->coord.t[1] += sc->d.fixed.vy.halves.integer;
            coord->coord.t[2] += sc->d.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->field_454;
            coord->coord.t[1] = work->field_458;
            coord->coord.t[2] = work->field_45C;
            if (work->field_4A6 == 0 && work->field_494 == 0) {
                work->field_494 = 1;
            }
            break;
    }
    switch (func_800E0C10(work->rec2, &sc->d, 6, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->d.fixed.vx.halves.integer;
            coord->coord.t[1] += sc->d.fixed.vy.halves.integer;
            coord->coord.t[2] += sc->d.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->field_454;
            coord->coord.t[1] = work->field_458;
            coord->coord.t[2] = work->field_45C;
            if (work->field_4A6 == 0 && work->field_494 == 0) {
                work->field_494 = 1;
            }
            break;
    }
    if (work->field_49E != 0 && --work->field_49E <= 0) {
        work->field_49E = 0;
    }

    for (i = 0; i < 6; i++) {
        switch ((u32)work->rec2[i].key.parts.kind) {
            case 1:
                if (work->field_4A6 == 0 && (u16)work->field_49A - 1U < 3) {
                    angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
                    if (abs(angle) > 0x200 && dist < 2000) {
                        work->field_48C = angle < 0 ? 7 : 6;
                        work->field_490 = 0;
                        work->field_49A = 5;
                    }
                }
                break;
            case 2:
                if (work->field_49E != 0) {
                    break;
                }
                sc->d.vector.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                sc->d.vector.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                sc->d.vector.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                damage          = SquareRoot0(sc->d.vector.vx * sc->d.vector.vx +
                                              sc->d.vector.vy * sc->d.vector.vy +
                                              sc->d.vector.vz * sc->d.vector.vz);
                Gp_GetIdParam0(work->rec2[i].key.value);
                damage = Gp_ComputeDamage(work->rec2[i].key.value, damage, 0, 0);
                func_800FDB18((u16)Gp_GetIdParam1(work->rec2[i].key.value),
                              arg0->extra.tmd->coords + 1, &D_actor_207200_80153F10, &work->eff1);
                n = Gp_GetIdParam2(work->rec2[i].key.value);
                if ((s16)n > 0) {
                    work->field_49E = n;
                }
                if (work->field_4A6 != 0 && arg0->killCountdown == 0) {
                    func_800DA6E8(&enemy->node, damage, 0);
                    if (damage != 0) {
                        arg0->state++;
                        work->field_48C                             = 0xB;
                        SCRATCH_STACK_CURSOR(Actor207200DmgScratch) = SCRATCH_STACK_CURSOR(Actor207200DmgScratch) + 1;
                        return;
                    }
                } else {
                    func_800DA6E8(&enemy->node, 0, 0);
                }
                if (work->field_486 == 0) {
                    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480006;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    work->field_48C = 5;
                    work->field_490 = 0;
                    work->field_486 = 4;
                    work->field_492 = 0;
                    work->field_4A2 = 0;
                }
                break;
            case 3:
                sc->d.vector.vx = coord->workm.t[0] - work->rec2[i].point.vx;
                sc->d.vector.vy = 0;
                sc->d.vector.vz = coord->workm.t[2] - work->rec2[i].point.vz;
                damage          = work->rec2[i].distance -
                         SquareRoot0(sc->d.vector.vx * sc->d.vector.vx + sc->d.vector.vz * sc->d.vector.vz);
                // Clamped through a second variable: clamping `damage` in
                // place drops the copy the original makes.
                push = damage;
                if (damage <= 0) {
                    push = 0;
                }
                damage          = push;
                sc->d.vector.vx = coord->workm.t[0] - work->rec2[i].point.vx;
                sc->d.vector.vy = coord->workm.t[1] - work->rec2[i].point.vy;
                sc->d.vector.vz = coord->workm.t[2] - work->rec2[i].point.vz;
                VectorNormal(&sc->d.vector, &sc->norm);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &sc->norm, &sc->d.vector);
                if (work->field_48C == 2) {
                    coord->coord.t[0] += (damage * sc->d.vector.vx) >> 12;
                    n                  = damage * sc->d.vector.vy;
                    if (n < 0) {
                        coord->coord.t[1] += n >> 12;
                    }
                    coord->coord.t[2] += (damage * sc->d.vector.vz) >> 12;
                }
                break;
        }
    }

    if (work->field_4A6 == 0) {
        for (i = 0; i < 6; i++) {
            if ((work->rec3[i].key.value & 0xFFFF0000) != 0x20000) {
                continue;
            }
            if (work->field_49E != 0) {
                break;
            }
            sc->d.vector.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->d.vector.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            sc->d.vector.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage          = SquareRoot0(sc->d.vector.vx * sc->d.vector.vx + sc->d.vector.vy * sc->d.vector.vy +
                                          sc->d.vector.vz * sc->d.vector.vz);
            param           = Gp_GetIdParam0(work->rec3[i].key.value);
            damage          = Gp_ComputeDamage(work->rec3[i].key.value, damage, 0, 0);
            switch ((u16)param) {
                case 1:
                case 4:
                case 5:
                case 6:
                    Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 2, NULL);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    func_800DA6E8(&enemy->node, D_actor_207200_8014E7D4.hpMax * 2 + (u16)((gRandomLcgState >> 16) % 100), 0);
                    func_actor_207200_8014D128(arg0);
                    work->field_4A8 = 1;
                    arg0->state++;
                    return;
                case 8:
                case 9:
                    Gp_SetObjFlag2(enemy, work->rec3[i].key.value, 0);
                default:
                    if ((Gp_RollEnemyChance(arg0->spawnArg2.pointer, work->rec3[i].key.value, 0) != 0 ||
                         work->field_486 == 3) &&
                        damage != 0) {
                        func_800E2C78(enemy, work->rec3[i].key.value, damage, 0);
                        func_actor_207200_8014CFEC(arg0);
                        return;
                    }
                    func_800E2C78(enemy, work->rec3[i].key.value, damage, 0);
                    func_actor_207200_8014C870(arg0, damage);
                    func_800FDB18((u16)Gp_GetIdParam1(work->rec3[i].key.value),
                                  arg0->extra.tmd->coords + 3, &D_actor_207200_80153F08, &work->eff0);
                    n = Gp_GetIdParam2(work->rec3[i].key.value);
                    if ((s16)n > 0) {
                        work->field_49E = n;
                    }
                    break;
            }
        }
    } else {
        work->obj4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj5.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    Gp_ClearRec18Occupied(work->rec2);
    Gp_ClearRec18Occupied(work->rec3);
    if (work->field_49A != 0) {
        if (Gp_FindRec18(work->rec4, 0) != 0) {
            work->field_4A0   = 1;
            work->obj4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            Gp_ClearRec18Occupied(work->rec4);
        }
        if (Gp_FindRec18(work->rec5, 0) != 0) {
            work->obj5.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            Gp_ClearRec18Occupied(work->rec5);
        }
    }
    SCRATCH_STACK_CURSOR(Actor207200DmgScratch) = SCRATCH_STACK_CURSOR(Actor207200DmgScratch) + 1;
}

/// Ticks the shatter timers the enemy runs while it dies. Every time a timer
/// runs out the work is armed with a fresh sound effect - one per stage of the
/// death animation - and the frame it is handed plays.
static void func_actor_207200_8014C870(Task* arg0, s32 arg1)
{
    _Actor207200LargeWork* work;
    Enemy*                 ctx;
    GfxCoord*              coord;
    EffectSpawnArg*        effArg;
    s32                    snd;

    work  = arg0->work;
    ctx   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;

    ctx->hp = (s16)((u16)ctx->hp - arg1);
    func_800DA6E8(&ctx->node, arg1, 0);
    if ((s16)ctx->hp <= 0) {
        if (work->field_4A6 == 0) {
            ctx->hp = 1;
            snd     = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480003;
            SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            effArg = &work->eff2;
            func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
            func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
            work->obj4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj5.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            Gp_UnlinkObj(&work->obj3);
            work->field_4A6     = 1;
            ctx->recs           = work->rec2;
            work->field_488     = 0;
            arg0->killCountdown = 0x14;
        }
    } else {
        if (work->field_48C == 1) {
            snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480006;
            SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            work->field_48C = 5;
            work->field_490 = 0;
            work->field_486 = 4;
            work->field_492 = 0;
            work->field_4A2 = 0;
            return;
        }
        snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480001;
        SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// `func_actor_207200_8014D65C`'s body, inlined: re-arm the six helper slots
/// when the animation id changed, otherwise advance them by one frame.
static __inline__ void Actor207200_TickAnim(Task* arg0)
{
    _Actor207200LargeWork* work;
    s32                    i;

    work = arg0->work;
    if (work->field_48C != (s16)work->field_48E) {
        work->field_48E = work->field_48C;
        work->field_490 = 0;
        for (i = 1; i < 7; i++) {
            animationSeekSlotWithBlend((AnimationContext*)work, i, work->field_48C, 0, 8);
        }
    } else {
        work->field_490++;
        for (i = 1; i < 7; i++) {
            animationTickSlot((AnimationContext*)work, i);
        }
    }
}

/// `func_actor_207200_8014D70C`'s body, inlined: push the model's second coordinate's
/// world position onto the scratch stack and hand it to `Gp_UpdateActorColor`.
static __inline__ void Actor207200_UpdateColor(Enemy* enemy, Task* actor)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &actor->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(enemy, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Teardown tick. Mode 2 of `gSceneCombatState.actorControl` hides the model, mode 1 does nothing;
/// otherwise the teardown stage in `field_488` advances: 0 releases the actor's
/// state reference, snapshots the model transform and unlinks its node and
/// five display objects; 1 moves on once animation 5 has run 100 frames (or
/// at once for any other animation or once `field_4A8` is set); 2 counts 60
/// frames, spawning an effect on frame 15; 3 destroys the enemy. Every stage
/// but the last then ticks the animation, the attach coordinates and the colour.
static void func_actor_207200_8014CA84(Enemy* arg0, Task* arg1)
{
    _Actor207200LargeWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    s16                    state;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            state = work->field_488;
            switch (state) {
                case 0:
                    Gp_ReleaseStateF0Add(arg1, 0x2B);
                    work->field_488 = 1;
                    work->field_48A = 0;
                    work->field_49C = 0x1000;
                    work->field_464 = coord->coord;
                    arg0->recs      = 0;
                    worldTargetUnlinkNode(&arg0->node);
                    Gp_UnlinkObj(&work->obj1);
                    Gp_UnlinkObj(&work->obj2);
                    Gp_UnlinkObj(&work->obj3);
                    Gp_UnlinkObj(&work->obj4);
                    Gp_UnlinkObj(&work->obj5);
                    break;
                case 1:
                    if (work->field_4A8 == 0) {
                        if (work->field_48C == 5) {
                            if ((s16)work->field_490 >= 100) {
                                work->field_488 = 2;
                            }
                        } else {
                            work->field_488 = 2;
                        }
                    } else {
                        obj->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        work->field_488 = 2;
                    }
                    break;
                case 2:
                    work->field_48A++;
                    if ((s16)work->field_48A >= 0x3D) {
                        work->field_488 = 3;
                    }
                    if (work->field_4A8 == 0) {
                        func_actor_207200_8014D7E8(arg1);
                        if ((s16)work->field_48A == 0xA) {
                            obj->flags = TMD_OBJECT_SEMI_TRANS;
                        }
                        if ((s16)work->field_48A == 0xF) {
                            Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
                        }
                    }
                    break;
                case 3:
                    enemyDestroy(arg0, arg1);
                    return;
            }
            Actor207200_TickAnim(arg1);
            func_actor_207200_8014D97C(arg1, &arg1->extra.tmd->coords[2]);
            func_actor_207200_8014D97C(arg1, &arg1->extra.tmd->coords[3]);
            arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
            arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&arg1->extra.tmd->coords[1]);
            Actor207200_UpdateColor(arg0, arg1);
            break;
    }
}

/// Measures the model held in pointer slot 3 from coordinate `arg0`: returns
/// its bearing in `arg0`'s own frame, folded into -0x800..0x800, and stores
/// in `*arg1` the planar x/z distance between the two coordinates' local
/// translations. The work is staged in a block of the scratch stack.
static s32 func_actor_207200_8014CE20(GfxCoord* arg0, u32* arg1)
{
    GfxCoord*            other;
    ActorBearingScratch* blk;
    s32                  angle;

    other         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    blk           = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
    angle         = actorBearingInFrame(blk, arg0, other);
    blk->delta.vx = other->coord.t[0] - arg0->coord.t[0];
    blk->delta.vz = other->coord.t[2] - arg0->coord.t[2];
    *arg1         = SquareRoot0(blk->delta.vx * blk->delta.vx + blk->delta.vz * blk->delta.vz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    return angle;
}

/// Spawns the pair of effects that carry this actor's death animation, hands
/// the spawned task `_gActor207200CreepingStrangerBurstHead` as its setup argument, arms the
/// two timers on the work area and unlinks its third display object.
static void func_actor_207200_8014CFEC(Task* arg0)
{
    EffectSpawnArg*        effArg;
    EffectWork*            effect;
    _Actor207200LargeWork* work;
    Enemy*                 ctx;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;

    Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 0, NULL);
    func_800DA6E8(&ctx->node, ctx->hp - 1, 0);
    D_800626EC[5].data.model = &_gActor207200CreepingStrangerBurstHead;
    effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 3, 0, NULL);
    if (effect != NULL) {
        func_actor_207200_8014DAF8(effect->task, arg0);
    }
    effArg = &work->eff2;
    func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
    func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
    work->field_4A4   = 1;
    work->field_48C   = 5;
    work->field_4A6   = 1;
    ctx->recs         = work->rec2;
    ctx->hp           = 1;
    work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_UnlinkObj(&work->obj3);
    arg0->killCountdown = 0x14;
}

static void func_actor_207200_8014D128(Task* arg0)
{
    EffectWork* effect;
    s32         r;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    r               = (gRandomLcgState >> 16) & 3;
    switch (r) {
        case 0:
        case 1:
            D_800626EC[5].data.model = &_gActor207200CreepingStrangerBurstHead;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 3, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
        case 2:
            D_800626EC[5].data.model = &_gActor207200CreepingStrangerBurstArm;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 5, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
        case 3:
            D_800626EC[5].data.model = &_gActor207200CreepingStrangerBurstLeg;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 2, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
    }
    Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 3, 0x300, NULL);
    Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 2, 0x300, NULL);
}

void func_actor_207200_8014D280(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Per-frame tick of the actor's live state. `gSceneCombatState.actorControl` gates it: mode 1
/// skips the update and runs only the tail, mode 2 puts the model in its
/// hidden pose (part flag 0x80, node not lockable) and returns without updating,
/// mode 0 clears both flags before falling into the update, and any other mode
/// updates directly. The update drives the model's two attach coordinates,
/// clears the display flags of the first two parts and recomputes the second
/// part's world matrix; the tail then colours the actor from that part and
/// draws its ground shadow.
static void func_actor_207200_8014D2DC(Enemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = gSceneCombatState.actorControl;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg1->extra.tmd->flags       = 0;
    arg0->node.state.parts.flags = 0;
    goto default_body;
case2:
    arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    func_actor_207200_8014D41C(arg1);
    func_actor_207200_8014D8DC(arg1);
    func_actor_207200_8014BEF4(arg1);
    func_actor_207200_8014D49C(arg1);
    func_actor_207200_8014D5C4(arg1);
    func_actor_207200_8014D65C(arg1);
    func_actor_207200_8014D97C(arg1, &arg1->extra.tmd->coords[2]);
    func_actor_207200_8014D97C(arg1, &arg1->extra.tmd->coords[3]);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg1->extra.tmd->coords[1]);
case1:
    func_actor_207200_8014D70C(arg0, arg1);
    func_actor_207200_8014D77C(arg1);
}

/// Consumes the pending flag bits on the actor's spawn object once the actor
/// has been set up. Bit 0x1 (the "flag 1" request) is cleared first; bit 0x2
/// then re-arms the six helper slots - back to state 3 with slot id 1 at weight
/// 9 and every frame counter reset - and clears itself; bits 0xC (the "flag 4"
/// request) are cleared last. Nothing happens while the whole byte is zero.
static void func_actor_207200_8014D41C(Task* arg0)
{
    Enemy*                 obj;
    _Actor207200LargeWork* work;
    u8                     flags;

    obj   = arg0->spawnArg2.pointer;
    flags = obj->reactionFlags;
    work  = arg0->work;
    if (flags != 0) {
        if (flags & ENEMY_REACTION_STAGGER) {
            obj->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if (obj->reactionFlags & ENEMY_REACTION_BUILDUP) {
            obj->reactionFlags = obj->reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->field_486    = 3;
            work->field_48E    = 1;
            work->field_48A    = 0;
            work->field_492    = 0;
            work->field_48C    = 9;
            work->field_490    = 0;
        }
        flags = obj->reactionFlags;
        if (flags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            obj->reactionFlags = flags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Per-frame tick of the actor's six helper slots, driven by
/// `work->field_486`. The kill countdown on the task is decremented first and
/// clamped at zero. State 0 and state 1 hand the actor to the two helper
/// setup/tick bodies; state 3 runs the slot animation, counting
/// `work->field_48A` up to 0x3D frames before re-arming the slots with id 1 at
/// weight 9, and drops back to state 0 once `Gp_TickObjFlag2` reports that the
/// spawn argument is done; state 4 waits out `work->field_490` frames and then
/// either returns to state 1 when the actor is idle (`work->field_4A6 != 0`)
/// or clears both state words and marks `work->field_4A2`.
static void func_actor_207200_8014D49C(Task* arg0)
{
    _Actor207200LargeWork* work;
    s16                    countdown;

    work                = arg0->work;
    countdown           = (u16)arg0->killCountdown - 1;
    arg0->killCountdown = countdown;
    if (countdown < 0) {
        arg0->killCountdown = 0;
    }
    switch (work->field_486) {
        case 0:
            func_actor_207200_8014B628(arg0);
            break;
        case 1:
            func_actor_207200_8014B87C(arg0);
            break;
        case 3:
            work->field_48A = work->field_48A + 1;
            if ((s16)work->field_48A >= 0x3D) {
                work->field_48E = 1;
                work->field_48C = 9;
                work->field_490 = 0;
                work->field_48A = 0;
            }
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->field_486 = 0;
            }
            break;
        case 4:
            if ((s16)work->field_490 >= 0x69) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                    break;
                }
                work->field_486 = 0;
                work->field_4A2 = 1;
            }
            break;
    }
}

/// Walks the model's root part forward. While the actor is not idle
/// (`work->field_4A6 == 0`) the part's current translation is remembered in the
/// work area, and the part is then displaced along its own forward axis - the
/// third basis column of its local matrix, scaled by `work->field_492` - and
/// lifted by 0x80.
static void func_actor_207200_8014D5C4(Task* arg0)
{
    _Actor207200LargeWork* work;
    GfxCoord*              coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_4A6 == 0) {
        work->field_454 = coord->coord.t[0];
        work->field_458 = coord->coord.t[1];
        work->field_45C = coord->coord.t[2];
    }
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_492) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_492) >> 12;
}

/// Rebinds the work's animation id to its six helper slots. When the id has
/// changed since the last frame the remembered id follows it, the frame counter
/// restarts and every slot is pointed at the new id at weight 8; otherwise the
/// counter ticks and the slots are simply advanced by one.
static void func_actor_207200_8014D65C(Task* arg0)
{
    Actor207200_TickAnim(arg0);
}

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off the scratch stack, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
static void func_actor_207200_8014D70C(Enemy* arg0, Task* task)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Draws the enemy's ground quad under its model root, at the translation of
/// the root part's `workm`, staged in a `VECTOR3` on the scratch stack.
static void func_actor_207200_8014D77C(Task* task)
{
    GfxCoord* coord;
    VECTOR3*  vec;

    coord   = task->extra.tmd->coords;
    vec     = (VECTOR3*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    vec->vx = coord->workm.t[0];
    vec->vy = coord->workm.t[1];
    vec->vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(vec, 0x1C0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

/// Rebuilds the first coordinate node of the actor's model from the transform
/// stored in `work->field_464`, scaled along Y by `work->field_49C` (its own
/// angle field, decaying by 0x50 a frame while it sits above 0x200). The
/// `ActorScaleScratch` block that holds the scaling matrix and its `VECTOR` is
/// borrowed from the scratch stack and released again; the node's
/// `composeStamp` is cleared so the next `Gp_UpdateCoord` recomputes it.
static void func_actor_207200_8014D7E8(Task* arg0)
{
    GfxCoord*              coord;
    ActorScaleScratch*     head;
    ActorScaleScratch*     scratch;
    _Actor207200LargeWork* work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->field_49C >= 0x201) {
        work->field_49C = (u16)work->field_49C - 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = (s32)work->field_49C;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->field_464;
    scratch->matrix.rotationWords.m00M01 = ONE;
    scratch->matrix.rotationWords.m02M10 = 0;
    scratch->matrix.rotationWords.m11M12 = ONE;
    scratch->matrix.rotationWords.m20M21 = 0;
    scratch->matrix.rotationWords.m22    = ONE;
    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Re-picks the model part the enemy's `coord` points at and relinks its
/// lock-on node. Once `field_4A6` is set it is always the second part;
/// before that it is the fourth part while the model in pointer slot 3 lies
/// within a quarter turn of the root's heading, and the second otherwise.
static void func_actor_207200_8014D8DC(Task* arg0)
{
    _Actor207200LargeWork* work;
    Enemy*                 ctx;
    GfxCoord*              coord;
    s32                    dist;
    s32                    angle;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->field_4A6 != 0) {
        coord = arg0->extra.tmd->coords + 1;
    } else {
        angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
        if (angle < 0) {
            angle = -angle;
        }
        if (angle < 0x400) {
            coord = arg0->extra.tmd->coords + 3;
        } else {
            coord = arg0->extra.tmd->coords + 1;
        }
    }
    ctx->coord = coord;
    Gp_LinkNode(&ctx->node);
}

/// While `work->field_4A6` is set, runs each column of the node's rotation
/// matrix through GTE `gpf 12` with a zero interpolation factor, zeroing the
/// 3x3 part, and clears `composeStamp` so the node is recomputed.
static void func_actor_207200_8014D97C(Task* arg0, GfxCoord* arg1)
{
    SVECTOR vec;
    MATRIX* m;

    if (((_Actor207200LargeWork*)arg0->work)->field_4A6 != 0) {
        m = &arg1->coord;
        gte_ReadMatrixColumn(m, 0, &vec);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 0);

        gte_ReadMatrixColumn(m, 1, &vec);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 1);

        gte_ReadMatrixColumn(m, 2, &vec);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 2);

        arg1->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Copies the texture page and CLUT from `src`'s model onto `dst`'s and, when
/// `dst` has a stream buffer, processes it twice so both halves pick the new
/// pair up. The enemy calls it with a freshly spawned effect as `dst` and
/// itself as `src`.
static void func_actor_207200_8014DAF8(Task* dst, Task* src)
{
    TmdObject* to;
    TmdObject* from;

    from                  = src->extra.tmd;
    to                    = dst->extra.tmd;
    to->texturePageOffset = from->texturePageOffset;
    to->clutRowOffset     = from->clutRowOffset;
    if (to->buffer != NULL) {
        tmdProcessStream(to);
        tmdProcessStream(to);
    }
}

static void func_actor_207200_8014DB4C(Task* arg0)
{
    Enemy*                 ctx;
    _Actor207200LargeWork* work;

    ctx       = arg0->spawnArg2.pointer;
    work      = arg0->work;
    ctx->recs = 0;
    worldTargetUnlinkNode(&ctx->node);
    Gp_UnlinkObj(&work->obj1);
    Gp_UnlinkObj(&work->obj2);
    Gp_UnlinkObj(&work->obj3);
    Gp_UnlinkObj(&work->obj4);
    Gp_UnlinkObj(&work->obj5);
    enemyTaskExit(arg0);
}
