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

extern ActorSpriteUv Actor02600_D08A78[];
extern s16           Actor02600_D08A98[];

extern EnemyParams   Actor02600_D08968;
extern SVECTOR       Actor02600_D089B0[];
extern s16           Actor02600_D089D0[];
extern SVECTOR       Actor02600_D089E8[];
extern s16           Actor02600_D08A08[];
extern TaskDesc      Actor02600_D08AB4;
extern AnimationSet* Actor02600_D08ACC[15];
extern u16           Actor02600_D08978[];
extern u16           Actor02600_D08988[];
extern u16           Actor02600_D08998[];
extern s16           Actor02600_D089A8[];
extern s16           Actor02600_D089D8[];
extern s16           Actor02600_D089E0[];
extern s16           Actor02600_D08A16;
extern s16           Actor02600_D08A18;
extern s16           Actor02600_D08A30[][2];
extern s16           Actor02600_D08A54[][2];
extern DamageAttack  Actor02600_D08950[6];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/* Animation id -> slot blend value table in this overlay's own data. */
extern s16 Actor02600_D08A10[];

/* `D_80067704` is the third word of a `D_800676A8` record: it selects the model
 * stream the next `Gp_SpawnEff` uses for the effect's own `TmdObject`. Declared
 * as a one-element array so GCC 2.8.1 cannot treat the store as non-aliasing
 * with the struct traffic that follows and sink it past the loads. */
extern void* D_80067704[1];

/* Model stream in this overlay's own data. */
extern TmdSource Actor02600_D05F10;

static void Actor02600_Fn02364(Enemy* arg0, Task* arg1);
static void Actor02600_Fn02780(Enemy* arg0, Task* arg1);
static void Actor02600_Fn02954(Task* actor, s32 frame);
static void Actor02600_Fn02C94(Task* actor);
static void Actor02600_Fn02FFC(Enemy* ctx, Task* actor);
static void Actor02600_Fn03558(Enemy* arg0, Task* arg1);
static void Actor02600_Fn0366C(Task* arg0);
static void Actor02600_Fn03784(Task* arg0);
static void Actor02600_Fn0385C(Task* arg0);
static void Actor02600_Fn03910(Task* arg0);
static void Actor02600_Fn039A4(Task* arg0);
static void Actor02600_Fn03A68(Task* arg0);
static void Actor02600_Fn03AC0(Task* arg0);
static void Actor02600_Fn03B58(Task* arg0);
static void Actor02600_Fn03C4C(Task* actor);
static void Actor02600_Fn03D38(Task* actor);
static void Actor02600_Fn03E2C(Enemy* enemy, Task* task);

/// Stores the yaw that `coord`'s frame faces in `work->field_3A2`, then
/// rebuilds the frame's rotation as a level turn half a revolution away from
/// it, with `rot` holding the angles.
#define ACTOR02600_TURN_AROUND(work, coord, rot)                                            \
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
#define ACTOR02600_CONTACT_OVERLAP(out, coord, rec, delta)                                    \
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
#define ACTOR02600_GRID_DIRECTION(delta, unit, out)                            \
    do {                                                                       \
        VectorNormal((VECTOR*)(delta), (unit));                                \
        ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (unit), (out)); \
    } while (0)

/// Sets `work->field_3CE` when contact `rec` is a body, or a face of the
/// collision grid whose normal has no vertical component.
#define ACTOR02600_NOTE_BLOCKING_CONTACT(work, rec)                                              \
    do {                                                                                         \
        if ((((rec).key.value & 0xFFFF0000) == 0x10000) ||                                       \
            ((((rec).key.value & 0xFFFF0000) == 0x100000) && ((rec).response.normal.vy == 0))) { \
            (work)->field_3CE = 1;                                                               \
        }                                                                                        \
    } while (0)

extern AnimationSet Actor02600_D06198;
extern AnimationSet Actor02600_D0644C;
extern AnimationSet Actor02600_D068D0;
extern AnimationSet Actor02600_D06E14;
extern AnimationSet Actor02600_D074DC;
extern AnimationSet Actor02600_D077A4;
extern AnimationSet Actor02600_D07934;
extern AnimationSet Actor02600_D07B48;
extern AnimationSet Actor02600_D07E88;
extern AnimationSet Actor02600_D08140;
extern AnimationSet Actor02600_D083EC;
extern AnimationSet Actor02600_D08778;
extern AnimationSet Actor02600_D08928;
extern TmdSource    Actor02600_D0576C;
void                Actor02600_Fn03DD0(Task*);
void                Actor02600_Fn03F80(Task*);

TmdBone Actor02600_D03FDC[8] = {
#include "assets/actor_102600_model_0576C_skeleton.inc"
};

u32 Actor02600_D040FC[8] = {
#include "assets/actor_102600_model_0576C_partVerts.inc"
};

SVECTOR Actor02600_D0411C[101] = {
#include "assets/actor_102600_model_0576C_verts.inc"
};

SVECTOR Actor02600_D04444[107] = {
#include "assets/actor_102600_model_0576C_normals.inc"
};

u32 Actor02600_D0479C[1012] = {
#include "assets/actor_102600_model_0576C_stream.inc"
};

TmdSource Actor02600_D0576C = {
    0,
    5400,
    1872,
    8,
    Actor02600_D040FC,
    Actor02600_D0411C,
    Actor02600_D04444,
    Actor02600_D03FDC,
    Actor02600_D0479C,
};

TmdBone Actor02600_D05790[1] = {
#include "assets/actor_102600_model_05F10_skeleton.inc"
};

u32 Actor02600_D057B4[1] = {
#include "assets/actor_102600_model_05F10_partVerts.inc"
};

SVECTOR Actor02600_D057B8[40] = {
#include "assets/actor_102600_model_05F10_verts.inc"
};

SVECTOR Actor02600_D058F8[40] = {
#include "assets/actor_102600_model_05F10_normals.inc"
};

u32 Actor02600_D05A38[310] = {
#include "assets/actor_102600_model_05F10_stream.inc"
};

TmdSource Actor02600_D05F10 = {
    0,
    2144,
    0,
    1,
    Actor02600_D057B4,
    Actor02600_D057B8,
    Actor02600_D058F8,
    Actor02600_D05790,
    Actor02600_D05A38,
};

AnimationPackedPose Actor02600_D05F34[10] = {
#include "assets/actor_102600_animation_06198_bank1.inc"
};

AnimationPackedRotation Actor02600_D05FAC[45] = {
#include "assets/actor_102600_animation_06198_bank4.inc"
};

AnimationRecord Actor02600_D06060[74] = {
#include "assets/actor_102600_animation_06198_records.inc"
};

u16 Actor02600_D06188[8] = {
#include "assets/actor_102600_animation_06198_indices.inc"
};

AnimationSet Actor02600_D06198 = {
    Actor02600_D06060,
    Actor02600_D06188,
    { NULL, Actor02600_D05F34, NULL, NULL, Actor02600_D05FAC, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D061C0[11] = {
#include "assets/actor_102600_animation_0644C_bank1.inc"
};

AnimationPackedRotation Actor02600_D06244[44] = {
#include "assets/actor_102600_animation_0644C_bank4.inc"
};

AnimationRecord Actor02600_D062F4[82] = {
#include "assets/actor_102600_animation_0644C_records.inc"
};

u16 Actor02600_D0643C[8] = {
#include "assets/actor_102600_animation_0644C_indices.inc"
};

AnimationSet Actor02600_D0644C = {
    Actor02600_D062F4,
    Actor02600_D0643C,
    { NULL, Actor02600_D061C0, NULL, NULL, Actor02600_D06244, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D06474[13] = {
#include "assets/actor_102600_animation_068D0_bank1.inc"
};

AnimationPackedRotation Actor02600_D06510[96] = {
#include "assets/actor_102600_animation_068D0_bank4.inc"
};

AnimationRecord Actor02600_D06690[140] = {
#include "assets/actor_102600_animation_068D0_records.inc"
};

u16 Actor02600_D068C0[8] = {
#include "assets/actor_102600_animation_068D0_indices.inc"
};

AnimationSet Actor02600_D068D0 = {
    Actor02600_D06690,
    Actor02600_D068C0,
    { NULL, Actor02600_D06474, NULL, NULL, Actor02600_D06510, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D068F8[31] = {
#include "assets/actor_102600_animation_06E14_bank1.inc"
};

AnimationPackedRotation Actor02600_D06A6C[82] = {
#include "assets/actor_102600_animation_06E14_bank4.inc"
};

AnimationRecord Actor02600_D06BB4[148] = {
#include "assets/actor_102600_animation_06E14_records.inc"
};

u16 Actor02600_D06E04[8] = {
#include "assets/actor_102600_animation_06E14_indices.inc"
};

AnimationSet Actor02600_D06E14 = {
    Actor02600_D06BB4,
    Actor02600_D06E04,
    { NULL, Actor02600_D068F8, NULL, NULL, Actor02600_D06A6C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D06E3C[27] = {
#include "assets/actor_102600_animation_074DC_bank1.inc"
};

AnimationPackedRotation Actor02600_D06F80[148] = {
#include "assets/actor_102600_animation_074DC_bank4.inc"
};

AnimationRecord Actor02600_D071D0[191] = {
#include "assets/actor_102600_animation_074DC_records.inc"
};

u16 Actor02600_D074CC[8] = {
#include "assets/actor_102600_animation_074DC_indices.inc"
};

AnimationSet Actor02600_D074DC = {
    Actor02600_D071D0,
    Actor02600_D074CC,
    { NULL, Actor02600_D06E3C, NULL, NULL, Actor02600_D06F80, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D07504[11] = {
#include "assets/actor_102600_animation_077A4_bank1.inc"
};

AnimationPackedRotation Actor02600_D07588[51] = {
#include "assets/actor_102600_animation_077A4_bank4.inc"
};

AnimationRecord Actor02600_D07654[80] = {
#include "assets/actor_102600_animation_077A4_records.inc"
};

u16 Actor02600_D07794[8] = {
#include "assets/actor_102600_animation_077A4_indices.inc"
};

AnimationSet Actor02600_D077A4 = {
    Actor02600_D07654,
    Actor02600_D07794,
    { NULL, Actor02600_D07504, NULL, NULL, Actor02600_D07588, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D077CC[6] = {
#include "assets/actor_102600_animation_07934_bank1.inc"
};

AnimationPackedRotation Actor02600_D07814[26] = {
#include "assets/actor_102600_animation_07934_bank4.inc"
};

AnimationRecord Actor02600_D0787C[42] = {
#include "assets/actor_102600_animation_07934_records.inc"
};

u16 Actor02600_D07924[8] = {
#include "assets/actor_102600_animation_07934_indices.inc"
};

AnimationSet Actor02600_D07934 = {
    Actor02600_D0787C,
    Actor02600_D07924,
    { NULL, Actor02600_D077CC, NULL, NULL, Actor02600_D07814, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D0795C[9] = {
#include "assets/actor_102600_animation_07B48_bank1.inc"
};

AnimationPackedRotation Actor02600_D079C8[34] = {
#include "assets/actor_102600_animation_07B48_bank4.inc"
};

AnimationRecord Actor02600_D07A50[58] = {
#include "assets/actor_102600_animation_07B48_records.inc"
};

u16 Actor02600_D07B38[8] = {
#include "assets/actor_102600_animation_07B48_indices.inc"
};

AnimationSet Actor02600_D07B48 = {
    Actor02600_D07A50,
    Actor02600_D07B38,
    { NULL, Actor02600_D0795C, NULL, NULL, Actor02600_D079C8, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D07B70[13] = {
#include "assets/actor_102600_animation_07E88_bank1.inc"
};

AnimationPackedRotation Actor02600_D07C0C[58] = {
#include "assets/actor_102600_animation_07E88_bank4.inc"
};

AnimationRecord Actor02600_D07CF4[97] = {
#include "assets/actor_102600_animation_07E88_records.inc"
};

u16 Actor02600_D07E78[8] = {
#include "assets/actor_102600_animation_07E88_indices.inc"
};

AnimationSet Actor02600_D07E88 = {
    Actor02600_D07CF4,
    Actor02600_D07E78,
    { NULL, Actor02600_D07B70, NULL, NULL, Actor02600_D07C0C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D07EB0[9] = {
#include "assets/actor_102600_animation_08140_bank1.inc"
};

AnimationPackedRotation Actor02600_D07F1C[53] = {
#include "assets/actor_102600_animation_08140_bank4.inc"
};

AnimationRecord Actor02600_D07FF0[80] = {
#include "assets/actor_102600_animation_08140_records.inc"
};

u16 Actor02600_D08130[8] = {
#include "assets/actor_102600_animation_08140_indices.inc"
};

AnimationSet Actor02600_D08140 = {
    Actor02600_D07FF0,
    Actor02600_D08130,
    { NULL, Actor02600_D07EB0, NULL, NULL, Actor02600_D07F1C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D08168[9] = {
#include "assets/actor_102600_animation_083EC_bank1.inc"
};

AnimationPackedRotation Actor02600_D081D4[49] = {
#include "assets/actor_102600_animation_083EC_bank4.inc"
};

AnimationRecord Actor02600_D08298[81] = {
#include "assets/actor_102600_animation_083EC_records.inc"
};

u16 Actor02600_D083DC[8] = {
#include "assets/actor_102600_animation_083EC_indices.inc"
};

AnimationSet Actor02600_D083EC = {
    Actor02600_D08298,
    Actor02600_D083DC,
    { NULL, Actor02600_D08168, NULL, NULL, Actor02600_D081D4, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D08414[20] = {
#include "assets/actor_102600_animation_08778_bank1.inc"
};

AnimationPackedRotation Actor02600_D08504[61] = {
#include "assets/actor_102600_animation_08778_bank4.inc"
};

AnimationRecord Actor02600_D085F8[92] = {
#include "assets/actor_102600_animation_08778_records.inc"
};

u16 Actor02600_D08768[8] = {
#include "assets/actor_102600_animation_08778_indices.inc"
};

AnimationSet Actor02600_D08778 = {
    Actor02600_D085F8,
    Actor02600_D08768,
    { NULL, Actor02600_D08414, NULL, NULL, Actor02600_D08504, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D087A0[5] = {
#include "assets/actor_102600_animation_08928_bank1.inc"
};

AnimationPackedRotation Actor02600_D087DC[19] = {
#include "assets/actor_102600_animation_08928_bank4.inc"
};

AnimationRecord Actor02600_D08828[60] = {
#include "assets/actor_102600_animation_08928_records.inc"
};

u16 Actor02600_D08918[8] = {
#include "assets/actor_102600_animation_08928_indices.inc"
};

AnimationSet Actor02600_D08928 = {
    Actor02600_D08828,
    Actor02600_D08918,
    { NULL, Actor02600_D087A0, NULL, NULL, Actor02600_D087DC, NULL, NULL, NULL },
};

DamageAttack Actor02600_D08950[6] = {
    { 14, 0 },
    { 20, 0 },
    { 10, 1 },
    { 16, 6 },
    { 24, 6 },
    { 10, 0 },
};

EnemyParams Actor02600_D08968 = { Actor02600_D08950, 160, 16, 68, 1, 100, 10, 100, 5 };

u16 Actor02600_D08978[8] = {
    25,
    10,
    10,
    10,
    10,
    10,
    10,
    35,
};

u16 Actor02600_D08988[8] = {
    1600,
    2400,
    2400,
    2400,
    2400,
    2400,
    2400,
    1600,
};

u16 Actor02600_D08998[8] = {
    100,
    100,
    180,
    130,
    100,
    100,
    100,
    100,
};

s16 Actor02600_D089A8[4] = {
    0,
    2,
    4,
    6,
};

SVECTOR Actor02600_D089B0[4] = {
    { -3000, 0, -600, 0 },
    { -3000, 0, -1600, 0 },
    { -2000, 0, -600, 0 },
    { -2000, 0, -1600, 0 },
};

s16 Actor02600_D089D0[4] = {
    0,
    1024,
    2048,
    3072,
};

s16 Actor02600_D089D8[4] = {
    5,
    10,
    15,
    20,
};

s16 Actor02600_D089E0[4] = {
    100,
    100,
    100,
    100,
};

SVECTOR Actor02600_D089E8[4] = {
    { -7000, 0, -7600, 0 },
    { -5600, 0, -7500, 0 },
    { -4700, 0, -6500, 0 },
    { -5600, 0, -5600, 0 },
};

s16 Actor02600_D08A08[4] = {
    600,
    1024,
    2048,
    1800,
};

s16 Actor02600_D08A10[3] = {
    0,
    10,
    0,
};

s16 Actor02600_D08A16 = 8;

s16 Actor02600_D08A18 = 8;

s32 Actor02600_D08A1C[5] = {
    0,
    0,
    0,
    0,
    4,
};

s16 Actor02600_D08A30[9][2] = {
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

s16 Actor02600_D08A54[9][2] = {
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

ActorSpriteUv Actor02600_D08A78[8] = {
    { 0, 0, 0, 0 },
    { 32, 0, 0, 0 },
    { 64, 0, 0, 0 },
    { 96, 0, 0, 0 },
    { 0, 0, 32, 0 },
    { 32, 0, 32, 0 },
    { 64, 0, 32, 0 },
    { 96, 0, 32, 0 },
};

s16 Actor02600_D08A98[14] = {
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

TaskDesc Actor02600_D08AB4 = { TASK_BODY_TMD, 96, Actor02600_Fn03F80, { .model = &Actor02600_D0576C } };

TaskDesc Actor02600_D08AC0 = { TASK_BODY_COORD, 96, Actor02600_Fn03DD0, { .model = NULL } };

AnimationSet* Actor02600_D08ACC[15] = {
    NULL,
    &Actor02600_D06198,
    &Actor02600_D0644C,
    &Actor02600_D068D0,
    &Actor02600_D06E14,
    &Actor02600_D074DC,
    &Actor02600_D077A4,
    &Actor02600_D07934,
    NULL,
    &Actor02600_D07B48,
    &Actor02600_D07E88,
    &Actor02600_D08140,
    &Actor02600_D083EC,
    &Actor02600_D08778,
    &Actor02600_D08928,
};

static void        Actor02600_Fn0006C(Task* arg0);
static void        Actor02600_Fn00754(Task* arg0);
static void        Actor02600_Fn00914(Task* arg0);
static void        Actor02600_Fn00A94(Task* actor);
static void        Actor02600_Fn00FA0(Task* arg0);
static void        Actor02600_Fn012E8(Task* arg0);
static void        Actor02600_Fn0143C(Task* arg0);
static void        Actor02600_Fn01A0C(Task* arg0);
static void        Actor02600_Fn01B30(Task* arg0);
static void        Actor02600_Fn020D4(Task* arg0);
static void        Actor02600_Fn02214(Task* arg0);
static inline void _actor02600TickAnim(Task* task);

static void Actor02600_Fn0006C(Task* arg0)
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
                src                 = gPlayerActorTasks[((u32)work->field_2B4[i].key.value >> 7) & 1]->extra.tmd->coords;
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
                        ACTOR02600_TURN_AROUND(work, coord, &scratch->rot);
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
        ACTOR02600_CONTACT_OVERLAP(push, coord, work->field_2B4[i], scratch->delta);
        if (best < push) {
            best = push;
            ACTOR02600_GRID_DIRECTION(&scratch->delta, unit, &scratch->local);
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
        ACTOR02600_NOTE_BLOCKING_CONTACT(work, work->field_304[0]);
        work->field_2E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        Gp_ClearRec18Occupied(work->field_304);
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor105500HitScratch);
}

/// State handlers of the projectile task `Actor02600_Fn03DD0` dispatches,
/// indexed by `Task::state`: setup, per-frame tick and `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 Actor02600_D0002C = {
    {
        Actor02600_Fn03E2C,
        Actor02600_Fn02780,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the actor task `Actor02600_Fn03F80` dispatches, indexed
/// by `Task::state`: spawn, per-frame tick and the dying sequence.
static const GpEnemyTaskFuncTable3 Actor02600_D00038 = {
    {
        Actor02600_Fn02FFC,
        Actor02600_Fn03558,
        Actor02600_Fn02364,
    },
};

static void Actor02600_Fn00754(Task* arg0)
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
            if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x9C4) || (work->field_3D0 != 0) || (Gp_StateF0.prefix.bytes.field_3 == 2)) {
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
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = Actor02600_D08978[index] + ((random >> 0x10) & 0xF);
            }
            break;
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}

static void Actor02600_Fn00914(Task* arg0)
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
    if ((distance < 0x9C4) && (difference < 0x80)) {
        work->field_39A = 5;
        work->field_39C = 0;
        work->field_392 = 4;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}

static void Actor02600_Fn00A94(Task* actor)
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
                scratchEnd[-1].vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
                delta->vy         = 0;
                dz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                delta->vz         = dz;
                dx                = scratchEnd[-1].vx;
                if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
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
                work->field_3A8        = Actor02600_D08998[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->field_2E4.coord  = coord;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(Actor02600_D08950, 5);
                work->field_2E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55200006;
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
                work->field_3A8        = Actor02600_D08998[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->field_3BC        = 0x2D;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.coord  = coord;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(Actor02600_D08950, 5);
                work->field_2E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55200006;
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
                sound                  = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
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
                work->field_39E        = Actor02600_D08978[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + (((Gp_LcgState = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
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
    Actor02600_Fn02C94(actor);
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}

static void Actor02600_Fn00FA0(Task* arg0)
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
                work->field_39E = Actor02600_D08988[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0x3FF);
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
                work->field_39E = Actor02600_D08978[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random2 >> 0x10) & 0xF);
                Gp_LcgState     = random2;
                return;
            }
            if ((s16)work->field_396 == 0xC) {
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0001;
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
                if ((work->field_3C0 == 0) && (distance < 0x578) && (work->field_3B0 == 0) && !(Player_Status.statusFlags & PLAYER_STATUS_DARKNESS)) {
                    work->field_39A = 4;
                    work->field_39C = 0;
                    work->field_392 = 3;
                    work->field_3AC = 0;
                } else if (distance < 0x9C4) {
                    work->field_39A = 5;
                    work->field_39C = 0;
                    work->field_392 = 4;
                }
            }
            break;
    }
}

static void Actor02600_Fn012E8(Task* arg0)
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
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0003;
        pan   = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
    }
    if ((u32)(work->field_396 - 0x2B) < 7U) {
        Gp_SpawnEnemyFromTable(work->field_36C, 1, 0, arg0->spawnArg2.pointer);
        work->field_3AC = (u16)(work->field_3AC + 1);
    }
    if ((s16)work->field_396 >= (Actor02600_D08A16 + 0x3C)) {
        work->field_39A = 3;
        work->field_39C = 0;
        work->field_392 = 1;
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_39E = Actor02600_D08978[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
        Gp_LcgState     = random;
    }
}

static void Actor02600_Fn0143C(Task* arg0)
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

    rotation = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
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
                work->field_2E4.key = Gp_PackPair(Actor02600_D08950, index);
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
            motion0 = Actor02600_D08A30;
            for (; index < 9; index++, motion0++) {
                if ((s16)work->field_396 <= (motion0[0][0] + Actor02600_D08A18)) {
                    coord->coord.t[0] += (s32)(motion0[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion0[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((s16)work->field_396 == 0x28) {
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
                work->field_3C8 = 0;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                }
            }
            if ((s16)work->field_396 >= (Actor02600_D08A18 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor02600_D08978[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
                Gp_LcgState     = random;
            }
            break;
        case 1:
            index           = 0;
            motion1         = Actor02600_D08A54;
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
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan1  = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan1, (s8)gpGetObjDepth(coord));
                work->field_3C8 = 2;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp <= 0) {
                    work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx    = 0;
                    rotation->vy    = (u16)work->field_3A2 + 0x800;
                    rotation->vz    = 0;
                    RotMatrix(rotation, &coord->coord);
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                }
            }
            if ((s16)work->field_396 >= 0x46) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                work->field_39E = Actor02600_D08978[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + (((Gp_LcgState = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
                work->field_3AA = 1;
                if (work->field_3C8 == 2) {
                    work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx    = 0;
                    rotation->vy    = (u16)work->field_3A2 + 0x800;
                    rotation->vz    = 0;
                    RotMatrix(rotation, &coord->coord);
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

/// Behaviour state 6, entered when a hit does damage. On entry it starts
/// animation 0xB, stops the forward and turn steps and plays sound
/// 0x401A0004 with the top nibble of the context's `field_8` in bits 8-11,
/// panned to the actor. Once the
/// animation has run 0x15 frames it goes to state 7 when `field_3D2` is 1,
/// otherwise to state 3 with animation 1 and a random 0..15 in `field_39E`.
static void Actor02600_Fn01A0C(Task* arg0)
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
            sound           = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0004;
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

static void Actor02600_Fn01B30(Task* arg0)
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
    Enemy*   ctx;
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
            work->field_294.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_214.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            obj->flags                  = (u16)obj->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            ctx->node.state.parts.flags = one;
            if (Gp_StateF0.field_1E == one) {
                if (work->field_3C2 == 0) {
                    work->field_39E = Actor02600_D089A8[work->field_3C4];
                } else {
                    work->field_39E = Actor02600_D089D8[work->field_3C4];
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
                work->field_3A8        = Actor02600_D089E0[work->field_3C4];
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
                indexOrSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x510D0012;
                pan1         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan1, (s8)gpGetObjDepth(coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 == 0x1E) {
                indexOrSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51090007;
                pan2         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan2, (s8)gpGetObjDepth(coord));
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
            motion = Actor02600_D08A30;
            do {
                indexOrSound++;
                if ((s16)work->field_396 <= ((*motion)[0] + Actor02600_D08A18)) {
                    coord->coord.t[0] += ((*motion)[1] * rsin(work->field_3A2)) >> 12;
                    coord->coord.t[2] += ((*motion)[1] * rcos(work->field_3A2)) >> 12;
                    break;
                }
                motion++;
            } while (indexOrSound < 9);
            if ((s16)work->field_396 == 0x28) {
                indexOrSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan3         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan3, (s8)gpGetObjDepth(coord));
                work->field_3A8        = 0x80;
                work->field_294.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->field_214.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            }
            if ((s16)work->field_396 >= (Actor02600_D08A18 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                randomDelay     = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor02600_D08978[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((randomDelay >> 0x10) & 0xF);
                Gp_LcgState     = randomDelay;
                Gp_ArmStateF0(1);
            }
            break;
    }
    *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}

/// Status-effect step, run every frame while `field_3B0` is set. `field_3B2`
/// cycles through 0x50 frames; every 12 frames effect 3 is spawned, alternating
/// between model nodes 3 and 5; every 0x24 frames sound 0x401A0005 is played
/// with the top nibble of the context's `field_8` in bits 8-11, panned to the
/// actor.
static void Actor02600_Fn020D4(Task* arg0)
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
        sound           = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0005;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(coord));
    }
}

/// Turn step, run every frame while `field_3A6` is non-zero: reads the
/// heading back from the coordinate, turns it toward `field_3A4` by at most
/// `field_3A6` the shorter way round the circle, keeps the result in
/// `field_3A2` and rebuilds the coordinate's rotation as that pure yaw.
static void Actor02600_Fn02214(Task* arg0)
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

    sc    = (ActorFaceScratch*)SCRATCH_STACK_RESERVE_BYTES(0x18);
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
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

/// Switches the work's animation id, resetting the slots to the blend value the
/// table gives for the new id; otherwise ticks every slot one frame.
static inline void _actor02600TickAnim(Task* task)
{
    Actor105500Work* work;
    s32              i;
    s32              value;

    work = task->work;
    if (work->field_392 != work->field_394) {
        work->field_394 = work->field_392;
        work->field_396 = 0;
        value           = Actor02600_D08A10[work->field_392];
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

/// Dying sequence, the actor task's third state. While the global mode is 1
/// only the colour is updated, and mode 2 sets the model's `field_C` to 0x80.
/// Otherwise the work's `field_39C` steps: step 0 saves the coordinate in
/// `field_370` for the squash, unlinks the context node and the work's four
/// collision objects, passes 0x37 (0x1A when `field_3C0` is clear) to
/// `Gp_ReleaseStateF0Add` and starts animation 0xB; step 1 squashes the model
/// (and, once `field_3BA` has passed 1, frees its buffers and spawns the model
/// effect of `Actor02600_Fn03C4C` in its place), spawns effect 0x600A5 at frame
/// 0xF and moves to step 2 at frame 0x3C; step 2 destroys the enemy 0x3C
/// frames later.
static void Actor02600_Fn02364(Enemy* arg0, Task* arg1)
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
                    Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
                    if (work->field_3BA != 0) {
                        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->field_392 = 0xB;
                    _actor02600TickAnim(arg1);
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
                            Actor02600_Fn03C4C(arg1);
                            Actor02600_Fn03D38(arg1);
                        } else {
                            work->field_3BA++;
                        }
                    }
                    Actor02600_Fn03B58(arg1);
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
                    _actor02600TickAnim(arg1);
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

/// Per-frame tick of the homing projectile: while the global mode is 1 the
/// frame is just drawn, in mode 2 nothing happens at all, and otherwise the
/// work is stepped. A live collision record whose kind is not 0x10 drops the
/// object's 0x8000 linked bit and wipes the record, which sends the tick
/// straight past the frame counter. Every other frame the work's flags mirror
/// the low two bits of the counter, the coordinate is advanced along its own
/// forward axis by `field_3A`, and the counter is bumped; at 0xF frames the
/// object is unlinked and the actor switches to state 2, otherwise `field_3A`
/// decays by an LCG-derived 0..0x1F and clamps at zero.
static void Actor02600_Fn02780(Enemy* arg0, Task* arg1)
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
            Actor02600_Fn02954(arg1, work->field_38);
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
            Actor02600_Fn02954(arg1, work->field_38);
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

/// Projects one frame's sprite into the scratch quad and, when the depth
/// clears the near plane, emits the semi-transparent `POLY_FT4` for it. The
/// model whose texture is drawn is the *parent* task's (`Task::parent`), not
/// this actor's, so the atlas and tpage come from whoever spawned it.
static void Actor02600_Fn02954(Task* actor, s32 frame)
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
        radius                 = (s32)(Actor02600_D08A98[frame] * 0x300) / depth;
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
        uv          = &Actor02600_D08A78[frame >> 1];
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

/// Draws a vertical semi-transparent gouraud line in the space of the matrix
/// `field_370`, from height `field_3A0 - 0x352` to height -0x352; nothing is
/// drawn when either end projects nearer than depth 30. The line runs from grey
/// 0x80 to 0xC0; while `field_3BC` counts down (never below 1) both ends are
/// scaled by `field_3BC / 45`.
static void Actor02600_Fn02C94(Task* actor)
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

/// Spawn handler: allocates the actor's work block, links the four `WorldCollisionBody`
/// nodes (two collision-record tables, the coordinates and the effect arg) and
/// seeds the initial state from the spawn parameters. The spawn mode splits
/// into a tens digit (unused) and a units digit: 0 and 1 either place the actor
/// on a table row and rotate it to face that row's angle, or fall through to
/// the "walk to the player" state, 2 and 3 lift the coordinate and arm a
/// timer. Modes >= 10 re-read the variant index from the parameters.
static void Actor02600_Fn02FFC(Enemy* ctx, Task* actor)
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
    work->field_3C0     = 0;
    work->field_36C     = &Actor02600_D08AB4;
    ctx->field_4        = matrix;
    ctx->field_48       = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord                 = actor->extra.tmd->coords + 1;
    ctx->bodyPos.vy            = -0x64;
    ctx->recs                  = work->field_2B4;
    ctx->bodyPos.vx            = 0;
    ctx->bodyPos.vz            = 0;
    ctx->param                 = &Actor02600_D08968;
    ctx->hp                    = (s16)Actor02600_D08968.hpMax;
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
                    rot.vy          = Actor02600_D089D0[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor02600_D089B0;
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
                    rot.vy          = Actor02600_D08A08[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor02600_D089E8;
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
    func_800B3F84(&work->anim, Actor02600_D08ACC, obj, work->field_154, work->slots);
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
    work->field_214.key              = 0x3001A;
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
    work->field_294.key              = 0x3001A;
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

/// Per-frame tick, selected by the global mode `Gp_StateF0.field_4`. Mode 1 only
/// updates the colour and the ground shadow; mode 2 sets the model's `field_C` to
/// 0x80 and the context's `field_14` to 1 and stops there; mode 0 clears both
/// and then runs the full tick like any other mode. The full tick applies the
/// timed status damage when the context flags ask for it, resolves the
/// collision records, runs the behaviour state, the effect step while
/// `field_3B0` is set and the turn step while `field_3A6` is, moves and
/// animates the actor and refreshes its coordinate.
static void Actor02600_Fn03558(Enemy* arg0, Task* arg1)
{
    s32              state;
    TmdObject*       obj;
    Actor105500Work* work;
    GfxCoord*        coord;

    obj   = arg1->extra.tmd;
    state = Gp_StateF0.field_4;
    work  = arg1->work;
    coord = obj->coords;
    if (state == 1) {
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
    obj->flags                   = 0;
    arg0->node.state.parts.flags = 0;
    goto default_body;
case2:
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor02600_Fn0366C(arg1);
    }
    Actor02600_Fn0006C(arg1);
    Actor02600_Fn03784(arg1);
    if (work->field_3B0 != 0) {
        Actor02600_Fn020D4(arg1);
    }
    if (work->field_3A6 != 0) {
        Actor02600_Fn02214(arg1);
    }
    Actor02600_Fn03910(arg1);
    Actor02600_Fn039A4(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
case1:
    Actor02600_Fn03A68(arg1);
    Actor02600_Fn03AC0(arg1);
}

/// Status handling, run when the enemy's `reactionFlags` are non-zero.
/// Buildup is consumed, unless `field_3C8` is 1, by switching to state 7 with
/// `field_3D2` set. While damage over time is set, `Gp_TickObjFlag4` yields a
/// per-frame damage that is passed to `func_800DA6E8` and taken from `hp`;
/// outside `field_3C8` 1 the actor then enters state 9 when they
/// run out (setting `field_30` to 2) or state 6 otherwise. The bits are
/// cleared once `Gp_ObjFlag4Expired` returns non-zero.
static void Actor02600_Fn0366C(Task* arg0)
{
    Actor105500Work* work;
    s32              damage;
    s32              remaining;
    u8               flags;
    Enemy*           ctx;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if ((flags & ENEMY_REACTION_BUILDUP) && (work->field_3C8 != 1)) {
        ctx->reactionFlags = (u8)(flags & ENEMY_REACTION_BUILDUP_CLEAR);
        work->field_39A    = 7;
        work->field_39C    = 0;
        work->field_3D2    = 1;
    }
    if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
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
            ctx->reactionFlags = (u8)(ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
        }
    }
}

/// Runs the handler of the work's current behaviour state (`field_39A`, 0-8);
/// state 9, entered when the hit points run out, runs nothing.
static void Actor02600_Fn03784(Task* arg0)
{
    switch (((Actor105500Work*)arg0->work)->field_39A) {
        case 0:
            Actor02600_Fn00754(arg0);
            break;
        case 1:
            Actor02600_Fn00914(arg0);
            break;
        case 2:
            Actor02600_Fn00A94(arg0);
            break;
        case 3:
            Actor02600_Fn00FA0(arg0);
            break;
        case 4:
            Actor02600_Fn012E8(arg0);
            break;
        case 5:
            Actor02600_Fn0143C(arg0);
            break;
        case 6:
            Actor02600_Fn01A0C(arg0);
            break;
        case 7:
            Actor02600_Fn0385C(arg0);
            break;
        case 8:
            Actor02600_Fn01B30(arg0);
            break;
        case 9:
            break;
    }
}

/// Behaviour state 7. On entry it starts animation 0xE and stops the forward
/// and turn steps; each frame after that `Gp_TickObjFlag2` is ticked on the
/// context, and when it returns non-zero the actor goes to state 3
/// with animation 0xB, `field_3D2` cleared and a random 0..15 in `field_39E`.
static void Actor02600_Fn0385C(Task* arg0)
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

/// Steps the actor's coordinate, saving the previous translation in
/// `field_35C` first. The horizontal step follows the coordinate's forward
/// axis (`coord.m[*][2]`) scaled by the speed `field_398`, where 0x1000 is one
/// unit; `field_3A8` is added to the height unscaled.
static void Actor02600_Fn03910(Task* arg0)
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

/// Out-of-line form of `_actor02600TickAnim`: switches the work's animation
/// id, or ticks every slot one frame when it is unchanged.
static void Actor02600_Fn039A4(Task* arg0)
{
    _actor02600TickAnim(arg0);
}

/// Passes the world position of the model's root coordinate to
/// `Gp_UpdateActorColor` for the context, with both trailing arguments 0.
static void Actor02600_Fn03A68(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Ground shadow under the actor's coordinate. With `field_39A` at 2 the
/// position is cast down to the ground by `func_800EA1A8` and the shade comes
/// from `func_800EA318`; otherwise the coordinate's own world translation is
/// used at full shade.
static void Actor02600_Fn03AC0(Task* arg0)
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

/// Squashes the model vertically: `field_3A0` shrinks by 0x50 a frame while
/// above 0x200, and the root coordinate becomes the matrix `field_370` scaled
/// on Y by `field_3A0` (0x1000 = 1), built through a 0x30-byte scratchpad
/// block that is released again.
static void Actor02600_Fn03B58(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    Actor105500Work*   work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
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

static void Actor02600_Fn03C4C(Task* actor)
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

    D_80067704[0] = &Actor02600_D05F10;
    eff           = Gp_SpawnEff(0x40007, actor->extra.tmd->coords + 4, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = &gGameSession->location.loc;
    raw        = ((Enemy*)actor->spawnArg2.pointer)->placeKey;
    model      = eff->task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    areaSyncLocationVariant(&key);
    rec = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry                    = gpAreaPlaceAt(rec->field_0, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// Shrinks the model's third coordinate node to 1/16 through a 0x30-byte
/// block taken from the scratchpad and released again: an identity rotation
/// is written word-wise, `ScaleMatrix` scales its diagonal to 0x100 and
/// `MulMatrix` multiplies it into `field_8[2].coord`.
static void Actor02600_Fn03D38(Task* actor)
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

void Actor02600_Fn03DD0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02600_D0002C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Setup state of the projectile task: allocates its 0x40-byte work, places
/// its model's coordinate at the spawning model's fifth node (expressed
/// relative to the view coordinate), and links the work's collision object
/// with its single record, carrying the parent work's `field_3AC`. The enemy
/// is destroyed when the allocation fails.
static void Actor02600_Fn03E2C(Enemy* enemy, Task* task)
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
    work->obj.key              = Gp_PackPair(Actor02600_D08950, 2);
    work->obj.radius           = 0x100;
    work->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(&work->rec, 1, 0);
    work->obj.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state      = 1;
}

void Actor02600_Fn03F80(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02600_D00038;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
