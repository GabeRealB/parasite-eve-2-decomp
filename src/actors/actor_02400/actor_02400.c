#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
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
#include "../../shared/fireball.h"

/// Main-executable counter whose lowest bit the flicker alternates on.

/// Work block of the main body, hung off `Task::work` by its spawn handler
/// (`memCalloc(0x154)`).
///
/// `obj40` is the body's collision object with the four contact records
/// `rec60`; `objC0` is the second body with its single record `recE0`, whose
/// `flags` bit 15 is raised while the body may be hit. `effArg` is the
/// coordinate and argument the hit sparks are spawned with. `field_100` keeps
/// the unscaled model matrix that the per-axis scale `field_128..field_12C` is
/// applied to each frame, and `field_120..field_124` the position before this
/// frame's step. `field_13C` is the behaviour mode and `field_13E` the phase
/// within it, `field_140` their frame counter; `field_130` is the task of the
/// effect the body holds while it grows. `variant` picks one of the two model
/// and parameter sets.
typedef struct Actor02400Work {
    /* 0x000 */ MATRIX                color;
    /* 0x020 */ MATRIX                light;
    /* 0x040 */ WorldCollisionBody    obj40;
    /* 0x060 */ WorldCollisionContact rec60[4];
    /* 0x0C0 */ WorldCollisionBody    objC0;
    /* 0x0E0 */ WorldCollisionContact recE0;
    /* 0x0F8 */ EffectSpawnArg        effArg;
    /* 0x100 */ MATRIX                field_100;
    /* 0x120 */ s16                   field_120;
    /* 0x122 */ s16                   field_122;
    /* 0x124 */ s16                   field_124;
    /* 0x126 */ byte                  pad_126[2];
    /* 0x128 */ s16                   field_128;
    /* 0x12A */ s16                   field_12A;
    /* 0x12C */ s16                   field_12C;
    /* 0x12E */ byte                  pad_12E[2];
    /* 0x130 */ Task**                field_130;
    /* 0x134 */ s16                   field_134;
    /* 0x136 */ s16                   field_136;
    /* 0x138 */ s16                   field_138;
    /* 0x13A */ s16                   field_13A;
    /* 0x13C */ s16                   field_13C;
    /* 0x13E */ s16                   field_13E;
    /* 0x140 */ s16                   field_140;
    /* 0x142 */ s16                   field_142;
    /* 0x144 */ s16                   field_144;
    /* 0x146 */ u16                   field_146;
    /* 0x148 */ s16                   field_148;
    /* 0x14A */ u16                   field_14A;
    /* 0x14C */ s16                   field_14C;
    /* 0x14E */ s16                   variant;
    /* 0x150 */ u16                   field_150;
    /* 0x152 */ s16                   field_152;
} Actor02400Work;
STATIC_ASSERT_SIZEOF(Actor02400Work, 0x154);

/// Work block of the projectile the main body spawns, hung off `Task::work` by
/// its spawn handler (`memCalloc(0xB4)`).
///
/// `obj_0` and `obj_20` are its two collision bodies sharing the record
/// `rec_40`, and `obj_58` the swept shape `pose_78`, whose table is `field_90`.
/// `field_A8..field_AC` is the direction it travels, taken from the parent's
/// Z axis. `field_B0` is the lifetime timer and `field_B2` the teardown phase.
typedef struct Actor02400ChildWork {
    /* 0x00 */ WorldCollisionBody    obj_0;
    /* 0x20 */ WorldCollisionBody    obj_20;
    /* 0x40 */ WorldCollisionContact rec_40;
    /* 0x58 */ WorldCollisionBody    obj_58;
    /* 0x78 */ WorldCollisionCapsule pose_78;
    /* 0x90 */ WorldCollisionContact field_90;
    /* 0xA8 */ s16                   field_A8;
    /* 0xAA */ s16                   field_AA;
    /* 0xAC */ s16                   field_AC;
    /* 0xAE */ byte                  pad_AE[0x2];
    /* 0xB0 */ u16                   field_B0;
    /* 0xB2 */ s16                   field_B2;
} Actor02400ChildWork;
STATIC_ASSERT_SIZEOF(Actor02400ChildWork, 0xB4);

/// Scratchpad block the model's scale is applied through: an identity `mat`
/// scaled per axis by `scale`, and the coordinate's translation `t`, restored
/// after the multiply.
typedef struct Actor02400ScaleScratch {
    /* 0x00 */ GfxMatrix mat;
    /* 0x20 */ VECTOR    scale;
    /* 0x30 */ VECTOR    t;
} Actor02400ScaleScratch;
STATIC_ASSERT_SIZEOF(Actor02400ScaleScratch, 0x40);

extern DamageAttack Actor02400_BodyPairs[4];
extern EnemyParams  Actor02400_Params0;
extern EnemyParams  Actor02400_Params1;
/// Frames the grown body waits before it spawns, indexed by `variant`.
extern s16      Actor02400_D045D4[];
extern s16      Actor02400_D045D8[];
extern s16      Actor02400_D045DC[];
extern s16      Actor02400_D0463C[];
extern TaskDesc Actor02400_D0465C[];

static void Actor02400_Fn0095C(Enemy* enemy, Task* task);
static void Actor02400_Fn024F8(Enemy* enemy, Task* task);
static void Actor02400_Fn02790(Enemy* enemy, Task* task);
static void Actor02400_Fn02AF0(Enemy* enemy, Task* task);
static void Actor02400_Fn02E0C(Enemy* enemy, Task* task);
static void Actor02400_Fn033B4(Enemy* enemy, Task* task);
static void Actor02400_Fn02EDC(Task* task);
static void Actor02400_Fn02F94(Task* task);
static void Actor02400_Fn03098(Task* task);
static void Actor02400_Fn03140(Task* task);
static void Actor02400_Fn031D0(Task* task);
static void Actor02400_Fn03228(Task* task);
static void Actor02400_Fn03278(Task* task);

static TmdSource _gActor02400AmoebaBody;
void             Actor02400_Fn02DB0(Task*);
void             Actor02400_Fn03358(Task*);

static TmdBone _gActor02400AmoebaBodySkeleton[4] = {
#include "assets/amoeba_body_skeleton.inc"
};

static u32 _gActor02400AmoebaBodyPartVerts[4] = {
#include "assets/amoeba_body_partVerts.inc"
};

static SVECTOR _gActor02400AmoebaBodyVerts[87] = {
#include "assets/amoeba_body_verts.inc"
};

static SVECTOR _gActor02400AmoebaBodyNormals[58] = {
#include "assets/amoeba_body_normals.inc"
};

static u32 _gActor02400AmoebaBodyStream[772] = {
#include "assets/amoeba_body_stream.inc"
};

static TmdSource _gActor02400AmoebaBody = {
    0,
    4168,
    1040,
    4,
    _gActor02400AmoebaBodyPartVerts,
    _gActor02400AmoebaBodyVerts,
    _gActor02400AmoebaBodyNormals,
    _gActor02400AmoebaBodySkeleton,
    _gActor02400AmoebaBodyStream,
};

DamageAttack Actor02400_BodyPairs[4] = {
    { 0, 8 },
    { 28, 6 },
    { 0, 11 },
    { 38, 6 },
};

EnemyParams Actor02400_Params0 = { Actor02400_BodyPairs, 80, 12, 86, 8, 0, 0, 100, 99 };

EnemyParams Actor02400_Params1 = { Actor02400_BodyPairs, 280, 16, 420, 30, 0, 0, 100, 99 };

s16 Actor02400_D045D4[2] = {
    90,
    30,
};

s16 Actor02400_D045D8[2] = {
    2,
    5,
};

s16 Actor02400_D045DC[48] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    3,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
    2,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    1,
    0,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
};

s16 Actor02400_D0463C[16] = {
    0,
    1,
    0,
    0,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
};

TaskDesc Actor02400_D0465C[2] = {
    { { { TASK_BODY_TMD, 96 } }, Actor02400_Fn02DB0, { .model = &_gActor02400AmoebaBody } },
    { { { TASK_BODY_COORD, 96 } }, Actor02400_Fn03358, { .value = 0 } },
};

static void Actor02400_Fn00C08(Task* task);
static void Actor02400_Fn01420(Task* task);
static void Actor02400_Fn01590(Task* task);
static void Actor02400_Fn01A10(Task* task);
static void Actor02400_Fn01B90(Task* task);
static void Actor02400_Fn01F74(Task* task);
static void Actor02400_Fn0208C(Task* task);
static void Actor02400_Fn02264(Task* task);
static void Actor02400_Fn023B4(Task* task);

#include "../../shared/fireball_glow.inc.c"

#include "../../shared/fireball_ground_glow.inc.c"

/// The main body's state handlers, run by `Actor02400_Fn02DB0` for the task's
/// state: spawn, per-frame tick and death.
static const EnemyTaskFuncTable3 Actor02400_D00004 = {
    { Actor02400_Fn0095C, Actor02400_Fn02E0C, Actor02400_Fn024F8 },
};

/// Spawn handler of the main body: allocates the work block, picks the model
/// and parameter variant from the placement, links the enemy node and both
/// collision bodies, starts the scale at 0x600 with a random idle countdown,
/// and moves the task to state 1.
static void Actor02400_Fn0095C(Enemy* enemy, Task* task)
{
    TmdObject*      obj;
    GfxCoord*       coord;
    Actor02400Work* work;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x154, false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    work->variant       = enemy->place->mode & 1;
    if (work->variant != 0) {
        obj->clutRowOffset += 1;
        tmdProcessStream(obj);
        tmdProcessStream(obj);
    }
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    enemy->bodyPos.vy             = -0x96;
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->recs                   = work->rec60;
    if (work->variant == 0) {
        enemy->param = &Actor02400_Params0;
        enemy->hp    = Actor02400_Params0.hpMax;
    } else {
        enemy->param = &Actor02400_Params1;
        enemy->hp    = Actor02400_Params1.hpMax;
    }
    work->effArg.coord      = &task->extra.tmd->coords[1];
    work->effArg.spawnArgLo = 0x200;
    work->effArg.spawnArgHi = 1;
    Gp_IncStateF0Ref(0);
    work->field_128              = 0x600;
    work->field_12A              = 0x600;
    work->field_12C              = 0x600;
    work->field_100              = coord->coord;
    gRandomLcgState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_140              = (gRandomLcgState >> 16) & 0xF;
    work->obj40.key              = 0x30018;
    work->obj40.coord            = coord;
    work->obj40.context.contacts = work->rec60;
    work->obj40.pos.vy           = -0xC8;
    work->obj40.pos.vx           = 0;
    work->obj40.pos.vz           = 0;
    work->obj40.radius           = 0xC8;
    work->obj40.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj40);
    Gp_InitRec18Table(work->rec60, 4, 0);
    work->obj40.flags           |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->objC0.coord            = &task->extra.tmd->coords[3];
    work->objC0.context.contacts = &work->recE0;
    work->objC0.pos.vx           = 0;
    work->objC0.pos.vy           = 0;
    work->objC0.pos.vz           = 0x1F4;
    work->objC0.key              = Gp_PackPair(Actor02400_BodyPairs, work->variant * 2);
    work->objC0.radius           = 0x64;
    work->objC0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->objC0);
    Gp_InitRec18Table(&work->recE0, 1, 0);
    work->objC0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    task->state        = 1;
}

/// Resolves this frame's contacts. The push-back from `func_800E0C10` moves
/// the body (or restores its saved position), then each of the four records is
/// handled by kind: a hit (kind 2) outside the post-hit cooldown `field_136`
/// is classified by the attacker's parameters, deals damage, may knock the
/// body back into mode 4 or 5, spawns sparks and plays the hurt sound, and a
/// kill sets the task to state 2; a solid contact (kind 3) pushes the body
/// out along the deepest overlap. A hit on the second body's record costs the
/// player the variant's MP.
static void Actor02400_Fn00C08(Task* task)
{
    ActorPushFrame* scratch;
    GfxCoord*       coord;
    GfxCoord*       src;
    Actor02400Work* work;
    Enemy*          enemy;
    s32             push;
    s32             reach;
    s32             val;
    s32             res;
    s32             i;
    s32             z;
    s32             kind;
    s32             param;
    s32             damage;
    s32             lastId;
    s16             dmg;
    s32             sndId;
    s32             pan;
    s32             stun;

    push    = 0;
    lastId  = 0;
    work    = task->work;
    scratch = (ActorPushFrame*)SCRATCH_STACK_RESERVE_BYTES(0x58);
    coord   = task->extra.tmd->coords;
    enemy   = task->spawnArg2.pointer;
    res     = func_800E0C10(work->rec60, &scratch->delta, 4, NULL);
    if (res == 1)
        goto move_delta;
    if (res < 2)
        goto move_done;
    if (res == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
    coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
    z                  = coord->coord.t[2] + scratch->delta.fixed.vz.halves.integer;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_120;
    coord->coord.t[1] = work->field_122;
    z                 = work->field_124;
move_z:
    coord->coord.t[2] = z;
move_done:
    if (work->field_136 != 0) {
        work->field_136--;
        if (work->field_136 <= 0) {
            work->field_136 = 0;
        }
    }
    i = 0;
    do {
        switch ((u32)work->rec60[i].key.value >> 16) {
            case 2:
                if (work->field_136 != 0) {
                    break;
                }
                kind   = 0;
                damage = 0;
                if (!(work->rec60[i].key.value & 0x8000)) {
                    kind = Actor02400_D045DC[work->rec60[i].key.value & 0x7F];
                }
                switch (Gp_GetIdParam0(work->rec60[i].key.value) & 0xFFFF) {
                    case 3:
                    case 4:
                        kind = 3;
                        break;
                    case 1:
                    case 7:
                        kind = 1;
                        break;
                    case 0:
                    case 2:
                    case 5:
                    case 6:
                    case 8:
                    case 9:
                        break;
                }
                switch (kind) {
                    case 0:
                        src                      = gPlayerActorTasks[(work->rec60[i].key.value >> 7) & 1]->extra.tmd->coords;
                        scratch->delta.vector.vx = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vector.vy = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vector.vz = src->coord.t[2] - coord->coord.t[2];
                        work->field_150         += Gp_ComputeDamage(work->rec60[i].key.value, SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz), 0, 0);
                        if ((s16)work->field_150 >= 20 || work->field_13C == 5) {
                            work->field_13C = 5;
                            work->field_13E = 0;
                            work->field_152 = 0;
                            work->field_150 = 0;
                        } else {
                            work->field_13C = 4;
                            work->field_13E = 0;
                            work->field_152 = 60;
                        }
                        work->field_140 = 0;
                        work->field_138 = 0;
                        work->field_13A = 0;
                        work->field_134 = 0;
                        break;
                    case 1:
                        src                      = gPlayerActorTasks[(work->rec60[i].key.value >> 7) & 1]->extra.tmd->coords;
                        scratch->delta.vector.vx = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vector.vy = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vector.vz = src->coord.t[2] - coord->coord.t[2];
                        damage                   = Gp_ComputeDamage(work->rec60[i].key.value, SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz), 0, 0);
                        work->field_13C          = 4;
                        work->field_13E          = 0;
                        work->field_140          = 0;
                        work->field_138          = 0;
                        work->field_13A          = 0;
                        work->field_134          = 0;
                        param                    = Gp_GetIdParam1(work->rec60[i].key.value) & 0xFFFF;
                        if (Actor02400_D0463C[param] == 0 && lastId != work->rec60[i].key.value) {
                            lastId = work->rec60[i].key.value;
                            func_800FDB18(param, coord, NULL, &work->effArg);
                        }
                        break;
                    case 2:
                        src                      = gPlayerActorTasks[(work->rec60[i].key.value >> 7) & 1]->extra.tmd->coords;
                        scratch->delta.vector.vx = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vector.vy = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vector.vz = src->coord.t[2] - coord->coord.t[2];
                        damage                   = (s16)Gp_ComputeDamage(work->rec60[i].key.value, SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz), 0, 0) * 5;
                        work->field_13C          = 4;
                        work->field_13E          = 0;
                        work->field_140          = 0;
                        work->field_138          = 0;
                        work->field_13A          = 0;
                        work->field_134          = 0;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 2, NULL);
                        break;
                    case 3:
                        work->field_13C    = 4;
                        work->field_13E    = 0;
                        work->objC0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (work->variant == 0) {
                            damage = Actor02400_Params0.hpMax;
                        } else {
                            damage = Actor02400_Params1.hpMax;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        damage         += (s16)(((gRandomLcgState >> 16) & 0x7F) + 200);
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 2, NULL);
                        break;
                }
                dmg = damage;
                func_800E2C78(task->spawnArg2.pointer, work->rec60[i].key.value, dmg, 0);
                func_800DA6E8(&((Enemy*)task->spawnArg2.pointer)->node, dmg, 0);
                if ((enemy->hp -= damage) <= 0) {
                    task->state = 2;
                }
                stun = Gp_GetIdParam2(work->rec60[i].key.value);
                if (stun > 0) {
                    work->field_136 = stun;
                }
                sndId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40180003;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sndId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                break;
            case 0:
            case 1:
                break;
            case 3:
                scratch->delta.vector.vx = coord->workm.t[0] - work->rec60[i].point.vx;
                scratch->delta.vector.vy = coord->workm.t[1] - work->rec60[i].point.vy;
                scratch->delta.vector.vz = coord->workm.t[2] - work->rec60[i].point.vz;
                reach                    = work->rec60[i].distance - SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz);
                val                      = reach;
                if (reach <= 0) {
                    val = 0;
                }
                reach = val;
                if (push < reach) {
                    push = reach;
                    VectorNormal(&scratch->delta.vector, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->dir);
                }
                break;
        }
    } while (++i < 4);
    if (push > 0) {
        coord->coord.t[0] += (push * scratch->dir.vx) >> 12;
        coord->coord.t[2] += (push * scratch->dir.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->rec60);
    if (Gp_FindRec18(&work->recE0, 0) != 0) {
        work->objC0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(&work->recE0);
        work->field_14C = 1;
        Gp_SpendMp(Actor02400_D045D8[work->variant]);
    }
    if (work->field_152 != 0) {
        work->field_152--;
        if (work->field_152 <= 0) {
            work->field_150 = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x58);
}

/// The projectile's state handlers, run by `Actor02400_Fn03358` for the task's
/// state: spawn, flight and teardown.
static const EnemyTaskFuncTable3 Actor02400_D0003C = {
    { Actor02400_Fn02790, Actor02400_Fn02AF0, Actor02400_Fn033B4 },
};

/// Mode 0, dormant: counts `field_140` down, re-arming it at random and
/// checking the global wake flag each time it runs out, and wakes the body
/// (mode 1, phase 2, unit scale) when the player comes within 1500 units on
/// the ground plane or once a projectile has been spawned.
static void Actor02400_Fn01420(Task* task)
{
    Actor02400Work* work;
    GfxCoord*       coord;
    s32             flag;
    s32             dx;
    s32             dz;
    u32             random;
    VECTOR*         delta;
    VECTOR*         scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                      = task->work;
    coord                                                                     = task->extra.tmd->coords;
    flag                                                                      = 0;
    if (--work->field_140 < 0) {
        random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->field_140 = (random >> 0x10) & 0xF;
        gRandomLcgState = random;
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_PE_ACTIVE) {
            flag = 1;
        }
    }
    scratchEnd[-1].vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy         = 0;
    dz                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    delta->vz         = dz;
    dx                = scratchEnd[-1].vx;
    if (SquareRoot0((dx * dx) + (dz * dz)) < 0x5DC) {
        flag = 1;
    }
    if (gSceneCombatState.actor02400Alert != 0) {
        flag = 1;
    }
    if (flag != 0) {
        work->field_13C = 1;
        work->field_13E = 2;
        work->field_128 = 0x1000;
        work->field_12A = 0x1000;
        work->field_12C = 0x1000;
        work->field_140 = 0;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += 1;
}

/// Mode 1, awake: phase 0 turns to face the player and phase 1 rests, each
/// for a random time before handing to the other; phase 2 pulses the scale up
/// until Y passes 0x1C00. Outside phase 2 X and Z relax to unit scale while Y
/// bobs around a ceiling, and the forward speed `field_138` eases toward
/// `field_148`. Once `field_14C` reports a hit on the second body, the body
/// resets into mode 2, rolls its second part at random, plays the sound and
/// takes hold of the `gRoomEffectGlowDiscId` effect it spawns.
static void Actor02400_Fn01590(Task* task)
{
    Actor02400Work*   work;
    GfxCoord*         coord;
    s16               limit;
    s32               diff;
    s32               step;
    s32               sound;
    s32               pan;
    Task**            eff;
    ActorFaceScratch* scratch;
    ActorFaceScratch* scratchEnd;

    scratchEnd                                                                          = *(ActorFaceScratch**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    *(ActorFaceScratch**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = scratchEnd - 1;
    scratch                                                                             = scratchEnd - 1;
    work                                                                                = task->work;
    coord                                                                               = task->extra.tmd->coords;
    limit                                                                               = 0x1000;
    switch (work->field_13E) {
        case 0:
            scratchEnd[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            scratch->delta.vy       = 0;
            scratch->delta.vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_146         = ratan2((s16)scratchEnd[-1].delta.vx, (s16)scratch->delta.vz) & 0xFFF;
            work->field_13A         = 0x19;
            work->field_148         = 0xA;
            work->field_140--;
            if (work->field_140 < 0) {
                work->field_13E = 1;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_140 = ((gRandomLcgState >> 16) & 0x3F) + 0x1E;
            }
            break;
        case 1:
            work->field_13A = 0;
            work->field_148 = 0x14;
            work->field_140--;
            limit = 0x600;
            if (work->field_140 < 0) {
                work->field_13E = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_140 = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
        case 2:
            work->field_13A = 0;
            if (work->field_142 == 0) {
                work->field_140 += 0x40;
                if (work->field_140 >= 0x100) {
                    work->field_142 = 1;
                }
            } else {
                work->field_140 -= 0x40;
                if (work->field_140 < -0xFF) {
                    work->field_142 = 0;
                }
            }
            work->field_128 += (u16)work->field_140 + 0x80;
            work->field_12A += (u16)work->field_140 + 0x80;
            work->field_12C += (u16)work->field_140 + 0x80;
            if (work->field_12A > 0x1C00) {
                work->field_13E = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_140 = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
    }
    if (work->field_13E < 2) {
        work->field_128 -= 0x80;
        if (work->field_128 <= 0x1000) {
            work->field_128 = 0x1000;
        }
        if (work->field_142 == 0) {
            work->field_12A += 0x80;
            if (work->field_12A > limit + 0x100) {
                work->field_142 = 1;
            }
        } else {
            work->field_12A -= 0x80;
            if (work->field_12A < limit - 0x100) {
                work->field_142 = 0;
            }
        }
        work->field_12C -= 0x80;
        if (work->field_12C <= 0x1000) {
            work->field_12C = 0x1000;
        }
    }
    diff = work->field_148 - work->field_138;
    step = -3;
    if (diff > 0) {
        step = 2;
    }
    if ((diff >= 0 ? diff : -diff) < (step >= 0 ? step : -step)) {
        work->field_138 = work->field_148;
    } else {
        work->field_138 += step;
    }
    if (work->field_14C != 0) {
        work->field_13C = 2;
        work->field_13E = 0;
        work->field_140 = 0;
        work->field_13A = 0;
        work->field_138 = 0;
        work->field_128 = 0x1000;
        work->field_12A = 0x1000;
        work->field_12C = 0x1000;
        scratch->rot.vy = 0;
        scratch->rot.vz = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        scratch->rot.vx = ((gRandomLcgState >> 16) & 0xFF) + 0x100;
        RotMatrix(&scratch->rot, &task->extra.tmd->coords[2].coord);
        sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40180002;
        pan   = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        eff             = (Task**)Gp_SpawnEff(gRoomEffectGlowDiscId, coord, (s32)(work->variant), NULL);
        work->field_130 = eff;
        if (eff != NULL) {
            taskReparent(task, *eff);
        }
    }
    *(ActorFaceScratch**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += 1;
}

/// Mode 2, recoiling: phase 0 raises `field_134` for up to 7 frames (cut short
/// by a further hit), phase 1 lowers it and after 7 frames either moves on to
/// mode 3 when `field_14C` is set or returns to mode 1 with a random wait.
/// Every frame the Y scale swings between 0xF00 and 0x1100.
static void Actor02400_Fn01A10(Task* task)
{
    Actor02400Work* work;
    s32             state;

    work  = task->work;
    state = work->field_13E;
    switch (state) {
        case 0:
            work->field_134 = 1;
            work->field_140++;
            if (work->field_140 >= 7) {
                work->field_13E = 1;
                work->field_140 = 0;
            }
            if (work->field_14C != 0) {
                work->field_13E = 1;
                work->field_140 = 0;
            }
            break;
        case 1:
            work->field_134 = 0;
            work->field_140++;
            if (work->field_140 >= 7) {
                if (work->field_14C != 0) {
                    work->field_14C = 0;
                    work->field_13C = 3;
                    work->field_13E = 0;
                    work->field_140 = 0;
                    work->field_142 = 0;
                    work->field_128 = work->field_12A;
                    work->field_12C = work->field_12A;
                } else {
                    work->field_13C    = state;
                    work->field_13E    = 0;
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_140    = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
                    work->objC0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
            break;
    }
    if (work->field_142 == 0) {
        work->field_12A += 0x80;
        if (work->field_12A > 0x1100) {
            work->field_142 = 1;
        }
    } else {
        work->field_12A -= 0x80;
        if (work->field_12A < 0xF00) {
            work->field_142 = 0;
        }
    }
}

/// Mode 3, spawning: phase 0 pulses the scale up until Y passes 0x1C00 and
/// sets the held effect to state 2, phase 1 faces the player while Y wobbles
/// around 0x1C00 for the variant's wait, phase 2 spawns the projectile from
/// `Actor02400_D0465C`, raises the wake flag, releases the effect and plays
/// the sound, and phase 3 shrinks the scale back to its floor before returning
/// to mode 1 with a random wait.
static void Actor02400_Fn01B90(Task* task)
{
    Actor02400Work* work;
    GfxCoord*       coord;
    s32             flags;
    s32             sound;
    s32             pan;
    VECTOR*         delta;
    VECTOR*         scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                      = task->work;
    coord                                                                     = task->extra.tmd->coords;
    flags                                                                     = 0;
    switch (work->field_13E) {
        case 0:
            work->field_13A = 0;
            work->field_138 = 0;
            if (work->field_142 == 0) {
                work->field_140 += 0x40;
                if (work->field_140 >= 0x100) {
                    work->field_142 = 1;
                }
            } else {
                work->field_140 -= 0x40;
                if (work->field_140 < -0xFF) {
                    work->field_142 = 0;
                }
            }
            work->field_128 += (u16)work->field_140 + 0x40;
            work->field_12A += (u16)work->field_140 + 0x40;
            work->field_12C += (u16)work->field_140 + 0x40;
            if (work->field_12A > 0x1C00) {
                work->field_13E = 1;
                work->field_140 = 0;
                if (work->field_130 != NULL) {
                    (*work->field_130)->state = 2;
                }
            }
            break;
        case 1:
            work->field_13A   = 0x19;
            scratchEnd[-1].vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vy         = 0;
            delta->vz         = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_146   = ratan2((s16)scratchEnd[-1].vx, (s16)delta->vz) & 0xFFF;
            if (work->field_142 == 0) {
                work->field_12A += 0x80;
                if (work->field_12A > 0x1D00) {
                    work->field_142 = 1;
                }
            } else {
                work->field_12A -= 0x80;
                if (work->field_12A < 0x1B00) {
                    work->field_142 = 0;
                }
            }
            work->field_140++;
            if (work->field_140 > Actor02400_D045D4[work->variant]) {
                work->field_13E = 2;
                work->field_140 = 0;
                work->field_13A = 0;
            }
            break;
        case 2:
            Gp_SpawnEnemyFromTable(Actor02400_D0465C, 1, 0, task->spawnArg2.pointer);
            gSceneCombatState.actor02400Alert = 1;
            work->field_13E                   = 3;
            if (work->field_130 != NULL) {
                (*work->field_130)->state = 3;
            }
            work->field_130 = NULL;
            sound           = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40180004;
            pan             = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 3:
            work->field_128 -= 0x80;
            if (work->field_128 <= 0x1000) {
                work->field_128 = 0x1000;
                flags           = 1;
            }
            work->field_12A -= 0x80;
            if (work->field_12A <= 0x600) {
                work->field_12A = 0x600;
                flags          |= 2;
            }
            work->field_12C -= 0x80;
            if (work->field_12C <= 0x1000) {
                work->field_12C = 0x1000;
                flags          |= 4;
            }
            if (flags == 7) {
                work->field_13C    = 1;
                work->field_13E    = 0;
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_140    = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
                work->objC0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            break;
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += 1;
}

/// Mode 5, stunned: shrinks the scale toward its floor, sets the held effect
/// to state 4 and lets it go, and keeps the second body unhittable; after 360
/// frames returns to mode 1 with a random wait.
static void Actor02400_Fn01F74(Task* task)
{
    Actor02400Work* work = task->work;

    work->field_128 -= 0x40;
    if (work->field_128 < 0x1000) {
        work->field_128 = 0x1000;
    }
    work->field_12A -= 0x80;
    if (work->field_12A < 0x600) {
        work->field_12A = 0x600;
    }
    work->field_12C -= 0x40;
    if (work->field_12C < 0x1000) {
        work->field_12C = 0x1000;
    }
    if (work->field_130 != NULL) {
        (*work->field_130)->state = 4;
        work->field_130           = NULL;
    }
    work->objC0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->field_140++;
    if (work->field_140 > 0x168) {
        work->field_13C    = 1;
        work->field_13E    = 0;
        gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_140    = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
        work->objC0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// Rebuilds the model coordinate from the saved matrix `field_100`, scaled per
/// axis by `field_128..field_12C`, keeping the coordinate's translation. The
/// first body's height and the second body's reach follow the Y and Z scale.
static void Actor02400_Fn0208C(Task* task)
{
    GfxCoord*               coord;
    Actor02400Work*         work;
    Actor02400ScaleScratch* scratch;
    Actor02400ScaleScratch* head;

    coord                                        = task->extra.tmd->coords;
    work                                         = task->work;
    work->field_100                              = coord->coord;
    head                                         = SCRATCH_STACK_CURSOR(Actor02400ScaleScratch);
    scratch                                      = head - 1;
    SCRATCH_STACK_CURSOR(Actor02400ScaleScratch) = scratch;
    work->obj40.pos.vy                           = -0xC8000 / work->field_12A;
    work->objC0.pos.vz                           = (work->field_12C * 250) / 4096;
    scratch->scale.vx                            = work->field_128;
    scratch->scale.vy                            = work->field_12A;
    scratch->scale.vz                            = work->field_12C;
    scratch->t.vx                                = coord->coord.t[0];
    scratch->t.vy                                = coord->coord.t[1];
    scratch->t.vz                                = coord->coord.t[2];
    coord->coord                                 = work->field_100;
    scratch->mat.rotationWords.m00M01            = ONE;
    scratch->mat.rotationWords.m02M10            = 0;
    scratch->mat.rotationWords.m11M12            = ONE;
    scratch->mat.rotationWords.m20M21            = 0;
    scratch->mat.rotationWords.m22               = ONE;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->coord.t[0] = scratch->t.vx;
    coord->coord.t[1] = scratch->t.vy;
    SCRATCH_STACK_RELEASE_BLOCK(Actor02400ScaleScratch);
    coord->coord.t[2]   = scratch->t.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Turns the body towards `field_146` by at most `field_13A` per frame and
/// rebuilds the root coordinate's rotation from the result. The yaw wraps at
/// 0x1000: when the remaining turn would overshoot through the wrap the body
/// snaps to the target instead.
static void Actor02400_Fn02264(Task* task)
{
    Actor02400Work*   work;
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
    coord = task->extra.tmd->coords;
    work  = task->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_146;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_144 = ang;
    if (adiff < 0x800) {
        step = work->field_13A;
        if (step >= adiff) {
            work->field_144 = want;
        } else {
            next = work->field_144;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_144 = next;
        }
    } else {
        step = work->field_13A;
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
        work->field_144 = work->field_146;
        goto done;
    turn:
        wrapStep = work->field_13A;
        cur      = work->field_144;
        if (diff > 0) {
            work->field_144 = cur - wrapStep;
        } else {
            work->field_144 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_144;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

/// Every 25 frames plays the body's idle sound, panned and placed from the
/// root coordinate and made louder the taller the body has grown.
static void Actor02400_Fn023B4(Task* task)
{
    GfxCoord*       object;
    s16             scale;
    s16             ramp;
    s32             soundId;
    s32             volume;
    s8              depth;
    u16             counter;
    Actor02400Work* work;

    work            = task->work;
    object          = task->extra.tmd->coords;
    counter         = work->field_14A + 1;
    work->field_14A = counter;
    if ((s16)counter >= 0x19) {
        work->field_14A = 0;
        scale           = work->field_12A;
        soundId         = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40180001;
        if (scale >= 0x1D01) {
            ramp = 0x1700;
        } else {
            ramp = (u16)work->field_12A - 0x600;
            if (scale < 0x600) {
                ramp = 0;
            }
        }
        volume = (ramp * 0x32) / 5888 + 0x32;
        depth  = 0x7F - (((0x7F - worldCoordGetOriginAudioDepth(object)) * (s16)volume) / 100);
        SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(object), depth);
    }
}

/// Death handler of the main body. In global mode 1 it only refreshes the
/// colour and in mode 2 hides the model. Otherwise phase 0 saves the model
/// matrix, unlinks the enemy node and both bodies, starts the light fade and
/// releases the held effect; phase 1 squashes the body flat, turning it
/// semi-transparent at frame 10, spawning the death effect at frame 15 and
/// hiding it at frame 60; phase 2 destroys the enemy.
static void Actor02400_Fn024F8(Enemy* arg0, Task* arg1)
{
    VECTOR          pos;
    Actor02400Work* work;
    TmdObject*      obj;
    GfxCoord*       coord;
    GfxCoord*       cur;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            switch (work->field_13E) {
                case 0:
                    work->field_140 = 0;
                    work->field_12A = 0x1000;
                    work->field_100 = coord->coord;
                    arg0->recs      = 0;
                    worldTargetUnlinkNode(&arg0->node);
                    Gp_UnlinkObj(&work->obj40);
                    Gp_UnlinkObj(&work->objC0);
                    Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
                    Gp_ReleaseStateF0Add(arg1, 0x18);
                    work->field_13E = 1;
                    cur             = arg1->extra.tmd->coords;
                    pos.vx          = cur->workm.t[0];
                    pos.vy          = cur->workm.t[1];
                    pos.vz          = cur->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
                    if (work->field_130 != NULL) {
                        (*work->field_130)->state = 4;
                    }
                    break;
                case 1:
                    work->field_140++;
                    if ((s16)work->field_140 == 10) {
                        obj->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    if ((s16)work->field_140 == 15) {
                        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 3, NULL);
                    }
                    if ((s16)work->field_140 >= 60) {
                        work->field_13E = 2;
                        obj->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    if (work->field_12A > 0x200) {
                        work->field_12A -= 0x50;
                    }
                    Actor02400_Fn03278(arg1);
                    cur    = arg1->extra.tmd->coords;
                    pos.vx = cur->workm.t[0];
                    pos.vy = cur->workm.t[1];
                    pos.vz = cur->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
                    break;
                case 2:
                    enemyDestroy(arg0, arg1);
                    break;
            }
            break;
    }
}

/// Spawn handler of the projectile: allocates its work block, places its model
/// 0x15E units along the parent's Y axis with the parent's rotation, takes the
/// parent's Z axis as its direction, links its three collision bodies (keys
/// picked by the parent's variant), arms the 90-frame lifetime, detaches from
/// the parent and moves the task to state 1.
static void Actor02400_Fn02790(Enemy* arg0, Task* arg1)
{
    Actor02400Work*      parentWork;
    Task*                parent;
    Actor02400ChildWork* work;
    ActorOffsetScratch*  scratch;
    ActorOffsetScratch*  head;
    GfxCoord*            objCoord;
    GfxCoord*            objCoord2;
    GfxCoord*            objCoord3;
    SVECTOR*             offset;
    GfxCoord*            coord;
    GfxCoord*            parentCoord;

    head                                     = SCRATCH_STACK_CURSOR(ActorOffsetScratch);
    scratch                                  = head - 1;
    SCRATCH_STACK_CURSOR(ActorOffsetScratch) = scratch;
    offset                                   = &scratch->offset;
    parent                                   = arg1->parent;
    coord                                    = arg1->extra.tmd->coords;
    parentCoord                              = parent->extra.tmd->coords;
    parentWork                               = parent->work;
    work                                     = memCalloc(0xB4, 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work         = work;
    scratch->offset.vx = 0;
    scratch->offset.vy = -0x15E;
    scratch->offset.vz = 0;
    gte_SetRotMatrix(&parentCoord->coord);
    gte_ldv0(offset);
    gte_rtv0();
    gte_stlvnl(&scratch->result);
    coord->parent       = &gGfxViewCoord;
    coord->coord        = parentCoord->coord;
    coord->coord.t[0]   = parentCoord->coord.t[0] + scratch->result.vx;
    coord->coord.t[1]   = parentCoord->coord.t[1] + scratch->result.vy;
    coord->coord.t[2]   = parentCoord->coord.t[2] + scratch->result.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_A8      = parentCoord->coord.m[0][2];
    work->field_AA      = parentCoord->coord.m[1][2];
    work->field_AC      = parentCoord->coord.m[2][2];

    objCoord                     = arg1->extra.tmd->coords;
    work->obj_0.context.contacts = &work->rec_40;
    work->obj_0.pos.vx           = 0;
    work->obj_0.pos.vy           = 0;
    work->obj_0.pos.vz           = 0;
    work->obj_0.coord            = objCoord;
    work->obj_0.key              = Gp_PackPair(Actor02400_BodyPairs, (parentWork->variant * 2) | 1);
    work->obj_0.radius           = 0xC8;
    work->obj_0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj_0);
    Gp_InitRec18Table(&work->rec_40, 1, 0);
    work->obj_0.flags            |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    objCoord2                     = arg1->extra.tmd->coords;
    work->obj_20.context.contacts = &work->rec_40;
    work->obj_20.pos.vx           = 0;
    work->obj_20.pos.vy           = 0;
    work->obj_20.pos.vz           = 0;
    work->obj_20.coord            = objCoord2;
    if (parentWork->variant == 0) {
        work->obj_20.key = 0x22D2D;
    } else {
        work->obj_20.key = 0x22E2E;
    }
    work->obj_20.radius = 0xC8;
    work->obj_20.flags  = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->obj_20);

    work->pose_78.ends[1].vz     = -0xD2;
    work->pose_78.end0Radius     = 1;
    work->pose_78.end1Radius     = 1;
    work->pose_78.ends[0].vx     = 0;
    work->pose_78.ends[0].vy     = 0;
    work->pose_78.ends[0].vz     = 0;
    work->pose_78.ends[1].vx     = 0;
    work->pose_78.ends[1].vy     = 0;
    work->pose_78.contacts       = &work->field_90;
    work->obj_20.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    objCoord3                    = arg1->extra.tmd->coords;
    work->obj_58.context.capsule = &work->pose_78;
    work->obj_58.pos.vx          = 0;
    work->obj_58.pos.vy          = 0;
    work->obj_58.pos.vz          = 0;
    work->obj_58.key             = 0;
    work->obj_58.radius          = 0;
    work->obj_58.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->obj_58.coord           = objCoord3;
    Gp_LinkObj(3, &work->obj_58);
    Gp_InitRec18Table(&work->field_90, 1, 0);
    work->field_B0      = 0x5A;
    work->obj_58.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    taskDetachFromParent(arg1);
    arg1->state = 1;
    SCRATCH_STACK_RELEASE_BLOCK(ActorOffsetScratch);
}

/// Flight handler of the projectile: moves it along `field_A8` / `field_AC`
/// and draws its glow. Once the lifetime `field_B0` runs out, its record is
/// hit, or its swept shape touches a surface whose room parameter blocks it,
/// it spawns the burst effect and moves the task to state 2.
static void Actor02400_Fn02AF0(Enemy* arg0, Task* arg1)
{
    Actor02400ChildWork* work;
    GfxCoord*            coord;
    s32                  rec;
    s32                  spawn;
    u16                  timer;

    coord = arg1->extra.tmd->coords;
    work  = arg1->work;
    spawn = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            fireballDrawGlow(coord, 0x100);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            coord->coord.t[0]  += (work->field_A8 * 0x19) >> 9;
            coord->coord.t[2]  += (work->field_AC * 0x19) >> 9;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            fireballDrawGlow(coord, 0x100);
            rec = work->field_90.key.value;
            if ((rec != 0) &&
                (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1]
                                   [func_800E1B24(rec)]
                                       ->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES)) {
                spawn = 1;
            }
            Gp_ClearRec18Occupied(&work->field_90);
            timer          = work->field_B0 - 1;
            work->field_B0 = timer;
            if (((timer << 0x10) <= 0) || (work->rec_40.flags & 1) || (spawn != 0)) {
                Gp_SpawnEff(gRoomEffectOrangeBurst2Id, coord, 0, NULL);
                arg1->state    = 2;
                work->field_B2 = 0;
            }
            break;
    }
}

#include "../../shared/fireball_ember.inc.c"

/// Task callback of the main body: runs the `Actor02400_D00004` handler for
/// the task's state.
void Actor02400_Fn02DB0(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02400_D00004;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Per-frame handler of the main body. In global mode 2 it only hides the
/// model; in mode 1 it only refreshes the colour and the ground mark. Otherwise
/// it resolves contacts, runs the current mode, turns, steps forward, moves
/// the two front parts, rescales the model and updates its coordinate first.
static void Actor02400_Fn02E0C(Enemy* enemy, Task* task)
{
    GfxCoord*  coord;
    TmdObject* obj;

    obj   = task->extra.tmd;
    coord = obj->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            goto case1;
        case SCENE_COMBAT_ACTORS_RUNNING:
            obj->flags                    = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    Actor02400_Fn00C08(task);
    Actor02400_Fn02EDC(task);
    Actor02400_Fn02264(task);
    Actor02400_Fn03140(task);
    Actor02400_Fn03098(task);
    Actor02400_Fn0208C(task);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
case1:
    Actor02400_Fn031D0(task);
    Actor02400_Fn03228(task);
}

/// Runs the handler of the body's current mode `field_13C`: dormant, awake,
/// recoiling, spawning, hurt or stunned. The awake, recoiling and spawning
/// modes also play the idle sound.
static void Actor02400_Fn02EDC(Task* task)
{
    switch (((Actor02400Work*)task->work)->field_13C) {
        case 0:
            Actor02400_Fn01420(task);
            break;
        case 1:
            Actor02400_Fn01590(task);
            Actor02400_Fn023B4(task);
            break;
        case 2:
            Actor02400_Fn01A10(task);
            Actor02400_Fn023B4(task);
            break;
        case 3:
            Actor02400_Fn01B90(task);
            Actor02400_Fn023B4(task);
            break;
        case 4:
            Actor02400_Fn02F94(task);
            break;
        case 5:
            Actor02400_Fn01F74(task);
            break;
    }
}

/// Mode 4, hurt: swings the Y scale between 0x1400 and 0x1800 in steps of
/// 0x200, releases the held effect (state 4) and keeps the second body
/// unhittable; after 16 frames returns to mode 1 with a random wait.
static void Actor02400_Fn02F94(Task* task)
{
    Actor02400Work* work;
    Task**          held;
    u16             sweep;
    u16             counter;
    u32             state;

    work = task->work;
    if (work->field_142 == 0) {
        sweep           = work->field_12A + 0x200;
        work->field_12A = sweep;
        if ((s16)sweep >= 0x1801) {
            work->field_142 = 1;
        }
    } else {
        sweep           = work->field_12A - 0x200;
        work->field_12A = sweep;
        if ((s16)sweep < 0x1400) {
            work->field_142 = 0;
        }
    }
    held = work->field_130;
    if (held != NULL) {
        (*held)->state  = 4;
        work->field_130 = NULL;
    }
    work->objC0.flags = work->objC0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    counter           = work->field_140 + 1;
    work->field_140   = counter;
    if ((s16)counter >= 0x10) {
        state             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->field_13C   = 1;
        work->field_13E   = 0;
        gRandomLcgState   = state;
        work->field_140   = (u16)(((state >> 0x10) & 0x1F) + 0x1E);
        work->objC0.flags = work->objC0.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// Moves the model's two front parts: coordinates 2 and 3 slide out along
/// their Z axis while `field_134` is set and back while it is clear, part 2
/// down to 0 and part 3 down to 0x1E.
static void Actor02400_Fn03098(Task* task)
{
    Actor02400Work* work;
    GfxCoord*       coord;
    GfxCoord*       c2;
    GfxCoord*       c3;

    work  = task->work;
    coord = task->extra.tmd->coords;
    c2    = coord + 2;
    c3    = coord + 3;

    c2->coord.t[0] = 0;
    c2->coord.t[1] = -0x5F;
    if (work->field_134 != 0) {
        c2->coord.t[2] += 0x14;
    } else {
        c2->coord.t[2] -= 0x28;
        if (c2->coord.t[2] < 0) {
            c2->coord.t[2] = 0;
        }
    }
    c2->composeStamp = GRAPHICS_COORD_DIRTY;
    c3->coord.t[0]   = 0;
    c3->coord.t[1]   = 0;
    if (work->field_134 != 0) {
        c3->coord.t[2] += 0x50;
    } else {
        c3->coord.t[2] -= 0xA0;
        if (c3->coord.t[2] < 0x1E) {
            c3->coord.t[2] = 0x1E;
        }
    }
    c3->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Saves the root coordinate's position into `field_120..field_124`, then
/// steps it forward along its own Z axis by the speed `field_138` and drops it
/// 0x80.
static void Actor02400_Fn03140(Task* task)
{
    Actor02400Work* work;
    GfxCoord*       coord;

    coord = task->extra.tmd->coords;
    work  = task->work;

    work->field_120    = coord->coord.t[0];
    work->field_122    = coord->coord.t[1];
    work->field_124    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_138) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_138) >> 12;
}

/// Refreshes the body's colour from where its root coordinate stands.
static void Actor02400_Fn031D0(Task* task)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = task->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2.pointer, &vec, 0, 0);
}

/// Draws the body's ground mark at its root coordinate's world position.
static void Actor02400_Fn03228(Task* task)
{
    GfxCoord* coord;
    VECTOR3   vec;

    coord  = task->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x200, 0x30);
}

/// Squashes the dying body: restores the saved model matrix `field_100` into
/// the root coordinate and scales it on Y by `field_12A`.
static void Actor02400_Fn03278(Task* task)
{
    void**             scratch;
    ActorScaleScratch* head;
    ActorScaleScratch* blk;
    Actor02400Work*    work;
    GfxCoord*          coord;

    scratch                                     = SCRATCH_HEAD_ADDR;
    head                                        = SCRATCH_HEAD_AT(scratch, ActorScaleScratch);
    blk                                         = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleScratch) = blk;
    coord                                       = task->extra.tmd->coords;
    work                                        = task->work;

    blk->scale.vx                    = ONE;
    blk->scale.vy                    = work->field_12A;
    blk->scale.vz                    = ONE;
    coord->coord                     = work->field_100;
    blk->matrix.rotationWords.m00M01 = ONE;
    blk->matrix.rotationWords.m02M10 = 0;
    blk->matrix.rotationWords.m11M12 = ONE;
    blk->matrix.rotationWords.m20M21 = 0;
    blk->matrix.rotationWords.m22    = ONE;
    ScaleMatrix(&blk->matrix.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_POP_AT(scratch, ActorScaleScratch);
}

/// Task callback of the projectile: runs the `Actor02400_D0003C` handler for
/// the task's state.
void Actor02400_Fn03358(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02400_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Teardown handler of the projectile: phase 0 unlinks its three collision
/// bodies and waits 60 frames, then phase 1 destroys the enemy.
static void Actor02400_Fn033B4(Enemy* arg0, Task* arg1)
{
    Actor02400ChildWork* work;
    u16                  temp_v0;

    work = arg1->work;
    switch (work->field_B2) {
        case 0:
            Gp_UnlinkObj(&work->obj_0);
            Gp_UnlinkObj(&work->obj_20);
            Gp_UnlinkObj(&work->obj_58);
            work->field_B2 = 1;
            work->field_B0 = 0x3C;
            return;
        case 1:
            temp_v0        = work->field_B0 - 1;
            work->field_B0 = temp_v0;
            if ((temp_v0 << 0x10) <= 0) {
                enemyDestroy(arg0, arg1);
            }
            return;
    }
}
