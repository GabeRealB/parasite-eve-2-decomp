#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/coord_math.h"
#include "../../shared/actor_messages.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).

/// The actor's per-instance work block, reached through `Task::work`. `field_0`
/// is the display mode the tick dispatches on, `field_2` the mode dispatched
/// last and `field_4` set on the frame it changed; `field_174` is the motion
/// the slots play, `field_4A` the current animation id (low ten bits), and
/// `field_220` latches the last trigger id reported.
typedef struct Actor123200Work {
    /* 0x000 */ s16              field_0;
    /* 0x002 */ s16              field_2;
    /* 0x004 */ s16              field_4; // non-zero restarts the model (`func_actor_123200_80133820`)
    /* 0x006 */ u16              field_6; // frames since the restart branch last ran
    /* 0x008 */ s16              field_8;
    /* 0x00A */ byte             pad_A[0x2];
    /* 0x00C */ AnimationContext anim;     // `func_800B3F84` arg0
    /* 0x020 */ AnimationSlot    slots[1]; // slots 1..5 continue past here, overlapping the fields below
    /* 0x048 */ byte             pad_48[0x2];
    /* 0x04A */ u16              field_4A; // low ten bits: animation id (`slots[1].field_2`)
    /* 0x04C */ byte             pad_4C[0xC];
    /* 0x058 */ u16              field_58;
    /* 0x05A */ byte             pad_5A[0xB6];
    /* 0x110 */ byte             poses[0x60]; // `func_800B3F84` arg3
    /* 0x170 */ s16              field_170;   // motion state `animDriverTick` switches on
    /* 0x172 */ s16              field_172;
    /* 0x174 */ s16              field_174;
    /* 0x176 */ u16              field_176;
    /* 0x178 */ s16              field_178;
    /* 0x17A */ s16              field_17A; // frames since the motion last restarted
    /* 0x17C */ s16              field_17C; // frames since then with `field_58` bit 1 set
    /* 0x17E */ s16              field_17E;
    /* 0x180 */ byte             pad_180[0x14];
    /* 0x194 */ u8               field_194;
    /* 0x195 */ u8               field_195;
    /* 0x196 */ u8               field_196;
    /* 0x197 */ byte             pad_197[0x1];
    /* 0x198 */ u16              field_198;
    /* 0x19A */ u16              field_19A;
    /* 0x19C */ byte             pad_19C[0xC];
    /// World X/Y/Z of the model's coordinate, narrowed to 16 bits as the spawn
    /// handler samples the low 16 bits of each local translation component.
    /* 0x1A8 */ u16    field_1A8;
    /* 0x1AA */ u16    field_1AA;
    /* 0x1AC */ u16    field_1AC;
    /* 0x1AE */ byte   pad_1AE[0x2];
    /* 0x1B0 */ s16    field_1B0;
    /* 0x1B2 */ s16    field_1B2;
    /* 0x1B4 */ s16    field_1B4;
    /* 0x1B6 */ byte   pad_1B6[0x6];
    /* 0x1BC */ MATRIX field_1BC; // installed at `TmdObject.lightMtx` by `func_actor_123200_8013352C`
    /* 0x1DC */ MATRIX field_1DC; // installed at `TmdObject.colorMtx`
    /* 0x1FC */ byte   pad_1FC[0x20];
    /// Model scale `func_actor_123200_80133BA0` puts on `field_1BC` through
    /// `ScaleMatrix`; 0x1000 is 1.0 and skips the scale entirely. Picked from
    /// the top nibble of the enemy's `placeKey` by `func_actor_123200_80133EDC`.
    /* 0x21C */ s16  field_21C;
    /* 0x21E */ byte pad_21E[0x2];
    /* 0x220 */ u16  field_220;
    /* 0x222 */ byte pad_222[0xA];
} Actor123200Work;
STATIC_ASSERT_SIZEOF(Actor123200Work, 0x22C);

/// Enemy parameters the spawn handler installs at `Enemy::param`.
extern EnemyParams D_actor_123200_80134208;

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_123200_80137154[];

/// Message table the spawn handler publishes as `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, ActorCommand* request);
        s32 (*call1)(Task*, s32, ActorTransform*);
        s32 (*call2)(Task*, s32, s32);
    } handler;
} Actor123200MessageEntry;
STATIC_ASSERT_SIZEOF(Actor123200MessageEntry, 8);

extern Actor123200MessageEntry D_actor_123200_80137214[4];

/// Integer part of the last movement step `func_actor_123200_801329F0`
/// applied.
static SVECTOR ActorContact_ScratchPosition;

/// Overlay-wide spawn record the spawn handler fills for the instance's own
/// coordinate, with the 0x100 / 1 argument pair. Each overlay that spawns this
/// way keeps one, and they differ only in the coordinate and the argument.
extern EffectSpawnArg D_actor_123200_80137248;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void func_actor_123200_80134178(Enemy* arg0, Task* arg1);

extern TmdSource D_actor_123200_80135AF0;
void             func_actor_123200_801341A8(Task*);

s32 func_actor_123200_80133E30(Task*, s32, s32);
s32 func_actor_123200_80133EDC(Task*, s32, ActorCommand* msg);

#include "../../shared/actor_contacts.h"
#include "../../shared/anim_driver.h"

DamageAttack D_actor_123200_80134204[1] = {
    { 24, 7 },
};

EnemyParams D_actor_123200_80134208 = { D_actor_123200_80134204, 1, 6, 20, 3, 100, 0, 100, 0 };

TmdBone D_actor_123200_80134218[6] = {
#include "assets/actor_123200_model_03CD0_skeleton.inc"
};

u32 D_actor_123200_801342F0[6] = {
#include "assets/actor_123200_model_03CD0_partVerts.inc"
};

SVECTOR D_actor_123200_80134308[102] = {
#include "assets/actor_123200_model_03CD0_verts.inc"
};

SVECTOR D_actor_123200_80134638[139] = {
#include "assets/actor_123200_model_03CD0_normals.inc"
};

u32 D_actor_123200_80134A90[1048] = {
#include "assets/actor_123200_model_03CD0_stream.inc"
};

TmdSource D_actor_123200_80135AF0 = {
    0,
    5984,
    1264,
    6,
    D_actor_123200_801342F0,
    D_actor_123200_80134308,
    D_actor_123200_80134638,
    D_actor_123200_80134218,
    D_actor_123200_80134A90,
};

AnimationPackedPose D_actor_123200_80135B14[4] = {
#include "assets/actor_123200_animation_03E30_bank1.inc"
};

AnimationPackedRotation D_actor_123200_80135B44[21] = {
#include "assets/actor_123200_animation_03E30_bank4.inc"
};

AnimationRecord D_actor_123200_80135B98[43] = {
#include "assets/actor_123200_animation_03E30_records.inc"
};

u16 D_actor_123200_80135C44[6] = {
#include "assets/actor_123200_animation_03E30_indices.inc"
};

AnimationSet D_actor_123200_80135C50 = {
    D_actor_123200_80135B98,
    D_actor_123200_80135C44,
    { NULL, D_actor_123200_80135B14, NULL, NULL, D_actor_123200_80135B44, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_123200_80135C78[19] = {
#include "assets/actor_123200_animation_040F4_bank1.inc"
};

AnimationPackedRotation D_actor_123200_80135D5C[36] = {
#include "assets/actor_123200_animation_040F4_bank4.inc"
};

AnimationRecord D_actor_123200_80135DEC[71] = {
#include "assets/actor_123200_animation_040F4_records.inc"
};

u16 D_actor_123200_80135F08[6] = {
#include "assets/actor_123200_animation_040F4_indices.inc"
};

AnimationSet D_actor_123200_80135F14 = {
    D_actor_123200_80135DEC,
    D_actor_123200_80135F08,
    { NULL, D_actor_123200_80135C78, NULL, NULL, D_actor_123200_80135D5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_123200_80135F3C[20] = {
#include "assets/actor_123200_animation_04398_bank1.inc"
};

AnimationPackedRotation D_actor_123200_8013602C[31] = {
#include "assets/actor_123200_animation_04398_bank4.inc"
};

AnimationRecord D_actor_123200_801360A8[65] = {
#include "assets/actor_123200_animation_04398_records.inc"
};

u16 D_actor_123200_801361AC[6] = {
#include "assets/actor_123200_animation_04398_indices.inc"
};

AnimationSet D_actor_123200_801361B8 = {
    D_actor_123200_801360A8,
    D_actor_123200_801361AC,
    { NULL, D_actor_123200_80135F3C, NULL, NULL, D_actor_123200_8013602C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_123200_801361E0[6] = {
#include "assets/actor_123200_animation_04578_bank1.inc"
};

AnimationPackedRotation D_actor_123200_80136228[38] = {
#include "assets/actor_123200_animation_04578_bank4.inc"
};

AnimationRecord D_actor_123200_801362C0[51] = {
#include "assets/actor_123200_animation_04578_records.inc"
};

u16 D_actor_123200_8013638C[6] = {
#include "assets/actor_123200_animation_04578_indices.inc"
};

AnimationSet D_actor_123200_80136398 = {
    D_actor_123200_801362C0,
    D_actor_123200_8013638C,
    { NULL, D_actor_123200_801361E0, NULL, NULL, D_actor_123200_80136228, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_123200_801363C0[4] = {
#include "assets/actor_123200_animation_046B0_bank1.inc"
};

AnimationPackedRotation D_actor_123200_801363F0[13] = {
#include "assets/actor_123200_animation_046B0_bank4.inc"
};

AnimationRecord D_actor_123200_80136424[40] = {
#include "assets/actor_123200_animation_046B0_records.inc"
};

u16 D_actor_123200_801364C4[6] = {
#include "assets/actor_123200_animation_046B0_indices.inc"
};

AnimationSet D_actor_123200_801364D0 = {
    D_actor_123200_80136424,
    D_actor_123200_801364C4,
    { NULL, D_actor_123200_801363C0, NULL, NULL, D_actor_123200_801363F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_123200_801364F8[7] = {
#include "assets/actor_123200_animation_04850_bank1.inc"
};

AnimationPackedRotation D_actor_123200_8013654C[28] = {
#include "assets/actor_123200_animation_04850_bank4.inc"
};

AnimationRecord D_actor_123200_801365BC[42] = {
#include "assets/actor_123200_animation_04850_records.inc"
};

u16 D_actor_123200_80136664[6] = {
#include "assets/actor_123200_animation_04850_indices.inc"
};

AnimationSet D_actor_123200_80136670 = {
    D_actor_123200_801365BC,
    D_actor_123200_80136664,
    { NULL, D_actor_123200_801364F8, NULL, NULL, D_actor_123200_8013654C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_123200_80136698[6] = {
#include "assets/actor_123200_animation_04A64_bank1.inc"
};

AnimationPackedRotation D_actor_123200_801366E0[42] = {
#include "assets/actor_123200_animation_04A64_bank4.inc"
};

AnimationRecord D_actor_123200_80136788[60] = {
#include "assets/actor_123200_animation_04A64_records.inc"
};

u16 D_actor_123200_80136878[6] = {
#include "assets/actor_123200_animation_04A64_indices.inc"
};

AnimationSet D_actor_123200_80136884 = {
    D_actor_123200_80136788,
    D_actor_123200_80136878,
    { NULL, D_actor_123200_80136698, NULL, NULL, D_actor_123200_801366E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_123200_801368AC[17] = {
#include "assets/actor_123200_animation_04E74_bank1.inc"
};

AnimationPackedRotation D_actor_123200_80136978[85] = {
#include "assets/actor_123200_animation_04E74_bank4.inc"
};

AnimationRecord D_actor_123200_80136ACC[111] = {
#include "assets/actor_123200_animation_04E74_records.inc"
};

u16 D_actor_123200_80136C88[6] = {
#include "assets/actor_123200_animation_04E74_indices.inc"
};

AnimationSet D_actor_123200_80136C94 = {
    D_actor_123200_80136ACC,
    D_actor_123200_80136C88,
    { NULL, D_actor_123200_801368AC, NULL, NULL, D_actor_123200_80136978, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_123200_80136CBC[12] = {
#include "assets/actor_123200_animation_05134_bank1.inc"
};

AnimationPackedRotation D_actor_123200_80136D4C[52] = {
#include "assets/actor_123200_animation_05134_bank4.inc"
};

AnimationRecord D_actor_123200_80136E1C[75] = {
#include "assets/actor_123200_animation_05134_records.inc"
};

u16 D_actor_123200_80136F48[6] = {
#include "assets/actor_123200_animation_05134_indices.inc"
};

AnimationSet D_actor_123200_80136F54 = {
    D_actor_123200_80136E1C,
    D_actor_123200_80136F48,
    { NULL, D_actor_123200_80136CBC, NULL, NULL, D_actor_123200_80136D4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_123200_80136F7C[5] = {
#include "assets/actor_123200_animation_0530C_bank1.inc"
};

AnimationPackedRotation D_actor_123200_80136FB8[19] = {
#include "assets/actor_123200_animation_0530C_bank4.inc"
};

AnimationRecord D_actor_123200_80137004[71] = {
#include "assets/actor_123200_animation_0530C_records.inc"
};

u16 D_actor_123200_80137120[6] = {
#include "assets/actor_123200_animation_0530C_indices.inc"
};

AnimationSet D_actor_123200_8013712C = {
    D_actor_123200_80137004,
    D_actor_123200_80137120,
    { NULL, D_actor_123200_80136F7C, NULL, NULL, D_actor_123200_80136FB8, NULL, NULL, NULL },
};

u8 D_actor_123200_80137154[192] = {
    0,
    0,
    0,
    0,
    80,
    92,
    19,
    128,
    20,
    95,
    19,
    128,
    184,
    97,
    19,
    128,
    152,
    99,
    19,
    128,
    208,
    100,
    19,
    128,
    112,
    102,
    19,
    128,
    132,
    104,
    19,
    128,
    148,
    108,
    19,
    128,
    84,
    111,
    19,
    128,
    44,
    113,
    19,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    9,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    6,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

Actor123200MessageEntry D_actor_123200_80137214[4] = {
    { 2005, { .call2 = func_actor_123200_80133E30 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_123200_80133EDC } },
    { 2004, { .call1 = actorMsgPlace } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_123200_80137234 = { TASK_BODY_TMD, 96, func_actor_123200_801341A8, { .model = &D_actor_123200_80135AF0 } };

static SVECTOR ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

EffectSpawnArg D_actor_123200_80137248;

static s32             func_actor_123200_80133450(Actor123200Work* arg0);
static __inline__ void Actor123200_ScaleForward(SVECTOR* dir);
static void            func_actor_123200_8013352C(Enemy* enemy, Task* task);
static __inline__ void Actor123200_StepForward(GfxCoord* coord);
static void            func_actor_123200_80133820(Enemy* enemy, Task* task);
static __inline__ void Actor123200_MoveForward(GfxCoord* coord);
static void            func_actor_123200_801339F0(Enemy* enemy, Task* task);
static void            func_actor_123200_80133BA0(Enemy* enemy, Task* arg1);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/anim_driver_tick.inc.c"

/// In motion states 2 and 3, reports 0x400C0001 the first time the animation id
/// in `field_4A` reaches one of that state's trigger ids (latched in
/// `field_220`); in state 5, 0x400C0005 while bit 1 of `field_58` is set.
/// Returns 0 otherwise.
static s32 func_actor_123200_80133450(Actor123200Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_220 == v) {
                goto same;
            }
            arg0->field_220 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_220 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_220 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

/// Normalises `dir` in place and scales it to 0x3E8/0x1000 of unit length on
/// the GTE. The pointer stays in one register across `VectorNormalSS` because
/// the GTE loads read it back afterwards.
static __inline__ void Actor123200_ScaleForward(SVECTOR* dir)
{
    VectorNormalSS(dir, dir);
    gte_lddp(0x3E8);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);
}

/// Spawn state of this enemy: allocates the work block, publishes it as
/// `Task::work`, reparents the model to `gGfxViewCoord`, seeds its animation
/// slots from `D_actor_123200_80137154` and hangs the enemy's display node off
/// part 2 of the model's coordinate array. The placement index in
/// `Enemy::placeKey` biases the initial values in `field_176`, `field_198`
/// and `field_19A`: odd indices add the index, even indices subtract half of it.
static void func_actor_123200_8013352C(Enemy* enemy, Task* task)
{
    SVECTOR          dir;
    Actor123200Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    u32              placementIndex;
    u32              placementParity;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(Actor123200Work), false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->msgTable = D_actor_123200_80137214;
    coord->parent  = &gGfxViewCoord;
    obj->flags     = 0;
    func_800B3F84(&work->anim, D_actor_123200_80137154, obj, work->poses, work->slots);

    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->param                  = &D_actor_123200_80134208;
    enemy->reactionFlags          = 0;
    enemy->hpMax                  = 0;
    enemy->hp                     = 0;
    enemy->recs                   = 0;

    work->field_174 = 1;
    work->field_170 = 2;
    work->field_176 = 0x10;
    work->field_178 = 0;
    animDriverTick(task);
    work->field_17E     = 0;
    work->field_8       = 0;
    obj->lightMtx       = &work->field_1BC;
    obj->colorMtx       = &work->field_1DC;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_198     = 5;
    work->field_19A     = 0x14;

    placementIndex  = (u16)(enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT);
    placementParity = placementIndex & 1;
    if (placementParity == 1) {
        work->field_176 += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_19A += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_198 += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->field_176 -= placementIndex >> 1;
        work->field_19A -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_198 -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }

    work->field_1A8 = (u16)task->extra.tmd->coords->coord.t[0];
    work->field_1AA = (u16)task->extra.tmd->coords->coord.t[1];
    work->field_1AC = (u16)task->extra.tmd->coords->coord.t[2];

    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    Actor123200_ScaleForward(&dir);

    work->field_0                      = 0;
    work->field_2                      = -1;
    D_actor_123200_80137248.coord      = task->extra.tmd->coords;
    D_actor_123200_80137248.spawnArgLo = 0x100;
    D_actor_123200_80137248.spawnArgHi = 1;
    task->state++;
}

/// Steps `coord` 5/0x1000 of the way along its own forward axis (column 2 of
/// its rotation, normalised and GPF-scaled) and flags it for rebuild. The
/// direction vector lives in an `SVECTOR` carved off the scratch head and
/// handed straight back.
static __inline__ void Actor123200_StepForward(GfxCoord* coord)
{
    u8*      head;
    SVECTOR* dir;

    head                       = SCRATCH_STACK_CURSOR(u8);
    dir                        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_STACK_CURSOR(void) = dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(5);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0]  += dir->vx;
    coord->coord.t[1]  += dir->vy;
    coord->coord.t[2]  += dir->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

/// Display mode 1 handler (entry 1 of `D_actor_123200_80131E24`). On the frame
/// the mode is entered (`field_4` set) it re-arms the model -- clearing
/// `TmdObject.flags` and reinstating its buffers, rewriting the
/// 0x1B0/0x1B2/0x1B4 triple, restarting the motion on state 2 and the frame
/// counter `field_6`, and marking the enemy's lock-on node not lockable -- and
/// returns. Otherwise the frame counter runs, 0xC bytes are reserved off the
/// scratch head, and unless the game is frozen the model is stepped forward
/// along its facing; the reservation is released after the animation update
/// and the model's coordinate is flagged for rebuild.
static void func_actor_123200_80133820(Enemy* enemy, Task* task)
{
    Actor123200Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;

    work = (Actor123200Work*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_1B0 = 0x115D;
        work->field_1B2 = 1;
        work->field_1B4 = 0x12D5;
        work->field_174 = 2;
        work->field_170 = 2;
        animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_6                         = 0;
        return;
    }
    work->field_6++;
    SCRATCH_STACK_RESERVE_BYTES(0xC);
    coord = task->extra.tmd->coords;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        Actor123200_StepForward(coord);
    }
    animDriverTick(task);
    SCRATCH_STACK_RELEASE_BYTES(0xC);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Unless the game is frozen, steps `coord` 5/0x1000 of the way along its own
/// forward axis through an `SVECTOR` carved off the scratch head, and flags it
/// for rebuild.
static __inline__ void Actor123200_MoveForward(GfxCoord* coord)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(5);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Display mode 2 handler (entry 2 of `D_actor_123200_80131E24`): the same
/// re-arm on entry as `func_actor_123200_80133820`; on later frames it counts
/// the frame, steps the model along its facing unless the game is frozen, and
/// updates its animation, without the extra scratch reservation.
static void func_actor_123200_801339F0(Enemy* enemy, Task* task)
{
    Actor123200Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;

    work = (Actor123200Work*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_1B0 = 0x115D;
        work->field_1B2 = 1;
        work->field_1B4 = 0x12D5;
        work->field_174 = 2;
        work->field_170 = 2;
        animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_6                         = 0;
        return;
    }
    work->field_6++;
    coord = task->extra.tmd->coords;
    Actor123200_MoveForward(coord);
    animDriverTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// The three display-mode handlers `func_actor_123200_80133BA0` picks between
/// by the work block's `field_0`, copied onto its stack before the call: 0 the
/// idle state, 1 and 2 the two stepping handlers.
static const GpEnemyTaskFuncTable3 D_actor_123200_80131E24 = {
    {
        func_actor_123200_80134178,
        func_actor_123200_80133820,
        func_actor_123200_801339F0,
    },
};

/// Per-frame tick: flags the model's coordinate for rebuild, refreshes its
/// colour from the part matrix's translation, then scales that matrix from the
/// work block's `field_21C`. The render mode in `Gp_StateF0.field_4` runs next -- modes
/// 0 and 1 draw the ground quad while the display mode is non-zero, and 1 and 2
/// return without ticking. The rest re-records the display mode in `field_2`
/// (`field_4` restarting the model when it changed), dispatches the display
/// mode's handler from `D_actor_123200_80131E24`, and plays the sound that
/// handler reports, panned and depth-tagged from the model's coordinate. A
/// raised `gGameSession->viewReady` flags the coordinate for rebuild again.
static void func_actor_123200_80133BA0(Enemy* enemy, Task* arg1)
{
    VECTOR                pos;
    GpEnemyTaskFuncTable3 table;
    Actor123200Work*      work;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                                  = (Actor123200Work*)arg1->work;
    table                                 = D_actor_123200_80131E24;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    if (work->field_21C != 0x1000) {
        pos.vx = pos.vy = pos.vz = work->field_21C;
        ScaleMatrix(&work->field_1BC, &pos);
    }
    switch (Gp_StateF0.field_4) {
        case 0:
            if (work->field_0 != 0) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case 1:
            if (work->field_0 != 0) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            return;
        case 2:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.funcs[work->field_0](enemy, arg1);
    id = func_actor_123200_80133450(work);
    if (id != 0) {
        snd = id | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(arg1->extra.tmd->coords));
    }
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// The enemy's three task states -- spawn, per-frame tick and teardown -- which
/// `func_actor_123200_801341A8` runs by `Task::state`.
static const GpEnemyTaskFuncTable3 D_actor_123200_80131E30 = {
    {
        func_actor_123200_8013352C,
        func_actor_123200_80133BA0,
        Gp_DestroyEnemy,
    },
};

/// Message handler (id 0x7D5 in `D_actor_123200_80137214`). `arg2` selects the
/// mode: 0 hides the model (`TmdObject.flags` bit 0x80), 1 clears its flags and
/// so shows it, 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER`, and 3 and 4 both clear
/// the flags and then set `TMD_OBJECT_SKIP_AUTO_BUFFER`. Modes 0 and 1 reinstate the model's buffers through
/// `Tmd_AllocBuffers` and set the work block's display mode `field_0` to 1;
/// modes 2, 3 and 4 set it to 0. `arg1` is unused. Always returns 0.
s32 func_actor_123200_80133E30(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor123200Work* work;

    obj  = task->extra.tmd;
    work = (Actor123200Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 2:
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_0 = 0;
            break;
        case 3:
        case 4:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Message handler (id 0x7DB in `D_actor_123200_80137214`). Copies the
/// message's first three bytes into the work block and handles type 0xB02
/// commands: 1 selects display mode 2, at full scale when the top nibble of
/// the enemy's `placeKey` is 1 and at quarter scale otherwise; 2 selects mode 1
/// at full scale; 3 selects mode 0. Always returns 0.
s32 func_actor_123200_80133EDC(Task* task, s32 arg1, ActorCommand* msg)
{
    Actor123200Work* work;
    Enemy*           enemy;

    work            = (Actor123200Work*)task->work;
    enemy           = (Enemy*)task->spawnArg2.pointer;
    work->field_194 = msg->context.loc.stage;
    work->field_195 = msg->context.loc.area;
    work->field_196 = (u8)msg->command;
    if (msg->context.key == 0xB02) {
        switch (msg->command) {
            case 1:
                if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 1) {
                    work->field_21C = 0x1000;
                } else {
                    work->field_21C = 0x400;
                }
                work->field_0 = 2;
                break;
            case 2:
                work->field_21C = 0x1000;
                work->field_0   = 1;
                break;
            case 3:
                work->field_0 = 0;
                break;
            case 0:
                break;
        }
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

/// Idle state of this enemy (entry 0 of `D_actor_123200_80131E24`). On the
/// frame the state is entered (`field_4` set) it marks the enemy not lockable
/// and sets the model's flags to 0x80; it does nothing on later frames.
static void func_actor_123200_80134178(Enemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor123200Work*)arg1->work)->field_4 != 0) {
        model                        = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Runs the handler of `D_actor_123200_80131E30` that `Task::state` selects --
/// spawn, per-frame tick or teardown -- on the enemy in `Task::spawnArg2`,
/// copying the table onto the stack before the call.
void func_actor_123200_801341A8(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_123200_80131E30;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
