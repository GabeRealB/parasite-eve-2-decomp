#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actors_shared_80135c4c.h"

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
#include "gameplay/hud_sprites.h"
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
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

extern void* D_80067704[1];

extern TmdSource     Actor05500_D05F18;
extern GpU16Pair     Actor05500_D08958[6];
extern GpPairSrcE    Actor05500_D08970;
extern u16           Actor05500_D08980[];
extern u16           Actor05500_D08990[];
extern u16           Actor05500_D089A0[];
extern s16           Actor05500_D089B0[];
extern SVECTOR       Actor05500_D089B8[];
extern s16           Actor05500_D089D8[];
extern s16           Actor05500_D089E0[];
extern s16           Actor05500_D089E8[];
extern SVECTOR       Actor05500_D089F0[];
extern s16           Actor05500_D08A10[];
extern s16           Actor05500_D08A18[];
extern s16           Actor05500_D08A1E;
extern s16           Actor05500_D08A20;
extern s16           Actor05500_D08A38[][2];
extern s16           Actor05500_D08A5C[][2];
extern ActorSpriteUv Actor05500_D08A80[];
extern s16           Actor05500_D08AA0[];
extern TaskDesc      Actor05500_D08ABC;
extern AnimationSet* Actor05500_D08AD4[15];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void Actor05500_Fn00754(Task* arg0);
static void Actor05500_Fn00914(Task* arg0);
static void Actor05500_Fn00A94(Task* arg0);
static void Actor05500_Fn00FA0(Task* arg0);
static void Actor05500_Fn012E8(Task* arg0);
static void Actor05500_Fn0143C(Task* arg0);
static void Actor05500_Fn01A0C(Task* arg0);
static void Actor05500_Fn01B30(Task* arg0);
static void Actor05500_Fn020D4(Task* arg0);
static void Actor05500_Fn02214(Task* arg0);
static void Actor05500_Fn02364(GpEnemy* arg0, Task* arg1);
static void Actor05500_Fn02780(GpEnemy* arg0, Task* arg1);
static void Actor05500_Fn02954(Task* arg0, s32 arg1);
static void Actor05500_Fn02C94(Task* arg0);
static void Actor05500_Fn02FFC(GpEnemy* ctx, Task* actor);
static void Actor05500_Fn03560(GpEnemy* arg0, Task* arg1);
static void Actor05500_Fn03674(Task* arg0, TmdObject* arg1, s32 arg2);
static void Actor05500_Fn0378C(Task* arg0);
static void Actor05500_Fn03864(Task* arg0);
static void Actor05500_Fn03918(Task* arg0);
static void Actor05500_Fn039AC(Task* arg0);
static void Actor05500_Fn03A70(Task* arg0);
static void Actor05500_Fn03AC8(Task* arg0);
static void Actor05500_Fn03B60(Task* arg0);
static void Actor05500_Fn03C54(Task* arg0);
static void Actor05500_Fn03D40(Task* arg0);
static void Actor05500_Fn03E34(GpEnemy* enemy, Task* task);

/// Stores the yaw that `coord`'s frame faces in `work->field_3A2`, then
/// rebuilds the frame's rotation as a level turn half a revolution away from
/// it, with `rot` holding the angles.
#define ACTOR05500_TURN_AROUND(work, coord, rot)                                            \
    do {                                                                                    \
        (work)->field_3A2 = ratan2((coord)->coord.m[0][2], (coord)->coord.m[2][2]) & 0xFFF; \
        (rot)->vx         = 0;                                                              \
        (rot)->vy         = (u16)(work)->field_3A2 + 0x800;                                 \
        (rot)->vz         = 0;                                                              \
        RotMatrix((rot), &(coord)->coord);                                                  \
    } while (0)

/// How far the origin of `coord`'s frame lies inside contact `rec`, clamped at
/// zero, into `out`. `delta` receives the offset from the contact point to
/// the origin.
#define ACTOR05500_CONTACT_OVERLAP(out, coord, rec, delta)                                    \
    do {                                                                                      \
        s32 offX;                                                                             \
        s32 offY;                                                                             \
        s32 offZ;                                                                             \
        s32 clamped;                                                                          \
        offX         = (coord)->workm.t[0] - (rec).point.vx;                                  \
        (delta).vx.w = offX;                                                                  \
        offY         = (coord)->workm.t[1] - (rec).point.vy;                                  \
        (delta).vy.w = offY;                                                                  \
        offZ         = (coord)->workm.t[2] - (rec).point.vz;                                  \
        (delta).vz.w = offZ;                                                                  \
        (out)        = (rec).distance - SquareRoot0(offX * offX + offY * offY + offZ * offZ); \
        clamped      = (out);                                                                 \
        if ((out) <= 0) {                                                                     \
            clamped = 0;                                                                      \
        }                                                                                     \
        (out) = clamped;                                                                      \
    } while (0)

/// Normalises `delta` into `unit` and expresses the direction in the frame of
/// the collision grid, into `out`.
#define ACTOR05500_GRID_DIRECTION(delta, unit, out)                            \
    do {                                                                       \
        VectorNormal((VECTOR*)(delta), (unit));                                \
        ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (unit), (out)); \
    } while (0)

/// Sets `work->field_3CE` when contact `rec` is a body, or a face of the
/// collision grid whose normal has no vertical component.
#define ACTOR05500_NOTE_BLOCKING_CONTACT(work, rec)                                              \
    do {                                                                                         \
        if ((((rec).key.value & 0xFFFF0000) == 0x10000) ||                                       \
            ((((rec).key.value & 0xFFFF0000) == 0x100000) && ((rec).response.normal.vy == 0))) { \
            (work)->field_3CE = 1;                                                               \
        }                                                                                        \
    } while (0)

extern AnimationSet Actor05500_D061A0;
extern AnimationSet Actor05500_D06454;
extern AnimationSet Actor05500_D068D8;
extern AnimationSet Actor05500_D06E1C;
extern AnimationSet Actor05500_D074E4;
extern AnimationSet Actor05500_D077AC;
extern AnimationSet Actor05500_D0793C;
extern AnimationSet Actor05500_D07B50;
extern AnimationSet Actor05500_D07E90;
extern AnimationSet Actor05500_D08148;
extern AnimationSet Actor05500_D083F4;
extern AnimationSet Actor05500_D08780;
extern AnimationSet Actor05500_D08930;
extern TmdSource    Actor05500_D05774;
void                Actor05500_Fn03DD8(Task*);
void                Actor05500_Fn03F88(Task*);

TmdBone Actor05500_D03FE4[8] = {
#include "assets/actor_105500_model_05774_skeleton.inc"
};

u32 Actor05500_D04104[8] = {
#include "assets/actor_105500_model_05774_partVerts.inc"
};

SVECTOR Actor05500_D04124[101] = {
#include "assets/actor_105500_model_05774_verts.inc"
};

SVECTOR Actor05500_D0444C[107] = {
#include "assets/actor_105500_model_05774_normals.inc"
};

u32 Actor05500_D047A4[1012] = {
#include "assets/actor_105500_model_05774_stream.inc"
};

TmdSource Actor05500_D05774 = {
    0,
    5400,
    1872,
    8,
    Actor05500_D04104,
    Actor05500_D04124,
    Actor05500_D0444C,
    Actor05500_D03FE4,
    Actor05500_D047A4,
};

TmdBone Actor05500_D05798[1] = {
#include "assets/actor_105500_model_05F18_skeleton.inc"
};

u32 Actor05500_D057BC[1] = {
#include "assets/actor_105500_model_05F18_partVerts.inc"
};

SVECTOR Actor05500_D057C0[40] = {
#include "assets/actor_105500_model_05F18_verts.inc"
};

SVECTOR Actor05500_D05900[40] = {
#include "assets/actor_105500_model_05F18_normals.inc"
};

u32 Actor05500_D05A40[310] = {
#include "assets/actor_105500_model_05F18_stream.inc"
};

TmdSource Actor05500_D05F18 = {
    0,
    2144,
    0,
    1,
    Actor05500_D057BC,
    Actor05500_D057C0,
    Actor05500_D05900,
    Actor05500_D05798,
    Actor05500_D05A40,
};

AnimationPackedPose Actor05500_D05F3C[10] = {
#include "assets/actor_105500_animation_061A0_bank1.inc"
};

AnimationPackedRotation Actor05500_D05FB4[45] = {
#include "assets/actor_105500_animation_061A0_bank4.inc"
};

AnimationRecord Actor05500_D06068[74] = {
#include "assets/actor_105500_animation_061A0_records.inc"
};

u16 Actor05500_D06190[8] = {
#include "assets/actor_105500_animation_061A0_indices.inc"
};

AnimationSet Actor05500_D061A0 = {
    Actor05500_D06068,
    Actor05500_D06190,
    { NULL, Actor05500_D05F3C, NULL, NULL, Actor05500_D05FB4, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D061C8[11] = {
#include "assets/actor_105500_animation_06454_bank1.inc"
};

AnimationPackedRotation Actor05500_D0624C[44] = {
#include "assets/actor_105500_animation_06454_bank4.inc"
};

AnimationRecord Actor05500_D062FC[82] = {
#include "assets/actor_105500_animation_06454_records.inc"
};

u16 Actor05500_D06444[8] = {
#include "assets/actor_105500_animation_06454_indices.inc"
};

AnimationSet Actor05500_D06454 = {
    Actor05500_D062FC,
    Actor05500_D06444,
    { NULL, Actor05500_D061C8, NULL, NULL, Actor05500_D0624C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D0647C[13] = {
#include "assets/actor_105500_animation_068D8_bank1.inc"
};

AnimationPackedRotation Actor05500_D06518[96] = {
#include "assets/actor_105500_animation_068D8_bank4.inc"
};

AnimationRecord Actor05500_D06698[140] = {
#include "assets/actor_105500_animation_068D8_records.inc"
};

u16 Actor05500_D068C8[8] = {
#include "assets/actor_105500_animation_068D8_indices.inc"
};

AnimationSet Actor05500_D068D8 = {
    Actor05500_D06698,
    Actor05500_D068C8,
    { NULL, Actor05500_D0647C, NULL, NULL, Actor05500_D06518, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D06900[31] = {
#include "assets/actor_105500_animation_06E1C_bank1.inc"
};

AnimationPackedRotation Actor05500_D06A74[82] = {
#include "assets/actor_105500_animation_06E1C_bank4.inc"
};

AnimationRecord Actor05500_D06BBC[148] = {
#include "assets/actor_105500_animation_06E1C_records.inc"
};

u16 Actor05500_D06E0C[8] = {
#include "assets/actor_105500_animation_06E1C_indices.inc"
};

AnimationSet Actor05500_D06E1C = {
    Actor05500_D06BBC,
    Actor05500_D06E0C,
    { NULL, Actor05500_D06900, NULL, NULL, Actor05500_D06A74, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D06E44[27] = {
#include "assets/actor_105500_animation_074E4_bank1.inc"
};

AnimationPackedRotation Actor05500_D06F88[148] = {
#include "assets/actor_105500_animation_074E4_bank4.inc"
};

AnimationRecord Actor05500_D071D8[191] = {
#include "assets/actor_105500_animation_074E4_records.inc"
};

u16 Actor05500_D074D4[8] = {
#include "assets/actor_105500_animation_074E4_indices.inc"
};

AnimationSet Actor05500_D074E4 = {
    Actor05500_D071D8,
    Actor05500_D074D4,
    { NULL, Actor05500_D06E44, NULL, NULL, Actor05500_D06F88, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D0750C[11] = {
#include "assets/actor_105500_animation_077AC_bank1.inc"
};

AnimationPackedRotation Actor05500_D07590[51] = {
#include "assets/actor_105500_animation_077AC_bank4.inc"
};

AnimationRecord Actor05500_D0765C[80] = {
#include "assets/actor_105500_animation_077AC_records.inc"
};

u16 Actor05500_D0779C[8] = {
#include "assets/actor_105500_animation_077AC_indices.inc"
};

AnimationSet Actor05500_D077AC = {
    Actor05500_D0765C,
    Actor05500_D0779C,
    { NULL, Actor05500_D0750C, NULL, NULL, Actor05500_D07590, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D077D4[6] = {
#include "assets/actor_105500_animation_0793C_bank1.inc"
};

AnimationPackedRotation Actor05500_D0781C[26] = {
#include "assets/actor_105500_animation_0793C_bank4.inc"
};

AnimationRecord Actor05500_D07884[42] = {
#include "assets/actor_105500_animation_0793C_records.inc"
};

u16 Actor05500_D0792C[8] = {
#include "assets/actor_105500_animation_0793C_indices.inc"
};

AnimationSet Actor05500_D0793C = {
    Actor05500_D07884,
    Actor05500_D0792C,
    { NULL, Actor05500_D077D4, NULL, NULL, Actor05500_D0781C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D07964[9] = {
#include "assets/actor_105500_animation_07B50_bank1.inc"
};

AnimationPackedRotation Actor05500_D079D0[34] = {
#include "assets/actor_105500_animation_07B50_bank4.inc"
};

AnimationRecord Actor05500_D07A58[58] = {
#include "assets/actor_105500_animation_07B50_records.inc"
};

u16 Actor05500_D07B40[8] = {
#include "assets/actor_105500_animation_07B50_indices.inc"
};

AnimationSet Actor05500_D07B50 = {
    Actor05500_D07A58,
    Actor05500_D07B40,
    { NULL, Actor05500_D07964, NULL, NULL, Actor05500_D079D0, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D07B78[13] = {
#include "assets/actor_105500_animation_07E90_bank1.inc"
};

AnimationPackedRotation Actor05500_D07C14[58] = {
#include "assets/actor_105500_animation_07E90_bank4.inc"
};

AnimationRecord Actor05500_D07CFC[97] = {
#include "assets/actor_105500_animation_07E90_records.inc"
};

u16 Actor05500_D07E80[8] = {
#include "assets/actor_105500_animation_07E90_indices.inc"
};

AnimationSet Actor05500_D07E90 = {
    Actor05500_D07CFC,
    Actor05500_D07E80,
    { NULL, Actor05500_D07B78, NULL, NULL, Actor05500_D07C14, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D07EB8[9] = {
#include "assets/actor_105500_animation_08148_bank1.inc"
};

AnimationPackedRotation Actor05500_D07F24[53] = {
#include "assets/actor_105500_animation_08148_bank4.inc"
};

AnimationRecord Actor05500_D07FF8[80] = {
#include "assets/actor_105500_animation_08148_records.inc"
};

u16 Actor05500_D08138[8] = {
#include "assets/actor_105500_animation_08148_indices.inc"
};

AnimationSet Actor05500_D08148 = {
    Actor05500_D07FF8,
    Actor05500_D08138,
    { NULL, Actor05500_D07EB8, NULL, NULL, Actor05500_D07F24, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D08170[9] = {
#include "assets/actor_105500_animation_083F4_bank1.inc"
};

AnimationPackedRotation Actor05500_D081DC[49] = {
#include "assets/actor_105500_animation_083F4_bank4.inc"
};

AnimationRecord Actor05500_D082A0[81] = {
#include "assets/actor_105500_animation_083F4_records.inc"
};

u16 Actor05500_D083E4[8] = {
#include "assets/actor_105500_animation_083F4_indices.inc"
};

AnimationSet Actor05500_D083F4 = {
    Actor05500_D082A0,
    Actor05500_D083E4,
    { NULL, Actor05500_D08170, NULL, NULL, Actor05500_D081DC, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D0841C[20] = {
#include "assets/actor_105500_animation_08780_bank1.inc"
};

AnimationPackedRotation Actor05500_D0850C[61] = {
#include "assets/actor_105500_animation_08780_bank4.inc"
};

AnimationRecord Actor05500_D08600[92] = {
#include "assets/actor_105500_animation_08780_records.inc"
};

u16 Actor05500_D08770[8] = {
#include "assets/actor_105500_animation_08780_indices.inc"
};

AnimationSet Actor05500_D08780 = {
    Actor05500_D08600,
    Actor05500_D08770,
    { NULL, Actor05500_D0841C, NULL, NULL, Actor05500_D0850C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D087A8[5] = {
#include "assets/actor_105500_animation_08930_bank1.inc"
};

AnimationPackedRotation Actor05500_D087E4[19] = {
#include "assets/actor_105500_animation_08930_bank4.inc"
};

AnimationRecord Actor05500_D08830[60] = {
#include "assets/actor_105500_animation_08930_records.inc"
};

u16 Actor05500_D08920[8] = {
#include "assets/actor_105500_animation_08930_indices.inc"
};

AnimationSet Actor05500_D08930 = {
    Actor05500_D08830,
    Actor05500_D08920,
    { NULL, Actor05500_D087A8, NULL, NULL, Actor05500_D087E4, NULL, NULL, NULL },
};

GpU16Pair Actor05500_D08958[6] = {
    { 10, 0 },
    { 16, 0 },
    { 8, 1 },
    { 12, 6 },
    { 18, 6 },
    { 8, 0 },
};

GpPairSrcE Actor05500_D08970 = { Actor05500_D08958, 80, 6, 28, 1, 100, 20, 100, 0, 0 };

u16 Actor05500_D08980[8] = {
    40,
    35,
    30,
    25,
    20,
    15,
    10,
    5,
};

u16 Actor05500_D08990[8] = {
    1600,
    1800,
    2000,
    2100,
    2200,
    2300,
    2400,
    2500,
};

u16 Actor05500_D089A0[8] = {
    100,
    110,
    120,
    130,
    140,
    150,
    160,
    170,
};

s16 Actor05500_D089B0[4] = {
    0,
    2,
    4,
    6,
};

SVECTOR Actor05500_D089B8[4] = {
    { -3000, 0, -600, 0 },
    { -3000, 0, -1600, 0 },
    { -2000, 0, -600, 0 },
    { -2000, 0, -1600, 0 },
};

s16 Actor05500_D089D8[4] = {
    0,
    1024,
    2048,
    3072,
};

s16 Actor05500_D089E0[4] = {
    5,
    10,
    15,
    20,
};

s16 Actor05500_D089E8[4] = {
    100,
    100,
    100,
    100,
};

SVECTOR Actor05500_D089F0[4] = {
    { -7000, 0, -7600, 0 },
    { -5600, 0, -7500, 0 },
    { -4700, 0, -6500, 0 },
    { -5600, 0, -5600, 0 },
};

s16 Actor05500_D08A10[4] = {
    600,
    1024,
    2048,
    1800,
};

s16 Actor05500_D08A18[3] = {
    0,
    10,
    0,
};

s16 Actor05500_D08A1E = 8;

s16 Actor05500_D08A20 = 8;

s32 Actor05500_D08A24[5] = {
    0,
    0,
    0,
    0,
    4,
};

s16 Actor05500_D08A38[9][2] = {
    { 20, 0 },
    { 21, 180 },
    { 22, 300 },
    { 30, 216 },
    { 35, 134 },
    { 38, 13 },
    { 40, 10 },
    { 43, 13 },
    { 50, 3 },
};

s16 Actor05500_D08A5C[9][2] = {
    { 2, 250 },
    { 3, 10 },
    { 5, -30 },
    { 7, -55 },
    { 9, -80 },
    { 10, -300 },
    { 13, -97 },
    { 15, -55 },
    { 18, -10 },
};

ActorSpriteUv Actor05500_D08A80[8] = {
    { 0, 0, 0, 0 },
    { 32, 0, 0, 0 },
    { 64, 0, 0, 0 },
    { 96, 0, 0, 0 },
    { 0, 0, 32, 0 },
    { 32, 0, 32, 0 },
    { 64, 0, 32, 0 },
    { 96, 0, 32, 0 },
};

s16 Actor05500_D08AA0[14] = {
    8,
    12,
    16,
    20,
    24,
    28,
    32,
    32,
    32,
    36,
    36,
    36,
    40,
    0,
};

TaskDesc Actor05500_D08ABC = { TASK_BODY_TMD, 96, Actor05500_Fn03F88, { .model = &Actor05500_D05774 } };

TaskDesc Actor05500_D08AC8 = { 2, 96, Actor05500_Fn03DD8, { .model = NULL } };

AnimationSet* Actor05500_D08AD4[15] = {
    NULL,
    &Actor05500_D061A0,
    &Actor05500_D06454,
    &Actor05500_D068D8,
    &Actor05500_D06E1C,
    &Actor05500_D074E4,
    &Actor05500_D077AC,
    &Actor05500_D0793C,
    NULL,
    &Actor05500_D07B50,
    &Actor05500_D07E90,
    &Actor05500_D08148,
    &Actor05500_D083F4,
    &Actor05500_D08780,
    &Actor05500_D08930,
};

static void        Actor05500_Fn0006C(Task* arg0);
static inline void _actor05500TickAnim(Task* task);

static void Actor05500_Fn0006C(Task* arg0)
{
    Actor105500Work*       work;
    Actor105500HitScratch* head;
    Actor105500HitScratch* scratch;
    GpEnemy*               enemy;
    GfxCoord*              coord;
    GfxCoord*              src;
    s32                    result;
    s32                    lastId;
    u32                    damage;
    s16                    amount;
    s32                    best;
    s32                    push;
    s32                    dx;
    s32                    dy;
    s32                    dz;
    VECTOR*                unit;
    s32                    i;
    s16                    timer;
    s32                    one;
    u32                    kind;

    best    = 0;
    lastId  = 0;
    work    = arg0->work;
    coord   = arg0->extra.tmd->coords;
    head    = SCRATCH_HEAD(Actor105500HitScratch);
    scratch = SCRATCH_HEAD(Actor105500HitScratch) = head - 1;
    enemy                                         = (GpEnemy*)arg0->spawnArg2.pointer;
    work->field_3CC                               = 0;
    result                                        = func_800E0C10(work->field_234, &scratch->delta, 4, NULL);
    if (result != 0) {
        if (work->field_39A == 2) {
            work->field_3CC = 1;
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += head[-1].delta.vx.h.hi;
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                coord->coord.t[2] += scratch->delta.vz.h.hi;
                break;
            case 2:
                coord->coord.t[0] = work->field_35C.vx;
                coord->coord.t[1] = work->field_35C.vy;
                coord->coord.t[2] = work->field_35C.vz;
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_234);
    if (work->field_390 != 0) {
        timer           = (u16)work->field_390 - 1;
        work->field_390 = timer;
        if (timer <= 0) {
            work->field_390 = 0;
        }
    }
    one = 1;

    work->field_3D0 = 0;
    work->field_3BA = 0;
    unit            = &scratch->normal;
    for (i = 0; i < 2; i++) {
        kind = (u32)work->field_2B4[i].key.value >> 16;
        if (kind == one)
            goto physical;
        if (kind == 0)
            goto next_contact;
        if (kind == 2)
            goto damage_contact;
        if (kind == 3)
            goto physical;
        goto next_contact;
    damage_contact:
        if (work->field_390 == 0) {
            result = 0;
            if ((((u32)work->field_2B4[i].key.value >> 8) & 0x3F) == 0x24) {
                if ((work->field_2B4[i].key.value & 0x3F) == 0x24) {
                    result = 1;
                }
            }
            if ((result != one) || (work->field_3B2 == 0)) {
                src                 = Gp_ActorSlots[((u32)work->field_2B4[i].key.value >> 7) & 1]->extra.tmd->coords;
                dx                  = src->coord.t[0] - coord->coord.t[0];
                scratch->delta.vx.w = dx;
                dy                  = src->coord.t[1] - coord->coord.t[1];
                scratch->delta.vy.w = dy;
                dz                  = src->coord.t[2] - coord->coord.t[2];
                scratch->delta.vz.w = dz;
                damage              = Gp_ComputeDamage((u32)work->field_2B4[i].key.value, SquareRoot0(dx * dx + dy * dy + dz * dz), 0, 0);
                amount              = damage;
                if (result == 0) {
                    if (work->field_3CA != 0) {
                        amount = (damage << 16) >> 15;
                        Gp_SpawnEff(0x6009C, arg0->extra.tmd->coords + 1, 3, NULL);
                    }
                    if (Gp_RollEnemyChance(enemy, (u32)work->field_2B4[i].key.value, 0) != 0) {
                        amount = (amount << 16) >> 14;
                        if (work->field_3CA == 0) {
                            Gp_SpawnEff(0x6009C, arg0->extra.tmd->coords + 1, 0, NULL);
                        }
                    }
                    func_800E2C78(enemy, (u32)work->field_2B4[i].key.value, amount, 0);
                }
                func_800DA6E8(&enemy->node, amount, 0);
                enemy->hp -= amount;
                if (work->field_3C8 != one) {
                    if (enemy->hp <= 0) {
                        work->field_39A = 9;
                        work->field_39C = 0;
                        arg0->state     = 2;
                    } else if (result == 0) {
                        work->field_39A = 6;
                        work->field_39C = 0;
                    }
                }
                if (work->field_3C8 == 2) {
                    if ((work->field_39A == 9) || (result == 0)) {
                        work->field_3C8 = 0;
                        ACTOR05500_TURN_AROUND(work, coord, &scratch->rot);
                    }
                }
                if (result == 0) {
                    work->field_3D0        = one;
                    work->field_2E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
                switch (Gp_GetIdParam0(work->field_2B4[i].key.value) & 0xFFFF) {
                    case 0:
                    case 1:
                    case 5:
                    case 8:
                    case 9:
                        break;
                    case 2:
                        Gp_SetObjFlag2(enemy, work->field_2B4[i].key.value, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(enemy, work->field_2B4[i].key.value, 0);
                        break;
                    case 4:
                    case 6:
                        if (work->field_3C8 != one) {
                            work->field_3BA = one;
                        }
                        break;
                    case 7:
                        if (work->field_3B0 == 0) {
                            work->field_3B0        = one;
                            work->field_3BE        = 0;
                            work->field_3B2        = 0;
                            work->field_31C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                            Gp_SetLightMode(arg0->spawnArg2.pointer, 3);
                        }
                        break;
                }
                if (lastId != work->field_2B4[i].key.value) {
                    lastId          = work->field_2B4[i].key.value;
                    scratch->rot.vx = 0;
                    scratch->rot.vy = -0xC8;
                    scratch->rot.vz = 0;
                    func_800FDB18(Gp_GetIdParam1(work->field_2B4[i].key.value) & 0xFFFF, arg0->extra.tmd->coords + 1, &scratch->rot, &work->field_354);
                }
                result = Gp_GetIdParam2(work->field_2B4[i].key.value);
                if (result > 0) {
                    work->field_390 = result;
                }
            }
        }
        goto next_contact;
    physical:
        ACTOR05500_CONTACT_OVERLAP(push, coord, work->field_2B4[i], scratch->delta);
        if (best < push) {
            best = push;
            ACTOR05500_GRID_DIRECTION(&scratch->delta, unit, &scratch->local);
        }
    next_contact:;
    }
    if (best > 0) {
        coord->coord.t[0] += (best * scratch->local.vx) >> 12;
        coord->coord.t[2] += (best * scratch->local.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->field_2B4);
    work->field_3CE = 0;
    if (work->field_304[0].flags & 1) {
        ACTOR05500_NOTE_BLOCKING_CONTACT(work, work->field_304[0]);
        work->field_2E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        Gp_ClearRec18Occupied(work->field_304);
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor105500HitScratch);
}

/// State handlers of the task `Actor05500_Fn03DD8` dispatches, indexed by the
/// task's state: `Actor05500_Fn03E34` sets up its collision object,
/// `Actor05500_Fn02780` runs it, and `Gp_DestroyEnemy` tears it down.
static const GpEnemyTaskFuncTable3 Actor05500_D0002C = {
    {
        Actor05500_Fn03E34,
        Actor05500_Fn02780,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the task `Actor05500_Fn03F88` dispatches, indexed by the
/// task's state: `Actor05500_Fn02FFC` allocates and sets up the work block,
/// `Actor05500_Fn03560` runs the actor, and `Actor05500_Fn02364` handles its
/// last state.
static const GpEnemyTaskFuncTable3 Actor05500_D00038 = {
    {
        Actor05500_Fn02FFC,
        Actor05500_Fn03560,
        Actor05500_Fn02364,
    },
};

static void Actor05500_Fn00754(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              dx;
    s32              dz;
    u32              random;
    s32              index;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                      = arg0->work;
    state                                                                     = work->field_39C;
    coord                                                                     = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            scratchEnd[-1].vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
            delta->vy         = 0;
            dz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            delta->vz         = dz;
            dx                = scratchEnd[-1].vx;
            if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) || (work->field_3D0 != 0) || (Gp_StateF0.prefix.bytes.field_3 == 2)) {
                work->field_39C = 1;
                work->field_392 = 0xD;
                Gp_ArmStateF0(1);
            }
            break;
        case 1:
            if ((u32)(work->field_396 - 0xB) < 0x32U) {
                coord->coord.t[2] += 4;
            }
            if ((s16)work->field_396 >= 0x4B) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = state;
                index           = ((GpEnemy*)arg0->spawnArg2.pointer)->place->rowIndex;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = Actor05500_D08980[index] + ((random >> 0x10) & 0xF);
            }
            break;
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}

static void Actor05500_Fn00914(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    s16              angle;
    s32              magnitude;
    s16              wrapped;
    s16              difference;
    s32              distance;
    s32              dx;
    s32              dz;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    coord                                                                     = arg0->extra.tmd->coords;
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                      = arg0->work;
    work->field_3A2                                                           = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
    scratchEnd[-1].vx                                                         = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy                                                                 = 0;
    dz                                                                        = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    delta->vz                                                                 = dz;
    dx                                                                        = scratchEnd[-1].vx;
    distance                                                                  = SquareRoot0((dx * dx) + (dz * dz));
    angle                                                                     = (u16)work->field_3A2 - (ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)delta->vz) & 0xFFF);
    magnitude                                                                 = __builtin_abs((s32)angle);
    if (magnitude < 0x800) {
        difference = magnitude;
    } else {
        if (angle > 0) {
            wrapped = 0x1000 - angle;
        } else {
            wrapped = angle + 0x1000;
        }
        difference = wrapped;
    }
    if ((distance < 0x8FC) && (difference < 0x80)) {
        work->field_39A = 5;
        work->field_39C = 0;
        work->field_392 = 4;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}

static void Actor05500_Fn00A94(Task* actor)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              pan0;
    s32              pan1;
    s32              pan2;
    s32              sessionFlags;
    s32              dx;
    s32              dz;
    s32              value;
    s32              sound;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    coord                                                                     = actor->extra.tmd->coords;
    work                                                                      = actor->work;
    state                                                                     = work->field_39C;
    sessionFlags                                                              = GAME_LOCATION_WORD(gGameSession->location.loc);
    value                                                                     = 0;
    switch (state) {
        case 0:
            if (work->field_3C6 == 0) {
                scratchEnd[-1].vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
                delta->vy         = 0;
                dz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                delta->vz         = dz;
                dx                = scratchEnd[-1].vx;
                if (SquareRoot0((dx * dx) + (dz * dz)) < 0x5DC) {
                    value = 1;
                }
            }
            if ((value != 0) || (Gp_StateF0.field_22 != 0) || (Gp_StateF0.field_8 != 0)) {
                if (work->field_3C6 == 0) {
                    Gp_StateF0.field_22 = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C        = 1;
                work->field_392        = 7;
                work->field_3A8        = Actor05500_D089A0[((GpEnemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->field_2E4.coord  = coord;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(Actor05500_D08958, 5);
                work->field_2E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((sessionFlags & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    sound = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x55200006;
                    pan0  = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan0, (s8)gpGetObjDepth(coord));
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                work->field_370 = coord->workm;
                break;
            }
            if (work->field_3D0 != 0) {
                if (work->field_3C6 == 0) {
                    Gp_StateF0.field_22 = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C        = 2;
                work->field_392        = 9;
                work->field_3A8        = Actor05500_D089A0[((GpEnemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->field_3BC        = 0x2D;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.coord  = coord;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(Actor05500_D08958, 5);
                work->field_2E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((sessionFlags & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    sound = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x55200006;
                    pan1  = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan1, (s8)gpGetObjDepth(coord));
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                work->field_370 = coord->workm;
                break;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            work->field_370 = coord->workm;
            break;
        case 1:
            work->field_3A0 = (u16)work->field_3A0 + ((u16)work->field_35C.vy - (u16)coord->coord.t[1]);
            if (((Player_Status.coordMtx->t[1] - 0x3E8) < coord->coord.t[1]) || (work->field_3D0 != 0) || (work->field_3CE != 0)) {
                work->field_39C = 2;
                work->field_392 = 9;
                work->field_3BC = 0x2D;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            work->field_370 = coord->workm;
            break;
        case 2:
            work->field_3A8 = ((s16)work->field_396 >= 0xC) << 7;
            if (work->field_3CC != 0) {
                work->field_39C        = 3;
                work->field_392        = 0xA;
                work->field_3A8        = 0x80;
                work->field_2E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                sound                  = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x401A0002;
                pan2                   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan2, (s8)gpGetObjDepth(coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 >= 0x1E) {
                Gp_ArmStateF0(1);
                work->field_39A        = state;
                work->field_39C        = 0;
                work->field_392        = 1;
                work->field_39E        = Actor05500_D08980[((GpEnemy*)actor->spawnArg2.pointer)->place->rowIndex] + (((Gp_LcgState = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
                work->field_2E4.coord  = actor->extra.tmd->coords + 4;
                work->field_2E4.radius = 0xC8;
                work->field_2E4.pos.vy = 0;
                work->field_3C8        = 0;
                if (((GpEnemy*)actor->spawnArg2.pointer)->hp <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    actor->state    = 2;
                }
            }
            break;
    }
    Actor05500_Fn02C94(actor);
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}

static void Actor05500_Fn00FA0(Task* arg0)
{
    Actor105500Work*  work;
    GfxCoord*         coord;
    s32               state;
    s16               timer;
    s16               timer2;
    s32               distance;
    s32               sound;
    s32               dx;
    s32               dz;
    s32               pan;
    u32               random;
    u32               random2;
    ActorFaceScratch* delta;
    ActorFaceScratch* scratchEnd;

    scratchEnd                                                                          = *(ActorFaceScratch**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                               = scratchEnd - 1;
    *(ActorFaceScratch**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                                = arg0->work;
    state                                                                               = work->field_39C;
    coord                                                                               = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_3C8 = 0;
            work->field_398 = 0;
            work->field_3A6 = 0;
            timer           = (u16)work->field_39E - 1;
            work->field_39E = timer;
            if (timer <= 0) {
                work->field_39C = 1;
                work->field_392 = 2;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor05500_D08990[((GpEnemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0x3FF);
                Gp_LcgState     = random;
                return;
            }
            return;
        case 1:
            scratchEnd[-1].delta.vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
            delta->delta.vy         = 0;
            delta->delta.vz         = (s32)(Player_Status.coordMtx->t[2] - coord->coord.t[2]);
            work->field_3A4         = ratan2((s32)(s16)scratchEnd[-1].delta.vx, (s32)(s16)delta->delta.vz) & 0xFFF;
            work->field_3A6         = 0x12;
            if ((s16)work->field_396 >= 0xB) {
                work->field_398 = 0x17;
            }
            timer2          = (u16)work->field_39E - (u16)work->field_398;
            work->field_39E = timer2;
            if (timer2 <= 0) {
                work->field_39C = 0;
                work->field_392 = state;
                random2         = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor05500_D08980[((GpEnemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random2 >> 0x10) & 0xF);
                Gp_LcgState     = random2;
                return;
            }
            if ((s16)work->field_396 == 0xC) {
                sound = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x401A0001;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_396 >= 0x29) {
                work->field_396 = 0xB;
            }
            if (work->field_3A4 == work->field_3A2) {
                scratchEnd[-1].delta.vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
                delta->delta.vy         = 0;
                dz                      = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                delta->delta.vz         = dz;
                dx                      = scratchEnd[-1].delta.vx;
                distance                = SquareRoot0((dx * dx) + (dz * dz));
                if ((work->field_3C0 == 0) && (distance < 0x578) && (work->field_3B0 == 0) && !(Player_Status.peStateFlags & 1)) {
                    work->field_39A = 4;
                    work->field_39C = 0;
                    work->field_392 = 3;
                    work->field_3AC = 0;
                } else if (distance < 0x8FC) {
                    work->field_39A = 5;
                    work->field_39C = 0;
                    work->field_392 = 4;
                }
            }
            break;
    }
}

static void Actor05500_Fn012E8(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    s32              sound;
    s32              pan;
    u32              random;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    work->field_398 = 0;
    work->field_3A6 = 0;
    if ((s16)work->field_396 == 0x28) {
        sound = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x401A0003;
        pan   = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
    }
    if ((u32)(work->field_396 - 0x2B) < 7U) {
        Gp_SpawnEnemyFromTable(work->field_36C, 1, 0, arg0->spawnArg2.pointer);
        work->field_3AC = (u16)(work->field_3AC + 1);
    }
    if ((s16)work->field_396 >= (Actor05500_D08A1E + 0x3C)) {
        work->field_39A = 3;
        work->field_39C = 0;
        work->field_392 = 1;
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_39E = Actor05500_D08980[((GpEnemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
        Gp_LcgState     = random;
    }
}

static void Actor05500_Fn0143C(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    SVECTOR*         rotation;
    s16(*motion0)[2];
    s16(*motion1)[2];
    s16 state;
    s32 sound;
    s32 index;
    s32 pan;
    s32 pan1;
    u32 random;

    rotation = SCRATCH_PUSH(SVECTOR);
    work     = arg0->work;
    state    = work->field_39C;
    coord    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_3C8 = 0;
            work->field_3CA = 0;
            work->field_398 = 0;
            work->field_3A6 = 0;
            if ((u32)(work->field_396 - 0x1B) < 0xDU) {
                work->field_3C8        = 1;
                work->field_3CA        = 1;
                work->field_2E4.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (work->field_3B0 == 0) {
                    index = (s16)work->field_396 < 0x22;
                } else {
                    index = 3;
                    if ((s16)work->field_396 < 0x22) {
                        index = 4;
                    }
                }
                work->field_2E4.key = Gp_PackPair(Actor05500_D08958, index);
                if (((s16)work->field_396 < 0x23) && ((work->field_3D0 != 0) || (work->field_3CE != 0))) {
                    work->field_39C        = 1;
                    work->field_3CA        = 0;
                    work->field_392        = 5;
                    work->field_2E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    break;
                }
            } else {
                work->field_2E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            index   = 0;
            motion0 = Actor05500_D08A38;
            for (; index < 9; index++, motion0++) {
                if ((s16)work->field_396 <= (motion0[0][0] + Actor05500_D08A20)) {
                    coord->coord.t[0] += (s32)(motion0[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion0[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((s16)work->field_396 == 0x28) {
                sound = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x401A0002;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
                work->field_3C8 = 0;
                if (((GpEnemy*)arg0->spawnArg2.pointer)->hp <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                }
            }
            if ((s16)work->field_396 >= (Actor05500_D08A20 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor05500_D08980[((GpEnemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
                Gp_LcgState     = random;
            }
            break;
        case 1:
            index           = 0;
            motion1         = Actor05500_D08A5C;
            work->field_398 = 0;
            work->field_3A6 = 0;
            for (; index < 9; index++, motion1++) {
                if ((s16)work->field_396 <= motion1[0][0]) {
                    coord->coord.t[0] += (s32)(motion1[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion1[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((u32)(work->field_396 - 0x1F) < 0xFU) {
                coord->coord.t[0] += (s32)(rcos((s32)work->field_3A2) * 0xB) >> 0xC;
                coord->coord.t[2] += (s32)(rsin((s32)work->field_3A2) * 0xB) >> 0xC;
            }
            if ((s16)work->field_396 == 0x10) {
                sound = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x401A0002;
                pan1  = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan1, (s8)gpGetObjDepth(coord));
                work->field_3C8 = 2;
                if (((GpEnemy*)arg0->spawnArg2.pointer)->hp <= 0) {
                    ACTOR05500_TURN_AROUND(work, coord, rotation);
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                }
            }
            if ((s16)work->field_396 >= 0x46) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                work->field_39E = Actor05500_D08980[((GpEnemy*)arg0->spawnArg2.pointer)->place->rowIndex] + (((Gp_LcgState = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
                work->field_3AA = 1;
                if (work->field_3C8 == 2) {
                    ACTOR05500_TURN_AROUND(work, coord, rotation);
                    work->field_3C8 = 0;
                }
                for (index = 1; index < 8; index++) {
                    func_800B4114(&work->anim, index, (s32)work->field_392, 0, 0);
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Action 6, the flinch after a hit. The first call starts animation 0xB and
/// plays sound event 4 of the actor's sound bank at its position; once the
/// animation has run 0x15 frames, the actor goes to action 7 when `field_3D2`
/// is set and otherwise back to action 3 with a fresh random count in
/// `field_39E`.
static void Actor05500_Fn01A0C(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              sound;
    s32              pan;
    u32              random;

    work  = arg0->work;
    state = work->field_39C;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_392 = 0xB;
            work->field_394 = 1;
            work->field_39C = 1;
            work->field_398 = 0;
            work->field_3A6 = 0;
            sound           = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x401A0004;
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            return;
        case 1:
            if ((s16)work->field_396 >= 0x15) {
                if (work->field_3D2 == state) {
                    work->field_39A = 7;
                    work->field_39C = 0;
                    return;
                }
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = state;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = (random >> 0x10) & 0xF;
            } else {
                return;
            }
            break;
    }
}

static void Actor05500_Fn01B30(Task* arg0)
{
    TmdObject*       obj;
    Actor105500Work* work;
    GfxCoord*        coord;
    s16(*motion)[2];
    SVECTOR* scratchEnd;
    SVECTOR* velocity;
    s32      state;
    s32      one;
    s32      pan1;
    s32      pan2;
    s32      pan3;
    s16      timer;
    GpEnemy* ctx;
    s32      indexOrSound;
    u32      randomY;
    u32      randomZ;
    u32      randomX;
    u32      randomDelay;
    u32      randomRise;

    obj        = arg0->extra.tmd;
    work       = arg0->work;
    ctx        = arg0->spawnArg2.pointer;
    scratchEnd = *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    velocity   = (*(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = scratchEnd - 1);
    state      = work->field_39C;
    coord      = obj->coords;
    one        = 1;
    switch (state) {
        case 0:
            work->field_294.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_214.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            obj->flags              = (u16)obj->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            ctx->node.state.b.flags = one;
            if (Gp_StateF0.field_1E == one) {
                if (work->field_3C2 == 0) {
                    work->field_39E = Actor05500_D089B0[work->field_3C4];
                } else {
                    work->field_39E = Actor05500_D089E0[work->field_3C4];
                }
                work->field_39C = 1;
            }
            break;
        case 1:
            timer           = (u16)work->field_39E - 1;
            work->field_39E = timer;
            if (timer <= 0) {
                work->field_39E = 0;
                work->field_39C = 2;
            }
            break;
        case 2:
            Tmd_AllocBuffers(obj);
            obj->flags   = (u16)obj->flags & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            indexOrSound = 0;
            if (work->field_3C2 == 0) {
                work->field_39C = 3;
                work->field_392 = 4;
                work->field_398 = 0;
                work->field_3A6 = 0;
                work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
            } else {
                work->field_392        = 7;
                work->field_39A        = state;
                work->field_39C        = 1;
                work->field_3A8        = Actor05500_D089E8[work->field_3C4];
                work->field_294.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->field_214.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                do {
                    velocity->vx = 0;
                    velocity->vz = 0;
                    randomRise   = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomRise;
                    velocity->vy = ((randomRise >> 0x10) & 0x1FF) + 0x2EE;
                    Gp_SpawnEff(0x6017C, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 5);
                indexOrSound = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x510D0012;
                pan1         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, (s32)pan1, (s8)gpGetObjDepth(coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 == 0x1E) {
                indexOrSound = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x51090007;
                pan2         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, (s32)pan2, (s8)gpGetObjDepth(coord));
            }
            indexOrSound = 0;
            if ((s16)work->field_396 == 0x27) {
                do {
                    randomX      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomX;
                    velocity->vx = -((coord->coord.m[0][2] * (s32)(((randomX >> 16) & 0x3F) + 0xAF)) >> 12);
                    randomY      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomY;
                    velocity->vy = ((randomY >> 16) & 0x1FF) - 0x6D6;
                    randomZ      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomZ;
                    velocity->vz = -((coord->coord.m[2][2] * (s32)(((randomZ >> 16) & 0x3F) + 0xAF)) >> 12);
                    Gp_SpawnEff(0x60051, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 3);
                indexOrSound = 0;
            }
            motion = Actor05500_D08A38;
            do {
                indexOrSound++;
                if ((s16)work->field_396 <= ((*motion)[0] + Actor05500_D08A20)) {
                    coord->coord.t[0] += ((*motion)[1] * rsin(work->field_3A2)) >> 12;
                    coord->coord.t[2] += ((*motion)[1] * rcos(work->field_3A2)) >> 12;
                    break;
                }
                motion++;
            } while (indexOrSound < 9);
            if ((s16)work->field_396 == 0x28) {
                indexOrSound = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x401A0002;
                pan3         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, (s32)pan3, (s8)gpGetObjDepth(coord));
                work->field_3A8        = 0x80;
                work->field_294.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->field_214.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            }
            if ((s16)work->field_396 >= (Actor05500_D08A20 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                randomDelay     = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor05500_D08980[((GpEnemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((randomDelay >> 0x10) & 0xF);
                Gp_LcgState     = randomDelay;
                Gp_ArmStateF0(1);
            }
            break;
    }
    *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}

/// Runs the actor's ambient timers: `field_3B2` wraps every 0x50 calls, every
/// 12th call spawns effect 3 at coordinate node 3 or 5 in turn, and every 0x24th
/// call plays sound event 5 of its sound bank at its position.
static void Actor05500_Fn020D4(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    s32              sound;
    s32              pan;
    u16              timer;
    u16              effectTimer;
    u16              countdown;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    timer           = work->field_3B2 + 1;
    work->field_3B2 = timer;
    if ((s16)timer >= 0x50) {
        work->field_3B2 = 0U;
    }
    effectTimer     = work->field_3B4 + 1;
    work->field_3B4 = effectTimer;
    if ((s16)effectTimer == 0xC) {
        work->field_3B4 = 0U;
        if (work->field_3B6 == 0) {
            func_800FDB18(3, arg0->extra.tmd->coords + 3, NULL, &work->field_354);
            work->field_3B6 = 1;
        } else {
            func_800FDB18(3, arg0->extra.tmd->coords + 5, NULL, &work->field_354);
            work->field_3B6 = 0;
        }
    }
    countdown       = work->field_3BE - 1;
    work->field_3BE = countdown;
    if ((s16)countdown <= 0) {
        work->field_3BE = 0x24U;
        sound           = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x401A0005;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(coord));
    }
}

/// Turns the actor's yaw towards `field_3A4` by at most `field_3A6`, the short
/// way round, snapping when the remaining angle is within one step, and
/// rebuilds its coordinate rotation from the result in a scratchpad block.
static void Actor05500_Fn02214(Task* arg0)
{
    Actor105500Work*  work;
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

    sc    = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_3A4;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_3A2 = ang;
    if (adiff < 0x800) {
        step = work->field_3A6;
        if (step >= adiff) {
            work->field_3A2 = want;
        } else {
            next = work->field_3A2;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_3A2 = next;
        }
    } else {
        step = work->field_3A6;
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
        work->field_3A2 = work->field_3A4;
        goto done;
    turn:
        wrapStep = work->field_3A6;
        cur      = work->field_3A2;
        if (diff > 0) {
            work->field_3A2 = cur - wrapStep;
        } else {
            work->field_3A2 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_3A2;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_POP_BYTES(0x18);
}

/// Switches the work's animation id, resetting the slots to the blend value the
/// table gives for the new id; otherwise ticks every slot one frame.
static inline void _actor05500TickAnim(Task* task)
{
    Actor105500Work* work;
    s32              i;
    s32              value;

    work = task->work;
    if (work->field_392 != work->field_394) {
        work->field_394 = work->field_392;
        work->field_396 = 0;
        value           = Actor05500_D08A18[work->field_392];
        for (i = 1; i < 8; i++) {
            func_800B4114(&work->anim, i, work->field_392, 0, value);
        }
    } else {
        work->field_396++;
        for (i = 1; i < 8; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

static void Actor05500_Fn02364(GpEnemy* arg0, Task* arg1)
{
    VECTOR           vec;
    Actor105500Work* work;
    GfxCoord*        coord;
    GfxCoord*        colorCoord;
    TmdObject*       obj;
    s32              releaseId;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    switch (Gp_StateF0.field_4) {
        case 1:
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            return;
        case 2:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
        default:
            switch (work->field_39C) {
                case 0:
                    work->field_3A0 = 0x1000;
                    work->field_370 = coord->coord;
                    arg0->recs      = 0;
                    Gp_UnlinkNode(&arg0->node);
                    Gp_UnlinkObj(&work->field_214);
                    Gp_UnlinkObj(&work->field_294);
                    Gp_UnlinkObj(&work->field_2E4);
                    Gp_UnlinkObj(&work->field_31C);
                    releaseId = 0x37;
                    if (work->field_3C0 == 0) {
                        releaseId = 0x1A;
                    }
                    Gp_ReleaseStateF0Add(arg1, releaseId);
                    Gp_SetStateF0Byte3(2);
                    work->field_39E = 0;
                    work->field_39C = 1;
                    Gp_SetLightMode(arg0, 1);
                    if (work->field_3BA != 0) {
                        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->field_392 = 0xB;
                    _actor05500TickAnim(arg1);
                    colorCoord = arg1->extra.tmd->coords;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
                    return;
                case 1:
                    if (work->field_3BA != 0) {
                        if (work->field_3BA >= 2) {
                            work->field_3BA = 0;
                            Tmd_FreeBuffers(obj);
                            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            Actor05500_Fn03C54(arg1);
                            Actor05500_Fn03D40(arg1);
                        } else {
                            work->field_3BA++;
                        }
                    }
                    Actor05500_Fn03B60(arg1);
                    work->field_39E++;
                    if (work->field_39E == 0xA) {
                        obj->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    if (work->field_39E == 0xF) {
                        Gp_SpawnEff(0x600A5, coord, 2, NULL);
                    }
                    if (work->field_39E >= 0x3C) {
                        work->field_39C = 2;
                        work->field_39E = 0;
                        obj->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    _actor05500TickAnim(arg1);
                    colorCoord = arg1->extra.tmd->coords;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
                    return;
                case 2:
                    work->field_39E++;
                    if (work->field_39E >= 0x3C) {
                        Gp_DestroyEnemy(arg0, arg1);
                    }
                    return;
            }
            break;
    }
}

static void Actor05500_Fn02780(GpEnemy* arg0, Task* arg1)
{
    ActorsShared80135c4cObjWork* work;
    GfxCoord*                    coord;
    s16                          age;
    s16                          speed;
    s32                          contact;
    u16                          flags;
    u32                          random;

    coord = arg1->extra.tmd->coords;
    work  = arg1->work;
    switch ((s32)Gp_StateF0.field_4) {
        case 1:
            Actor05500_Fn02954(arg1, work->field_38);
            return;
        default:
        default_case:
            contact = work->rec.key.value;
            if (contact != 0) {
                if ((contact & 0xFFFF0000) != 0x100000) {
                    work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ClearRec18Occupied(&work->rec);
                    goto block_7;
                }
                goto block_11;
            }
        block_7:
            if (!((u16)work->field_38 & 3)) {
                flags = work->obj.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else {
                flags = work->obj.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            work->obj.flags     = flags;
            coord->coord.t[0]  += (s32)(coord->coord.m[0][2] * work->field_3A) >> 0xC;
            coord->coord.t[1]  += (s32)(coord->coord.m[1][2] * work->field_3A) >> 0xC;
            coord->coord.t[2]  += (s32)(coord->coord.m[2][2] * work->field_3A) >> 0xC;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            Actor05500_Fn02954(arg1, work->field_38);
            age            = (u16)work->field_38 + 1;
            work->field_38 = age;
            if (age >= 0xF) {
            block_11:
                Gp_UnlinkObj(&work->obj);
                arg1->state = 2;
                return;
            }
            random         = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState    = random;
            speed          = (u16)work->field_3A - ((random >> 0x10) & 0x1F);
            work->field_3A = speed;
            if (speed < 0) {
                work->field_3A = 0;
            }
            return;
        case 0:
            goto default_case;
        case 2:
            return;
    }
}

static void Actor05500_Fn02954(Task* actor, s32 frame)
{
    POLY_FT4*         poly;
    GfxCoord*         coord;
    s32               depth;
    s32               screen;
    s32               y;
    s32               radius;
    s32               x;
    s32               bottom;
    s32               top;
    s32               left;
    s32               right;
    ActorQuadScratch* scratchEnd;
    ActorQuadScratch* s;
    TmdObject*        texture;
    ActorSpriteUv*    uv;
    SVECTOR*          projection;

    scratchEnd                                                            = (ActorQuadScratch*)*(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    coord                                                                 = actor->extra.tmd->coords;
    actor                                                                 = actor->parent;
    texture                                                               = actor->extra.tmd;
    scratchEnd[-1].v[0].vx                                                = (u16)coord->workm.t[0];
    s                                                                     = scratchEnd - 1;
    s->v[0].vy                                                            = (u16)coord->workm.t[1];
    *(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = (u8*)s;
    s->v[0].vz                                                            = (u16)coord->workm.t[2];
    projection                                                            = &s->v[0];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(projection);
    gte_rtps();
    gte_stsxy(&scratchEnd[-1].sxy);
    gte_stszotz(&scratchEnd[-1].otz);
    depth = s->otz;
    if (depth >= 0x14) {
        radius                 = (s32)(Actor05500_D08AA0[frame] * 0x300) / depth;
        poly                   = gGpuPrimCursor;
        screen                 = s->sxy;
        gGpuPrimCursor         = poly + 1;
        x                      = screen & 0xFFFF;
        y                      = screen >> 0x10;
        left                   = x - radius;
        top                    = y - radius;
        right                  = x + radius;
        bottom                 = y + radius;
        scratchEnd[-1].v[0].vx = left;
        s->v[0].vy             = top;
        s->v[0].vz             = 0;
        s->v[1].vx             = right;
        s->v[1].vy             = top;
        s->v[1].vz             = 0;
        s->v[2].vx             = left;
        s->v[2].vy             = bottom;
        s->v[2].vz             = 0;
        s->v[3].vx             = right;
        s->v[3].vy             = bottom;
        s->v[3].vz             = 0;
        setPolyFT4(poly);
        setSemiTrans(poly, 1);
        setRGB0(poly, 0x80, 0x80, 0x80);
        setShadeTex(poly, 1);
        poly->tpage = (s16)(((s32)(((texture->texturePageOffset << 6) + 0x180) & 0x3FF) >> 6) | 0xB0);
        poly->clut  = (s16)(((s32)((u8)texture->clutRowOffset << 0x18) >> 0x12) + 0x3D40);
        uv          = &Actor05500_D08A80[frame >> 1];
        poly->u0    = (u8)uv->u;
        poly->v0    = (u8)uv->v;
        poly->u1    = (s8)(uv->u + 0x1F);
        poly->v1    = (u8)uv->v;
        poly->u2    = (u8)uv->u;
        poly->v2    = (s8)(uv->v + 0x1F);
        poly->u3    = (s8)(uv->u + 0x1F);
        poly->v3    = (s8)(uv->v + 0x1F);
        poly->x0    = (u16)scratchEnd[-1].v[0].vx;
        poly->y0    = (u16)s->v[0].vy;
        poly->x1    = (u16)s->v[1].vx;
        poly->y1    = (u16)s->v[1].vy;
        poly->x2    = (u16)s->v[2].vx;
        poly->y2    = (u16)s->v[2].vy;
        poly->x3    = (u16)s->v[3].vx;
        poly->y3    = (u16)s->v[3].vy;
        addPrim((&gGpuCurrentOt[((((u32)(s->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
    }
    *(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += 0x28;
}

/// Draws a semi-transparent gouraud line through the actor's frame
/// `field_370`, from local height `field_3A0 - 0x352` down to -0x352, followed
/// by a texture-page primitive at the same depth; nothing is drawn when either
/// end is nearer than depth 30. With `field_3BC` at zero the ends are grey
/// levels 0x80 and 0xC0; otherwise `field_3BC` is decremented, holding at 1,
/// and scales both levels by `field_3BC / 45`.
static void Actor05500_Fn02C94(Task* actor)
{
    Actor105500LineScratch* s;
    Actor105500Work*        work;
    LINE_G2*                line;
    DR_TPAGE*               page;
    s32                     x;
    s32                     y;
    s32                     screen;
    s32                     screen1;

    s              = (Actor105500LineScratch*)(*(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) -= sizeof(Actor105500LineScratch));
    work           = actor->work;
    s->position.vx = 0;
    s->position.vy = work->field_3A0 - 0x352;
    s->position.vz = 0;
    gte_SetRotMatrix(&work->field_370);
    gte_SetTransMatrix(&work->field_370);
    gte_ldv0(&s->position);
    gte_rtps();
    gte_stsxy(&s->screen);
    gte_stszotz(&s->depth);
    if (s->depth < 30) {
        *(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += sizeof(Actor105500LineScratch);
        return;
    }
    screen         = s->screen;
    x              = screen & 0xFFFF;
    y              = screen >> 16;
    s->position.vx = 0;
    s->position.vy = -0x352;
    s->position.vz = 0;
    gte_SetRotMatrix(&work->field_370);
    gte_SetTransMatrix(&work->field_370);
    gte_ldv0(&s->position);
    gte_rtps();
    gte_stsxy(&s->screen);
    gte_stszotz(&s->depth);
    if (s->depth < 30) {
        *(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += sizeof(Actor105500LineScratch);
        return;
    }
    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    screen1        = s->screen;
    setLineG2(line);
    setSemiTrans(line, 1);
    line->x0 = x;
    line->y0 = y;
    line->x1 = screen1;
    line->y1 = screen1 >> 16;
    if (work->field_3BC == 0) {
        line->b0 = line->g0 = line->r0 = 0x80;
        line->b1 = line->g1 = line->r1 = 0xC0;
    } else {
        if (--work->field_3BC <= 0) {
            work->field_3BC = 1;
        }
        line->r0 = (work->field_3BC * 0x80) / 45;
        line->g0 = line->r0;
        line->b0 = line->r0;
        line->r1 = (work->field_3BC * 0xC0) / 45;
        line->g1 = line->r1;
        line->b1 = line->r1;
    }
    addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), line);
    page           = gGpuPrimCursor;
    gGpuPrimCursor = page + 1;
    setlen(page, 1);
    page->code[0] = 0xE1000620;
    addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), page);
    *(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += sizeof(Actor105500LineScratch);
}

static void Actor05500_Fn02FFC(GpEnemy* ctx, Task* actor)
{
    SVECTOR                rot;
    WorldCollisionContact* rec0;
    WorldCollisionContact* rec1;
    WorldCollisionContact* rec2;
    WorldCollisionContact* rec3;
    SVECTOR*               positions;
    MATRIX*                matrix;
    Actor105500Work*       work;
    s32                    variant;
    s32                    quotient;
    s32                    i;
    s32                    mode;
    AreaPlacement*         params;
    GfxCoord*              coord;
    TmdObject*             obj;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(Actor105500Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work         = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_1F4;
    obj->colorMtx       = &work->field_1D4;
    matrix              = &coord->coord;
    work->field_3C0     = 1;
    work->field_36C     = &Actor05500_D08ABC;
    ctx->field_4        = matrix;
    ctx->field_48       = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord                 = actor->extra.tmd->coords + 1;
    ctx->bodyPos.vy            = -0x64;
    ctx->recs                  = work->field_2B4;
    ctx->bodyPos.vx            = 0;
    ctx->bodyPos.vz            = 0;
    ctx->param                 = &Actor05500_D08970;
    ctx->hp                    = (s16)Actor05500_D08970.hpMax;
    work->field_354.coord      = coord;
    work->field_354.spawnArgLo = 0x100;
    work->field_354.spawnArgHi = 1;
    work->field_3C4            = (s16)ctx->place->variant;
    params                     = ctx->place;
    mode                       = params->mode;
    if (mode < 10) {
        switch (mode) {
            case 0:
                work->field_392 = 0xC;
                work->field_39A = 0;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 1:
                work->field_39A = mode;
                work->field_392 = mode;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 2:
                work->field_39A    = mode;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 0;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
            case 3:
                work->field_39A    = 2;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 1;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
        }
    } else {
        work->field_3C4 = (s16)params->variant;
        quotient        = mode / 10;
        variant         = mode - quotient * 10;
        switch (variant) {
            case 0:
                if (GameFlag_GetNibble(0xCC) == 1) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = Actor05500_D089D8[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor05500_D089B8;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                } else {
                    work->field_3C2 = 0;
                    work->field_39A = 8;
                    work->field_392 = 1;
                    work->field_3A8 = 0;
                }
                break;
            case 1:
                if (GameFlag_GetNibble(0xCB) == 2) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = Actor05500_D08A10[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor05500_D089F0;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                    break;
                }
                work->field_39A    = 8;
                work->field_3C2    = variant;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C8    = variant;
                coord->coord.t[1] += 0x3E8;
        }
    }
    func_800B3F84(&work->anim, Actor05500_D08AD4, obj, work->field_154, work->slots);
    for (i = 1; i < 8; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    rec0                             = work->field_234;
    work->field_214.coord            = coord;
    work->field_214.context.contacts = rec0;
    work->field_214.pos.vx           = 0;
    work->field_214.pos.vy           = -0x12C;
    work->field_214.pos.vz           = 0;
    work->field_214.key              = 0x30037;
    work->field_214.radius           = 0x12C;
    work->field_214.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_214);
    Gp_InitRec18Table(rec0, 4, 0);
    work->field_214.flags           |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->field_294.coord            = actor->extra.tmd->coords + 1;
    rec1                             = work->field_2B4;
    work->field_294.context.contacts = rec1;
    work->field_294.pos.vx           = 0;
    work->field_294.pos.vy           = -0x64;
    work->field_294.pos.vz           = 0;
    work->field_294.key              = 0x30037;
    work->field_294.radius           = 0x12C;
    work->field_294.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_294);
    Gp_InitRec18Table(rec1, 2, 0);
    work->field_294.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->field_2E4.coord            = actor->extra.tmd->coords + 4;
    rec2                             = work->field_304;
    work->field_2E4.context.contacts = rec2;
    work->field_2E4.pos.vx           = 0;
    work->field_2E4.pos.vy           = 0;
    work->field_2E4.pos.vz           = 0;
    work->field_2E4.key              = 0;
    work->field_2E4.radius           = 0xC8;
    work->field_2E4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->field_2E4);
    Gp_InitRec18Table(rec2, 1, 0);
    work->field_2E4.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->field_31C.coord            = actor->extra.tmd->coords + 4;
    rec3                             = work->field_33C;
    work->field_31C.context.contacts = rec3;
    work->field_31C.pos.vx           = 0;
    work->field_31C.pos.vy           = 0;
    work->field_31C.pos.vz           = 0;
    work->field_31C.key              = 0x22424;
    work->field_31C.radius           = 0x1F4;
    work->field_31C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->field_31C);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_31C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->state           = 1;
}

static void Actor05500_Fn03560(GpEnemy* arg0, Task* arg1)
{
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor105500Work* work;
    s32              state;
    s32              one;

    obj   = arg1->extra.tmd;
    state = Gp_StateF0.field_4;
    work  = arg1->work;
    coord = obj->coords;
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
    obj->flags               = 0;
    arg0->node.state.b.flags = 0;
    goto default_body;
case2:
    obj->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.b.flags = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor05500_Fn03674(arg1, obj, one);
    }
    Actor05500_Fn0006C(arg1);
    Actor05500_Fn0378C(arg1);
    if (work->field_3B0 != 0) {
        Actor05500_Fn020D4(arg1);
    }
    if (work->field_3A6 != 0) {
        Actor05500_Fn02214(arg1);
    }
    Actor05500_Fn03918(arg1);
    Actor05500_Fn039AC(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
case1:
    Actor05500_Fn03A70(arg1);
    Actor05500_Fn03AC8(arg1);
}

/// Applies what the actor's gameplay record reports. Flag 2 (cleared here)
/// sends the actor to action 7 and sets `field_3D2`; flags 4/8 bring the damage
/// `Gp_TickObjFlag4` returns, which is reported through `func_800DA6E8`,
/// taken off the hit points in `field_40`, and answered with action 9 and
/// `field_30 = 2` when they run out or action 6 otherwise. Neither reaction
/// happens while `field_3C8` is 1. Flags 4/8 are cleared once
/// `Gp_ObjFlag4Expired` reports them spent.
static void Actor05500_Fn03674(Task* arg0, TmdObject* arg1, s32 arg2)
{
    Actor105500Work* work;
    s32              damage;
    s32              remaining;
    u8               flags;
    GpEnemy*         ctx;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if ((flags & 2) && (work->field_3C8 != 1)) {
        ctx->reactionFlags = (u8)(flags & 0xFD);
        work->field_39A    = 7;
        work->field_39C    = 0;
        work->field_3D2    = 1;
    }
    if (ctx->reactionFlags & 0xC) {
        damage = Gp_TickObjFlag4(ctx);
        if ((s16)damage != 0) {
            func_800DA6E8(&ctx->node, (s16)damage, 0);
            remaining = (u16)ctx->hp - damage;
            ctx->hp   = remaining;
            if (work->field_3C8 != 1) {
                if ((s16)remaining <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                } else {
                    work->field_39A = 6;
                    work->field_39C = 0;
                }
            }
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags = (u8)(ctx->reactionFlags & 0xF3);
        }
    }
}

static void Actor05500_Fn0378C(Task* arg0)
{
    s16 state;

    state = ((Actor105500Work*)arg0->work)->field_39A;
    switch (state) {
        case 0:
            Actor05500_Fn00754(arg0);
            break;
        case 1:
            Actor05500_Fn00914(arg0);
            break;
        case 2:
            Actor05500_Fn00A94(arg0);
            break;
        case 3:
            Actor05500_Fn00FA0(arg0);
            break;
        case 4:
            Actor05500_Fn012E8(arg0);
            break;
        case 5:
            Actor05500_Fn0143C(arg0);
            break;
        case 6:
            Actor05500_Fn01A0C(arg0);
            break;
        case 7:
            Actor05500_Fn03864(arg0);
            break;
        case 8:
            Actor05500_Fn01B30(arg0);
            break;
        case 9:
            break;
    }
}

/// Action 7: plays animation 0xE until `Gp_TickObjFlag2` on the actor's
/// gameplay record returns non-zero, then returns to action 3 with animation 0xB,
/// `field_3D2` cleared and a fresh random count in `field_39E`.
static void Actor05500_Fn03864(Task* arg0)
{
    Actor105500Work* work;
    s16              state;
    u32              random;

    work  = arg0->work;
    state = work->field_39C;
    switch (state) {
        case 0:
            work->field_392 = 0xE;
            work->field_398 = 0;
            work->field_3A6 = 0;
            work->field_39C = 1;
            return;
        case 1:
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 0xB;
                work->field_3D2 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = (s16)((random >> 0x10) & 0xF);
            }
            return;
    }
}

static void Actor05500_Fn03918(Task* arg0)
{
    GfxCoord*        coord;
    Actor105500Work* work;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    work->field_35C.vx = coord->coord.t[0];
    work->field_35C.vy = coord->coord.t[1];
    work->field_35C.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_398) >> 12;
    coord->coord.t[1] += work->field_3A8;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_398) >> 12;
}

/// Out-of-line form of `_actor05500TickAnim`: switches the work's animation
/// id, or ticks every slot one frame when it is unchanged.
static void Actor05500_Fn039AC(Task* arg0)
{
    _actor05500TickAnim(arg0);
}

static void Actor05500_Fn03A70(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Draws the ground quad under the actor. In action 2 its position is cast
/// down by `func_800EA1A8`, and the quad is drawn there, shaded by
/// `func_800EA318`, only when that finds ground; in any other action it is
/// drawn at the root coordinate's world position at shade 0x80.
static void Actor05500_Fn03AC8(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    VECTOR3          vec;
    s16              hit;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_39A == 2) {
        hit = func_800EA1A8(MATRIX_TRANS(&coord->workm), &vec);
        if (hit != 0) {
            Gp_DrawEffGroundQuad(&vec, 0x200, func_800EA318(0x200, 0x80, hit));
        }
    } else {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&vec, 0x200, 0x80);
    }
}

/// Shrinks `field_3A0` by 0x50 per call while it is above 0x200, then rebuilds
/// the actor's root coordinate as `field_370` scaled vertically by
/// `field_3A0 / 0x1000`, through a 0x30-byte scratchpad block.
static void Actor05500_Fn03B60(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    Actor105500Work*   work;

    head                            = SCRATCH_HEAD(ActorScaleScratch);
    work                            = arg0->work;
    scratch                         = head - 1;
    SCRATCH_HEAD(ActorScaleScratch) = scratch;
    coord                           = arg0->extra.tmd->coords;
    if (work->field_3A0 >= 0x201) {
        work->field_3A0 = (u16)work->field_3A0 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_3A0;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_370;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

static void Actor05500_Fn03C54(Task* actor)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    u8               areaByte0;
    GpAreaVariant*   rec;
    AreaPlacement*   entry;
    GpEffWork*       eff;
    TmdObject*       model;
    s32              idx;
    u32              raw;

    D_80067704[0] = &Actor05500_D05F18;
    eff           = Gp_SpawnEff(0x40007, actor->extra.tmd->coords + 4, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = &gGameSession->location.loc;
    raw        = ((GpEnemy*)actor->spawnArg2.pointer)->placeKey;
    model      = eff->task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    areaSyncLocationVariant(&key);
    rec                      = Gp_GetNestedAreaRec(&key);
    entry                    = gpAreaPlaceAt(rec->field_0, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// Folds a uniform 1/16 scale into the model's third coordinate node, through a
/// 0x30-byte block borrowed from the scratchpad and released again: an identity
/// rotation is splatted word-wise, `ScaleMatrix` shrinks its diagonal to 0x100,
/// and `MulMatrix` multiplies the result into `field_8[2].coord`.
static void Actor05500_Fn03D40(Task* actor)
{
    void**             scratch;
    ActorScaleScratch* head;
    ActorScaleScratch* blk;
    GfxCoord*          coord;

    scratch                                     = SCRATCH_HEAD_ADDR;
    head                                        = SCRATCH_HEAD_AT(scratch, ActorScaleScratch);
    blk                                         = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleScratch) = blk;
    coord                                       = actor->extra.tmd->coords;

    blk->scale.vx          = 0x100;
    blk->scale.vy          = 0x100;
    blk->scale.vz          = 0x100;
    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;
    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord[2].coord, &blk->mat.mat);
    SCRATCH_POP_AT(scratch, ActorScaleScratch);
}

void Actor05500_Fn03DD8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05500_D0002C;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor05500_Fn03E34(GpEnemy* enemy, Task* task)
{
    Task*                        parent;
    TmdObject*                   parentObj;
    GfxCoord*                    coord;
    Actor105500Work*             parentWork;
    GfxCoord*                    parentCoord;
    ActorsShared80135c4cObjWork* work;
    u16                          pair;

    parent      = task->parent;
    parentObj   = parent->extra.tmd;
    coord       = task->extra.tmd->coords;
    parentWork  = (Actor105500Work*)parent->work;
    parentCoord = &parentObj->coords[4];
    work        = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work                 = work;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&gGfxViewCoord);
    parentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(parentCoord);
    coord->parent = &gGfxViewCoord;
    Gp_WorldToLocal(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    work->field_3A             = 0xC0;
    pair                       = parentWork->field_3AC;
    work->obj.coord            = coord;
    work->obj.pos.vx           = 0;
    work->obj.pos.vy           = 0;
    work->obj.pos.vz           = 0;
    work->obj.context.contacts = &work->rec;
    work->field_3C             = pair;
    work->obj.key              = Gp_PackPair(Actor05500_D08958, 2);
    work->obj.radius           = 0x100;
    work->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(&work->rec, 1, 0);
    work->obj.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state      = 1;
}

void Actor05500_Fn03F88(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05500_D00038;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2.pointer), arg0);
}
