#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
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

#include "overlay.h"

/// 0x348-byte work block `Actor02500_Fn00078` allocates and hangs off
/// `Task::work`. It opens with the animation context (`animationInitContext`
/// arg0) and its five slots, and carries the four list nodes plus their
/// `WorldCollisionContact` tables.
typedef struct Actor02500Work {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ AnimationSlot         field_14[5];
    /* 0x0DC */ byte                  field_DC[0x50];
    /* 0x12C */ byte                  field_12C[0x20];
    /* 0x14C */ byte                  field_14C[0x20];
    /* 0x16C */ WorldCollisionBody    obj16C;
    /* 0x18C */ WorldCollisionContact field_18C[1];
    /* 0x1A4 */ WorldCollisionBody    obj1A4;
    /* 0x1C4 */ WorldCollisionContact field_1C4[3];
    /* 0x20C */ WorldCollisionBody    obj20C;
    /* 0x22C */ WorldCollisionContact field_22C[5];
    /* 0x2A4 */ WorldCollisionBody    obj2A4;
    /* 0x2C4 */ WorldCollisionContact field_2C4[1];
    /* 0x2DC */ EffectSpawnArg        field_2DC; // record the hit's effect is spawned with
    /* 0x2E4 */ MATRIX                field_2E4;
    /* 0x304 */ s32                   field_304;
    /* 0x308 */ s32                   field_308;
    /* 0x30C */ s32                   field_30C;
    /* 0x310 */ byte                  pad_310[4];
    /* 0x314 */ s16                   field_314;
    /* 0x316 */ s16                   field_316;
    /* 0x318 */ s16                   field_318;
    /* 0x31A */ byte                  pad_31A[2];
    /* 0x31C */ s16                   field_31C;
    /* 0x31E */ s16                   field_31E;
    /* 0x320 */ u16                   field_320;
    /* 0x322 */ s16                   field_322;
    /* 0x324 */ s16                   field_324;
    /* 0x326 */ s16                   field_326;
    /* 0x328 */ s16                   field_328;
    /* 0x32A */ u16                   field_32A;
    /* 0x32C */ s16                   field_32C;
    /* 0x32E */ s16                   field_32E;
    /* 0x330 */ s16                   field_330;
    /* 0x332 */ s16                   field_332;
    /* 0x334 */ s16                   field_334;
    /* 0x336 */ s16                   field_336;
    /* 0x338 */ s16                   field_338;
    /* 0x33A */ s16                   field_33A;
    /* 0x33C */ s16                   field_33C;
    /* 0x33E */ s16                   field_33E;
    /* 0x340 */ s16                   field_340;
    /* 0x342 */ s16                   field_342;
    /* 0x344 */ s16                   field_344;
    /* 0x346 */ byte                  pad_346[2];
} Actor02500Work;
STATIC_ASSERT_SIZEOF(Actor02500Work, 0x348);

/// Work block of the small helper task `Actor02500_L02634` spawns, also parked
/// at `Task::work`. It opens with a list node and its one-entry
/// `WorldCollisionContact` table, then the spawned effect and the countdown/state
/// pair `Actor02500_Fn02874` runs on.
typedef struct Actor02500EffWork {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec18[1];
    /* 0x38 */ EffectWork*           field_38;
    /* 0x3C */ s16                   field_3C;
    /* 0x3E */ s16                   field_3E;
} Actor02500EffWork;
STATIC_ASSERT_SIZEOF(Actor02500EffWork, 0x40);

typedef struct Actor02500OffsetPair {
    s16 x;
    s16 z;
} Actor02500OffsetPair;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern EnemyParams          Actor02500_D05B38;
extern DamageAttack         Actor02500_D05B30[];
extern s16                  Actor02500_D05B48[];
extern s16                  Actor02500_D05B58[];
extern s16                  Actor02500_D05B68[];
extern s16                  Actor02500_D05B78[];
extern TaskDesc             Actor02500_D05B88[];
extern AnimationSet*        Actor02500_D05BA0[12];
extern s16                  Actor02500_D05BD0[];
extern Actor02500OffsetPair Actor02500_D05BE8[];
static TmdSource            _gActor02500ScorpionBurstHead;
static TmdSource            _gActor02500ScorpionBurstPincer2;
static TmdSource            _gActor02500ScorpionBurstPincer1;
extern void*                D_80067704[1];

static void Actor02500_Fn00078(Enemy* ctx, Task* actor);
static void Actor02500_Fn01AC8(Enemy* ctx, Task* actor);
static void Actor02500_Fn01E60(Enemy* ctx, Task* actor);
static void Actor02500_Fn01F8C(Task* actor);
static void Actor02500_Fn02008(Task* actor);
static void Actor02500_Fn020D0(Task* actor);
static void Actor02500_Fn02178(Task* actor);
static void Actor02500_Fn021F8(Task* actor);
static void Actor02500_Fn02288(Task* actor);
static void Actor02500_Fn02318(Task* actor);
static void Actor02500_Fn023D8(Task* actor);
static void Actor02500_Fn02430(Task* actor);
static void Actor02500_Fn02480(Task* actor);
static void Actor02500_Fn025D0(Enemy* ctx, Task* task);
static void Actor02500_Fn02750(Enemy* ctx, Task* task);
static void Actor02500_Fn02874(Enemy* ctx, Task* task);

/// State handlers of the enemy task `Actor02500_Fn01E04` dispatches, indexed
/// by `Task::state`: spawn, per-frame tick and the dying sequence.
static const EnemyTaskFuncTable3 Actor02500_D00004 = {
    {
        Actor02500_Fn00078,
        Actor02500_Fn01E60,
        Actor02500_Fn01AC8,
    },
};

static AnimationSet _gActor02500Actor102500Animation04C64;
static AnimationSet _gActor02500Actor102500Animation04E3C;
static AnimationSet _gActor02500Actor102500Animation04FF0;
static AnimationSet _gActor02500Actor102500Animation0534C;
static AnimationSet _gActor02500Actor102500Animation05520;
static AnimationSet _gActor02500Actor102500Animation057D4;
static AnimationSet _gActor02500Actor102500Animation05998;
static AnimationSet _gActor02500Actor102500Animation05B08;
static TmdSource    _gActor02500ScorpionBody;
void                Actor02500_Fn01E04(Task*);
void                Actor02500_Fn02574(Task*);

static TmdBone _gActor02500ScorpionBodySkeleton[5] = {
#include "assets/scorpion_body_skeleton.inc"
};

static u32 _gActor02500ScorpionBodyPartVerts[5] = {
#include "assets/scorpion_body_partVerts.inc"
};

static SVECTOR _gActor02500ScorpionBodyVerts[93] = {
#include "assets/scorpion_body_verts.inc"
};

static SVECTOR _gActor02500ScorpionBodyNormals[93] = {
#include "assets/scorpion_body_normals.inc"
};

static u32 _gActor02500ScorpionBodyStream[989] = {
#include "assets/scorpion_body_stream.inc"
};

static TmdSource _gActor02500ScorpionBody = {
    0,
    5684,
    1152,
    5,
    _gActor02500ScorpionBodyPartVerts,
    _gActor02500ScorpionBodyVerts,
    _gActor02500ScorpionBodyNormals,
    _gActor02500ScorpionBodySkeleton,
    _gActor02500ScorpionBodyStream,
};

static TmdBone _gActor02500ScorpionBurstHeadSkeleton[1] = {
#include "assets/scorpion_burst_head_skeleton.inc"
};

static u32 _gActor02500ScorpionBurstHeadPartVerts[1] = {
#include "assets/scorpion_burst_head_partVerts.inc"
};

static SVECTOR _gActor02500ScorpionBurstHeadVerts[22] = {
#include "assets/scorpion_burst_head_verts.inc"
};

static SVECTOR _gActor02500ScorpionBurstHeadNormals[22] = {
#include "assets/scorpion_burst_head_normals.inc"
};

static u32 _gActor02500ScorpionBurstHeadStream[223] = {
#include "assets/scorpion_burst_head_stream.inc"
};

static TmdSource _gActor02500ScorpionBurstHead = {
    0,
    1292,
    0,
    1,
    _gActor02500ScorpionBurstHeadPartVerts,
    _gActor02500ScorpionBurstHeadVerts,
    _gActor02500ScorpionBurstHeadNormals,
    _gActor02500ScorpionBurstHeadSkeleton,
    _gActor02500ScorpionBurstHeadStream,
};

static TmdBone _gActor02500ScorpionBurstPincer2Skeleton[1] = {
#include "assets/scorpion_burst_pincer_2_skeleton.inc"
};

static u32 _gActor02500ScorpionBurstPincer2PartVerts[1] = {
#include "assets/scorpion_burst_pincer_2_partVerts.inc"
};

static SVECTOR _gActor02500ScorpionBurstPincer2Verts[15] = {
#include "assets/scorpion_burst_pincer_2_verts.inc"
};

static SVECTOR _gActor02500ScorpionBurstPincer2Normals[15] = {
#include "assets/scorpion_burst_pincer_2_normals.inc"
};

static u32 _gActor02500ScorpionBurstPincer2Stream[130] = {
#include "assets/scorpion_burst_pincer_2_stream.inc"
};

static TmdSource _gActor02500ScorpionBurstPincer2 = {
    0,
    844,
    0,
    1,
    _gActor02500ScorpionBurstPincer2PartVerts,
    _gActor02500ScorpionBurstPincer2Verts,
    _gActor02500ScorpionBurstPincer2Normals,
    _gActor02500ScorpionBurstPincer2Skeleton,
    _gActor02500ScorpionBurstPincer2Stream,
};

static TmdBone _gActor02500ScorpionBurstPincer1Skeleton[1] = {
#include "assets/scorpion_burst_pincer_1_skeleton.inc"
};

static u32 _gActor02500ScorpionBurstPincer1PartVerts[1] = {
#include "assets/scorpion_burst_pincer_1_partVerts.inc"
};

static SVECTOR _gActor02500ScorpionBurstPincer1Verts[15] = {
#include "assets/scorpion_burst_pincer_1_verts.inc"
};

static SVECTOR _gActor02500ScorpionBurstPincer1Normals[15] = {
#include "assets/scorpion_burst_pincer_1_normals.inc"
};

static u32 _gActor02500ScorpionBurstPincer1Stream[130] = {
#include "assets/scorpion_burst_pincer_1_stream.inc"
};

static TmdSource _gActor02500ScorpionBurstPincer1 = {
    0,
    844,
    0,
    1,
    _gActor02500ScorpionBurstPincer1PartVerts,
    _gActor02500ScorpionBurstPincer1Verts,
    _gActor02500ScorpionBurstPincer1Normals,
    _gActor02500ScorpionBurstPincer1Skeleton,
    _gActor02500ScorpionBurstPincer1Stream,
};

static AnimationPackedPose _gActor02500Actor102500Animation04C64Bank1[8] = {
#include "assets/actor_102500_animation_04C64_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation04C64Bank4[21] = {
#include "assets/actor_102500_animation_04C64_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation04C64Records[44] = {
#include "assets/actor_102500_animation_04C64_records.inc"
};

static u16 _gActor02500Actor102500Animation04C64Indices[6] = {
#include "assets/actor_102500_animation_04C64_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation04C64 = {
    _gActor02500Actor102500Animation04C64Records,
    _gActor02500Actor102500Animation04C64Indices,
    { NULL, _gActor02500Actor102500Animation04C64Bank1, NULL, NULL, _gActor02500Actor102500Animation04C64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation04E3CBank1[10] = {
#include "assets/actor_102500_animation_04E3C_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation04E3CBank4[27] = {
#include "assets/actor_102500_animation_04E3C_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation04E3CRecords[48] = {
#include "assets/actor_102500_animation_04E3C_records.inc"
};

static u16 _gActor02500Actor102500Animation04E3CIndices[6] = {
#include "assets/actor_102500_animation_04E3C_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation04E3C = {
    _gActor02500Actor102500Animation04E3CRecords,
    _gActor02500Actor102500Animation04E3CIndices,
    { NULL, _gActor02500Actor102500Animation04E3CBank1, NULL, NULL, _gActor02500Actor102500Animation04E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation04FF0Bank1[7] = {
#include "assets/actor_102500_animation_04FF0_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation04FF0Bank4[27] = {
#include "assets/actor_102500_animation_04FF0_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation04FF0Records[48] = {
#include "assets/actor_102500_animation_04FF0_records.inc"
};

static u16 _gActor02500Actor102500Animation04FF0Indices[6] = {
#include "assets/actor_102500_animation_04FF0_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation04FF0 = {
    _gActor02500Actor102500Animation04FF0Records,
    _gActor02500Actor102500Animation04FF0Indices,
    { NULL, _gActor02500Actor102500Animation04FF0Bank1, NULL, NULL, _gActor02500Actor102500Animation04FF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation0534CBank1[20] = {
#include "assets/actor_102500_animation_0534C_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation0534CBank4[55] = {
#include "assets/actor_102500_animation_0534C_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation0534CRecords[87] = {
#include "assets/actor_102500_animation_0534C_records.inc"
};

static u16 _gActor02500Actor102500Animation0534CIndices[6] = {
#include "assets/actor_102500_animation_0534C_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation0534C = {
    _gActor02500Actor102500Animation0534CRecords,
    _gActor02500Actor102500Animation0534CIndices,
    { NULL, _gActor02500Actor102500Animation0534CBank1, NULL, NULL, _gActor02500Actor102500Animation0534CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation05520Bank1[10] = {
#include "assets/actor_102500_animation_05520_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation05520Bank4[27] = {
#include "assets/actor_102500_animation_05520_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation05520Records[47] = {
#include "assets/actor_102500_animation_05520_records.inc"
};

static u16 _gActor02500Actor102500Animation05520Indices[6] = {
#include "assets/actor_102500_animation_05520_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation05520 = {
    _gActor02500Actor102500Animation05520Records,
    _gActor02500Actor102500Animation05520Indices,
    { NULL, _gActor02500Actor102500Animation05520Bank1, NULL, NULL, _gActor02500Actor102500Animation05520Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation057D4Bank1[16] = {
#include "assets/actor_102500_animation_057D4_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation057D4Bank4[45] = {
#include "assets/actor_102500_animation_057D4_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation057D4Records[67] = {
#include "assets/actor_102500_animation_057D4_records.inc"
};

static u16 _gActor02500Actor102500Animation057D4Indices[6] = {
#include "assets/actor_102500_animation_057D4_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation057D4 = {
    _gActor02500Actor102500Animation057D4Records,
    _gActor02500Actor102500Animation057D4Indices,
    { NULL, _gActor02500Actor102500Animation057D4Bank1, NULL, NULL, _gActor02500Actor102500Animation057D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation05998Bank1[10] = {
#include "assets/actor_102500_animation_05998_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation05998Bank4[27] = {
#include "assets/actor_102500_animation_05998_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation05998Records[43] = {
#include "assets/actor_102500_animation_05998_records.inc"
};

static u16 _gActor02500Actor102500Animation05998Indices[6] = {
#include "assets/actor_102500_animation_05998_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation05998 = {
    _gActor02500Actor102500Animation05998Records,
    _gActor02500Actor102500Animation05998Indices,
    { NULL, _gActor02500Actor102500Animation05998Bank1, NULL, NULL, _gActor02500Actor102500Animation05998Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation05B08Bank1[7] = {
#include "assets/actor_102500_animation_05B08_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation05B08Bank4[18] = {
#include "assets/actor_102500_animation_05B08_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation05B08Records[40] = {
#include "assets/actor_102500_animation_05B08_records.inc"
};

static u16 _gActor02500Actor102500Animation05B08Indices[6] = {
#include "assets/actor_102500_animation_05B08_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation05B08 = {
    _gActor02500Actor102500Animation05B08Records,
    _gActor02500Actor102500Animation05B08Indices,
    { NULL, _gActor02500Actor102500Animation05B08Bank1, NULL, NULL, _gActor02500Actor102500Animation05B08Bank4, NULL, NULL, NULL },
};

DamageAttack Actor02500_D05B30[2] = {
    { 10, 3 },
    { 1, 3 },
};

EnemyParams Actor02500_D05B38 = { Actor02500_D05B30, 68, 20, 8, 1, 0, 20, 0, 0 };

s16 Actor02500_D05B48[8] = {
    16,
    18,
    20,
    22,
    22,
    22,
    22,
    22,
};

s16 Actor02500_D05B58[8] = {
    24,
    26,
    28,
    30,
    30,
    30,
    30,
    30,
};

s16 Actor02500_D05B68[8] = {
    18,
    22,
    26,
    30,
    30,
    30,
    30,
    30,
};

s16 Actor02500_D05B78[8] = {
    30,
    34,
    34,
    38,
    38,
    38,
    38,
    38,
};

TaskDesc Actor02500_D05B88[2] = {
    { { { TASK_BODY_TMD, 96 } }, Actor02500_Fn01E04, { .model = &_gActor02500ScorpionBody } },
    { { { TASK_BODY_COORD, 96 } }, Actor02500_Fn02574, { .value = 0 } },
};

AnimationSet* Actor02500_D05BA0[12] = {
    NULL,
    &_gActor02500Actor102500Animation04C64,
    NULL,
    &_gActor02500Actor102500Animation04E3C,
    &_gActor02500Actor102500Animation04FF0,
    NULL,
    &_gActor02500Actor102500Animation0534C,
    &_gActor02500Actor102500Animation05520,
    &_gActor02500Actor102500Animation057D4,
    NULL,
    &_gActor02500Actor102500Animation05998,
    &_gActor02500Actor102500Animation05B08,
};

s16 Actor02500_D05BD0[12] = {
    0,
    8,
    8,
    8,
    8,
    8,
    8,
    0,
    8,
    8,
    0,
    4,
};

Actor02500OffsetPair Actor02500_D05BE8[8] = {
    { 0, 4096 },
    { 2896, 2896 },
    { 4096, 0 },
    { 2896, -2896 },
    { 0, -4096 },
    { -2896, -2896 },
    { -4096, 0 },
    { -2896, 2896 },
};

static void Actor02500_Fn00494(Task* actor);
static void Actor02500_Fn00B18(Task* actor);
static void Actor02500_Fn00DD8(Task* actor);
static void Actor02500_Fn01144(Task* actor);
static void Actor02500_Fn012F0(Task* actor);
static void Actor02500_Fn016FC(Task* arg0);
static void Actor02500_Fn0184C(Task* arg0);

static void Actor02500_Fn00078(Enemy* ctx, Task* actor)
{
    Actor02500Work* work;
    TmdObject*      obj;
    GfxCoord*       coord;
    s32             i;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x348, 0);
    if (work == NULL) {
        enemyDestroy(ctx, actor);
        return;
    }
    actor->work         = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = (MATRIX*)work->field_14C;
    obj->colorMtx       = (MATRIX*)work->field_12C;
    ctx->field_4        = &coord->coord;
    ctx->field_48       = 0;
    Gp_LinkNode(&ctx->node);
    ctx->bodyPos.vy             = -0x96;
    ctx->coord                  = coord;
    ctx->node.state.parts.flags = 0;
    ctx->bodyPos.vx             = 0;
    ctx->bodyPos.vz             = 0;
    ctx->param                  = &Actor02500_D05B38;
    ctx->hp                     = Actor02500_D05B38.hpMax;
    work->field_2DC.spawnArgLo  = 0x200;
    work->field_2DC.coord       = coord;
    work->field_2DC.spawnArgHi  = 1;
    animationInitContext(&work->anim, Actor02500_D05BA0, obj, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->field_DC, work->field_14);
    work->field_31C = 1;
    work->field_31E = 1;
    for (i = 1; i < 5; i++) {
        animationResetSlot(&work->anim, i, work->field_31C);
    }
    Gp_IncStateF0Ref(0);
    switch (ctx->place->mode) {
        case 0:
            work->field_322 = 0;
            work->field_324 = 0;
            ctx->recs       = work->field_1C4;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_32E = ((gRandomLcgState >> 16) & 0x3F) + 0x1E;
            break;
        case 1:
            work->field_322 = 5;
            work->field_324 = 0;
            ctx->recs       = NULL;
            Gp_SetLightMode(ctx, ENEMY_COLOR_BLACK);
            break;
        case 2:
            work->field_322 = 5;
            work->field_324 = 1;
            ctx->recs       = NULL;
            Gp_SetLightMode(ctx, ENEMY_COLOR_BLACK);
            break;
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_338 = ((gRandomLcgState >> 16) & 0x7F) + 0x1E;
    work->field_314 = coord->coord.t[0];
    work->field_316 = coord->coord.t[1];
    work->field_318 = coord->coord.t[2];
    work->field_32A = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;

    work->obj16C.coord            = coord;
    work->obj16C.context.contacts = work->field_18C;
    work->obj16C.pos.vx           = 0;
    work->obj16C.pos.vy           = -0x190;
    work->obj16C.pos.vz           = 0x258;
    work->obj16C.key              = 0;
    work->obj16C.radius           = 0x258;
    work->obj16C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj16C);
    Gp_InitRec18Table(work->field_18C, 1, 0);

    work->obj1A4.coord            = coord;
    work->obj1A4.context.contacts = work->field_1C4;
    work->obj1A4.pos.vx           = 0;
    work->obj1A4.pos.vy           = -0x12C;
    work->obj1A4.pos.vz           = 0;
    work->obj1A4.key              = 0x30019;
    work->obj1A4.radius           = 0x12C;
    work->obj1A4.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->obj16C.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(2, &work->obj1A4);
    Gp_InitRec18Table(work->field_1C4, 3, 0);

    if (ctx->place->mode == 0) {
        work->obj1A4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->obj1A4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    work->obj20C.coord            = coord;
    work->obj20C.context.contacts = work->field_22C;
    work->obj20C.pos.vx           = 0;
    work->obj20C.pos.vy           = -0x12C;
    work->obj20C.pos.vz           = 0;
    work->obj20C.key              = 0x30019;
    work->obj20C.radius           = 0x12C;
    work->obj20C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj20C);
    Gp_InitRec18Table(work->field_22C, 5, 0);
    work->obj20C.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

    work->obj2A4.coord            = actor->extra.tmd->coords + 4;
    work->obj2A4.context.contacts = work->field_2C4;
    work->obj2A4.pos.vx           = 0;
    work->obj2A4.pos.vy           = -0x3B6;
    work->obj2A4.pos.vz           = 0x1CC;
    work->obj2A4.key              = Gp_PackPair(Actor02500_D05B30, 0);
    work->obj2A4.radius           = 0x12C;
    work->obj2A4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj2A4);
    Gp_InitRec18Table(work->field_2C4, 1, 0);
    work->obj2A4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->state        = 1;
}

/// Per-frame collision and damage pass. Carves a `ActorWallPushFrame` off
/// the scratchpad stack, lets `func_800E0C10` resolve this frame's movement
/// into it, then walks the three `field_1C4` records: kind 2 is a hit that
/// costs the enemy HP and plays a sound, kinds 1 and 3 push it away from the
/// obstacle, and the strongest push is applied to the coordinate at the end.
static void Actor02500_Fn00494(Task* actor)
{
    u32                 lastId;
    Actor02500Work*     work;
    Enemy*              ctx;
    GfxCoord*           coord;
    GfxCoord*           target;
    ActorWallPushFrame* head;
    ActorWallPushFrame* frame;
    VECTOR*             normal;
    s32                 i;
    s32                 push;
    s32                 bestPush;
    s32                 damage;
    s32                 param0;
    s32                 cooldown;
    s32                 soundId;

    bestPush = 0;
    lastId   = 0;
    work     = actor->work;
    head     = SCRATCH_STACK_CURSOR(ActorWallPushFrame);
    frame = SCRATCH_STACK_CURSOR(ActorWallPushFrame) = head - 1;
    coord                                            = actor->extra.tmd->coords;
    ctx                                              = actor->spawnArg2.pointer;
    work->field_340                                  = 0;
    switch (func_800E0C10(work->field_22C, &frame->delta, 5, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
            coord->coord.t[1] += frame->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += frame->delta.fixed.vz.halves.integer;
            if (head[-1].delta.fixed.vx.word != 0 || frame->delta.fixed.vz.word != 0) {
                work->field_340 = 1;
            }
            break;
        case 2:
            coord->coord.t[0] = work->field_304;
            coord->coord.t[1] = work->field_308;
            coord->coord.t[2] = work->field_30C;
            if (head[-1].delta.fixed.vx.word != 0 || frame->delta.fixed.vz.word != 0) {
                work->field_340 = 1;
            }
            break;
    }
    Gp_ClearRec18Occupied(work->field_22C);
    if (work->field_334 != 0) {
        work->field_334--;
        if (work->field_334 <= 0) {
            work->field_334 = 0;
        }
    }
    normal = &frame->normal;
    for (i = 0; i < 3; i++) {
        switch ((u32)work->field_1C4[i].key.value >> 16) {
            case 2:
                if (work->field_334 == 0) {
                    target                 = gPlayerActorTasks[((u32)work->field_1C4[i].key.value >> 7) & 1]->extra.tmd->coords;
                    frame->delta.vector.vx = target->coord.t[0] - coord->coord.t[0];
                    frame->delta.vector.vy = target->coord.t[1] - coord->coord.t[1];
                    frame->delta.vector.vz = target->coord.t[2] - coord->coord.t[2];
                    damage                 = Gp_ComputeDamage(work->field_1C4[i].key.value,
                                                              SquareRoot0(frame->delta.vector.vx * frame->delta.vector.vx + frame->delta.vector.vy * frame->delta.vector.vy +
                                                                          frame->delta.vector.vz * frame->delta.vector.vz),
                                                              0, 0);
                    param0                 = Gp_GetIdParam0(work->field_1C4[i].key.value);
                    if ((param0 & 0xFFFF) == 5) {
                        damage *= 2;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 2, NULL);
                    }
                    if (Gp_RollEnemyChance(ctx, work->field_1C4[i].key.value, 0) != 0) {
                        damage *= 4;
                        if ((param0 & 0xFFFF) != 5) {
                            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, NULL);
                        }
                    }
                    func_800E2C78(ctx, work->field_1C4[i].key.value, damage, 0);
                    func_800DA6E8(&ctx->node, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        work->field_322     = 6;
                        work->field_324     = 0;
                        work->obj2A4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        soundId             = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4019000A;
                        SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    } else {
                        if (work->field_342 == 0) {
                            work->field_322 = 2;
                            work->field_324 = 0;
                        }
                        work->field_342 = 0;
                        soundId         = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190009;
                        SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    switch (param0 & 0xFFFF) {
                        case 0:
                        case 3:
                        case 5:
                        case 7:
                        case 8:
                        case 9:
                            break;
                        case 1:
                            if (work->field_33E == 0) {
                                Gp_SetObjFlag1(ctx);
                            }
                            work->obj2A4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            break;
                        case 2:
                            Gp_SetObjFlag2(ctx, work->field_1C4[i].key.value, 0);
                            work->obj2A4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            break;
                        case 4:
                        case 6:
                            if (ctx->hp <= 0) {
                                work->field_33C = 1;
                            }
                            break;
                    }
                    if (lastId != work->field_1C4[i].key.value) {
                        lastId = work->field_1C4[i].key.value;
                        func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, coord, 0, &work->field_2DC);
                    }
                    cooldown = Gp_GetIdParam2(work->field_1C4[i].key.value);
                    if (cooldown > 0) {
                        work->field_334 = cooldown;
                    }
                }
                break;
            case 0:
                break;
            /* Kinds 1 and 3 push the enemy back out of the obstacle the same way. */
            case 1:
                frame->delta.vector.vx = coord->workm.t[0] - work->field_1C4[i].point.vx;
                frame->delta.vector.vy = coord->workm.t[1] - work->field_1C4[i].point.vy;
                frame->delta.vector.vz = coord->workm.t[2] - work->field_1C4[i].point.vz;
                push                   = work->field_1C4[i].distance -
                       SquareRoot0(frame->delta.vector.vx * frame->delta.vector.vx + frame->delta.vector.vy * frame->delta.vector.vy +
                                   frame->delta.vector.vz * frame->delta.vector.vz);
                push = (push <= 0) ? 0 : push;
                if (bestPush < push) {
                    bestPush = push;
                    VectorNormal(&frame->delta.vector, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal, &frame->dir);
                }
                break;
            case 3:
                frame->delta.vector.vx = coord->workm.t[0] - work->field_1C4[i].point.vx;
                frame->delta.vector.vy = coord->workm.t[1] - work->field_1C4[i].point.vy;
                frame->delta.vector.vz = coord->workm.t[2] - work->field_1C4[i].point.vz;
                push                   = work->field_1C4[i].distance -
                       SquareRoot0(frame->delta.vector.vx * frame->delta.vector.vx + frame->delta.vector.vy * frame->delta.vector.vy +
                                   frame->delta.vector.vz * frame->delta.vector.vz);
                push = (push <= 0) ? 0 : push;
                if (bestPush < push) {
                    bestPush = push;
                    VectorNormal(&frame->delta.vector, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal, &frame->dir);
                }
                break;
        }
    }
    if (bestPush > 0) {
        coord->coord.t[0] += (bestPush * frame->dir.vx) >> 0xC;
        coord->coord.t[2] += (bestPush * frame->dir.vz) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_1C4);
    if (Gp_FindRec18(work->field_2C4, 0) != 0) {
        work->obj2A4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        soundId             = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190006;
        SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    }
    Gp_ClearRec18Occupied(work->field_2C4);
    if (Gp_CountRec18Hi(work->field_18C, 0x10000) != 0 && work->field_322 == 0) {
        work->field_322 = 1;
        work->field_324 = 0;
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(work->field_18C);
    SCRATCH_STACK_RELEASE_BYTES(0x30);
}

static void Actor02500_Fn00B18(Task* actor)
{
    Actor02500Work* work;
    GfxCoord*       coord;
    s16             timer;
    s16             moveTimer;
    s16             state;
    s32             randomAngle;
    s32             randomMoveTime;
    s32             dx;
    s32             randomIdleTime;
    s32             dz;
    s32             idleTime;
    VECTOR*         vector;
    VECTOR*         scratchEnd;

    scratchEnd                 = SCRATCH_STACK_CURSOR(VECTOR);
    vector                     = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(void) = vector;
    work                       = actor->work;
    state                      = work->field_324;
    coord                      = actor->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_31C = 1;
            work->field_326 = 0;
            timer           = (u16)work->field_32E - 1;
            work->field_32E = timer;
            if (timer <= 0) {
                work->field_336 = 0;
                work->field_31C = 3;
                work->field_324 = 1;
                randomAngle     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = randomAngle;
                work->field_32A = ((u32)randomAngle >> 0x10) & 0xFFF;
            }
            break;
        case 1:
            work->field_326 = 0;
            if (work->field_32C == (s16)work->field_32A) {
                work->field_324 = 2;
                randomMoveTime  = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = randomMoveTime;
                work->field_32E = (((u32)randomMoveTime >> 0x10) & 0x7F) + 0x1E;
            }
            break;
        case 2:
            work->field_326   = (s16)Actor02500_D05B58[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
            scratchEnd[-1].vx = (s32)(work->field_314 - coord->coord.t[0]);
            vector->vy        = 0;
            dz                = work->field_318 - coord->coord.t[2];
            vector->vz        = dz;
            dx                = scratchEnd[-1].vx;
            if ((SquareRoot0((dx * dx) + (dz * dz)) >= 0x7D0) && (work->field_336 == 0)) {
                work->field_324 = 3;
            } else {
                if (work->field_340 != 1) {
                    moveTimer       = (u16)work->field_32E - 1;
                    work->field_32E = moveTimer;
                    if (moveTimer > 0) {
                        break;
                    }
                }
                work->field_324 = 0;
                randomIdleTime  = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = randomIdleTime;
                idleTime        = ((u32)randomIdleTime >> 0x10) & 0x3F;
                work->field_32E = idleTime + 0x1E;
            }
            break;
        case 3:
            scratchEnd[-1].vx = (s32)(work->field_314 - coord->coord.t[0]);
            vector->vy        = 0;
            vector->vz        = (s32)(work->field_318 - coord->coord.t[2]);
            work->field_32A   = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vector->vz) & 0xFFF;
            work->field_336   = 1;
            work->field_324   = 1;
            break;
    }
    work->field_328 = (s16)Actor02500_D05B48[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void Actor02500_Fn00DD8(Task* actor)
{
    Actor02500Work* work;
    GfxCoord*       coord;
    s16             timer;
    s32             state;
    s16             diff;
    s32             frame;
    s32             absDiff;
    s16             angle;
    s32             sound;
    s32             dx;
    s32             dz;
    s32             homeDx;
    s32             homeDz;
    s32             pan;
    VECTOR*         vector;
    VECTOR*         scratchEnd;

    scratchEnd                 = SCRATCH_STACK_CURSOR(VECTOR);
    vector                     = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(void) = vector;
    work                       = actor->work;
    state                      = work->field_324;
    coord                      = actor->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_31C = 4;
            work->field_32E = 0xF0;
            work->field_326 = 0;
            work->field_324 = 1;
            break;
        case 1:
            work->field_326   = (s16)Actor02500_D05B78[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
            scratchEnd[-1].vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vector->vy        = 0;
            vector->vz        = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_32A   = ratan2((s16)scratchEnd[-1].vx, (s16)vector->vz) & 0xFFF;
            dx                = scratchEnd[-1].vx;
            dz                = vector->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) {
                diff            = work->field_32A - (u16)work->field_32C;
                absDiff         = diff >= 0 ? diff : -diff;
                work->field_326 = 0;
                if (absDiff < 0x800) {
                    angle = absDiff;
                } else if (diff > 0) {
                    angle = 0x1000 - diff;
                } else {
                    angle = diff + 0x1000;
                }
                if (angle < 0x30) {
                    work->field_324 = 2;
                    work->field_31C = 6;
                }
            } else {
                timer           = (u16)work->field_32E - 1;
                work->field_32E = timer;
                if (timer <= 0) {
                    scratchEnd[-1].vx = gPlayerStatus.coordMtx->t[0] - work->field_314;
                    vector->vy        = 0;
                    homeDz            = gPlayerStatus.coordMtx->t[2] - work->field_318;
                    vector->vz        = homeDz;
                    homeDx            = scratchEnd[-1].vx;
                    if (SquareRoot0((homeDx * homeDx) + (homeDz * homeDz)) >= 0x7D1) {
                        work->field_324 = 3;
                    }
                }
            }
            break;
        case 2:
            frame           = (s16)work->field_320;
            work->field_326 = 0;
            work->field_342 = 1;
            if (frame == 41) {
                work->obj2A4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            } else if (frame == 42) {
                sound = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190005;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (frame == 44) {
                work->field_342     = 0;
                work->obj2A4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else if (frame >= 76) {
                work->field_324 = 1;
                work->field_31C = 4;
            }
            break;
        case 3:
            work->field_322 = 0;
            work->field_324 = state;
            work->field_31C = state;
            break;
    }
    work->field_328 = (s16)Actor02500_D05B68[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void Actor02500_Fn01144(Task* actor)
{
    Actor02500Work* work;
    GfxCoord*       coord;
    s32             sound;
    s32             pan;
    s32             pan9;
    s32             pan18;
    u32             random;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    work->field_338--;
    if (work->field_338 <= 0) {
        random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_338 = ((random >> 16) & 0x7F) + 0x1E;
        gRandomLcgState = random;
        sound           = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190008;
        pan             = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    if (work->field_326 != 0) {
        work->field_33A++;
        if (work->field_33A == 9) {
            sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190001;
            pan9  = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan9, (s8)worldCoordGetOriginAudioDepth(coord));
        } else if (work->field_33A == 18) {
            sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190002;
            pan18 = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan18, (s8)worldCoordGetOriginAudioDepth(coord));
            work->field_33A = 0;
        }
    } else {
        work->field_33A = 0;
    }
}

static void Actor02500_Fn012F0(Task* actor)
{
    TmdObject*            obj;
    Actor02500Work*       work;
    GfxCoord*             coord;
    s16                   timer2;
    s16                   timer3;
    s16                   timer4;
    s16                   effectTimer;
    s32                   sound;
    s32                   dist;
    s32                   dx;
    s32                   dz;
    s32                   index;
    s32                   i;
    s32                   pan;
    u32                   random;
    ActorFaceScratch*     scratch;
    Actor02500OffsetPair* pair;

    coord   = actor->extra.tmd->coords;
    obj     = actor->extra.tmd;
    work    = actor->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    switch (work->field_324) {
        case 0:
            obj->flags                                                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            dx                                                         = gPlayerStatus.coordMtx->t[0] - work->field_314;
            scratch->delta.vy                                          = 0;
            scratch->delta.vx                                          = dx;
            dz                                                         = gPlayerStatus.coordMtx->t[2] - work->field_318;
            scratch->delta.vz                                          = dz;
            dist                                                       = SquareRoot0((dx * dx) + (dz * dz));
            if (dist < 0x7D0 || gSceneCombatState.actor02500EntranceReady != 0 || gSceneCombatState.expReward != 0) {
                gSceneCombatState.actor02500EntranceReady = 1;
                work->field_324                           = 2;
                work->field_32E                           = ((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 0xA;
            }
            break;
        case 1:
            obj->flags                                                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            if (gSceneCombatState.actor02500EntranceReady != 0 || gSceneCombatState.expReward != 0) {
                work->field_324 = 2;
                work->field_32E = ((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 0xA;
            }
            break;
        case 2:
            obj->flags                                                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            timer2                                                     = (u16)work->field_32E - 1;
            work->field_32E                                            = timer2;
            if (timer2 <= 0) {
                work->field_324 = 3;
                work->field_32E = 0xA;
                work->field_330 = 0x14;
                sound           = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190003;
                pan             = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 3:
            timer3          = (u16)work->field_32E - 1;
            work->field_32E = timer3;
            if (timer3 > 0) {
                obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                Gp_SetLightMode(actor->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
                obj->flags                               = (u16)obj->flags | TMD_OBJECT_SEMI_TRANS;
                work->obj1A4.flags                      |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                ((Enemy*)actor->spawnArg2.pointer)->recs = work->field_1C4;
                work->field_31C                          = 0xA;
                work->field_32E                          = 0;
                work->field_324                          = 4;
            }
            break;
        case 4:
            timer4          = (u16)work->field_32E + 1;
            work->field_32E = timer4;
            if (timer4 < 0x10) {
                obj->flags = (u16)obj->flags | TMD_OBJECT_SEMI_TRANS;
            }
            if (work->field_32E >= 0x1F) {
                work->field_322 = 1;
                work->field_324 = 0;
                Gp_ArmStateF0(1);
            }
            break;
    }
    if (work->field_330 != 0) {
        effectTimer     = (u16)work->field_330 - 1;
        work->field_330 = effectTimer;
        if (!(effectTimer & 3)) {
            random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            i               = 0;
            dist            = ((random >> 0x10) & 0x3F) + 0x12C;
            gRandomLcgState = random;
            index           = (((u16)work->field_330 >> 2) ^ 1) & 1;
            for (; i < 4; i++) {
                pair            = &Actor02500_D05BE8[index + i * 2];
                scratch->rot.vx = (pair->x * dist) >> 0xC;
                scratch->rot.vy = 0;
                scratch->rot.vz = (pair->z * dist) >> 0xC;
                Gp_SpawnEff(EFFECT_DUST_PUFF, actor->extra.tmd->coords, 0x80002400, &scratch->rot);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

static void Actor02500_Fn016FC(Task* arg0)
{
    Actor02500Work*   work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               ang;
    u16               want;
    s16               diff;
    s32               adiff;
    s32               step;
    s32               cur;
    s32               next;
    s32               wrapStep;

    sc    = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_32A;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_32C = ang;
    if (adiff < 0x800) {
        step = work->field_328;
        if (step >= adiff) {
            work->field_32C = want;
        } else {
            next = work->field_32C;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_32C = next;
        }
    } else {
        step = work->field_328;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_32C = work->field_32A;
        goto done;
    turn:
        wrapStep = work->field_328;
        cur      = work->field_32C;
        if (diff > 0) {
            work->field_32C = cur - wrapStep;
        } else {
            work->field_32C = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_32C;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

static void Actor02500_Fn0184C(Task* arg0)
{
    GameLocationKey  key;
    u32              raw1, raw2, raw3;
    u8               areaByte0;
    TmdObject*       model1;
    TmdObject*       model2;
    TmdObject*       model3;
    u32              index1;
    u32              index2;
    u32              index3;
    EffectWork*      effect1;
    EffectWork*      effect2;
    EffectWork*      effect3;
    AreaPlacement*   entry1;
    AreaPlacement*   entry2;
    AreaPlacement*   entry3;
    GameLocationKey* sessionKey1;
    GameLocationKey* sessionKey2;
    GameLocationKey* sessionKey3;

    D_80067704[0] = &_gActor02500ScorpionBurstHead;
    effect1       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect1 != NULL) {
        sessionKey1 = &gGameSession->location.loc;
        raw1        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model1      = effect1->task->extra.tmd;
        key.stage   = sessionKey1->stage;
        key.area    = sessionKey1->area;
        key.room    = sessionKey1->room;
        areaByte0   = gGameSession->location.loc.view;
        index1      = raw1 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry1                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index1);
        model1->texturePageOffset = entry1->texturePageOffset;
        model1->clutRowOffset     = entry1->clutRowOffset;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }
    D_80067704[0] = &_gActor02500ScorpionBurstPincer2;
    effect2       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect2 != NULL) {
        sessionKey2 = &gGameSession->location.loc;
        raw2        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model2      = effect2->task->extra.tmd;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->location.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry2                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index2);
        model2->texturePageOffset = entry2->texturePageOffset;
        model2->clutRowOffset     = entry2->clutRowOffset;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }
    D_80067704[0] = &_gActor02500ScorpionBurstPincer1;
    effect3       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect3 != NULL) {
        sessionKey3 = &gGameSession->location.loc;
        raw3        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model3      = effect3->task->extra.tmd;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->location.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry3                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index3);
        model3->texturePageOffset = entry3->texturePageOffset;
        model3->clutRowOffset     = entry3->clutRowOffset;
        if (model3->buffer != NULL) {
            tmdProcessStream(model3);
            tmdProcessStream(model3);
        }
    }
}

static void Actor02500_Fn01AC8(Enemy* arg0, Task* arg1)
{
    Actor02500Work* work;
    TmdObject*      obj;
    GfxCoord*       coord;
    GfxCoord*       c;
    VECTOR          vec;
    s32             mode;
    s32             one;
    s16             st;
    s16             phase;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    mode  = gSceneCombatState.actorControl;
    coord = obj->coords;
    if (mode == 1) {
        goto case1;
    }
    if (mode < 2) {
        goto common;
    }
    if (mode == 2) {
        goto case2;
    }
    goto common;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
case2:
    obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return;
common:
    one = 1;
    st  = work->field_324;
    if (st == one) {
        goto dying;
    }
    if (st >= 2) {
        goto ge2;
    }
    if (st == 0) {
        goto death;
    }
    return;
ge2:
    if (st == 2) {
        goto destroy;
    }
    if (st == 3) {
        goto case3;
    }
    return;
death:
    work->field_31C = 8;
    work->field_32E = 0;
    work->field_332 = 0x1000;
    work->field_2E4 = coord->coord;
    arg0->recs      = NULL;
    worldTargetUnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->obj16C);
    Gp_UnlinkObj(&work->obj1A4);
    Gp_UnlinkObj(&work->obj20C);
    Gp_UnlinkObj(&work->obj2A4);
    Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
    Gp_ReleaseStateF0Add(arg1, 0x19);
    c      = arg1->extra.tmd->coords;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    if (work->field_33C == 0) {
        work->field_324 = one;
        return;
    }
    obj->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->field_324 = 3;
    return;
dying:
    Actor02500_Fn02480(arg1);
    phase           = work->field_32E + 1;
    work->field_32E = phase;
    if (phase == 10) {
        obj->flags = TMD_OBJECT_SEMI_TRANS;
    }
    if (work->field_32E == 15) {
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
        Gp_SpawnEnemyFromTable(Actor02500_D05B88, 1, 0, arg0);
    }
    if (work->field_32E >= 0x3C) {
        obj->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_324 = 2;
    }
    c      = arg1->extra.tmd->coords;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
destroy:
    enemyDestroy(arg0, arg1);
    return;
case3:
    if (work->field_33C == 0) {
        goto timer;
    }
    if (work->field_33C < 2) {
        goto inc;
    }
    work->field_33C = 0;
    Tmd_FreeBuffers(obj);
    obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    Actor02500_Fn0184C(arg1);
    goto timer;
inc:
    work->field_33C++;
timer:
    phase           = work->field_32E + 1;
    work->field_32E = phase;
    if (phase < 0x3C) {
        return;
    }
    work->field_324 = 2;
}

void Actor02500_Fn01E04(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02500_D00004;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor02500_Fn01E60(Enemy* arg0, Task* arg1)
{
    Actor02500Work* work;
    TmdObject*      temp_a1;
    GfxCoord*       temp_s2;
    s32             state;
    s32             one;

    temp_a1 = arg1->extra.tmd;
    state   = gSceneCombatState.actorControl;
    work    = arg1->work;
    temp_s2 = temp_a1->coords;
    one     = 1;
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
    temp_a1->flags               = 0;
    arg0->node.state.parts.flags = 0;
    goto default_body;
case1:
    if (work->field_322 == 5) {
        return;
    }
    Actor02500_Fn023D8(arg1);
    goto tail;
case2:
    temp_a1->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor02500_Fn01F8C(arg1);
    }
    Actor02500_Fn00494(arg1);
    Actor02500_Fn02008(arg1);
    if (work->field_328 != 0) {
        Actor02500_Fn016FC(arg1);
    }
    Actor02500_Fn02288(arg1);
    Actor02500_Fn02318(arg1);
    temp_s2->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(temp_s2);
    Actor02500_Fn023D8(arg1);
    if (work->field_322 == 5) {
        return;
    }
tail:
    Actor02500_Fn02430(arg1);
}

static void Actor02500_Fn01F8C(Task* actor)
{
    u8              flags;
    u8              remainingFlags;
    Actor02500Work* work;
    Enemy*          ctx;

    ctx   = actor->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = actor->work;
    if (flags & ENEMY_REACTION_BUILDUP) {
        ctx->reactionFlags = (u8)(flags & ENEMY_REACTION_BUILDUP_CLEAR);
        work->field_322    = 4;
        work->field_324    = 0;
    }
    if (ctx->reactionFlags & ENEMY_REACTION_STAGGER) {
        ctx->reactionFlags = (u8)(ctx->reactionFlags & ENEMY_REACTION_STAGGER_CLEAR);
        if (work->field_322 != 4) {
            work->field_322 = 3;
            work->field_324 = 0;
        }
    }
    remainingFlags = ctx->reactionFlags;
    if (remainingFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        ctx->reactionFlags = (u8)(remainingFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
    }
}

/// State handlers of the helper task `Actor02500_Fn02574` dispatches, indexed
/// by `Task::state`: setup, per-frame tick and the countdown that
/// destroys it.
static const EnemyTaskFuncTable3 Actor02500_D00050 = {
    {
        Actor02500_Fn025D0,
        Actor02500_Fn02750,
        Actor02500_Fn02874,
    },
};

static void Actor02500_Fn02008(Task* arg0)
{
    switch (((Actor02500Work*)arg0->work)->field_322) {
        case 0:
            Actor02500_Fn00B18(arg0);
            Actor02500_Fn01144(arg0);
            break;
        case 1:
            Actor02500_Fn00DD8(arg0);
            Actor02500_Fn01144(arg0);
            break;
        case 2:
            Actor02500_Fn020D0(arg0);
            break;
        case 3:
            Actor02500_Fn02178(arg0);
            break;
        case 4:
            Actor02500_Fn021F8(arg0);
            break;
        case 5:
            Actor02500_Fn012F0(arg0);
            break;
        case 6:
            arg0->state = 2;
            break;
    }
}

static void Actor02500_Fn020D0(Task* arg0)
{
    Actor02500Work* work;
    s32             state;

    work  = arg0->work;
    state = work->field_324;
    switch (state) {
        case 0:
            work->field_31C = 7;
            work->field_326 = 0;
            work->field_328 = 0;
            work->field_324 = 1;
            return;
        case 1:
            if ((s16)work->field_320 >= 0x20) {
                if (work->field_33E == state) {
                    work->field_322 = 4;
                    work->field_324 = 0;
                    return;
                }
                if (work->field_344 == state) {
                    work->field_322 = 3;
                    work->field_324 = state;
                    work->field_32E = 0x3C;
                    return;
                }
                work->field_322 = state;
                work->field_324 = 0;
            } else {
                return;
            }
            break;
    }
}

static void Actor02500_Fn02178(Task* arg0)
{
    Actor02500Work* work;
    s32             state;

    work  = arg0->work;
    state = work->field_324;

    switch (state) {
        case 0:
            work->field_31C = 7;
            work->field_344 = 1;
            work->field_326 = 0;
            work->field_328 = 0;
            work->field_32E = 0x3C;
            work->field_324 = 1;
            break;
        case 1:
            if (--work->field_32E <= 0) {
                work->field_322 = state;
                work->field_324 = 0;
                work->field_344 = 0;
            }
            break;
    }
}

static void Actor02500_Fn021F8(Task* arg0)
{
    Actor02500Work* work;
    s32             state;

    work  = arg0->work;
    state = work->field_324;

    switch (state) {
        case 0:
            work->field_31C = 0xB;
            work->field_33E = 1;
            work->field_344 = 0;
            work->field_326 = 0;
            work->field_328 = 0;
            work->field_324 = 1;
            break;
        case 1:
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->field_322 = state;
                work->field_324 = 0;
                work->field_33E = 0;
            }
            break;
    }
}

static void Actor02500_Fn02288(Task* arg0)
{
    Actor02500Work* work;
    GfxCoord*       coord;

    coord              = arg0->extra.tmd->coords;
    work               = arg0->work;
    work->field_304    = coord->coord.t[0];
    work->field_308    = coord->coord.t[1];
    work->field_30C    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_326) >> 0xC;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_326) >> 0xC;
}

static void Actor02500_Fn02318(Task* arg0)
{
    Actor02500Work* work;
    s16             anim;
    s32             value;
    s32             i;
    s32             j;

    work = arg0->work;
    anim = work->field_31C;
    if (anim != work->field_31E) {
        value           = Actor02500_D05BD0[anim];
        i               = 1;
        work->field_31E = work->field_31C;
        work->field_320 = 0;
        do {
            animationSeekSlotWithBlend(&work->anim, i, work->field_31C, 0, value);
            i++;
        } while (i < 5);
        return;
    }
    j = 1;
    work->field_320++;
    do {
        animationTickSlot(&work->anim, j);
        j++;
    } while (j < 5);
}

static void Actor02500_Fn023D8(Task* arg0)
{
    VECTOR    vec;
    GfxCoord* coord;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

static void Actor02500_Fn02430(Task* arg0)
{
    VECTOR3   vec;
    GfxCoord* coord;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x200, 0x80);
}

static void Actor02500_Fn02480(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    Actor02500Work*    work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->field_332 >= 0x201) {
        work->field_332 = (u16)work->field_332 - 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = (s32)work->field_332;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->field_2E4;
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

void Actor02500_Fn02574(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02500_D00050;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor02500_Fn025D0(Enemy* ctx, Task* task)
{
    Actor02500EffWork*     work;
    GfxCoord*              coord;
    WorldCollisionContact* rec;
    GfxCoord*              parentCoord;
    void*                  effect;

    coord       = task->extra.tmd->coords;
    parentCoord = task->parent->extra.tmd->coords;
    work        = memCalloc(0x40, 0);
    if (work == NULL) {
        enemyDestroy(ctx, task);
        return;
    }
    task->work                 = work;
    coord->parent              = &gGfxViewCoord;
    coord->coord               = parentCoord->coord;
    coord->coord.t[0]          = parentCoord->coord.t[0];
    coord->coord.t[1]          = parentCoord->coord.t[1];
    coord->coord.t[2]          = parentCoord->coord.t[2];
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    effect                     = Gp_SpawnEff((EFFECT_GROUND_DECAL | EFFECT_SPAWN_UNLIMITED), coord, 0x10280, NULL);
    work->obj.coord            = coord;
    rec                        = work->rec18;
    work->field_38             = effect;
    work->obj.context.contacts = rec;
    work->obj.pos.vx           = 0;
    work->obj.pos.vy           = 0;
    work->obj.pos.vz           = 0;
    work->obj.key              = Gp_PackPair(Actor02500_D05B30, 1);
    work->obj.radius           = 0xC8;
    work->obj.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(rec, 1, 0);
    work->obj.flags = (u16)(work->obj.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    taskDetachFromParent(task);
    task->state = 1;
}

static void Actor02500_Fn02750(Enemy* ctx, Task* task)
{
    s32                    sound;
    GfxCoord*              coord;
    WorldCollisionContact* rec;
    s32                    done;
    s32                    pan;
    u16                    timer;
    Actor02500EffWork*     work;

    coord = task->extra.tmd->coords;
    work  = (Actor02500EffWork*)((Actor02500Work*)task->work);
    done  = 0;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        rec = work->rec18;
        if (Gp_CountRec18Hi(rec, 0x10000) != 0) {
            done  = 1;
            sound = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190007;
            pan   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        Gp_ClearRec18Occupied(rec);
        timer          = work->field_3C + 1;
        work->field_3C = timer;
        if ((s16)timer >= 0xF1) {
            done = 1;
        }
        if (gSceneCombatState.battleRefs == 0) {
            done = 1;
        }
        if (done != 0) {
            work->field_3E = 0;
            task->state    = 2;
        }
    }
}

static void Actor02500_Fn02874(Enemy* ctx, Task* task)
{
    Actor02500EffWork* work = (Actor02500EffWork*)((Actor02500Work*)task->work);

    switch (work->field_3E) {
        case 0:
            Gp_UnlinkObj(&work->obj);
            if (work->field_38 != NULL) {
                work->field_38->task->state = 3;
            }
            work->field_3C = 0x1E;
            work->field_3E = 1;
            break;
        case 1:
            if (--work->field_3C > 0) {
                break;
            }
            enemyDestroy(ctx, task);
            break;
    }
}
