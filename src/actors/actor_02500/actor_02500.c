#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/world_collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/object_fields.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "gameplay/actor.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/pairsrc.h"
#include "gameplay/world_state.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "overlay.h"

#include "gameplay/animation.h"

/// 0x348-byte work block `Actor02500_Fn00078` allocates and hangs off
/// `Task::work`. It opens with the animation context (`func_800B3F84`
/// arg0) and its five slots, and carries the four list nodes plus their
/// `GpRec18` tables.
typedef struct Actor02500Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot field_14[5];
    /* 0x0DC */ byte       field_DC[0x50];
    /* 0x12C */ byte       field_12C[0x20];
    /* 0x14C */ byte       field_14C[0x20];
    /* 0x16C */ GpObj      obj16C;
    /* 0x18C */ GpRec18    field_18C[1];
    /* 0x1A4 */ GpObj      obj1A4;
    /* 0x1C4 */ GpRec18    field_1C4[3];
    /* 0x20C */ GpObj      obj20C;
    /* 0x22C */ GpRec18    field_22C[5];
    /* 0x2A4 */ GpObj      obj2A4;
    /* 0x2C4 */ GpRec18    field_2C4[1];
    /* 0x2DC */ GpEffArg   field_2DC; // record the hit's effect is spawned with
    /* 0x2E4 */ MATRIX     field_2E4;
    /* 0x304 */ s32        field_304;
    /* 0x308 */ s32        field_308;
    /* 0x30C */ s32        field_30C;
    /* 0x310 */ byte       pad_310[4];
    /* 0x314 */ s16        field_314;
    /* 0x316 */ s16        field_316;
    /* 0x318 */ s16        field_318;
    /* 0x31A */ byte       pad_31A[2];
    /* 0x31C */ s16        field_31C;
    /* 0x31E */ s16        field_31E;
    /* 0x320 */ u16        field_320;
    /* 0x322 */ s16        field_322;
    /* 0x324 */ s16        field_324;
    /* 0x326 */ s16        field_326;
    /* 0x328 */ s16        field_328;
    /* 0x32A */ u16        field_32A;
    /* 0x32C */ s16        field_32C;
    /* 0x32E */ s16        field_32E;
    /* 0x330 */ s16        field_330;
    /* 0x332 */ s16        field_332;
    /* 0x334 */ s16        field_334;
    /* 0x336 */ s16        field_336;
    /* 0x338 */ s16        field_338;
    /* 0x33A */ s16        field_33A;
    /* 0x33C */ s16        field_33C;
    /* 0x33E */ s16        field_33E;
    /* 0x340 */ s16        field_340;
    /* 0x342 */ s16        field_342;
    /* 0x344 */ s16        field_344;
    /* 0x346 */ byte       pad_346[2];
} Actor02500Work;
STATIC_ASSERT_SIZEOF(Actor02500Work, 0x348);

/// Work block of the small helper task `Actor02500_L02634` spawns, also parked
/// at `Task::work`. It opens with a list node and its one-entry
/// `GpRec18` table, then the spawned effect and the countdown/state
/// pair `Actor02500_Fn02874` runs on.
typedef struct Actor02500EffWork {
    /* 0x00 */ GpObj      obj;
    /* 0x20 */ GpRec18    rec18[1];
    /* 0x38 */ GpEffWork* field_38;
    /* 0x3C */ s16        field_3C;
    /* 0x3E */ s16        field_3E;
} Actor02500EffWork;
STATIC_ASSERT_SIZEOF(Actor02500EffWork, 0x40);

/// 0x30-byte frame `Actor02500_Fn00494` carves off the scratchpad stack. It
/// opens with the 16.16 movement delta `func_800E0C10` resolves, and keeps the `VectorNormal` unit vector and the
/// grid-space direction `ApplyTransposeMatrixLV` produces from it.
typedef struct Actor02500MoveScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ VECTOR         normal;
    /* 0x20 */ VECTOR         dir;
} Actor02500MoveScratch;
STATIC_ASSERT_SIZEOF(Actor02500MoveScratch, 0x30);

typedef struct Actor02500OffsetPair {
    s16 x;
    s16 z;
} Actor02500OffsetPair;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern GpPairSrcE           Actor02500_D05B38;
extern GpU16Pair            Actor02500_D05B30[];
extern s16                  Actor02500_D05B48[];
extern s16                  Actor02500_D05B58[];
extern s16                  Actor02500_D05B68[];
extern s16                  Actor02500_D05B78[];
extern TaskDesc Actor02500_D05B88[];
extern GpAnimSet* Actor02500_D05BA0[12];
extern s16                  Actor02500_D05BD0[];
extern Actor02500OffsetPair Actor02500_D05BE8[];
extern TmdSource Actor02500_D04448;
extern TmdSource Actor02500_D0478C;
extern TmdSource Actor02500_D04AD0;
extern void*                D_80067704[1];

static void Actor02500_Fn00078(GpEnemy* ctx, Task* actor);
static void Actor02500_Fn01AC8(GpEnemy* ctx, Task* actor);
static void Actor02500_Fn01E60(GpEnemy* ctx, Task* actor);
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
static void Actor02500_Fn025D0(GpEnemy* ctx, Task* task);
static void Actor02500_Fn02750(GpEnemy* ctx, Task* task);
static void Actor02500_Fn02874(GpEnemy* ctx, Task* task);

/// State handlers of the enemy task `Actor02500_Fn01E04` dispatches, indexed
/// by `Task::state`: spawn, per-frame tick and the dying sequence.
static const GpEnemyTaskFuncTable3 Actor02500_D00004 = {
    {
        Actor02500_Fn00078,
        Actor02500_Fn01E60,
        Actor02500_Fn01AC8,
    },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} Actor02500PoseBank4AF4;

extern GpAnimSet Actor02500_D04C64;
extern GpAnimSet Actor02500_D04E3C;
extern GpAnimSet Actor02500_D04FF0;
extern GpAnimSet Actor02500_D0534C;
extern GpAnimSet Actor02500_D05520;
extern GpAnimSet Actor02500_D057D4;
extern GpAnimSet Actor02500_D05998;
extern GpAnimSet Actor02500_D05B08;
extern TmdSource Actor02500_D03F20;
void Actor02500_Fn01E04(Task *);
void Actor02500_Fn02574(Task *);

TmdBone Actor02500_D02914[5] = {
#include "assets/actor_102500_model_03F20_skeleton.inc"
};

u32 Actor02500_D029C8[5] = {
#include "assets/actor_102500_model_03F20_partVerts.inc"
};

SVECTOR Actor02500_D029DC[93] = {
#include "assets/actor_102500_model_03F20_verts.inc"
};

SVECTOR Actor02500_D02CC4[93] = {
#include "assets/actor_102500_model_03F20_normals.inc"
};

u32 Actor02500_D02FAC[989] = {
#include "assets/actor_102500_model_03F20_stream.inc"
};

TmdSource Actor02500_D03F20 = {
    0, 5684, 1152, 5,
    Actor02500_D029C8, Actor02500_D029DC, Actor02500_D02CC4, Actor02500_D02914, Actor02500_D02FAC,
};

TmdBone Actor02500_D03F44[1] = {
#include "assets/actor_102500_model_04448_skeleton.inc"
};

u32 Actor02500_D03F68[1] = {
#include "assets/actor_102500_model_04448_partVerts.inc"
};

SVECTOR Actor02500_D03F6C[22] = {
#include "assets/actor_102500_model_04448_verts.inc"
};

SVECTOR Actor02500_D0401C[22] = {
#include "assets/actor_102500_model_04448_normals.inc"
};

u32 Actor02500_D040CC[223] = {
#include "assets/actor_102500_model_04448_stream.inc"
};

TmdSource Actor02500_D04448 = {
    0, 1292, 0, 1,
    Actor02500_D03F68, Actor02500_D03F6C, Actor02500_D0401C, Actor02500_D03F44, Actor02500_D040CC,
};

TmdBone Actor02500_D0446C[1] = {
#include "assets/actor_102500_model_0478C_skeleton.inc"
};

u32 Actor02500_D04490[1] = {
#include "assets/actor_102500_model_0478C_partVerts.inc"
};

SVECTOR Actor02500_D04494[15] = {
#include "assets/actor_102500_model_0478C_verts.inc"
};

SVECTOR Actor02500_D0450C[15] = {
#include "assets/actor_102500_model_0478C_normals.inc"
};

u32 Actor02500_D04584[130] = {
#include "assets/actor_102500_model_0478C_stream.inc"
};

TmdSource Actor02500_D0478C = {
    0, 844, 0, 1,
    Actor02500_D04490, Actor02500_D04494, Actor02500_D0450C, Actor02500_D0446C, Actor02500_D04584,
};

TmdBone Actor02500_D047B0[1] = {
#include "assets/actor_102500_model_04AD0_skeleton.inc"
};

u32 Actor02500_D047D4[1] = {
#include "assets/actor_102500_model_04AD0_partVerts.inc"
};

SVECTOR Actor02500_D047D8[15] = {
#include "assets/actor_102500_model_04AD0_verts.inc"
};

SVECTOR Actor02500_D04850[15] = {
#include "assets/actor_102500_model_04AD0_normals.inc"
};

u32 Actor02500_D048C8[130] = {
#include "assets/actor_102500_model_04AD0_stream.inc"
};

TmdSource Actor02500_D04AD0 = {
    0, 844, 0, 1,
    Actor02500_D047D4, Actor02500_D047D8, Actor02500_D04850, Actor02500_D047B0, Actor02500_D048C8,
};

Actor02500PoseBank4AF4 Actor02500_D04AF4 = { .poses = {
#include "assets/actor_102500_animation_04C64_bank1.inc"
} };

GpPackedSvec Actor02500_D04B54[21] = {
#include "assets/actor_102500_animation_04C64_bank4.inc"
};

GpAnimRec Actor02500_D04BA8[44] = {
#include "assets/actor_102500_animation_04C64_records.inc"
};

u16 Actor02500_D04C58[6] = {
#include "assets/actor_102500_animation_04C64_indices.inc"
};

GpAnimSet Actor02500_D04C64 = {
    Actor02500_D04BA8, Actor02500_D04C58,
    { NULL, Actor02500_D04AF4.words, NULL, NULL, Actor02500_D04B54, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} Actor02500PoseBank4C8C;

Actor02500PoseBank4C8C Actor02500_D04C8C = { .poses = {
#include "assets/actor_102500_animation_04E3C_bank1.inc"
} };

GpPackedSvec Actor02500_D04D04[27] = {
#include "assets/actor_102500_animation_04E3C_bank4.inc"
};

GpAnimRec Actor02500_D04D70[48] = {
#include "assets/actor_102500_animation_04E3C_records.inc"
};

u16 Actor02500_D04E30[6] = {
#include "assets/actor_102500_animation_04E3C_indices.inc"
};

GpAnimSet Actor02500_D04E3C = {
    Actor02500_D04D70, Actor02500_D04E30,
    { NULL, Actor02500_D04C8C.words, NULL, NULL, Actor02500_D04D04, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} Actor02500PoseBank4E64;

Actor02500PoseBank4E64 Actor02500_D04E64 = { .poses = {
#include "assets/actor_102500_animation_04FF0_bank1.inc"
} };

GpPackedSvec Actor02500_D04EB8[27] = {
#include "assets/actor_102500_animation_04FF0_bank4.inc"
};

GpAnimRec Actor02500_D04F24[48] = {
#include "assets/actor_102500_animation_04FF0_records.inc"
};

u16 Actor02500_D04FE4[6] = {
#include "assets/actor_102500_animation_04FF0_indices.inc"
};

GpAnimSet Actor02500_D04FF0 = {
    Actor02500_D04F24, Actor02500_D04FE4,
    { NULL, Actor02500_D04E64.words, NULL, NULL, Actor02500_D04EB8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[20];
    GpPackedSvec words[60];
} Actor02500PoseBank5018;

Actor02500PoseBank5018 Actor02500_D05018 = { .poses = {
#include "assets/actor_102500_animation_0534C_bank1.inc"
} };

GpPackedSvec Actor02500_D05108[55] = {
#include "assets/actor_102500_animation_0534C_bank4.inc"
};

GpAnimRec Actor02500_D051E4[87] = {
#include "assets/actor_102500_animation_0534C_records.inc"
};

u16 Actor02500_D05340[6] = {
#include "assets/actor_102500_animation_0534C_indices.inc"
};

GpAnimSet Actor02500_D0534C = {
    Actor02500_D051E4, Actor02500_D05340,
    { NULL, Actor02500_D05018.words, NULL, NULL, Actor02500_D05108, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} Actor02500PoseBank5374;

Actor02500PoseBank5374 Actor02500_D05374 = { .poses = {
#include "assets/actor_102500_animation_05520_bank1.inc"
} };

GpPackedSvec Actor02500_D053EC[27] = {
#include "assets/actor_102500_animation_05520_bank4.inc"
};

GpAnimRec Actor02500_D05458[47] = {
#include "assets/actor_102500_animation_05520_records.inc"
};

u16 Actor02500_D05514[6] = {
#include "assets/actor_102500_animation_05520_indices.inc"
};

GpAnimSet Actor02500_D05520 = {
    Actor02500_D05458, Actor02500_D05514,
    { NULL, Actor02500_D05374.words, NULL, NULL, Actor02500_D053EC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[16];
    GpPackedSvec words[48];
} Actor02500PoseBank5548;

Actor02500PoseBank5548 Actor02500_D05548 = { .poses = {
#include "assets/actor_102500_animation_057D4_bank1.inc"
} };

GpPackedSvec Actor02500_D05608[45] = {
#include "assets/actor_102500_animation_057D4_bank4.inc"
};

GpAnimRec Actor02500_D056BC[67] = {
#include "assets/actor_102500_animation_057D4_records.inc"
};

u16 Actor02500_D057C8[6] = {
#include "assets/actor_102500_animation_057D4_indices.inc"
};

GpAnimSet Actor02500_D057D4 = {
    Actor02500_D056BC, Actor02500_D057C8,
    { NULL, Actor02500_D05548.words, NULL, NULL, Actor02500_D05608, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} Actor02500PoseBank57FC;

Actor02500PoseBank57FC Actor02500_D057FC = { .poses = {
#include "assets/actor_102500_animation_05998_bank1.inc"
} };

GpPackedSvec Actor02500_D05874[27] = {
#include "assets/actor_102500_animation_05998_bank4.inc"
};

GpAnimRec Actor02500_D058E0[43] = {
#include "assets/actor_102500_animation_05998_records.inc"
};

u16 Actor02500_D0598C[6] = {
#include "assets/actor_102500_animation_05998_indices.inc"
};

GpAnimSet Actor02500_D05998 = {
    Actor02500_D058E0, Actor02500_D0598C,
    { NULL, Actor02500_D057FC.words, NULL, NULL, Actor02500_D05874, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} Actor02500PoseBank59C0;

Actor02500PoseBank59C0 Actor02500_D059C0 = { .poses = {
#include "assets/actor_102500_animation_05B08_bank1.inc"
} };

GpPackedSvec Actor02500_D05A14[18] = {
#include "assets/actor_102500_animation_05B08_bank4.inc"
};

GpAnimRec Actor02500_D05A5C[40] = {
#include "assets/actor_102500_animation_05B08_records.inc"
};

u16 Actor02500_D05AFC[6] = {
#include "assets/actor_102500_animation_05B08_indices.inc"
};

GpAnimSet Actor02500_D05B08 = {
    Actor02500_D05A5C, Actor02500_D05AFC,
    { NULL, Actor02500_D059C0.words, NULL, NULL, Actor02500_D05A14, NULL, NULL, NULL },
};

GpU16Pair Actor02500_D05B30[2] = {
    { 10, 3 },
    { 1, 3 },
};

GpPairSrcE Actor02500_D05B38 = { Actor02500_D05B30, 68, 20, 8, 1, 0, 20, 0, 0, 0 };

s16 Actor02500_D05B48[8] = {
    16, 18, 20, 22, 22, 22, 22, 22,
};

s16 Actor02500_D05B58[8] = {
    24, 26, 28, 30, 30, 30, 30, 30,
};

s16 Actor02500_D05B68[8] = {
    18, 22, 26, 30, 30, 30, 30, 30,
};

s16 Actor02500_D05B78[8] = {
    30, 34, 34, 38, 38, 38, 38, 38,
};

TaskDesc Actor02500_D05B88[2] = {
    { 1, 96, Actor02500_Fn01E04, { .model = &Actor02500_D03F20 } },
    { 2, 96, Actor02500_Fn02574, { .model = NULL } },
};

GpAnimSet * Actor02500_D05BA0[12] = {
    NULL,
    &Actor02500_D04C64,
    NULL,
    &Actor02500_D04E3C,
    &Actor02500_D04FF0,
    NULL,
    &Actor02500_D0534C,
    &Actor02500_D05520,
    &Actor02500_D057D4,
    NULL,
    &Actor02500_D05998,
    &Actor02500_D05B08,
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

static void Actor02500_Fn00078(GpEnemy* ctx, Task* actor)
{
    Actor02500Work* work;
    TmdObject*      obj;
    GpCoord*        coord;
    s32             i;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x348, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work   = work;
    obj->flags    = 0;
    coord->flg    = 0;
    obj->lightMtx = (MATRIX*)work->field_14C;
    obj->colorMtx = (MATRIX*)work->field_12C;
    ctx->field_4  = &coord->coord;
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    ctx->bodyPos.vy            = -0x96;
    ctx->coord                 = coord;
    ctx->node.state.b.flags    = 0;
    ctx->bodyPos.vx            = 0;
    ctx->bodyPos.vz            = 0;
    ctx->param                 = &Actor02500_D05B38;
    ctx->hp                    = Actor02500_D05B38.hpMax;
    work->field_2DC.spawnArgLo = 0x200;
    work->field_2DC.coord      = coord;
    work->field_2DC.spawnArgHi = 1;
    func_800B3F84(&work->anim, Actor02500_D05BA0, obj, work->field_DC, work->field_14);
    work->field_31C = 1;
    work->field_31E = 1;
    for (i = 1; i < 5; i++) {
        Gp_AnimResetSlot(&work->anim, i, work->field_31C);
    }
    Gp_IncStateF0Ref(0);
    switch (ctx->place->mode) {
        case 0:
            work->field_322 = 0;
            work->field_324 = 0;
            ctx->recs       = work->field_1C4;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_32E = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x1E;
            break;
        case 1:
            work->field_322 = 5;
            work->field_324 = 0;
            ctx->recs       = NULL;
            Gp_SetLightMode(ctx, 2);
            break;
        case 2:
            work->field_322 = 5;
            work->field_324 = 1;
            ctx->recs       = NULL;
            Gp_SetLightMode(ctx, 2);
            break;
    }
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_338 = (((u32)Gp_LcgState >> 16) & 0x7F) + 0x1E;
    work->field_314 = coord->coord.t[0];
    work->field_316 = coord->coord.t[1];
    work->field_318 = coord->coord.t[2];
    work->field_32A = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;

    work->obj16C.coord    = coord;
    work->obj16C.ctx.recs = work->field_18C;
    work->obj16C.pos.vx   = 0;
    work->obj16C.pos.vy   = -0x190;
    work->obj16C.pos.vz   = 0x258;
    work->obj16C.key      = 0;
    work->obj16C.radius   = 0x258;
    work->obj16C.flags    = 1;
    Gp_LinkObj(3, &work->obj16C);
    Gp_InitRec18Table(work->field_18C, 1, 0);

    work->obj1A4.coord    = coord;
    work->obj1A4.ctx.recs = work->field_1C4;
    work->obj1A4.pos.vx   = 0;
    work->obj1A4.pos.vy   = -0x12C;
    work->obj1A4.pos.vz   = 0;
    work->obj1A4.key      = 0x30019;
    work->obj1A4.radius   = 0x12C;
    work->obj1A4.flags    = 1;
    work->obj16C.flags   |= 0x8000;
    Gp_LinkObj(2, &work->obj1A4);
    Gp_InitRec18Table(work->field_1C4, 3, 0);

    if (ctx->place->mode == 0) {
        work->obj1A4.flags |= 0x8000;
    } else {
        work->obj1A4.flags &= 0x7FFF;
    }

    work->obj20C.coord    = coord;
    work->obj20C.ctx.recs = work->field_22C;
    work->obj20C.pos.vx   = 0;
    work->obj20C.pos.vy   = -0x12C;
    work->obj20C.pos.vz   = 0;
    work->obj20C.key      = 0x30019;
    work->obj20C.radius   = 0x12C;
    work->obj20C.flags    = 1;
    Gp_LinkObj(2, &work->obj20C);
    Gp_InitRec18Table(work->field_22C, 5, 0);
    work->obj20C.flags |= 0x4200;

    work->obj2A4.coord    = actor->extra.tmd->coords + 4;
    work->obj2A4.ctx.recs = work->field_2C4;
    work->obj2A4.pos.vx   = 0;
    work->obj2A4.pos.vy   = -0x3B6;
    work->obj2A4.pos.vz   = 0x1CC;
    work->obj2A4.key      = Gp_PackPair(Actor02500_D05B30, 0);
    work->obj2A4.radius   = 0x12C;
    work->obj2A4.flags    = 1;
    Gp_LinkObj(3, &work->obj2A4);
    Gp_InitRec18Table(work->field_2C4, 1, 0);
    work->obj2A4.flags &= 0x7FFF;
    actor->state        = 1;
}

/// Per-frame collision and damage pass. Carves a `Actor02500MoveScratch` off
/// the scratchpad stack, lets `func_800E0C10` resolve this frame's movement
/// into it, then walks the three `field_1C4` records: kind 2 is a hit that
/// costs the enemy HP and plays a sound, kinds 1 and 3 push it away from the
/// obstacle, and the strongest push is applied to the coordinate at the end.
static void Actor02500_Fn00494(Task* actor)
{
    u32                    lastId;
    Actor02500Work*        work;
    GpEnemy*               ctx;
    GpCoord*               coord;
    GpCoord*               target;
    Actor02500MoveScratch* head;
    Actor02500MoveScratch* frame;
    VECTOR*                normal;
    s32                    i;
    s32                    push;
    s32                    bestPush;
    s32                    damage;
    s32                    param0;
    s32                    cooldown;
    s32                    soundId;

    bestPush = 0;
    lastId   = 0;
    work     = actor->work;
    head     = SCRATCH_HEAD(Actor02500MoveScratch);
    frame = SCRATCH_HEAD(Actor02500MoveScratch) = head - 1;
    coord                                       = actor->extra.tmd->coords;
    ctx                                         = actor->spawnArg2.pointer;
    work->field_340                             = 0;
    switch (func_800E0C10(work->field_22C, &frame->delta, 5, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].delta.vx.h.hi;
            coord->coord.t[1] += frame->delta.vy.h.hi;
            coord->coord.t[2] += frame->delta.vz.h.hi;
            if (head[-1].delta.vx.w != 0 || frame->delta.vz.w != 0) {
                work->field_340 = 1;
            }
            break;
        case 2:
            coord->coord.t[0] = work->field_304;
            coord->coord.t[1] = work->field_308;
            coord->coord.t[2] = work->field_30C;
            if (head[-1].delta.vx.w != 0 || frame->delta.vz.w != 0) {
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
        switch ((u32)work->field_1C4[i].key >> 16) {
            case 2:
                if (work->field_334 == 0) {
                    target            = Gp_ActorSlots[((u32)work->field_1C4[i].key >> 7) & 1]->extra.tmd->coords;
                    frame->delta.vx.w = target->coord.t[0] - coord->coord.t[0];
                    frame->delta.vy.w = target->coord.t[1] - coord->coord.t[1];
                    frame->delta.vz.w = target->coord.t[2] - coord->coord.t[2];
                    damage            = Gp_ComputeDamage(work->field_1C4[i].key,
                                                         SquareRoot0(frame->delta.vx.w * frame->delta.vx.w + frame->delta.vy.w * frame->delta.vy.w +
                                                                     frame->delta.vz.w * frame->delta.vz.w),
                                                         0, 0);
                    param0            = Gp_GetIdParam0(work->field_1C4[i].key);
                    if ((param0 & 0xFFFF) == 5) {
                        damage *= 2;
                        Gp_SpawnEff(0x6009C, coord, 2, NULL);
                    }
                    if (Gp_RollEnemyChance(ctx, work->field_1C4[i].key, 0) != 0) {
                        damage *= 4;
                        if ((param0 & 0xFFFF) != 5) {
                            Gp_SpawnEff(0x6009C, coord, 0, NULL);
                        }
                    }
                    func_800E2C78(ctx, work->field_1C4[i].key, damage, 0);
                    func_800DA6E8(&ctx->node, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        work->field_322     = 6;
                        work->field_324     = 0;
                        work->obj2A4.flags &= 0x7FFF;
                        soundId             = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x4019000A;
                        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    } else {
                        if (work->field_342 == 0) {
                            work->field_322 = 2;
                            work->field_324 = 0;
                        }
                        work->field_342 = 0;
                        soundId         = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x40190009;
                        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
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
                            work->obj2A4.flags &= 0x7FFF;
                            break;
                        case 2:
                            Gp_SetObjFlag2(ctx, work->field_1C4[i].key, 0);
                            work->obj2A4.flags &= 0x7FFF;
                            break;
                        case 4:
                        case 6:
                            if (ctx->hp <= 0) {
                                work->field_33C = 1;
                            }
                            break;
                    }
                    if (lastId != work->field_1C4[i].key) {
                        lastId = work->field_1C4[i].key;
                        func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, coord, 0, &work->field_2DC);
                    }
                    cooldown = Gp_GetIdParam2(work->field_1C4[i].key);
                    if (cooldown > 0) {
                        work->field_334 = cooldown;
                    }
                }
                break;
            case 0:
                break;
            /* Kinds 1 and 3 push the enemy back out of the obstacle the same way. */
            case 1:
                frame->delta.vx.w = coord->workm.t[0] - work->field_1C4[i].point.vx;
                frame->delta.vy.w = coord->workm.t[1] - work->field_1C4[i].point.vy;
                frame->delta.vz.w = coord->workm.t[2] - work->field_1C4[i].point.vz;
                push              = work->field_1C4[i].depth -
                       SquareRoot0(frame->delta.vx.w * frame->delta.vx.w + frame->delta.vy.w * frame->delta.vy.w +
                                   frame->delta.vz.w * frame->delta.vz.w);
                push = (push <= 0) ? 0 : push;
                if (bestPush < push) {
                    bestPush = push;
                    VectorNormal((VECTOR*)&frame->delta, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, &frame->dir);
                }
                break;
            case 3:
                frame->delta.vx.w = coord->workm.t[0] - work->field_1C4[i].point.vx;
                frame->delta.vy.w = coord->workm.t[1] - work->field_1C4[i].point.vy;
                frame->delta.vz.w = coord->workm.t[2] - work->field_1C4[i].point.vz;
                push              = work->field_1C4[i].depth -
                       SquareRoot0(frame->delta.vx.w * frame->delta.vx.w + frame->delta.vy.w * frame->delta.vy.w +
                                   frame->delta.vz.w * frame->delta.vz.w);
                push = (push <= 0) ? 0 : push;
                if (bestPush < push) {
                    bestPush = push;
                    VectorNormal((VECTOR*)&frame->delta, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, &frame->dir);
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
        work->obj2A4.flags &= 0x7FFF;
        soundId             = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x40190006;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    }
    Gp_ClearRec18Occupied(work->field_2C4);
    if (Gp_CountRec18Hi(work->field_18C, 0x10000) != 0 && work->field_322 == 0) {
        work->field_322 = 1;
        work->field_324 = 0;
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(work->field_18C);
    SCRATCH_POP_BYTES(0x30);
}

static void Actor02500_Fn00B18(Task* actor)
{
    Actor02500Work* work;
    GpCoord*        coord;
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

    scratchEnd         = SCRATCH_HEAD(VECTOR);
    vector             = scratchEnd - 1;
    SCRATCH_HEAD(void) = vector;
    work               = actor->work;
    state              = work->field_324;
    coord              = actor->extra.tmd->coords;
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
                randomAngle     = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = randomAngle;
                work->field_32A = ((u32)randomAngle >> 0x10) & 0xFFF;
            }
            break;
        case 1:
            work->field_326 = 0;
            if (work->field_32C == (s16)work->field_32A) {
                work->field_324 = 2;
                randomMoveTime  = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = randomMoveTime;
                work->field_32E = (((u32)randomMoveTime >> 0x10) & 0x7F) + 0x1E;
            }
            break;
        case 2:
            work->field_326   = (s16)Actor02500_D05B58[((GpEnemy*)actor->spawnArg2.pointer)->place->rowIndex];
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
                randomIdleTime  = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = randomIdleTime;
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
    work->field_328 = (s16)Actor02500_D05B48[((GpEnemy*)actor->spawnArg2.pointer)->place->rowIndex];
    SCRATCH_POP_BYTES(0x10);
}

static void Actor02500_Fn00DD8(Task* actor)
{
    Actor02500Work* work;
    GpCoord*        coord;
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

    scratchEnd         = SCRATCH_HEAD(VECTOR);
    vector             = scratchEnd - 1;
    SCRATCH_HEAD(void) = vector;
    work               = actor->work;
    state              = work->field_324;
    coord              = actor->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_31C = 4;
            work->field_32E = 0xF0;
            work->field_326 = 0;
            work->field_324 = 1;
            break;
        case 1:
            work->field_326   = (s16)Actor02500_D05B78[((GpEnemy*)actor->spawnArg2.pointer)->place->rowIndex];
            scratchEnd[-1].vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vector->vy        = 0;
            vector->vz        = Player_Status.coordMtx->t[2] - coord->coord.t[2];
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
                    scratchEnd[-1].vx = Player_Status.coordMtx->t[0] - work->field_314;
                    vector->vy        = 0;
                    homeDz            = Player_Status.coordMtx->t[2] - work->field_318;
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
                work->obj2A4.flags |= 0x8000;
            } else if (frame == 42) {
                sound = (((u16)((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x40190005;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            } else if (frame == 44) {
                work->field_342     = 0;
                work->obj2A4.flags &= 0x7FFF;
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
    work->field_328 = (s16)Actor02500_D05B68[((GpEnemy*)actor->spawnArg2.pointer)->place->rowIndex];
    SCRATCH_POP_BYTES(0x10);
}

static void Actor02500_Fn01144(Task* actor)
{
    Actor02500Work* work;
    GpCoord*        coord;
    s32             sound;
    s32             pan;
    s32             pan9;
    s32             pan18;
    u32             random;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    work->field_338--;
    if (work->field_338 <= 0) {
        random          = Gp_LcgState * 5 + 0x71357911;
        work->field_338 = ((random >> 16) & 0x7F) + 0x1E;
        Gp_LcgState     = random;
        sound           = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40190008;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
    }
    if (work->field_326 != 0) {
        work->field_33A++;
        if (work->field_33A == 9) {
            sound = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40190001;
            pan9  = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan9, (s8)gpGetObjDepth(coord));
        } else if (work->field_33A == 18) {
            sound = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40190002;
            pan18 = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan18, (s8)gpGetObjDepth(coord));
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
    GpCoord*              coord;
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
    scratch = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
    switch (work->field_324) {
        case 0:
            obj->flags                                       = 0x80;
            ((GpEnemy*)actor->spawnArg2.pointer)->node.state.b.flags = 1;
            dx                                               = Player_Status.coordMtx->t[0] - work->field_314;
            scratch->delta.vy                                = 0;
            scratch->delta.vx                                = dx;
            dz                                               = Player_Status.coordMtx->t[2] - work->field_318;
            scratch->delta.vz                                = dz;
            dist                                             = SquareRoot0((dx * dx) + (dz * dz));
            if (dist < 0x7D0 || Gp_StateF0.field_21 != 0 || Gp_StateF0.field_8 != 0) {
                Gp_StateF0.field_21 = 1;
                work->field_324     = 2;
                work->field_32E     = ((u16)((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) * 0xA;
            }
            break;
        case 1:
            obj->flags                                       = 0x80;
            ((GpEnemy*)actor->spawnArg2.pointer)->node.state.b.flags = 1;
            if (Gp_StateF0.field_21 != 0 || Gp_StateF0.field_8 != 0) {
                work->field_324 = 2;
                work->field_32E = ((u16)((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) * 0xA;
            }
            break;
        case 2:
            obj->flags                                       = 0x80;
            ((GpEnemy*)actor->spawnArg2.pointer)->node.state.b.flags = 1;
            timer2                                           = (u16)work->field_32E - 1;
            work->field_32E                                  = timer2;
            if (timer2 <= 0) {
                work->field_324 = 3;
                work->field_32E = 0xA;
                work->field_330 = 0x14;
                sound           = (((u16)((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x40190003;
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
            }
            break;
        case 3:
            timer3          = (u16)work->field_32E - 1;
            work->field_32E = timer3;
            if (timer3 > 0) {
                obj->flags = 0x80;
            } else {
                Gp_SetLightMode(actor->spawnArg2.pointer, 0);
                obj->flags                         = (u16)obj->flags | 2;
                work->obj1A4.flags                |= 0x8000;
                ((GpEnemy*)actor->spawnArg2.pointer)->recs = work->field_1C4;
                work->field_31C                    = 0xA;
                work->field_32E                    = 0;
                work->field_324                    = 4;
            }
            break;
        case 4:
            timer4          = (u16)work->field_32E + 1;
            work->field_32E = timer4;
            if (timer4 < 0x10) {
                obj->flags = (u16)obj->flags | 2;
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
            random      = (Gp_LcgState * 5) + 0x71357911;
            i           = 0;
            dist        = ((random >> 0x10) & 0x3F) + 0x12C;
            Gp_LcgState = random;
            index       = (((u16)work->field_330 >> 2) ^ 1) & 1;
            for (; i < 4; i++) {
                pair            = &Actor02500_D05BE8[index + i * 2];
                scratch->rot.vx = (pair->x * dist) >> 0xC;
                scratch->rot.vy = 0;
                scratch->rot.vz = (pair->z * dist) >> 0xC;
                Gp_SpawnEff(0x60054, actor->extra.tmd->coords, 0x80002400, &scratch->rot);
            }
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

static void Actor02500_Fn016FC(Task* arg0)
{
    Actor02500Work*   work;
    GpCoord*          coord;
    ActorFaceScratch* sc;
    s32               ang;
    u16               want;
    s16               diff;
    s32               adiff;
    s32               step;
    s32               cur;
    s32               next;
    s32               wrapStep;

    sc    = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
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
    SCRATCH_POP_BYTES(0x18);
}

static void Actor02500_Fn0184C(Task* arg0)
{
    GpAreaKey    key;
    u32          raw1, raw2, raw3;
    u8           areaByte0;
    TmdObject*   model1;
    TmdObject*   model2;
    TmdObject*   model3;
    u32          index1;
    u32          index2;
    u32          index3;
    GpEffWork*   effect1;
    GpEffWork*   effect2;
    GpEffWork*   effect3;
    GpAreaPlace* entry1;
    GpAreaPlace* entry2;
    GpAreaPlace* entry3;
    GpAreaKey*   sessionKey1;
    GpAreaKey*   sessionKey2;
    GpAreaKey*   sessionKey3;

    D_80067704[0] = &Actor02500_D04448;
    effect1       = Gp_SpawnEff(0x40007, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect1 != NULL) {
        sessionKey1 = &gGameSession->at4.loc;
        raw1        = ((GpEnemy*)arg0->spawnArg2.pointer)->placeKey;
        model1      = effect1->task->extra.tmd;
        key.stage   = sessionKey1->stage;
        key.area    = sessionKey1->area;
        key.room    = sessionKey1->room;
        areaByte0   = gGameSession->at4.loc.view;
        index1      = raw1 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry1        = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->field_0, index1);
        model1->tpage = entry1->tpage;
        model1->clut  = entry1->clut;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }
    D_80067704[0] = &Actor02500_D0478C;
    effect2       = Gp_SpawnEff(0x40007, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect2 != NULL) {
        sessionKey2 = &gGameSession->at4.loc;
        raw2        = ((GpEnemy*)arg0->spawnArg2.pointer)->placeKey;
        model2      = effect2->task->extra.tmd;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->at4.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2        = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->field_0, index2);
        model2->tpage = entry2->tpage;
        model2->clut  = entry2->clut;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }
    D_80067704[0] = &Actor02500_D04AD0;
    effect3       = Gp_SpawnEff(0x40007, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect3 != NULL) {
        sessionKey3 = &gGameSession->at4.loc;
        raw3        = ((GpEnemy*)arg0->spawnArg2.pointer)->placeKey;
        model3      = effect3->task->extra.tmd;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->at4.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3        = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->field_0, index3);
        model3->tpage = entry3->tpage;
        model3->clut  = entry3->clut;
        if (model3->buffer != NULL) {
            tmdProcessStream(model3);
            tmdProcessStream(model3);
        }
    }
}

static void Actor02500_Fn01AC8(GpEnemy* arg0, Task* arg1)
{
    Actor02500Work* work;
    TmdObject*      obj;
    GpCoord*        coord;
    GpCoord*        c;
    VECTOR          vec;
    s32             mode;
    s32             one;
    s16             st;
    s16             phase;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    mode  = Gp_StateF0.field_4;
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
    obj->flags = 0x80;
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
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->obj16C);
    Gp_UnlinkObj(&work->obj1A4);
    Gp_UnlinkObj(&work->obj20C);
    Gp_UnlinkObj(&work->obj2A4);
    Gp_SetLightMode(arg0, 1);
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
    obj->flags      = 0x80;
    work->field_324 = 3;
    return;
dying:
    Actor02500_Fn02480(arg1);
    phase           = work->field_32E + 1;
    work->field_32E = phase;
    if (phase == 10) {
        obj->flags = 2;
    }
    if (work->field_32E == 15) {
        Gp_SpawnEff(0x600A5, coord, 2, NULL);
        Gp_SpawnEnemyFromTable(Actor02500_D05B88, 1, 0, arg0);
    }
    if (work->field_32E >= 0x3C) {
        obj->flags      = 0x80;
        work->field_324 = 2;
    }
    c      = arg1->extra.tmd->coords;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, arg1);
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
    obj->flags |= 4;
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
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02500_D00004;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor02500_Fn01E60(GpEnemy* arg0, Task* arg1)
{
    Actor02500Work* work;
    TmdObject*      temp_a1;
    GpCoord*        temp_s2;
    s32             state;
    s32             one;

    temp_a1 = arg1->extra.tmd;
    state   = Gp_StateF0.field_4;
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
    temp_a1->flags           = 0;
    arg0->node.state.b.flags = 0;
    goto default_body;
case1:
    if (work->field_322 == 5) {
        return;
    }
    Actor02500_Fn023D8(arg1);
    goto tail;
case2:
    temp_a1->flags           = 0x80;
    arg0->node.state.b.flags = one;
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
    temp_s2->flg = 0;
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
    GpEnemy*        ctx;

    ctx   = actor->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = actor->work;
    if (flags & 2) {
        ctx->reactionFlags = (u8)(flags & 0xFD);
        work->field_322    = 4;
        work->field_324    = 0;
    }
    if (ctx->reactionFlags & 1) {
        ctx->reactionFlags = (u8)(ctx->reactionFlags & 0xFE);
        if (work->field_322 != 4) {
            work->field_322 = 3;
            work->field_324 = 0;
        }
    }
    remainingFlags = ctx->reactionFlags;
    if (remainingFlags & 0xC) {
        ctx->reactionFlags = (u8)(remainingFlags & 0xF3);
    }
}

/// State handlers of the helper task `Actor02500_Fn02574` dispatches, indexed
/// by `Task::state`: setup, per-frame tick and the countdown that
/// destroys it.
static const GpEnemyTaskFuncTable3 Actor02500_D00050 = {
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
    GpCoord*        coord;

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
            func_800B4114(&work->anim, i, work->field_31C, 0, value);
            i++;
        } while (i < 5);
        return;
    }
    j = 1;
    work->field_320++;
    do {
        Gp_AnimTickIndex(&work->anim, j);
        j++;
    } while (j < 5);
}

static void Actor02500_Fn023D8(Task* arg0)
{
    VECTOR   vec;
    GpCoord* coord;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

static void Actor02500_Fn02430(Task* arg0)
{
    VECTOR3  vec;
    GpCoord* coord;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x200, 0x80);
}

static void Actor02500_Fn02480(Task* arg0)
{
    GpCoord*           coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    Actor02500Work*    work;

    head                            = SCRATCH_HEAD(ActorScaleScratch);
    work                            = arg0->work;
    scratch                         = head - 1;
    SCRATCH_HEAD(ActorScaleScratch) = scratch;
    coord                           = arg0->extra.tmd->coords;
    if (work->field_332 >= 0x201) {
        work->field_332 = (u16)work->field_332 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_332;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_2E4;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg = 0;
    SCRATCH_POP(ActorScaleScratch);
}

void Actor02500_Fn02574(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02500_D00050;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor02500_Fn025D0(GpEnemy* ctx, Task* task)
{
    Actor02500EffWork* work;
    GpCoord*           coord;
    GpRec18*           rec;
    GpCoord*           parentCoord;
    void*              effect;

    coord       = task->extra.tmd->coords;
    parentCoord = task->parent->extra.tmd->coords;
    work        = memCalloc(0x40, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, task);
        return;
    }
    task->work         = (Actor02500Work*)work;
    coord->sub         = &gGfxViewCoord;
    coord->coord       = parentCoord->coord;
    coord->coord.t[0]  = parentCoord->coord.t[0];
    coord->coord.t[1]  = parentCoord->coord.t[1];
    coord->coord.t[2]  = parentCoord->coord.t[2];
    coord->flg         = 0;
    effect             = Gp_SpawnEff(0x80060046, coord, 0x10280, NULL);
    work->obj.coord    = coord;
    rec                = work->rec18;
    work->field_38     = effect;
    work->obj.ctx.recs = rec;
    work->obj.pos.vx   = 0;
    work->obj.pos.vy   = 0;
    work->obj.pos.vz   = 0;
    work->obj.key      = Gp_PackPair(Actor02500_D05B30, 1);
    work->obj.radius   = 0xC8;
    work->obj.flags    = 1U;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(rec, 1, 0);
    work->obj.flags = (u16)(work->obj.flags | 0x8000);
    Task_DetachFromParent(task);
    task->state = 1;
}

static void Actor02500_Fn02750(GpEnemy* ctx, Task* task)
{
    s32                sound;
    GpCoord*           coord;
    GpRec18*           rec;
    s32                done;
    s32                pan;
    u16                timer;
    Actor02500EffWork* work;

    coord = task->extra.tmd->coords;
    work  = (Actor02500EffWork*)((Actor02500Work*)task->work);
    done  = 0;
    if (Gp_StateF0.field_4 == 0) {
        rec = work->rec18;
        if (Gp_CountRec18Hi(rec, 0x10000) != 0) {
            done  = 1;
            sound = (((u16)ctx->placeKey >> 0xC) << 8) | 0x40190007;
            pan   = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
        }
        Gp_ClearRec18Occupied(rec);
        timer          = work->field_3C + 1;
        work->field_3C = timer;
        if ((s16)timer >= 0xF1) {
            done = 1;
        }
        if (Gp_StateF0.field_6 == 0) {
            done = 1;
        }
        if (done != 0) {
            work->field_3E = 0;
            task->state    = 2;
        }
    }
}

static void Actor02500_Fn02874(GpEnemy* ctx, Task* task)
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
            Gp_DestroyEnemy(ctx, task);
            break;
    }
}
