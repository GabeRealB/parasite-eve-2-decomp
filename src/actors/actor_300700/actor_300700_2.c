#include "actor_300700_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actor_300700_spawn2_private.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
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
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

extern s16 D_actor_300700_8016933C[8];

extern u16 D_actor_300700_8016934C[16];

extern s16 D_actor_300700_8016936C[8];

extern u16 D_actor_300700_8016937C[16];

extern s16 D_actor_300700_8016939C[8];

/// Per-state animation id handed to `func_800B4114`, indexed by `field_37E`.
extern s16 D_actor_300700_801693E4[];

static void func_actor_300700_801637E4(Task* arg0);
static void func_actor_300700_80164794(Task* arg0);
static void func_actor_300700_80163D64(Task* arg0);
static void func_actor_300700_80164070(Task* arg0);
static void func_actor_300700_801643D0(Task* arg0);
static void func_actor_300700_801645F8(Task* arg0);
static void func_actor_300700_80164E38(Task* arg0);
static void func_actor_300700_80164F68(Task* arg0);
static void func_actor_300700_80165000(Task* arg0);
static void func_actor_300700_801650C0(Task* arg0);
static void func_actor_300700_801651A0(Task* arg0);
static void func_actor_300700_80165230(Task* arg0);
static void func_actor_300700_801652F4(Task* arg0);
static void func_actor_300700_8016534C(Task* arg0);
static void func_actor_300700_8016539C(Task* arg0);

void func_actor_300700_801622B4(Task* arg0);
void func_actor_300700_8016252C(Task* arg0);
void func_actor_300700_801626C0(Task* arg0);
void func_actor_300700_801628C8(Task* arg0);
void func_actor_300700_801633B8(Task* arg0);
void func_actor_300700_80162EFC(Task* arg0);

static void func_actor_300700_801648E4(Enemy* arg0, Task* arg1);

static void func_actor_300700_80164D3C(Enemy* arg0, Task* arg1);

/// The second variant's state handlers, in the same order as the first's:
/// spawn, per-frame update and state 2. `func_actor_300700_80164CE0`
/// dispatches them.
static const GpEnemyTaskFuncTable3 D_actor_300700_80161E30 = {
    {
        func_actor_300700_80163510,
        func_actor_300700_80164D3C,
        func_actor_300700_801648E4,
    },
};

static void func_actor_300700_80164CE0(Task*);

extern AnimationSet D_actor_300700_8016790C;
extern AnimationSet D_actor_300700_80167BC8;
extern AnimationSet D_actor_300700_80168058;
extern AnimationSet D_actor_300700_80168398;
extern AnimationSet D_actor_300700_801685D8;
extern AnimationSet D_actor_300700_80168854;
extern AnimationSet D_actor_300700_80168B94;
extern AnimationSet D_actor_300700_80168D28;
extern AnimationSet D_actor_300700_80168FBC;
extern AnimationSet D_actor_300700_80169300;

AnimationPackedPose D_actor_300700_80167424[20] = {
#include "assets/actor_300700_animation_05AEC_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80167514[105] = {
#include "assets/actor_300700_animation_05AEC_bank4.inc"
};

AnimationRecord D_actor_300700_801676B8[145] = {
#include "assets/actor_300700_animation_05AEC_records.inc"
};

u16 D_actor_300700_801678FC[8] = {
#include "assets/actor_300700_animation_05AEC_indices.inc"
};

AnimationSet D_actor_300700_8016790C = {
    D_actor_300700_801676B8,
    D_actor_300700_801678FC,
    { NULL, D_actor_300700_80167424, NULL, NULL, D_actor_300700_80167514, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80167934[15] = {
#include "assets/actor_300700_animation_05DA8_bank1.inc"
};

AnimationPackedRotation D_actor_300700_801679E8[39] = {
#include "assets/actor_300700_animation_05DA8_bank4.inc"
};

AnimationRecord D_actor_300700_80167A84[77] = {
#include "assets/actor_300700_animation_05DA8_records.inc"
};

u16 D_actor_300700_80167BB8[8] = {
#include "assets/actor_300700_animation_05DA8_indices.inc"
};

AnimationSet D_actor_300700_80167BC8 = {
    D_actor_300700_80167A84,
    D_actor_300700_80167BB8,
    { NULL, D_actor_300700_80167934, NULL, NULL, D_actor_300700_801679E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80167BF0[24] = {
#include "assets/actor_300700_animation_06238_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80167D10[85] = {
#include "assets/actor_300700_animation_06238_bank4.inc"
};

AnimationRecord D_actor_300700_80167E64[121] = {
#include "assets/actor_300700_animation_06238_records.inc"
};

u16 D_actor_300700_80168048[8] = {
#include "assets/actor_300700_animation_06238_indices.inc"
};

AnimationSet D_actor_300700_80168058 = {
    D_actor_300700_80167E64,
    D_actor_300700_80168048,
    { NULL, D_actor_300700_80167BF0, NULL, NULL, D_actor_300700_80167D10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168080[14] = {
#include "assets/actor_300700_animation_06578_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80168128[65] = {
#include "assets/actor_300700_animation_06578_bank4.inc"
};

AnimationRecord D_actor_300700_8016822C[87] = {
#include "assets/actor_300700_animation_06578_records.inc"
};

u16 D_actor_300700_80168388[8] = {
#include "assets/actor_300700_animation_06578_indices.inc"
};

AnimationSet D_actor_300700_80168398 = {
    D_actor_300700_8016822C,
    D_actor_300700_80168388,
    { NULL, D_actor_300700_80168080, NULL, NULL, D_actor_300700_80168128, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_801683C0[9] = {
#include "assets/actor_300700_animation_067B8_bank1.inc"
};

AnimationPackedRotation D_actor_300700_8016842C[40] = {
#include "assets/actor_300700_animation_067B8_bank4.inc"
};

AnimationRecord D_actor_300700_801684CC[63] = {
#include "assets/actor_300700_animation_067B8_records.inc"
};

u16 D_actor_300700_801685C8[8] = {
#include "assets/actor_300700_animation_067B8_indices.inc"
};

AnimationSet D_actor_300700_801685D8 = {
    D_actor_300700_801684CC,
    D_actor_300700_801685C8,
    { NULL, D_actor_300700_801683C0, NULL, NULL, D_actor_300700_8016842C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168600[10] = {
#include "assets/actor_300700_animation_06A34_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80168678[46] = {
#include "assets/actor_300700_animation_06A34_bank4.inc"
};

AnimationRecord D_actor_300700_80168730[69] = {
#include "assets/actor_300700_animation_06A34_records.inc"
};

u16 D_actor_300700_80168844[8] = {
#include "assets/actor_300700_animation_06A34_indices.inc"
};

AnimationSet D_actor_300700_80168854 = {
    D_actor_300700_80168730,
    D_actor_300700_80168844,
    { NULL, D_actor_300700_80168600, NULL, NULL, D_actor_300700_80168678, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_8016887C[20] = {
#include "assets/actor_300700_animation_06D74_bank1.inc"
};

AnimationPackedRotation D_actor_300700_8016896C[50] = {
#include "assets/actor_300700_animation_06D74_bank4.inc"
};

AnimationRecord D_actor_300700_80168A34[84] = {
#include "assets/actor_300700_animation_06D74_records.inc"
};

u16 D_actor_300700_80168B84[8] = {
#include "assets/actor_300700_animation_06D74_indices.inc"
};

AnimationSet D_actor_300700_80168B94 = {
    D_actor_300700_80168A34,
    D_actor_300700_80168B84,
    { NULL, D_actor_300700_8016887C, NULL, NULL, D_actor_300700_8016896C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168BBC[5] = {
#include "assets/actor_300700_animation_06F08_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80168BF8[20] = {
#include "assets/actor_300700_animation_06F08_bank4.inc"
};

AnimationRecord D_actor_300700_80168C48[52] = {
#include "assets/actor_300700_animation_06F08_records.inc"
};

u16 D_actor_300700_80168D18[8] = {
#include "assets/actor_300700_animation_06F08_indices.inc"
};

AnimationSet D_actor_300700_80168D28 = {
    D_actor_300700_80168C48,
    D_actor_300700_80168D18,
    { NULL, D_actor_300700_80168BBC, NULL, NULL, D_actor_300700_80168BF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168D50[11] = {
#include "assets/actor_300700_animation_0719C_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80168DD4[49] = {
#include "assets/actor_300700_animation_0719C_bank4.inc"
};

AnimationRecord D_actor_300700_80168E98[69] = {
#include "assets/actor_300700_animation_0719C_records.inc"
};

u16 D_actor_300700_80168FAC[8] = {
#include "assets/actor_300700_animation_0719C_indices.inc"
};

AnimationSet D_actor_300700_80168FBC = {
    D_actor_300700_80168E98,
    D_actor_300700_80168FAC,
    { NULL, D_actor_300700_80168D50, NULL, NULL, D_actor_300700_80168DD4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168FE4[18] = {
#include "assets/actor_300700_animation_074E0_bank1.inc"
};

AnimationPackedRotation D_actor_300700_801690BC[54] = {
#include "assets/actor_300700_animation_074E0_bank4.inc"
};

AnimationRecord D_actor_300700_80169194[87] = {
#include "assets/actor_300700_animation_074E0_records.inc"
};

u16 D_actor_300700_801692F0[8] = {
#include "assets/actor_300700_animation_074E0_indices.inc"
};

AnimationSet D_actor_300700_80169300 = {
    D_actor_300700_80169194,
    D_actor_300700_801692F0,
    { NULL, D_actor_300700_80168FE4, NULL, NULL, D_actor_300700_801690BC, NULL, NULL, NULL },
};

DamageAttack D_actor_300700_80169328 = { 6, 3 };

EnemyParams D_actor_300700_8016932C = { &D_actor_300700_80169328, 18, 4, 22, 1, 100, 20, 100, 0 };

s16 D_actor_300700_8016933C[8] = {
    2,
    4,
    4,
    8,
    8,
    6,
    10,
    3,
};

u16 D_actor_300700_8016934C[16] = {
    15,
    20,
    25,
    30,
    32,
    34,
    36,
    38,
    40,
    42,
    44,
    46,
    48,
    53,
    58,
    63,
};

s16 D_actor_300700_8016936C[8] = {
    1,
    2,
    5,
    10,
    12,
    12,
    12,
    14,
};

u16 D_actor_300700_8016937C[16] = {
    10,
    11,
    12,
    13,
    14,
    16,
    18,
    20,
    22,
    24,
    26,
    28,
    29,
    30,
    31,
    32,
};

s16 D_actor_300700_8016939C[8] = {
    0,
    0,
    4,
    8,
    12,
    12,
    13,
    16,
};

TaskDesc D_actor_300700_801693AC = { { { TASK_BODY_TMD, 96 } }, func_actor_300700_80164CE0, { .model = &D_actor_300700_80167400 } };

u32 D_actor_300700_801693B8 = 0;

AnimationSet* D_actor_300700_801693BC[10] = {
    &D_actor_300700_8016790C,
    &D_actor_300700_80167BC8,
    &D_actor_300700_80168058,
    &D_actor_300700_80168398,
    &D_actor_300700_801685D8,
    &D_actor_300700_80168854,
    &D_actor_300700_80168B94,
    &D_actor_300700_80168D28,
    &D_actor_300700_80168FBC,
    &D_actor_300700_80169300,
};

s16 D_actor_300700_801693E4[10] = {
    0,
    3,
    3,
    0,
    2,
    0,
    3,
    0,
    0,
    0,
};

/// Per-frame collision pass: applies the pending move to the root coordinate,
/// then walks the three contact records - kind 2 is a hit that damages the
/// enemy, kinds 1 and 3 an obstacle to push out of - and applies the deepest
/// push at the end.
static void func_actor_300700_801637E4(Task* actor)
{
    Enemy*           ctx;
    u32              lastId;
    u32              id;
    Actor300700Work* work;
    GfxCoord*        coord;
    GfxCoord*        target;
    GpDeltaScratch*  head;
    GpDeltaScratch*  scratch;
    s32              i;
    s32              push;
    s32              bestPush;
    s32              damage;
    s32              cooldown;

    bestPush = 0;
    lastId   = 0;
    head     = SCRATCH_STACK_CURSOR(GpDeltaScratch);
    work     = actor->work;
    scratch = SCRATCH_STACK_CURSOR(GpDeltaScratch) = head - 3;
    coord                                          = actor->extra.tmd->coords;
    ctx                                            = actor->spawnArg2.pointer;
    switch (func_800E0C10((WorldCollisionContact*)&work->field_27C[0x20], scratch, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-3].vx.halves.integer;
            coord->coord.t[1] += scratch->vy.halves.integer;
            coord->coord.t[2] += scratch->vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->field_360;
            coord->coord.t[1] = work->field_364;
            coord->coord.t[2] = work->field_368;
            break;
    }
    Gp_ClearRec18Occupied((WorldCollisionContact*)&work->field_27C[0x20]);
    if (work->field_378 != 0) {
        work->field_378--;
        if (work->field_378 <= 0) {
            work->field_378 = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        switch ((u32)work->field_22C.contacts.recs[i].key.value >> 16) {
            case 0:
                break;
            /* Kinds 1 and 3 push the model back out of the obstacle the same way. */
            case 1:
                scratch->vx.word = coord->workm.t[0] - work->field_22C.contacts.recs[i].point.vx;
                scratch->vy.word = coord->workm.t[1] - work->field_22C.contacts.recs[i].point.vy;
                scratch->vz.word = coord->workm.t[2] - work->field_22C.contacts.recs[i].point.vz;
                push             = work->field_22C.contacts.recs[i].distance -
                       SquareRoot0(scratch->vx.word * scratch->vx.word + scratch->vy.word * scratch->vy.word + scratch->vz.word * scratch->vz.word);
                push = (push <= 0) ? 0 : push;
                if (bestPush < push) {
                    bestPush = push;
                    VectorNormal((VECTOR*)scratch, (VECTOR*)(scratch + 1));
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)(scratch + 1), (VECTOR*)(scratch + 2));
                }
                break;
            case 2:
                if (work->field_378 == 0) {
                    target           = gPlayerActorTasks[((u32)work->field_22C.contacts.recs[i].key.value >> 7) & 1]->extra.tmd->coords;
                    scratch->vx.word = target->coord.t[0] - coord->coord.t[0];
                    scratch->vy.word = target->coord.t[1] - coord->coord.t[1];
                    scratch->vz.word = target->coord.t[2] - coord->coord.t[2];
                    damage           = Gp_ComputeDamage(work->field_22C.contacts.recs[i].key.value,
                                                        SquareRoot0(scratch->vx.word * scratch->vx.word + scratch->vy.word * scratch->vy.word +
                                                                    scratch->vz.word * scratch->vz.word),
                                                        0, 0);
                    if (Gp_RollEnemyChance(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, 0) != 0) {
                        damage *= 4;
                        Gp_SpawnEff(0x6009C, actor->extra.tmd->coords, 0, NULL);
                    }
                    func_800DA6E8(&((Enemy*)actor->spawnArg2.pointer)->node, damage, 0);
                    func_800E2C78(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        work->field_37A = 5;
                        work->field_37C = 0;
                        actor->state    = 2;
                    } else if (work->field_398 == 0) {
                        work->field_37A = 4;
                        work->field_37C = 0;
                    }
                    work->field_31A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    switch (Gp_GetIdParam0(work->field_22C.contacts.recs[i].key.value) & 0xFFFF) {
                        case 0:
                        case 4:
                        case 5:
                        case 6:
                        case 7:
                        case 8:
                            break;
                        case 2:
                            Gp_SetObjFlag2(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, 0);
                            break;
                        case 1:
                        case 9:
                            Gp_SetObjFlag1(actor->spawnArg2.pointer);
                            break;
                    }
                    id = work->field_22C.contacts.recs[i].key.value;
                    if (lastId != id) {
                        lastId = id;
                        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, NULL, &work->field_334);
                    }
                    cooldown = Gp_GetIdParam2(work->field_22C.contacts.recs[i].key.value);
                    if (cooldown > 0) {
                        work->field_378 = cooldown;
                    }
                }
                break;
            case 3:
                scratch->vx.word = coord->workm.t[0] - work->field_22C.contacts.recs[i].point.vx;
                scratch->vy.word = coord->workm.t[1] - work->field_22C.contacts.recs[i].point.vy;
                scratch->vz.word = coord->workm.t[2] - work->field_22C.contacts.recs[i].point.vz;
                push             = work->field_22C.contacts.recs[i].distance -
                       SquareRoot0(scratch->vx.word * scratch->vx.word + scratch->vy.word * scratch->vy.word + scratch->vz.word * scratch->vz.word);
                push = (push <= 0) ? 0 : push;
                if (bestPush < push) {
                    bestPush = push;
                    VectorNormal((VECTOR*)scratch, (VECTOR*)(scratch + 1));
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)(scratch + 1), (VECTOR*)(scratch + 2));
                }
                break;
        }
    }
    if (bestPush > 0) {
        coord->coord.t[0] += (bestPush * scratch[2].vx.word) >> 0xC;
        coord->coord.t[2] += (bestPush * scratch[2].vz.word) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_22C.contacts.recs);
    if (Gp_FindRec18(work->attackContacts, 0) != 0) {
        work->field_31A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(work->attackContacts);
    }
    if (Gp_CountRec18Hi(work->sensorContacts, 0x10000) != 0) {
        target           = gPlayerActorTasks[(u8)work->sensorContacts[0].key.parts.id >> 7]->extra.tmd->coords;
        work->field_394  = 1;
        work->field_1FA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_33C  = target;
    }
    Gp_ClearRec18Occupied(work->sensorContacts);
    SCRATCH_STACK_CURSOR(GpDeltaScratch) += 3;
}

static void func_actor_300700_80163D64(Task* arg0)
{
    Actor300700Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              state;
    s32              one;
    s32              rng0;
    s32              rng1;
    s32              rng2;
    s32              rng3;
    s32              rng4;
    s32              rng5;
    s32              rng6;
    s32              timer;
    s32              next;
    s32              flags;
    s32              ang;
    s32              snd;
    s32              pan;

    one   = 1;
    work  = arg0->work;
    obj   = arg0->extra.tmd;
    state = work->field_37C;
    coord = obj->coords;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto tail;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto tail;
case0:
    flags           = work->field_1FA;
    work->field_384 = 0;
    work->field_1FA = flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    timer           = work->field_38C + 1;
    work->field_38C = timer;
    if ((s16)timer < 0x1E) {
        goto tail;
    }
    rng0            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = rng0;
    if ((s32)(((u32)rng0 >> 16) & 0xF) <
        D_actor_300700_8016933C[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex]) {
        work->field_37E = 7;
        next            = D_actor_300700_8016934C[((u32)(rng1 = rng0 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
        gRandomLcgState = rng1;
        work->field_37C = one;
        work->field_38C = next;
        goto tail;
    }
    rng2            = rng0 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = rng2;
    if ((s32)(((u32)rng2 >> 16) & 0xF) <
        D_actor_300700_8016936C[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex]) {
        work->field_37E = 2;
        next            = D_actor_300700_8016937C[((u32)(rng3 = rng2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
        gRandomLcgState = rng3;
        work->field_37C = 2;
        work->field_38C = next;
        goto tail;
    }
    work->field_38C = 0;
    goto tail;
case1:
    work->field_384 = 0x14;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
    goto tail;
case2:
    work->field_384 = 0x32;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
tail:
    work->field_38E = work->field_38E - 1;
    if ((s16)work->field_38E > 0) {
        goto post;
    }
    work->field_386 = 0x19;
    rng4            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    rng5            = rng4 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    ang             = ((u32)rng5 >> 16) & 0x3FF;
    gRandomLcgState = rng4;
    work->field_38E = ((u32)rng4 >> 16) & 0x1F;
    gRandomLcgState = rng5;
    if ((((u32)rng5 >> 16) & 0x400) == 0) {
        ang = -ang;
    }
    work->field_38A = ((u16)work->field_388 + ang) & 0xFFF;
post:
    if (work->field_394 != 0) {
        work->field_37A = 1;
        work->field_394 = 0;
        work->field_37C = 0;
        work->field_37E = 2;
        rng6            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_38C = (((u32)rng6 >> 16) & 0x1F) + 0x3C;
        snd             = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070003;
        gRandomLcgState = rng6;
        pan             = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    func_actor_300700_80165000(arg0);
}

static void func_actor_300700_80164070(Task* arg0)
{
    VECTOR*          vec;
    Actor300700Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GfxCoord*        target;
    s32              state;
    s32              one;
    s32              dist;
    s32              raw;
    s16              diff;
    s32              adiff;
    s32              ang;
    s32              vel;
    s32              pan;
    s32              snd;

    one   = 1;
    vec   = (VECTOR*)SCRATCH_STACK_RESERVE_BYTES(0x10);
    work  = arg0->work;
    obj   = arg0->extra.tmd;
    state = work->field_37C;
    coord = obj->coords;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto pop;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto pop;
case0:
    Gp_ArmStateF0(1);
    if (work->field_33C == 0) {
        work->field_33C = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    }
    target          = work->field_33C;
    vec->vx         = target->coord.t[0] - coord->coord.t[0];
    vec->vy         = 0;
    vec->vz         = target->coord.t[2] - coord->coord.t[2];
    work->field_38A = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
    work->field_386 = 0x19;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto dist;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
dist:
    dist = SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz);
    if (dist < 0x2BC) {
        raw   = work->field_38A - (u16)work->field_388;
        diff  = raw;
        adiff = diff >= 0 ? diff : -diff;
        if (adiff < 0x800) {
            ang = adiff;
            goto wrap_done;
        }
        if (diff > 0) {
            ang = 0x1000 - raw;
            goto wrap_done;
        }
        ang = raw + 0x1000;
    wrap_done:
        if ((s16)ang < 0x32) {
            work->field_37E = 4;
            work->field_384 = 0;
            work->field_386 = 0;
            work->field_37C = 1;
            goto pop;
        }
        work->field_384 = 0;
        goto pop;
    }
    work->field_384 = 0x32;
    goto pop;
case1:
    if ((s16)work->field_382 == 0x14) {
        work->field_31A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if ((s16)work->field_382 < 0x20) {
        goto pop;
    }
    work->field_37E  = 3;
    work->field_37C  = 2;
    work->field_31A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    goto pop;
case2:
    vel = 0;
    if ((s16)work->field_382 < 0xB) {
        vel = -0x78;
    }
    work->field_384 = vel;
    if ((s16)work->field_382 < 0x1F) {
        goto pop;
    }
    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070004;
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((s32)((gRandomLcgState >> 16) & 0xF) < D_actor_300700_8016939C[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex]) {
        work->field_37C = 0;
        work->field_37E = state;
        goto pop;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
    work->field_38E = 0;
    work->field_394 = 0;
pop:
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Three-state launcher. State 0 arms the timer from `gRandomLcgState` and stores
/// the direction from the player to the model's root coordinate into
/// `field_370` with `VectorNormalS`; state 1 pushes the coordinate along that
/// normal while `field_382` is below `0xF`, runs the `field_38C` countdown and
/// hands over to the teardown state 3 (or 2) when it expires; state 2 clears
/// the state machine once `field_382` reaches `0x20`.
static void func_actor_300700_801643D0(Task* arg0)
{
    VECTOR           vec;
    Actor300700Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              state;
    s32              one;
    s32              rng;
    s32              posX;

    one   = 1;
    work  = arg0->work;
    obj   = arg0->extra.tmd;
    state = work->field_37C;
    coord = obj->coords;
    switch (state) {
        case 0:
            work->field_37E = 0xA;
            work->field_380 = one;
            work->field_384 = 0;
            work->field_386 = 0;
            work->field_396 = one;
            work->field_37C = one;
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_38C = (((u32)rng >> 16) & 0x1F) + 0xF;
            gRandomLcgState = rng;
            posX            = coord->coord.t[0];
            vec.vx          = gPlayerStatus.coordMtx->t[0] - posX;
            vec.vy          = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            vec.vz          = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            VectorNormalS(&vec, &work->field_370);
            return;
        case 1:
            if ((s16)work->field_382 < 0xF) {
                coord->coord.t[0] += -(work->field_370.vx * 50) >> 12;
                coord->coord.t[2] += -(work->field_370.vz * 50) >> 12;
            }
            if ((u32)(work->field_382 - 6) < 9) {
                work->field_386 = 0x93;
                work->field_38A = (work->field_38A + 0x5C7) & 0xFFF;
            } else {
                work->field_386 = 0;
            }
            work->field_38C = work->field_38C - 1;
            if ((s16)work->field_38C <= 0) {
                if ((((Enemy*)arg0->spawnArg2.pointer)->reactionFlags & ENEMY_REACTION_BUILDUP) != 0) {
                    work->field_37E = 8;
                    work->field_37A = 3;
                    work->field_37C = 3;
                    return;
                }
                work->field_37E = 9;
                work->field_37C = 2;
            }
            return;
        case 2:
            if ((s16)work->field_382 >= 0x20) {
                work->field_37A = 0;
                work->field_37C = 0;
                work->field_37E = one;
                work->field_38C = 0;
                work->field_394 = one;
                work->field_396 = 0;
            }
            break;
    }
}

/// Per-frame tick. State 0 arms the timer and latches `field_396`; state 1
/// waits it out; state 2 counts `field_38C` down and moves to the teardown
/// state 3, which waits for `Gp_TickObjFlag2` on the spawn block and then
/// clears the hit flag and resets the state machine.
static void func_actor_300700_801645F8(Task* arg0)
{
    Enemy*           ctx;
    Actor300700Work* work;
    s16              state;
    s32              rng;
    s32              rng2;
    u16              timer;

    work  = arg0->work;
    state = work->field_37C;
    switch (state) {
        case 0:
            work->field_384 = 0;
            work->field_386 = 0;
            if (work->field_396 == 0) {
                work->field_37C = 1;
                work->field_37E = 6;
            } else {
                work->field_37C = 2;
                rng             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                work->field_38C = ((u32)rng >> 0x10) & 0xF;
            }
            work->field_396 = 1;
            work->field_380 = 1;
            return;
        case 1:
            if ((s16)work->field_382 >= 0x1D) {
                work->field_37C = 2;
                rng2            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng2;
                work->field_38C = ((u32)rng2 >> 0x10) & 0xF;
                return;
            }
            return;
        case 2:
            timer           = work->field_38C - 1;
            work->field_38C = timer;
            if ((timer << 0x10) <= 0) {
                work->field_37E = 8;
                work->field_37C = 3;
                return;
            }
            break;
        case 3:
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                ctx                 = arg0->spawnArg2.pointer;
                ctx->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
                work->field_37A     = 0;
                work->field_37C     = 0;
                work->field_37E     = 1;
                work->field_38C     = 0;
                work->field_394     = 1;
                work->field_396     = 0;
                work->field_398     = 0;
            }
            break;
    }
}

static void func_actor_300700_80164794(Task* arg0)
{
    Actor300700Work*  work;
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
    want  = work->field_38A;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_388 = ang;
    if (adiff < 0x800) {
        step = work->field_386;
        if (step >= adiff) {
            work->field_388 = want;
        } else {
            next = work->field_388;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_388 = next;
        }
    } else {
        step = work->field_386;
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
        work->field_388 = work->field_38A;
        goto done;
    turn:
        wrapStep = work->field_386;
        cur      = work->field_388;
        if (diff > 0) {
            work->field_388 = cur - wrapStep;
        } else {
            work->field_388 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_388;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void func_actor_300700_801648E4(Enemy* arg0, Task* arg1)
{
    Actor300700Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    Actor300700Work* work2;
    GfxCoord*        c;
    VECTOR           vec;
    s32              state;
    s32              i;
    s16              st;
    s16              phase;
    s16              val;
    s32              snd;
    s32              pan;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    state = gSceneCombatState.actorControl;
    coord = obj->coords;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
case2:
    obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return;
default_body:
    st = work->field_37C;
    if (st == 1) {
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
    return;
death:
    work->field_37E = 6;
    work->field_38C = 0;
    work->field_390 = 0x1000;
    work->field_340 = coord->coord;
    arg0->recs      = 0;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj1);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj2);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj3);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj4);
    Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
    Gp_ReleaseStateF0Add(arg1, 7);
    work->field_37C = 1;
    work2           = arg1->work;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = D_actor_300700_801693E4[(s16)work2->field_37E];
        for (i = 1; i < 7; i++) {
            func_800B4114(&work2->anim, i, (s16)work2->field_37E, 0, val);
        }
    } else {
        work2->field_382++;
        for (i = 1; i < 7; i++) {
            Gp_AnimTickIndex(&work2->anim, i);
        }
    }
    c      = arg1->extra.tmd->coords;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    snd = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070005;
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    return;
dying:
    func_actor_300700_8016539C(arg1);
    phase           = work->field_38C + 1;
    work->field_38C = phase;
    if (phase == 10) {
        obj->flags = TMD_OBJECT_SEMI_TRANS;
    }
    if ((s16)work->field_38C == 15) {
        Gp_SpawnEff(0x600A5, coord, 1, NULL);
    }
    if ((s16)work->field_38C >= 0x3C) {
        work->field_37C = 2;
    }
    work2 = arg1->work;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = D_actor_300700_801693E4[(s16)work2->field_37E];
        for (i = 1; i < 7; i++) {
            func_800B4114(&work2->anim, i, (s16)work2->field_37E, 0, val);
        }
    } else {
        work2->field_382++;
        for (i = 1; i < 7; i++) {
            Gp_AnimTickIndex(&work2->anim, i);
        }
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
}

static void func_actor_300700_80164CE0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_300700_80161E30;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_300700_80164D3C(Enemy* arg0, Task* arg1)
{
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor300700Work* work;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            obj->flags                   = 0;
            arg0->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            func_actor_300700_801652F4(arg1);
            func_actor_300700_8016534C(arg1);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (arg0->reactionFlags != 0) {
        func_actor_300700_80164E38(arg1);
    }
    func_actor_300700_801637E4(arg1);
    func_actor_300700_80164F68(arg1);
    if (work->field_386 != 0) {
        func_actor_300700_80164794(arg1);
    }
    func_actor_300700_801651A0(arg1);
    func_actor_300700_80165230(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    func_actor_300700_801652F4(arg1);
    func_actor_300700_8016534C(arg1);
}

/// Damage tick: folds the generic hit flags into the work state, applies the
/// pending hit and drops the actor to its death state once the hit points run
/// out.
static void func_actor_300700_80164E38(Task* arg0)
{
    Enemy*           ctx;
    Actor300700Work* work;
    s32              tick;
    u16              health;
    u8               flags;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if (flags & ENEMY_REACTION_STAGGER) {
        ctx->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        work->field_37A    = 2;
        work->field_37C    = 0;
    }
    if ((ctx->reactionFlags & ENEMY_REACTION_BUILDUP) && ((u32)((u16)work->field_37A - 2) >= 2U)) {
        work->field_37A = 3;
        work->field_37C = 0;
        work->field_398 = 1;
    }
    if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        tick = Gp_TickObjFlag4(ctx);
        if (tick != 0) {
            func_800DA6E8(&ctx->node, tick, 0);
            health  = ctx->hp - tick;
            ctx->hp = health;
            if ((health << 0x10) <= 0) {
                work->field_37A = 5;
                work->field_37C = 0;
                arg0->state     = 2;
            } else {
                work->field_37A = 4;
                work->field_37C = 0;
            }
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

static void func_actor_300700_80164F68(Task* arg0)
{
    switch (((Actor300700Work*)arg0->work)->field_37A) {
        case 0:
            func_actor_300700_80163D64(arg0);
            break;
        case 1:
            func_actor_300700_80164070(arg0);
            break;
        case 2:
            func_actor_300700_801643D0(arg0);
            break;
        case 3:
            func_actor_300700_801645F8(arg0);
            break;
        case 4:
            func_actor_300700_801650C0(arg0);
            break;
        case 5:
            break;
    }
}

/// Randomised footstep timer. Each tick decrements the counter and, when it
/// runs out, reseeds it from the shared LCG and plays the step sound at the
/// object's pan and depth.
static void func_actor_300700_80165000(Task* arg0)
{
    Actor300700Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pan;
    u16              timer;
    u32              random;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    timer           = work->field_392 - 1;
    work->field_392 = timer;
    if ((s16)timer <= 0) {
        random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->field_392 = (u16)(((random >> 0x10) & 0x7F) + 0x96);
        gRandomLcgState = random;
        snd             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070001;
        pan             = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, (s32)pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// State machine for the actor's contact sound: state 0 arms the timer and
/// plays the hit sound once, state 1 clears the state pair once the
/// countdown reaches 0x18.
static void func_actor_300700_801650C0(Task* arg0)
{
    Actor300700Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              state;
    s32              snd;
    s32              pan;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    state = work->field_37C;
    coord = obj->coords;
    if (state == 0) {
        goto case0;
    }
    if (state == 1) {
        goto case1;
    }
    return;
case0:
    work->field_37E = 5;
    work->field_380 = 1;
    work->field_384 = 0;
    work->field_386 = 0;
    work->field_37C = 1;
    snd             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070002;
    pan             = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    return;
case1:
    if ((s16)work->field_382 < 0x18) {
        return;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = state;
    work->field_38C = 0;
    work->field_394 = state;
}

/// Records the model's current root position in the work block, then displaces
/// the root coordinate by the work's step along the rotation's third column
/// (X and Z only) and by 0x80 on Y.
static void func_actor_300700_801651A0(Task* arg0)
{
    Actor300700Work* work;
    GfxCoord*        coord;

    coord              = arg0->extra.tmd->coords;
    work               = arg0->work;
    work->field_360    = coord->coord.t[0];
    work->field_364    = coord->coord.t[1];
    work->field_368    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_384) >> 0xC;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_384) >> 0xC;
}

static void func_actor_300700_80165230(Task* arg0)
{
    Actor300700Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    if ((s16)work->field_37E != work->field_380) {
        work->field_380 = work->field_37E;
        work->field_382 = 0;
        value           = D_actor_300700_801693E4[(s16)work->field_37E];
        for (i = 1; i < 7; i++) {
            func_800B4114(&work->anim, i, (s16)work->field_37E, 0, value);
        }
    } else {
        work->field_382++;
        for (i = 1; i < 7; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Updates the actor's lighting colour from the world position of its model
/// root, with both extra arguments zero.
static void func_actor_300700_801652F4(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

static void func_actor_300700_8016534C(Task* arg0)
{
    GfxCoord* coord;
    VECTOR3   vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x1C0, 0x80);
}

static void func_actor_300700_8016539C(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    Actor300700Work*   work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->field_390 >= 0x201) {
        work->field_390 = (u16)work->field_390 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_390;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_340;
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
