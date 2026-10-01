#include "actor_300700_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

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
#include "../../shared/rat.h"
#include "../../shared/moth.h"

extern s16 gRatSlowMoveChance[8];

extern u16 gRatSlowMoveTimes[16];

extern s16 gRatFastMoveChance[8];

extern u16 gRatFastMoveTimes[16];

extern s16 gRatAttackRepeatChance[8];

/// Per-state animation id handed to `func_800B4114`, indexed by `field_37E`.
extern s16 gRatAnimBlend[];

/// The second variant's state handlers, in the same order as the first's:
/// spawn, per-frame update and state 2. `ratTask`
/// dispatches them.
static const GpEnemyTaskFuncTable3 gRatStateHandlers = {
    {
        ratSpawn,
        ratUpdate,
        ratDeath,
    },
};

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

DamageAttack gRatAttack = { 6, 3 };

EnemyParams gRatParams = { &gRatAttack, 18, 4, 22, 1, 100, 20, 100, 0 };

s16 gRatSlowMoveChance[8] = {
    2,
    4,
    4,
    8,
    8,
    6,
    10,
    3,
};

u16 gRatSlowMoveTimes[16] = {
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

s16 gRatFastMoveChance[8] = {
    1,
    2,
    5,
    10,
    12,
    12,
    12,
    14,
};

u16 gRatFastMoveTimes[16] = {
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

s16 gRatAttackRepeatChance[8] = {
    0,
    0,
    4,
    8,
    12,
    12,
    13,
    16,
};

TaskDesc D_actor_300700_801693AC = { { { TASK_BODY_TMD, 96 } }, ratTask, { .model = &D_actor_300700_80167400 } };

AnimationSet* gRatAnimSets[11] = {
    NULL,
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

s16 gRatAnimBlend[10] = {
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
void ratContacts(Task* actor)
{
    Enemy*          ctx;
    u32             lastId;
    u32             id;
    RatWork*        work;
    GfxCoord*       coord;
    GfxCoord*       target;
    GpDeltaScratch* head;
    GpDeltaScratch* scratch;
    s32             i;
    s32             push;
    s32             bestPush;
    s32             damage;
    s32             cooldown;

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
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, (VECTOR*)(scratch + 1), (VECTOR*)(scratch + 2));
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
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, (VECTOR*)(scratch + 1), (VECTOR*)(scratch + 2));
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

#include "../../shared/rat_idle.inc.c"

#include "../../shared/rat_attack.inc.c"

#include "../../shared/rat_stagger.inc.c"

#include "../../shared/rat_buildup.inc.c"

#include "../../shared/rat_turn.inc.c"

#include "../../shared/rat_death.inc.c"

#include "../../shared/rat_task.inc.c"

#include "../../shared/rat_update.inc.c"

#include "../../shared/rat_reactions.inc.c"

void ratBehavior(Task* arg0)
{
    switch (((RatWork*)arg0->work)->field_37A) {
        case 0:
            ratIdle(arg0);
            break;
        case 1:
            ratAttack(arg0);
            break;
        case 2:
            ratStagger(arg0);
            break;
        case 3:
            ratBuildup(arg0);
            break;
        case 4:
            ratHurt(arg0);
            break;
        case 5:
            break;
    }
}

#include "../../shared/rat_idle_sound.inc.c"

#include "../../shared/rat_hurt.inc.c"

#include "../../shared/rat_step.inc.c"

#include "../../shared/rat_animate.inc.c"

#include "../../shared/rat_update_color.inc.c"

#include "../../shared/rat_shadow.inc.c"

#include "../../shared/rat_squash.inc.c"
