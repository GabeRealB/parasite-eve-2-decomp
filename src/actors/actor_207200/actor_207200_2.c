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
#include "gameplay/pairsrc.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gamemain.h"
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

extern GpPairSrcE    D_actor_207200_8014E7D4;
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
    /* 0x1DC */ GpObj                 obj1;
    /* 0x1FC */ WorldCollisionContact rec1[1];
    /* 0x214 */ GpObj                 obj2;
    /* 0x234 */ WorldCollisionContact rec2[6];
    /* 0x2C4 */ GpObj                 obj3;
    /* 0x2E4 */ WorldCollisionContact rec3[6];
    /* 0x374 */ GpObj                 obj4;
    /* 0x394 */ WorldCollisionContact rec4[1];
    /* 0x3AC */ GpObj                 obj5;
    /* 0x3CC */ WorldCollisionContact rec5[1];
    /* 0x3E4 */ GpEffArg              eff0;
    /* 0x3EC */ GpEffArg              eff1;
    /* 0x3F4 */ GpEffArg              eff2;
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

/// 0x48-byte block `func_actor_207200_8014BEF4` takes from `G_SCRATCH_HEAD`:
/// `d` receives the `func_800E0C10` push-back, then the offset to the player
/// or to a push record, which `norm` holds normalised.
typedef struct Actor207200DmgScratch {
    /* 0x00 */ byte pad_0[0x20];
    /* 0x20 */ union {
        GpDeltaScratch delta;
        VECTOR         vec;
    } d;
    /* 0x30 */ byte   pad_30[8];
    /* 0x38 */ VECTOR norm;
} Actor207200DmgScratch;
STATIC_ASSERT_SIZEOF(Actor207200DmgScratch, 0x48);

/// The records closing three of the overlay's model streams, handed to the
/// spawned effect as its model through `D_800626EC[5].arg.model`.
extern TmdSource D_actor_207200_80150BCC;
extern TmdSource D_actor_207200_80151074;
extern TmdSource D_actor_207200_801517F8;
extern SVECTOR   D_actor_207200_80153F18;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_207200_8014B278(GpEnemy* arg0, Task* arg1);
static void func_actor_207200_8014C870(Task* arg0, s32 arg1);
static s32  func_actor_207200_8014CE20(GfxCoord* arg0, u32* arg1);
static void func_actor_207200_8014CA84(GpEnemy* arg0, Task* arg1);
static void func_actor_207200_8014D2DC(GpEnemy* arg0, Task* arg1);
static void func_actor_207200_8014CFEC(Task* arg0);
static void func_actor_207200_8014D128(Task* arg0);
static void func_actor_207200_8014D41C(Task* arg0);
static void func_actor_207200_8014D49C(Task* arg0);
static void func_actor_207200_8014D5C4(Task* arg0);
static void func_actor_207200_8014D65C(Task* arg0);
static void func_actor_207200_8014D70C(GpEnemy* arg0, Task* task);
static void func_actor_207200_8014D77C(Task* task);
static void func_actor_207200_8014D7E8(Task* arg0);
static void func_actor_207200_8014D8DC(Task* arg0);
static void func_actor_207200_8014D97C(Task* arg0, GfxCoord* arg1);
static void func_actor_207200_8014DAF8(Task* dst, Task* src);
static void func_actor_207200_8014DB4C(Task* arg0);

/// The large enemy's state handlers - spawn, live tick and teardown tick -
/// which `func_actor_207200_8014D280` dispatches through by task state.
static const GpEnemyTaskFuncTable3 D_actor_207200_80149E30 = {
    { func_actor_207200_8014B278, func_actor_207200_8014D2DC, func_actor_207200_8014CA84 }
};

void func_actor_207200_8014D280(Task*);

GpPairSrcE D_actor_207200_8014E7D4 = { D_actor_207200_8014E7CC, 250, 15, 48, 1, 50, 10, 0, 0, 0 };

TmdBone D_actor_207200_8014E7E4[7] = {
#include "assets/actor_207200_model_06BE4_skeleton.inc"
};

u32 D_actor_207200_8014E8E0[7] = {
#include "assets/actor_207200_model_06BE4_partVerts.inc"
};

SVECTOR D_actor_207200_8014E8FC[125] = {
#include "assets/actor_207200_model_06BE4_verts.inc"
};

SVECTOR D_actor_207200_8014ECE4[136] = {
#include "assets/actor_207200_model_06BE4_normals.inc"
};

u32 D_actor_207200_8014F124[1592] = {
#include "assets/actor_207200_model_06BE4_stream.inc"
};

TmdSource D_actor_207200_80150A04 = {
    0,
    8356,
    2504,
    7,
    D_actor_207200_8014E8E0,
    D_actor_207200_8014E8FC,
    D_actor_207200_8014ECE4,
    D_actor_207200_8014E7E4,
    D_actor_207200_8014F124,
};

TmdBone D_actor_207200_80150A28[1] = {
#include "assets/actor_207200_model_06DAC_skeleton.inc"
};

u32 D_actor_207200_80150A4C[1] = {
#include "assets/actor_207200_model_06DAC_partVerts.inc"
};

SVECTOR D_actor_207200_80150A50[8] = {
#include "assets/actor_207200_model_06DAC_verts.inc"
};

SVECTOR D_actor_207200_80150A90[9] = {
#include "assets/actor_207200_model_06DAC_normals.inc"
};

u32 D_actor_207200_80150AD8[61] = {
#include "assets/actor_207200_model_06DAC_stream.inc"
};

TmdSource D_actor_207200_80150BCC = {
    0,
    368,
    0,
    1,
    D_actor_207200_80150A4C,
    D_actor_207200_80150A50,
    D_actor_207200_80150A90,
    D_actor_207200_80150A28,
    D_actor_207200_80150AD8,
};

TmdBone D_actor_207200_80150BF0[1] = {
#include "assets/actor_207200_model_07254_skeleton.inc"
};

u32 D_actor_207200_80150C14[1] = {
#include "assets/actor_207200_model_07254_partVerts.inc"
};

SVECTOR D_actor_207200_80150C18[20] = {
#include "assets/actor_207200_model_07254_verts.inc"
};

SVECTOR D_actor_207200_80150CB8[22] = {
#include "assets/actor_207200_model_07254_normals.inc"
};

u32 D_actor_207200_80150D68[195] = {
#include "assets/actor_207200_model_07254_stream.inc"
};

TmdSource D_actor_207200_80151074 = {
    0,
    1272,
    0,
    1,
    D_actor_207200_80150C14,
    D_actor_207200_80150C18,
    D_actor_207200_80150CB8,
    D_actor_207200_80150BF0,
    D_actor_207200_80150D68,
};

TmdBone D_actor_207200_80151098[1] = {
#include "assets/actor_207200_model_079D8_skeleton.inc"
};

u32 D_actor_207200_801510BC[1] = {
#include "assets/actor_207200_model_079D8_partVerts.inc"
};

SVECTOR D_actor_207200_801510C0[33] = {
#include "assets/actor_207200_model_079D8_verts.inc"
};

SVECTOR D_actor_207200_801511C8[40] = {
#include "assets/actor_207200_model_079D8_normals.inc"
};

u32 D_actor_207200_80151308[316] = {
#include "assets/actor_207200_model_079D8_stream.inc"
};

TmdSource D_actor_207200_801517F8 = {
    0,
    2116,
    0,
    1,
    D_actor_207200_801510BC,
    D_actor_207200_801510C0,
    D_actor_207200_801511C8,
    D_actor_207200_80151098,
    D_actor_207200_80151308,
};

AnimationPackedPose D_actor_207200_8015181C[8] = {
#include "assets/actor_207200_animation_07BF8_bank1.inc"
};

AnimationPackedRotation D_actor_207200_8015187C[27] = {
#include "assets/actor_207200_animation_07BF8_bank4.inc"
};

AnimationRecord D_actor_207200_801518E8[72] = {
#include "assets/actor_207200_animation_07BF8_records.inc"
};

u16 D_actor_207200_80151A08[8] = {
#include "assets/actor_207200_animation_07BF8_indices.inc"
};

AnimationSet D_actor_207200_80151A18 = {
    D_actor_207200_801518E8,
    D_actor_207200_80151A08,
    { NULL, D_actor_207200_8015181C, NULL, NULL, D_actor_207200_8015187C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_80151A40[16] = {
#include "assets/actor_207200_animation_07F34_bank1.inc"
};

AnimationPackedRotation D_actor_207200_80151B00[55] = {
#include "assets/actor_207200_animation_07F34_bank4.inc"
};

AnimationRecord D_actor_207200_80151BDC[90] = {
#include "assets/actor_207200_animation_07F34_records.inc"
};

u16 D_actor_207200_80151D44[8] = {
#include "assets/actor_207200_animation_07F34_indices.inc"
};

AnimationSet D_actor_207200_80151D54 = {
    D_actor_207200_80151BDC,
    D_actor_207200_80151D44,
    { NULL, D_actor_207200_80151A40, NULL, NULL, D_actor_207200_80151B00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_80151D7C[14] = {
#include "assets/actor_207200_animation_08248_bank1.inc"
};

AnimationPackedRotation D_actor_207200_80151E24[55] = {
#include "assets/actor_207200_animation_08248_bank4.inc"
};

AnimationRecord D_actor_207200_80151F00[86] = {
#include "assets/actor_207200_animation_08248_records.inc"
};

u16 D_actor_207200_80152058[8] = {
#include "assets/actor_207200_animation_08248_indices.inc"
};

AnimationSet D_actor_207200_80152068 = {
    D_actor_207200_80151F00,
    D_actor_207200_80152058,
    { NULL, D_actor_207200_80151D7C, NULL, NULL, D_actor_207200_80151E24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_80152090[14] = {
#include "assets/actor_207200_animation_08540_bank1.inc"
};

AnimationPackedRotation D_actor_207200_80152138[51] = {
#include "assets/actor_207200_animation_08540_bank4.inc"
};

AnimationRecord D_actor_207200_80152204[83] = {
#include "assets/actor_207200_animation_08540_records.inc"
};

u16 D_actor_207200_80152350[8] = {
#include "assets/actor_207200_animation_08540_indices.inc"
};

AnimationSet D_actor_207200_80152360 = {
    D_actor_207200_80152204,
    D_actor_207200_80152350,
    { NULL, D_actor_207200_80152090, NULL, NULL, D_actor_207200_80152138, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_80152388[31] = {
#include "assets/actor_207200_animation_08B98_bank1.inc"
};

AnimationPackedRotation D_actor_207200_801524FC[121] = {
#include "assets/actor_207200_animation_08B98_bank4.inc"
};

AnimationRecord D_actor_207200_801526E0[178] = {
#include "assets/actor_207200_animation_08B98_records.inc"
};

u16 D_actor_207200_801529A8[8] = {
#include "assets/actor_207200_animation_08B98_indices.inc"
};

AnimationSet D_actor_207200_801529B8 = {
    D_actor_207200_801526E0,
    D_actor_207200_801529A8,
    { NULL, D_actor_207200_80152388, NULL, NULL, D_actor_207200_801524FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_801529E0[21] = {
#include "assets/actor_207200_animation_09018_bank1.inc"
};

AnimationPackedRotation D_actor_207200_80152ADC[90] = {
#include "assets/actor_207200_animation_09018_bank4.inc"
};

AnimationRecord D_actor_207200_80152C44[121] = {
#include "assets/actor_207200_animation_09018_records.inc"
};

u16 D_actor_207200_80152E28[8] = {
#include "assets/actor_207200_animation_09018_indices.inc"
};

AnimationSet D_actor_207200_80152E38 = {
    D_actor_207200_80152C44,
    D_actor_207200_80152E28,
    { NULL, D_actor_207200_801529E0, NULL, NULL, D_actor_207200_80152ADC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_80152E60[19] = {
#include "assets/actor_207200_animation_093EC_bank1.inc"
};

AnimationPackedRotation D_actor_207200_80152F44[72] = {
#include "assets/actor_207200_animation_093EC_bank4.inc"
};

AnimationRecord D_actor_207200_80153064[102] = {
#include "assets/actor_207200_animation_093EC_records.inc"
};

u16 D_actor_207200_801531FC[8] = {
#include "assets/actor_207200_animation_093EC_indices.inc"
};

AnimationSet D_actor_207200_8015320C = {
    D_actor_207200_80153064,
    D_actor_207200_801531FC,
    { NULL, D_actor_207200_80152E60, NULL, NULL, D_actor_207200_80152F44, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_80153234[14] = {
#include "assets/actor_207200_animation_09704_bank1.inc"
};

AnimationPackedRotation D_actor_207200_801532DC[59] = {
#include "assets/actor_207200_animation_09704_bank4.inc"
};

AnimationRecord D_actor_207200_801533C8[83] = {
#include "assets/actor_207200_animation_09704_records.inc"
};

u16 D_actor_207200_80153514[8] = {
#include "assets/actor_207200_animation_09704_indices.inc"
};

AnimationSet D_actor_207200_80153524 = {
    D_actor_207200_801533C8,
    D_actor_207200_80153514,
    { NULL, D_actor_207200_80153234, NULL, NULL, D_actor_207200_801532DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_8015354C[14] = {
#include "assets/actor_207200_animation_099F4_bank1.inc"
};

AnimationPackedRotation D_actor_207200_801535F4[54] = {
#include "assets/actor_207200_animation_099F4_bank4.inc"
};

AnimationRecord D_actor_207200_801536CC[78] = {
#include "assets/actor_207200_animation_099F4_records.inc"
};

u16 D_actor_207200_80153804[8] = {
#include "assets/actor_207200_animation_099F4_indices.inc"
};

AnimationSet D_actor_207200_80153814 = {
    D_actor_207200_801536CC,
    D_actor_207200_80153804,
    { NULL, D_actor_207200_8015354C, NULL, NULL, D_actor_207200_801535F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_8015383C[13] = {
#include "assets/actor_207200_animation_09D08_bank1.inc"
};

AnimationPackedRotation D_actor_207200_801538D8[59] = {
#include "assets/actor_207200_animation_09D08_bank4.inc"
};

AnimationRecord D_actor_207200_801539C4[85] = {
#include "assets/actor_207200_animation_09D08_records.inc"
};

u16 D_actor_207200_80153B18[8] = {
#include "assets/actor_207200_animation_09D08_indices.inc"
};

AnimationSet D_actor_207200_80153B28 = {
    D_actor_207200_801539C4,
    D_actor_207200_80153B18,
    { NULL, D_actor_207200_8015383C, NULL, NULL, D_actor_207200_801538D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_80153B50[15] = {
#include "assets/actor_207200_animation_09FE4_bank1.inc"
};

AnimationPackedRotation D_actor_207200_80153C04[48] = {
#include "assets/actor_207200_animation_09FE4_bank4.inc"
};

AnimationRecord D_actor_207200_80153CC4[76] = {
#include "assets/actor_207200_animation_09FE4_records.inc"
};

u16 D_actor_207200_80153DF4[8] = {
#include "assets/actor_207200_animation_09FE4_indices.inc"
};

AnimationSet D_actor_207200_80153E04 = {
    D_actor_207200_80153CC4,
    D_actor_207200_80153DF4,
    { NULL, D_actor_207200_80153B50, NULL, NULL, D_actor_207200_80153C04, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_80153E2C[2] = {
#include "assets/actor_207200_animation_0A080_bank1.inc"
};

AnimationPackedRotation D_actor_207200_80153E44[5] = {
#include "assets/actor_207200_animation_0A080_bank4.inc"
};

AnimationRecord D_actor_207200_80153E58[14] = {
#include "assets/actor_207200_animation_0A080_records.inc"
};

u16 D_actor_207200_80153E90[8] = {
#include "assets/actor_207200_animation_0A080_indices.inc"
};

AnimationSet D_actor_207200_80153EA0 = {
    D_actor_207200_80153E58,
    D_actor_207200_80153E90,
    { NULL, D_actor_207200_80153E2C, NULL, NULL, D_actor_207200_80153E44, NULL, NULL, NULL },
};

TaskDesc D_actor_207200_80153EC8 = { 1, 96, func_actor_207200_8014D280, { .model = &D_actor_207200_80150A04 } };

AnimationSet* D_actor_207200_80153ED4[13] = {
    NULL,
    &D_actor_207200_80151A18,
    &D_actor_207200_80151D54,
    &D_actor_207200_80152068,
    &D_actor_207200_80152360,
    &D_actor_207200_801529B8,
    &D_actor_207200_80152E38,
    &D_actor_207200_8015320C,
    &D_actor_207200_80153524,
    &D_actor_207200_80153814,
    &D_actor_207200_80153B28,
    &D_actor_207200_80153E04,
    &D_actor_207200_80153EA0,
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
static __inline__ void Actor207200_UpdateColor(GpEnemy* enemy, Task* actor);

static void func_actor_207200_8014B278(GpEnemy* arg0, Task* arg1)
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
        Gp_DestroyEnemy(arg0, arg1);
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
    arg0->coord              = coord;
    arg0->node.state.b.flags = 0;
    arg0->bodyPos.vx         = 0;
    arg0->bodyPos.vy         = 0;
    arg0->bodyPos.vz         = 0;
    arg0->param              = &D_actor_207200_8014E7D4;
    arg0->recs               = work->rec3;
    arg0->hp                 = (u16)D_actor_207200_8014E7D4.hpMax;
    work->field_44C.vy       = (coord)->param.rot.vy;
    func_800B3F84((GpAnimCtx*)work, D_actor_207200_80153ED4, obj,
                  work->field_12C, (AnimationSlot*)work->field_14);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    (Gp_IncStateF0Ref)(0);

    work->field_48C = 1;
    work->field_48E = 1;
    work->field_4A4 = 0;
    work->field_4A6 = 0;
    work->field_488 = 0;
    work->field_494 = 0;
    work->field_49E = 0;

    work->obj1.coord    = coord;
    work->obj1.ctx.recs = work->rec1;
    work->obj1.pos.vx   = 0;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0;
    work->obj1.key      = 0;
    work->obj1.radius   = 0x7D0;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, ARRAY_SIZE(work->rec1), 0);

    work->obj2.pos.vy   = -0x12C;
    work->obj2.pos.vz   = -0xB4;
    work->obj2.coord    = coord;
    work->obj2.ctx.recs = work->rec2;
    work->obj2.pos.vx   = 0;
    work->obj2.key      = 0x3002B;
    work->obj2.radius   = 0x12C;
    work->obj2.flags    = 1;
    work->obj1.flags   |= 0x8000;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->rec2, ARRAY_SIZE(work->rec2), 0);

    work->obj3.coord    = part3;
    work->obj3.ctx.recs = work->rec3;
    work->obj3.pos.vx   = 0;
    work->obj3.pos.vy   = 0;
    work->obj3.pos.vz   = 0;
    work->obj3.key      = 0x3002B;
    work->obj3.radius   = 0x96;
    work->obj3.flags    = 1;
    work->obj2.flags   |= 0xC200;
    Gp_LinkObj(2, &work->obj3);
    Gp_InitRec18Table(work->rec3, ARRAY_SIZE(work->rec3), 0);

    work->obj4.coord    = part3;
    work->obj4.ctx.recs = work->rec4;
    work->obj4.pos.vx   = 0;
    work->obj4.pos.vy   = 0x50;
    work->obj4.pos.vz   = 0x8C;
    work->obj3.flags   |= 0xC000;
    work->obj4.key      = Gp_PackPair(D_actor_207200_8014E7CC, 0);
    work->obj4.radius   = 0x12C;
    work->obj4.flags    = 1;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec4, ARRAY_SIZE(work->rec4), 0);

    work->obj5.coord    = part6;
    work->obj5.ctx.recs = work->rec5;
    work->obj5.pos.vx   = 0xFA;
    work->obj5.pos.vy   = 0;
    work->obj5.pos.vz   = 0;
    work->obj4.flags   &= 0x7FFF;
    work->obj5.key      = Gp_PackPair(D_actor_207200_8014E7CC, 1);
    work->obj5.radius   = 0x12C;
    work->obj5.flags    = 1;
    Gp_LinkObj(3, &work->obj5);
    Gp_InitRec18Table(work->rec5, ARRAY_SIZE(work->rec5), 0);
    work->obj5.flags &= 0x7FFF;

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
/// the first render node's table (or the global flag `Gp_StateF0.prefix.bytes.field_3`) arms the
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

    SCRATCH_PUSH_BYTES(8);
    work = (_Actor207200LargeWork*)arg0->work;
    obj  = arg0->extra.tmd->coords;
    if (work->field_4A6 == 0) {
        if (Gp_CountRec18Hi(work->rec1, 0x10000) != 0) {
            work->field_4A2 = 1;
        }
        if (work->field_4A2 != 0 || Gp_StateF0.prefix.bytes.field_3 != 0) {
            rnd               = Gp_LcgState * 5 + 0x71357911;
            hi                = rnd >> 16;
            work->field_49A   = 0;
            work->field_492   = 0;
            work->field_486   = 1;
            Gp_LcgState       = rnd;
            work->obj1.flags &= 0x7FFF;
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
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((u16)((Gp_LcgState >> 16) % 100) < 30) {
                        work->field_48C = 9;
                    }
                }
            }
            break;
        case 9:
            if ((s16)work->field_490 == 5) {
                id  = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40480004;
                pan = (s8)Gp_GetObjPan(obj);
                SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(obj));
            }
            if ((s16)work->field_490 >= 0x2D) {
                work->field_48C = 1;
                work->field_490 = 0;
            }
            break;
    }
    SCRATCH_POP_BYTES(8);
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
                work->obj3.flags &= 0xBFFF;
                break;
            }
            work->field_48C   = 2;
            work->field_498   = 0;
            work->obj3.flags |= 0x4000;
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
                id              = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40480002;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_490 == 42) {
                work->obj4.flags |= 0x8000;
            }
            if ((s16)work->field_490 == 45) {
                work->obj4.flags &= 0x7FFF;
            }
            if (work->field_4A0 != 0 && (s16)work->field_490 == 45) {
                id = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40480005;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (work->field_48C == 8 && (s16)work->field_490 >= 60) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((u16)((Gp_LcgState >> 16) % 100) < 40) {
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
                work->obj5.flags |= 0x8000;
            }
            if ((s16)work->field_490 == 60) {
                work->obj5.flags &= 0x7FFF;
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
                id = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40480006;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
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
    GpEnemy*               enemy;
    s32                    i;
    s32                    angle;
    s32                    damage;
    s32                    push;
    s32                    param;
    s32                    n;
    s32                    snd;

    work                                = (_Actor207200LargeWork*)arg0->work;
    head                                = SCRATCH_HEAD(Actor207200DmgScratch);
    SCRATCH_HEAD(Actor207200DmgScratch) = head - 1;
    sc                                  = head - 1;
    coord                               = arg0->extra.tmd->coords;
    enemy                               = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->rec3, &head[-1].d.delta, 6, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->d.delta.vx.h.hi;
            coord->coord.t[1] += sc->d.delta.vy.h.hi;
            coord->coord.t[2] += sc->d.delta.vz.h.hi;
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
    switch (func_800E0C10(work->rec2, &sc->d.delta, 6, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->d.delta.vx.h.hi;
            coord->coord.t[1] += sc->d.delta.vy.h.hi;
            coord->coord.t[2] += sc->d.delta.vz.h.hi;
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
                sc->d.delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->d.delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                sc->d.delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                damage           = SquareRoot0(sc->d.delta.vx.w * sc->d.delta.vx.w +
                                               sc->d.delta.vy.w * sc->d.delta.vy.w +
                                               sc->d.delta.vz.w * sc->d.delta.vz.w);
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
                        work->field_48C                     = 0xB;
                        SCRATCH_HEAD(Actor207200DmgScratch) = SCRATCH_HEAD(Actor207200DmgScratch) + 1;
                        return;
                    }
                } else {
                    func_800DA6E8(&enemy->node, 0, 0);
                }
                if (work->field_486 == 0) {
                    snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40480006;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    work->field_48C = 5;
                    work->field_490 = 0;
                    work->field_486 = 4;
                    work->field_492 = 0;
                    work->field_4A2 = 0;
                }
                break;
            case 3:
                sc->d.delta.vx.w = coord->workm.t[0] - work->rec2[i].point.vx;
                sc->d.delta.vy.w = 0;
                sc->d.delta.vz.w = coord->workm.t[2] - work->rec2[i].point.vz;
                damage           = work->rec2[i].distance -
                         SquareRoot0(sc->d.delta.vx.w * sc->d.delta.vx.w + sc->d.delta.vz.w * sc->d.delta.vz.w);
                // Clamped through a second variable: clamping `damage` in
                // place drops the copy the original makes.
                push = damage;
                if (damage <= 0) {
                    push = 0;
                }
                damage           = push;
                sc->d.delta.vx.w = coord->workm.t[0] - work->rec2[i].point.vx;
                sc->d.delta.vy.w = coord->workm.t[1] - work->rec2[i].point.vy;
                sc->d.delta.vz.w = coord->workm.t[2] - work->rec2[i].point.vz;
                VectorNormal(&sc->d.vec, &sc->norm);
                ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &sc->norm, &sc->d.vec);
                if (work->field_48C == 2) {
                    coord->coord.t[0] += (damage * sc->d.vec.vx) >> 12;
                    n                  = damage * sc->d.vec.vy;
                    if (n < 0) {
                        coord->coord.t[1] += n >> 12;
                    }
                    coord->coord.t[2] += (damage * sc->d.vec.vz) >> 12;
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
            sc->d.delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->d.delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
            sc->d.delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            damage           = SquareRoot0(sc->d.delta.vx.w * sc->d.delta.vx.w + sc->d.delta.vy.w * sc->d.delta.vy.w +
                                           sc->d.delta.vz.w * sc->d.delta.vz.w);
            param            = Gp_GetIdParam0(work->rec3[i].key.value);
            damage           = Gp_ComputeDamage(work->rec3[i].key.value, damage, 0, 0);
            switch ((u16)param) {
                case 1:
                case 4:
                case 5:
                case 6:
                    Gp_SpawnEff(0x6009C, arg0->extra.tmd->coords, 2, NULL);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    func_800DA6E8(&enemy->node, D_actor_207200_8014E7D4.hpMax * 2 + (u16)((Gp_LcgState >> 16) % 100), 0);
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
        work->obj4.flags &= 0x7FFF;
        work->obj5.flags &= 0x7FFF;
    }
    Gp_ClearRec18Occupied(work->rec2);
    Gp_ClearRec18Occupied(work->rec3);
    if (work->field_49A != 0) {
        if (Gp_FindRec18(work->rec4, 0) != 0) {
            work->field_4A0   = 1;
            work->obj4.flags &= 0x7FFF;
            Gp_ClearRec18Occupied(work->rec4);
        }
        if (Gp_FindRec18(work->rec5, 0) != 0) {
            work->obj5.flags &= 0x7FFF;
            Gp_ClearRec18Occupied(work->rec5);
        }
    }
    SCRATCH_HEAD(Actor207200DmgScratch) = SCRATCH_HEAD(Actor207200DmgScratch) + 1;
}

/// Ticks the shatter timers the enemy runs while it dies. Every time a timer
/// runs out the work is armed with a fresh sound effect - one per stage of the
/// death animation - and the frame it is handed plays.
static void func_actor_207200_8014C870(Task* arg0, s32 arg1)
{
    _Actor207200LargeWork* work;
    GpEnemy*               ctx;
    GfxCoord*              coord;
    GpEffArg*              effArg;
    s32                    snd;

    work  = arg0->work;
    ctx   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;

    ctx->hp = (s16)((u16)ctx->hp - arg1);
    func_800DA6E8(&ctx->node, arg1, 0);
    if ((s16)ctx->hp <= 0) {
        if (work->field_4A6 == 0) {
            ctx->hp = 1;
            snd     = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40480003;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            effArg = &work->eff2;
            func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
            func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
            work->obj4.flags &= 0x7FFF;
            work->obj5.flags &= 0x7FFF;
            Gp_UnlinkObj(&work->obj3);
            work->field_4A6     = 1;
            ctx->recs           = work->rec2;
            work->field_488     = 0;
            arg0->killCountdown = 0x14;
        }
    } else {
        if (work->field_48C == 1) {
            snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40480006;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            work->field_48C = 5;
            work->field_490 = 0;
            work->field_486 = 4;
            work->field_492 = 0;
            work->field_4A2 = 0;
            return;
        }
        snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40480001;
        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
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
            func_800B4114((GpAnimCtx*)work, i, work->field_48C, 0, 8);
        }
    } else {
        work->field_490++;
        for (i = 1; i < 7; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
}

/// `func_actor_207200_8014D70C`'s body, inlined: push the model's second coordinate's
/// world position onto `G_SCRATCH_HEAD` and hand it to `Gp_UpdateActorColor`.
static __inline__ void Actor207200_UpdateColor(GpEnemy* enemy, Task* actor)
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

/// Teardown tick. Mode 2 of `Gp_StateF0.field_4` hides the model, mode 1 does nothing;
/// otherwise the teardown stage in `field_488` advances: 0 releases the actor's
/// state reference, snapshots the model transform and unlinks its node and
/// five display objects; 1 moves on once animation 5 has run 100 frames (or
/// at once for any other animation or once `field_4A8` is set); 2 counts 60
/// frames, spawning an effect on frame 15; 3 destroys the enemy. Every stage
/// but the last then ticks the animation, the attach coordinates and the colour.
static void func_actor_207200_8014CA84(GpEnemy* arg0, Task* arg1)
{
    _Actor207200LargeWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    s16                    state;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    switch (Gp_StateF0.field_4) {
        case 1:
            break;
        case 2:
            obj->flags              |= TMD_OBJECT_HIDDEN;
            arg0->node.state.b.flags = 1;
            break;
        case 0:
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
                    Gp_UnlinkNode(&arg0->node);
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
                        obj->flags      = TMD_OBJECT_HIDDEN;
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
                            Gp_SpawnEff(0x600A5, coord, 2, NULL);
                        }
                    }
                    break;
                case 3:
                    Gp_DestroyEnemy(arg0, arg1);
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

    other         = gameGetPtrSlot(3)->extra.tmd->coords;
    blk           = SCRATCH_PUSH(ActorBearingScratch);
    angle         = actorBearingInFrame(blk, arg0, other);
    blk->delta.vx = other->coord.t[0] - arg0->coord.t[0];
    blk->delta.vz = other->coord.t[2] - arg0->coord.t[2];
    *arg1         = SquareRoot0(blk->delta.vx * blk->delta.vx + blk->delta.vz * blk->delta.vz);
    SCRATCH_POP(ActorBearingScratch);
    return angle;
}

/// Spawns the pair of effects that carry this actor's death animation, hands
/// the spawned task `D_actor_207200_801517F8` as its setup argument, arms the
/// two timers on the work area and unlinks its third display object.
static void func_actor_207200_8014CFEC(Task* arg0)
{
    GpEffArg*              effArg;
    struct GpEffWork*      effect;
    _Actor207200LargeWork* work;
    GpEnemy*               ctx;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;

    Gp_SpawnEff(0x6009C, arg0->extra.tmd->coords, 0, NULL);
    func_800DA6E8(&ctx->node, ctx->hp - 1, 0);
    D_800626EC[5].arg.model = &D_actor_207200_801517F8;
    effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 3, 0, NULL);
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
    work->obj3.flags &= 0x7FFF;
    Gp_UnlinkObj(&work->obj3);
    arg0->killCountdown = 0x14;
}

static void func_actor_207200_8014D128(Task* arg0)
{
    GpEffWork* effect;
    s32        r;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    r           = (Gp_LcgState >> 16) & 3;
    switch (r) {
        case 0:
        case 1:
            D_800626EC[5].arg.model = &D_actor_207200_801517F8;
            effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 3, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
        case 2:
            D_800626EC[5].arg.model = &D_actor_207200_80151074;
            effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 5, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
        case 3:
            D_800626EC[5].arg.model = &D_actor_207200_80150BCC;
            effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 2, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
    }
    Gp_SpawnEff(0x60030, arg0->extra.tmd->coords + 3, 0x300, NULL);
    Gp_SpawnEff(0x60030, arg0->extra.tmd->coords + 2, 0x300, NULL);
}

void func_actor_207200_8014D280(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Per-frame tick of the actor's live state. `Gp_StateF0.field_4` gates it: mode 1
/// skips the update and runs only the tail, mode 2 puts the model in its
/// hidden pose (part flag 0x80, node flag 1) and returns without updating,
/// mode 0 clears both flags before falling into the update, and any other mode
/// updates directly. The update drives the model's two attach coordinates,
/// clears the display flags of the first two parts and recomputes the second
/// part's world matrix; the tail then colours the actor from that part and
/// draws its ground shadow.
static void func_actor_207200_8014D2DC(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = Gp_StateF0.field_4;
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
    arg1->extra.tmd->flags   = 0;
    arg0->node.state.b.flags = 0;
    goto default_body;
case2:
    arg1->extra.tmd->flags   = TMD_OBJECT_HIDDEN;
    arg0->node.state.b.flags = one;
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
    GpEnemy*               obj;
    _Actor207200LargeWork* work;
    u8                     flags;

    obj   = arg0->spawnArg2.pointer;
    flags = obj->reactionFlags;
    work  = arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            obj->reactionFlags = flags & 0xFE;
        }
        if (obj->reactionFlags & 2) {
            obj->reactionFlags = obj->reactionFlags & 0xFD;
            work->field_486    = 3;
            work->field_48E    = 1;
            work->field_48A    = 0;
            work->field_492    = 0;
            work->field_48C    = 9;
            work->field_490    = 0;
        }
        flags = obj->reactionFlags;
        if (flags & 0xC) {
            obj->reactionFlags = flags & 0xF3;
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
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
static void func_actor_207200_8014D70C(GpEnemy* arg0, Task* task)
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
    vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
    vec->vx = coord->workm.t[0];
    vec->vy = coord->workm.t[1];
    vec->vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(vec, 0x1C0, 0);
    SCRATCH_POP_BYTES(0x18);
}

/// Rebuilds the first coordinate node of the actor's model from the transform
/// stored in `work->field_464`, scaled along Y by `work->field_49C` (its own
/// angle field, decaying by 0x50 a frame while it sits above 0x200). The 0x30
/// bytes that hold the scaling matrix and its `VECTOR` are borrowed from the
/// scratchpad and released again; the node's `composeStamp` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
static void func_actor_207200_8014D7E8(Task* arg0)
{
    GfxCoord*              coord;
    ActorScaleScratch*     head;
    ActorScaleScratch*     scratch;
    _Actor207200LargeWork* work;

    head               = SCRATCH_HEAD(ActorScaleScratch);
    work               = arg0->work;
    scratch            = head - 1;
    SCRATCH_HEAD(void) = scratch;
    coord              = arg0->extra.tmd->coords;
    if (work->field_49C >= 0x201) {
        work->field_49C = (u16)work->field_49C - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_49C;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_464;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_POP(ActorScaleScratch);
}

/// Re-picks the model part the enemy's `coord` points at and relinks its
/// lock-on node. Once `field_4A6` is set it is always the second part;
/// before that it is the fourth part while the model in pointer slot 3 lies
/// within a quarter turn of the root's heading, and the second otherwise.
static void func_actor_207200_8014D8DC(Task* arg0)
{
    _Actor207200LargeWork* work;
    GpEnemy*               ctx;
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
    GpEnemy*               ctx;
    _Actor207200LargeWork* work;

    ctx       = arg0->spawnArg2.pointer;
    work      = arg0->work;
    ctx->recs = 0;
    Gp_UnlinkNode(&ctx->node);
    Gp_UnlinkObj(&work->obj1);
    Gp_UnlinkObj(&work->obj2);
    Gp_UnlinkObj(&work->obj3);
    Gp_UnlinkObj(&work->obj4);
    Gp_UnlinkObj(&work->obj5);
    Gp_EnemyTaskExit(arg0);
}
