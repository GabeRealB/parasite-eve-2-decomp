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
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
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
#include "../../shared/web_spider.h"

extern void* D_80067704[1];

extern TmdSource     gSpiderHuskModel;
extern DamageAttack  gSpiderAttacks[6];
extern EnemyParams   Actor05500_D08970;
extern u16           gSpiderIdleDelay[];
extern u16           Actor05500_D08990[];
extern u16           Actor05500_D089A0[];
extern s16           gSpiderLeapInDelay[];
extern SVECTOR       Actor05500_D089B8[];
extern s16           Actor05500_D089D8[];
extern s16           gSpiderDropInDelay[];
extern s16           gSpiderDropInSpeed[];
extern SVECTOR       Actor05500_D089F0[];
extern s16           Actor05500_D08A10[];
extern s16           gSpiderAnimBlend[];
extern s16           gSpiderSprayTail;
extern s16           gSpiderPounceLead;
extern s16           gSpiderPounceStride[][2];
extern s16           gSpiderReboundStride[][2];
extern ActorSpriteUv gSpiderPuffCells[];
extern s16           gSpiderPuffRadius[];
extern TaskDesc      Actor05500_D08ABC;
extern AnimationSet* Actor05500_D08AD4[15];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void Actor05500_Fn00754(Task* arg0);
static void Actor05500_Fn00914(Task* arg0);
static void Actor05500_Fn00A94(Task* arg0);
static void Actor05500_Fn00FA0(Task* arg0);
static void Actor05500_Fn02FFC(Enemy* ctx, Task* actor);

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
#define ACTOR05500_CONTACT_OVERLAP(out, coord, rec, delta)                                       \
    do {                                                                                         \
        s32 offX;                                                                                \
        s32 offY;                                                                                \
        s32 offZ;                                                                                \
        s32 clamped;                                                                             \
        offX            = (coord)->workm.t[0] - (rec).point.vx;                                  \
        (delta).vx.word = offX;                                                                  \
        offY            = (coord)->workm.t[1] - (rec).point.vy;                                  \
        (delta).vy.word = offY;                                                                  \
        offZ            = (coord)->workm.t[2] - (rec).point.vz;                                  \
        (delta).vz.word = offZ;                                                                  \
        (out)           = (rec).distance - SquareRoot0(offX * offX + offY * offY + offZ * offZ); \
        clamped         = (out);                                                                 \
        if ((out) <= 0) {                                                                        \
            clamped = 0;                                                                         \
        }                                                                                        \
        (out) = clamped;                                                                         \
    } while (0)

/// Normalises `delta` into `unit` and expresses the direction in the frame of
/// the collision grid, into `out`.
#define ACTOR05500_GRID_DIRECTION(delta, unit, out)                              \
    do {                                                                         \
        VectorNormal((VECTOR*)(delta), (unit));                                  \
        ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, (unit), (out)); \
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

TmdSource gSpiderHuskModel = {
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

DamageAttack gSpiderAttacks[6] = {
    { 10, 0 },
    { 16, 0 },
    { 8, 1 },
    { 12, 6 },
    { 18, 6 },
    { 8, 0 },
};

EnemyParams Actor05500_D08970 = { gSpiderAttacks, 80, 6, 28, 1, 100, 20, 100, 0 };

u16 gSpiderIdleDelay[8] = {
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

s16 gSpiderLeapInDelay[4] = {
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

s16 gSpiderDropInDelay[4] = {
    5,
    10,
    15,
    20,
};

s16 gSpiderDropInSpeed[4] = {
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

s16 gSpiderAnimBlend[3] = {
    0,
    10,
    0,
};

s16 gSpiderSprayTail = 8;

s16 gSpiderPounceLead = 8;

s32 Actor05500_D08A24[5] = {
    0,
    0,
    0,
    0,
    4,
};

s16 gSpiderPounceStride[9][2] = {
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

s16 gSpiderReboundStride[9][2] = {
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

ActorSpriteUv gSpiderPuffCells[8] = {
    { 0, 0, 0, 0 },
    { 32, 0, 0, 0 },
    { 64, 0, 0, 0 },
    { 96, 0, 0, 0 },
    { 0, 0, 32, 0 },
    { 32, 0, 32, 0 },
    { 64, 0, 32, 0 },
    { 96, 0, 32, 0 },
};

s16 gSpiderPuffRadius[14] = {
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

TaskDesc Actor05500_D08ABC = { { { TASK_BODY_TMD, 96 } }, Actor05500_Fn03F88, { .model = &Actor05500_D05774 } };

TaskDesc Actor05500_D08AC8 = { { { TASK_BODY_COORD, 96 } }, Actor05500_Fn03DD8, { .value = 0 } };

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

void spiderResolveContacts(Task* arg0)
{
    Actor105500Work*       work;
    Actor105500HitScratch* head;
    Actor105500HitScratch* scratch;
    Enemy*                 enemy;
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
    head    = SCRATCH_STACK_CURSOR(Actor105500HitScratch);
    scratch = SCRATCH_STACK_CURSOR(Actor105500HitScratch) = head - 1;
    enemy                                                 = (Enemy*)arg0->spawnArg2.pointer;
    work->field_3CC                                       = 0;
    result                                                = func_800E0C10(work->field_234, &scratch->delta, 4, NULL);
    if (result != 0) {
        if (work->field_39A == 2) {
            work->field_3CC = 1;
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += head[-1].delta.vx.halves.integer;
                coord->coord.t[1] += scratch->delta.vy.halves.integer;
                coord->coord.t[2] += scratch->delta.vz.halves.integer;
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
                src                    = gPlayerActorTasks[((u32)work->field_2B4[i].key.value >> 7) & 1]->extra.tmd->coords;
                dx                     = src->coord.t[0] - coord->coord.t[0];
                scratch->delta.vx.word = dx;
                dy                     = src->coord.t[1] - coord->coord.t[1];
                scratch->delta.vy.word = dy;
                dz                     = src->coord.t[2] - coord->coord.t[2];
                scratch->delta.vz.word = dz;
                damage                 = Gp_ComputeDamage((u32)work->field_2B4[i].key.value, SquareRoot0(dx * dx + dy * dy + dz * dz), 0, 0);
                amount                 = damage;
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
                            Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_TINT);
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
/// task's state: `spiderPuffSetup` sets up its collision object,
/// `spiderPuffTick` runs it, and `Gp_DestroyEnemy` tears it down.
static const GpEnemyTaskFuncTable3 Actor05500_D0002C = {
    {
        spiderPuffSetup,
        spiderPuffTick,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the task `Actor05500_Fn03F88` dispatches, indexed by the
/// task's state: `Actor05500_Fn02FFC` allocates and sets up the work block,
/// `spiderTick` runs the actor, and `spiderDyingState` handles its
/// last state.
static const GpEnemyTaskFuncTable3 Actor05500_D00038 = {
    {
        Actor05500_Fn02FFC,
        spiderTick,
        spiderDyingState,
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
            scratchEnd[-1].vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            delta->vy         = 0;
            dz                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            delta->vz         = dz;
            dx                = scratchEnd[-1].vx;
            if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) || (work->field_3D0 != 0) || (gSceneCombatState.signals.bytes.enemyAlert == 2)) {
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
                index           = ((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex;
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random;
                work->field_39E = gSpiderIdleDelay[index] + ((random >> 0x10) & 0xF);
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
    scratchEnd[-1].vx                                                         = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy                                                                 = 0;
    dz                                                                        = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
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
    s32              locationWord;
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
    locationWord                                                              = GAME_LOCATION_WORD(gGameSession->location.loc);
    value                                                                     = 0;
    switch (state) {
        case 0:
            if (work->field_3C6 == 0) {
                scratchEnd[-1].vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
                delta->vy         = 0;
                dz                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                delta->vz         = dz;
                dx                = scratchEnd[-1].vx;
                if (SquareRoot0((dx * dx) + (dz * dz)) < 0x5DC) {
                    value = 1;
                }
            }
            if ((value != 0) || (gSceneCombatState.spiderAmbushReady != 0) || (gSceneCombatState.expReward != 0)) {
                if (work->field_3C6 == 0) {
                    gSceneCombatState.spiderAmbushReady = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C        = 1;
                work->field_392        = 7;
                work->field_3A8        = Actor05500_D089A0[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->field_2E4.coord  = coord;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(gSpiderAttacks, 5);
                work->field_2E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55200006;
                    pan0  = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan0, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                work->field_370 = coord->workm;
                break;
            }
            if (work->field_3D0 != 0) {
                if (work->field_3C6 == 0) {
                    gSceneCombatState.spiderAmbushReady = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C        = 2;
                work->field_392        = 9;
                work->field_3A8        = Actor05500_D089A0[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->field_3BC        = 0x2D;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.coord  = coord;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(gSpiderAttacks, 5);
                work->field_2E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55200006;
                    pan1  = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan1, (s8)worldCoordGetOriginAudioDepth(coord));
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
            if (((gPlayerStatus.coordMtx->t[1] - 0x3E8) < coord->coord.t[1]) || (work->field_3D0 != 0) || (work->field_3CE != 0)) {
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
                sound                  = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan2                   = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 >= 0x1E) {
                Gp_ArmStateF0(1);
                work->field_39A        = state;
                work->field_39C        = 0;
                work->field_392        = 1;
                work->field_39E        = gSpiderIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + (((gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF);
                work->field_2E4.coord  = actor->extra.tmd->coords + 4;
                work->field_2E4.radius = 0xC8;
                work->field_2E4.pos.vy = 0;
                work->field_3C8        = 0;
                if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    actor->state    = 2;
                }
            }
            break;
    }
    spiderDrawThread(actor);
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
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->field_39E = Actor05500_D08990[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0x3FF);
                gRandomLcgState = random;
                return;
            }
            return;
        case 1:
            scratchEnd[-1].delta.vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            delta->delta.vy         = 0;
            delta->delta.vz         = (s32)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
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
                random2         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->field_39E = gSpiderIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random2 >> 0x10) & 0xF);
                gRandomLcgState = random2;
                return;
            }
            if ((s16)work->field_396 == 0xC) {
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0001;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((s16)work->field_396 >= 0x29) {
                work->field_396 = 0xB;
            }
            if (work->field_3A4 == work->field_3A2) {
                scratchEnd[-1].delta.vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
                delta->delta.vy         = 0;
                dz                      = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                delta->delta.vz         = dz;
                dx                      = scratchEnd[-1].delta.vx;
                distance                = SquareRoot0((dx * dx) + (dz * dz));
                if ((work->field_3C0 == 0) && (distance < 0x578) && (work->field_3B0 == 0) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
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

#include "../../shared/web_spider_spray.inc.c"

#include "../../shared/web_spider_pounce.inc.c"

#include "../../shared/web_spider_hurt.inc.c"

#include "../../shared/web_spider_entrance.inc.c"

#include "../../shared/web_spider_burn.inc.c"

#include "../../shared/web_spider_turn.inc.c"

#include "../../shared/web_spider_inlines.inc.c"

#include "../../shared/web_spider_dying.inc.c"

#include "../../shared/web_spider_puff_tick.inc.c"

#include "../../shared/web_spider_draw_puff.inc.c"

#include "../../shared/web_spider_draw_thread.inc.c"

static void Actor05500_Fn02FFC(Enemy* ctx, Task* actor)
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

#include "../../shared/web_spider_tick.inc.c"

#include "../../shared/web_spider_status.inc.c"

void spiderRunBehaviour(Task* arg0)
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
            spiderSprayState(arg0);
            break;
        case 5:
            spiderPounceState(arg0);
            break;
        case 6:
            spiderHurtState(arg0);
            break;
        case 7:
            spiderStunState(arg0);
            break;
        case 8:
            spiderEntranceState(arg0);
            break;
        case 9:
            break;
    }
}

#include "../../shared/web_spider_stun.inc.c"

#include "../../shared/web_spider_move.inc.c"

/// Out-of-line form of `spiderTickAnimInline`: switches the work's animation
/// id, or ticks every slot one frame when it is unchanged.
void spiderTickAnim(Task* arg0)
{
    spiderTickAnimInline(arg0);
}

void spiderUpdateColor(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

#include "../../shared/web_spider_shadow.inc.c"

#include "../../shared/web_spider_squash.inc.c"

#include "../../shared/web_spider_husk.inc.c"

#include "../../shared/web_spider_shrink_node.inc.c"

void Actor05500_Fn03DD8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05500_D0002C;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

#include "../../shared/web_spider_puff_setup.inc.c"

void Actor05500_Fn03F88(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05500_D00038;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}
