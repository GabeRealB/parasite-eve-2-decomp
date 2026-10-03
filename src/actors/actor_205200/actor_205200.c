#include "actor_205200_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/neo_ark_eve_access_tunnel.h"

#include "rooms/shelter_b6_corridor.h"

#include "rooms/shelter_b6_training_room.h"
#include "../../shared/screen_wave.h"

/// Work block of the controller task, allocated by its setup handler
/// `func_actor_205200_8014A72C`. It spawns the parts, tracks the live ones
/// (`field_0`/`field_18`, indexed by the slot each part took) and drives the
/// looping sound and the screen-wave timer.
typedef struct Actor205200CtrlWork {
    /* 0x00 */ GfxCoord* field_0[3];  // coords of the parts, measured by `func_actor_205200_8014ACD4`
    /* 0x0C */ GfxCoord* field_C;     // nearest of `field_0` to the stage view
    /* 0x10 */ u32       field_10;    // its distance
    /* 0x14 */ s32       field_14;    // sound-event id `func_actor_205200_8014A958` plays
    /* 0x18 */ s16       field_18[3]; // 1 marks the matching `field_0` slot live
    /* 0x1E */ s16       field_1E;    // kind from the placement; selects the spawn tables
    /* 0x20 */ s16       field_20;    // live part count, also the next part's slot and the timer reload index
    /* 0x22 */ u16       field_22;    // countdown `func_actor_205200_8014AB98` ticks in both of its sub-states
    /* 0x24 */ s16       field_24;    // state of `func_actor_205200_8014A958` (0 wait, 1 run, 2 stop, 3 done)
    /* 0x26 */ s16       field_26;    // sub-state of `func_actor_205200_8014AB98`
    /* 0x28 */ s16       field_28;    // set while the screen wave is running
    /* 0x2A */ s16       field_2A;    // delay before the sound starts
    /* 0x2C */ s16       field_2C;    // set when a part dies, forcing the nearest part to be re-measured
    /* 0x2E */ s16       field_2E;    // raised by message 0x7DB; stops the sound and sends the parts to state 2
} Actor205200CtrlWork;
STATIC_ASSERT_SIZEOF(Actor205200CtrlWork, 0x30);

/// Work block of a part task, allocated by its spawn handler
/// `func_actor_205200_8014AE0C`. `field_78` is the slot the part took in the
/// controller's `field_0` / `field_18` arrays.
typedef struct Actor205200Part {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact recs[3];
    /* 0x68 */ EffectSpawnArg        field_68; // record the part's effects are spawned with
    /* 0x70 */ s16                   field_70; // hit-stun countdown; hits are ignored while non-zero
    /* 0x72 */ s16                   field_72; // state of the teardown handler `func_actor_205200_8014B484`
    /* 0x74 */ u16                   field_74; // effect timer
    /* 0x76 */ s16                   field_76; // spark cooldown
    /* 0x78 */ s16                   field_78;
    /* 0x7A */ byte                  pad_7A[2];
} Actor205200Part;
STATIC_ASSERT_SIZEOF(Actor205200Part, 0x7C);

extern s32 gScreenWaveRamp;

extern u16      D_actor_205200_8014C9CC[];
extern s16      D_actor_205200_8014CA1C[];
extern TaskDesc D_actor_205200_8014CA60[];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_205200_8014CA78[2];
extern TaskDesc         D_actor_205200_8014CA44[];

extern EnemyParams D_actor_205200_8014C9BC;
extern SVECTOR*    D_actor_205200_8014CA24[];
extern u16*        D_actor_205200_8014CA34[];

static void func_actor_205200_8014AB98(Task* arg0);
static void func_actor_205200_8014ACD4(Task* arg0);
static s32  func_actor_205200_8014B914(s32 arg0);
static void func_actor_205200_8014B9D4(Enemy* arg0, Task* arg1);
static void func_actor_205200_8014BA94(Task* arg0);

void func_actor_205200_8014B8C0(Task*);
void func_actor_205200_8014B978(Task*);

s32 func_actor_205200_8014B94C(Task* task, s32 msgId, ActorCommand* request, s32 arg3);

EnemyParams D_actor_205200_8014C9BC = { NULL, 200, 150, 0, 0, 100, 0, 0, 0 };

u16 D_actor_205200_8014C9CC[4] = {
    0,
    60,
    40,
    20,
};

SVECTOR D_actor_205200_8014C9D4[2] = {
    { -4550, -1100, 1030, 0 },
    { -1260, -1100, 7420, 0 },
};

u16 D_actor_205200_8014C9E4[2] = {
    1024,
    2048,
};

SVECTOR D_actor_205200_8014C9E8[3] = {
    { 3000, -1500, 1600, 0 },
    { 5000, -1500, -1600, 0 },
    { 7000, -1500, 1600, 0 },
};

u16 D_actor_205200_8014CA00[4] = {
    0,
    2048,
    2048,
    0,
};

SVECTOR D_actor_205200_8014CA08[2] = {
    { -150, -1200, 4000, 0 },
    { 5150, -1200, 4000, 0 },
};

u16 D_actor_205200_8014CA18[2] = {
    3072,
    1024,
};

s16 D_actor_205200_8014CA1C[4] = {
    0,
    2,
    3,
    2,
};

SVECTOR* D_actor_205200_8014CA24[4] = {
    NULL,
    D_actor_205200_8014C9D4,
    D_actor_205200_8014C9E8,
    D_actor_205200_8014CA08,
};

u16* D_actor_205200_8014CA34[4] = {
    NULL,
    D_actor_205200_8014C9E4,
    D_actor_205200_8014CA00,
    D_actor_205200_8014CA18,
};

TaskDesc D_actor_205200_8014CA44[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveGridTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskDesc D_actor_205200_8014CA60[2] = {
    { { { TASK_BODY_COORD, 96 } }, func_actor_205200_8014B8C0, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_205200_8014B978, { .value = 0 } },
};

TaskMessageEntry D_actor_205200_8014CA78[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_205200_8014B94C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static TmdBone _gActor205200EveBreaMaskedBodySkeleton[19] = {
#include "assets/eve_brea_masked_body_skeleton.inc"
};

static u32 _gActor205200EveBreaMaskedBodyPartVerts[19] = {
#include "assets/eve_brea_masked_body_partVerts.inc"
};

static SVECTOR _gActor205200EveBreaMaskedBodyVerts[312] = {
#include "assets/eve_brea_masked_body_verts.inc"
};

static SVECTOR _gActor205200EveBreaMaskedBodyNormals[338] = {
#include "assets/eve_brea_masked_body_normals.inc"
};

static u32 _gActor205200EveBreaMaskedBodyStream[3463] = {
#include "assets/eve_brea_masked_body_stream.inc"
};

TmdSource gActor205200EveBreaMaskedBody = {
    0,
    18392,
    6232,
    19,
    _gActor205200EveBreaMaskedBodyPartVerts,
    _gActor205200EveBreaMaskedBodyVerts,
    _gActor205200EveBreaMaskedBodyNormals,
    _gActor205200EveBreaMaskedBodySkeleton,
    _gActor205200EveBreaMaskedBodyStream,
};

static void func_actor_205200_8014A72C(Enemy* enemy, Task* task);
static void func_actor_205200_8014A958(Enemy* enemy, Task* task);
static void func_actor_205200_8014AE0C(Enemy* arg0, Task* arg1);
static void func_actor_205200_8014B048(Task* arg0, s32 arg1);
static void func_actor_205200_8014B484(Enemy* arg0, Task* arg1);

#include "../../shared/screen_wave_grid.inc.c"

static void func_actor_205200_8014A72C(Enemy* enemy, Task* task)
{
    Actor205200CtrlWork* work;
    u16                  kind;
    s32                  i;
    u16                  timer;

    kind = enemy->place->mode;
    if ((u16)(kind - 1) >= 3) {
        enemyDestroy(enemy, task);
        return;
    }
    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work                    = work;
    work->field_1E                = kind;
    D_actor_205200_8015B458.state = SCREEN_WAVE_RAMP_FINISHED;
    for (i = 0; i < D_actor_205200_8014CA1C[work->field_1E]; i++) {
        Gp_SpawnEnemyFromTable(D_actor_205200_8014CA60, 1, 0, enemy);
    }
    timer          = D_actor_205200_8014C9CC[D_actor_205200_8014CA1C[work->field_1E]];
    work->field_2A = 5;
    work->field_22 = timer;
    /* The empty `case 0` is load-bearing: a fourth case node makes GCC root
       the decision tree at 1 (`beq 1; slti <2`) instead of at 2. */
    switch (work->field_1E) {
        case 1:
            func_neo_ark_eve_access_tunnel_8017E090(0, 0);
            func_neo_ark_eve_access_tunnel_8017E090(1, 0);
            GameFlag_SetNibble(GAME_FLAG_EVE_ACCESS_TUNNEL_PART_0_DOWN, 0);
            GameFlag_SetNibble(GAME_FLAG_EVE_ACCESS_TUNNEL_PART_1_DOWN, 0);
            break;
        case 2:
            func_shelter_b6_corridor_8017EE08(0, 0);
            func_shelter_b6_corridor_8017EE08(1, 0);
            func_shelter_b6_corridor_8017EE08(2, 0);
            GameFlag_SetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_0_DOWN, 0);
            GameFlag_SetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_1_DOWN, 0);
            break;
        case 3:
            func_shelter_b6_training_room_80182A14(0, 0);
            func_shelter_b6_training_room_80182A14(1, 0);
            GameFlag_SetNibble(GAME_FLAG_153, 0);
            GameFlag_SetNibble(GAME_FLAG_154, 0);
            break;
        case 0:
            break;
    }
    task->msgTable = D_actor_205200_8014CA78;
    task->state    = 1;
}

static void func_actor_205200_8014A958(Enemy* enemy, Task* task)
{
    Actor205200CtrlWork* work = task->work;
    s16                  state;
    s32                  pulse;

    if (gGameSession->eventState != 0 || work->field_2E != 0) {
        pulse = work->field_28;
        if (pulse == SCREEN_WAVE_RAMP_FALLING) {
            D_actor_205200_8015B458.state = pulse;
            work->field_28                = 0;
        }
        if (work->field_2E != 0) {
            work->field_24 = 3;
            if (work->field_2E != 0) {
                if (work->field_14 != 0) {
                    SndEvt_EnqueueType7(work->field_14, 1);
                    work->field_14 = 0;
                }
            }
        }
    } else if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        state = work->field_24;
        switch (state) {
            case 0:
                if ((u16)--work->field_2A == 0) {
                    func_actor_205200_8014ACD4(task);
                    if (work->field_C != NULL) {
                        work->field_14 = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40340001;
                        SndEvt_EnqueueType6(
                            work->field_14, 0, (s8)func_actor_205200_8014B914(work->field_10));
                        work->field_24 = 1;
                    }
                }
                break;
            case 1:
                if (gGameSession->viewReady == state || work->field_2C == state) {
                    work->field_2C = 0;
                    func_actor_205200_8014ACD4(task);
                    if (work->field_C != NULL) {
                        SndEvt_EnqueueTypeA(
                            work->field_14, 0, (s8)func_actor_205200_8014B914(work->field_10));
                    }
                }
                func_actor_205200_8014AB98(task);
                if (work->field_20 <= 0) {
                    work->field_24 = 2;
                }
                break;
            case 2:
                pulse = work->field_28;
                if (pulse == SCREEN_WAVE_RAMP_FALLING) {
                    D_actor_205200_8015B458.state = pulse;
                    work->field_28                = 0;
                }
                SndEvt_EnqueueType7(work->field_14, 1);
                work->field_24 = 3;
                if (work->field_1E == state) {
                    SndEvt_EnqueueType2(0, 0x3C);
                }
                break;
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

static void func_actor_205200_8014AB98(Task* arg0)
{
    Actor205200CtrlWork* work  = arg0->work;
    s32                  state = work->field_26;

    switch (state) {
        case 0:
            if ((s16)--work->field_22 <= 0) {
                if (D_actor_205200_8015B458.state == SCREEN_WAVE_RAMP_FINISHED) {
                    D_actor_205200_8015B458.span  = 0xF;
                    D_actor_205200_8015B458.scale = 0xA0;
                    Task_SpawnFromTable(D_actor_205200_8014CA44, 0, 0, &D_actor_205200_8015B458);
                    Gp_ArmStateF0(1);
                    work->field_28 = SCREEN_WAVE_RAMP_FALLING;
                    SndEvt_EnqueueType6(((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40340002, 0, 0);
                }
                work->field_22 = 20;
                work->field_26 = 1;
            }
            break;
        case 1:
            if ((s16)--work->field_22 <= 0) {
                D_actor_205200_8015B458.state = state;
                work->field_22                = D_actor_205200_8014C9CC[work->field_20];
                work->field_26                = 0;
                Gp_SpendMp(1);
                work->field_28 = 0;
            }
            break;
    }
}

static void func_actor_205200_8014ACD4(Task* arg0)
{
    Actor205200CtrlWork* work = arg0->work;
    ViewCamera*          view;
    VECTOR               d;
    u32                  dist;
    s32                  i;

    work->field_C  = NULL;
    work->field_10 = -1;
    view           = Gp_GetStageView(&gGameSession->location.loc);
    for (i = 0; i < 3; i++) {
        if (work->field_18[i] == 1) {
            work->field_0[i]->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(work->field_0[i]);
            d.vx = view->transform.t[0] + work->field_0[i]->coord.t[0];
            d.vy = view->transform.t[1] + work->field_0[i]->coord.t[1];
            d.vz = view->transform.t[2] + work->field_0[i]->coord.t[2];
            dist = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
            if (dist < work->field_10) {
                work->field_C  = work->field_0[i];
                work->field_10 = dist;
            }
        }
    }
}

static void func_actor_205200_8014AE0C(Enemy* arg0, Task* arg1)
{
    GfxCoord*            coord;
    Actor205200CtrlWork* pwork;
    Actor205200Part*     part;
    SVECTOR*             pos;
    SVECTOR              rot;
    MATRIX*              mat;
    u16*                 tbl;

    coord = arg1->extra.tmd->coords;
    pwork = (Actor205200CtrlWork*)arg1->parent->work;
    part  = memCalloc(0x7CU, false);
    if (part == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work     = part;
    part->field_78 = pwork->field_20;
    pwork->field_20++;
    pwork->field_0[part->field_78]  = coord;
    pwork->field_18[part->field_78] = 1;
    tbl                             = D_actor_205200_8014CA34[pwork->field_1E];
    rot.vx                          = 0;
    mat                             = &coord->coord;
    rot.vy                          = tbl[part->field_78];
    rot.vz                          = 0;
    RotMatrix(&rot, mat);
    pos                 = D_actor_205200_8014CA24[pwork->field_1E];
    coord->coord.t[0]   = pos[part->field_78].vx;
    coord->coord.t[1]   = pos[part->field_78].vy;
    coord->coord.t[2]   = pos[part->field_78].vz;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    arg0->field_4       = mat;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = coord;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &D_actor_205200_8014C9BC;
    arg0->recs       = part->recs;
    arg0->hp         = D_actor_205200_8014C9BC.hpMax;
    (Gp_IncStateF0Ref)(0);
    part->field_68.spawnArgLo  = 0x400;
    part->field_68.spawnArgHi  = 3;
    part->field_68.coord       = coord;
    part->obj.coord            = coord;
    part->obj.context.contacts = part->recs;
    part->obj.pos.vx           = 0;
    part->obj.pos.vy           = 0;
    part->obj.pos.vz           = 0;
    part->obj.key              = 0x30034;
    part->obj.radius           = 0x1C2;
    part->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &part->obj);
    Gp_InitRec18Table(part->recs, 3, 0);
    part->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    arg1->state      = 1;
}

/// Hit handling of a live part: applies the part's damage-kind hits (records
/// of kind 2) to the owning enemy's HP, killing the part at zero, and otherwise
/// arms the hit-stun timer `field_70`, the effect timer `field_74` and the
/// spark cooldown `field_76`. `arg1` is passed as 1 by
/// `func_actor_205200_8014B9D4` and unused.
static void func_actor_205200_8014B048(Task* arg0, s32 arg1)
{
    VECTOR*              vec;
    Actor205200Part*     part;
    Enemy*               enemy;
    GfxCoord*            coord;
    Actor205200CtrlWork* parentWork;
    s32                  damage;
    s32                  i;
    s32                  snd;
    s32                  hitTime;
    s32                  clamped;

    vec   = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    coord = arg0->extra.tmd->coords;
    part  = (Actor205200Part*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (part->field_70 != 0) {
        part->field_70--;
        if (part->field_70 <= 0) {
            part->field_70 = 0;
        }
    }
    if (part->field_76 != 0) {
        part->field_76--;
    }
    if (part->field_70 == 0) {
        for (i = 0; i < 3; i++) {
            if ((part->recs[i].key.value & 0xFFFF0000) != 0x20000) {
                continue;
            }
            if (part->recs[i].key.value & 0x8000) {
                func_800DA6E8(&enemy->node, 0, 0);
                break;
            }
            vec->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            vec->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage  = Gp_ComputeDamage(part->recs[i].key.value, SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz), 0, 0);
            if (Gp_RollEnemyChance(enemy, part->recs[i].key.value, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, NULL);
            }
            func_800DA6E8(&enemy->node, damage, 0);
            enemy->hp -= damage;
            if (enemy->hp <= 0) {
                arg0->state                                                          = 2;
                part->field_72                                                       = 0;
                ((Actor205200CtrlWork*)arg0->parent->work)->field_18[part->field_78] = 0;
                ((Actor205200CtrlWork*)arg0->parent->work)->field_0[part->field_78]  = NULL;
                Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x01002600, NULL);
                Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x01002600, NULL);
                Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x01002600, NULL);
                Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x02002600, NULL);
                Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x02002600, NULL);
                snd = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40340004;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                Gp_SpawnPadLerp(10, 0xFF, 0x80);
            } else if (damage > 0) {
                if (part->field_76 == 0) {
                    if ((Gp_GetIdParam0(part->recs[i].key.value) & 0xFFFF) == 7) {
                        func_800FDB18(3, coord, NULL, &part->field_68);
                    }
                    func_800FDB18(7, coord, NULL, &part->field_68);
                    part->field_76 = 10;
                }
                if (damage < 201) {
                    clamped = damage;
                } else {
                    clamped = 200;
                }
                part->field_74 = (clamped * 120) / 200 + 30;
                hitTime        = Gp_GetIdParam2(part->recs[i].key.value);
                if (hitTime > 0) {
                    part->field_70 = hitTime;
                }
                snd = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40340003;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
        }
    }
    Gp_ClearRec18Occupied(part->recs);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

static void func_actor_205200_8014B484(Enemy* arg0, Task* arg1)
{
    Actor205200Part*     part;
    GfxCoord*            coord;
    Actor205200CtrlWork* work;
    ViewCamera*          view;
    VECTOR               d;
    s32                  dist;
    s32                  snd;
    s32                  pan;
    s32                  vol;

    part  = (Actor205200Part*)arg1->work;
    coord = arg1->extra.tmd->coords;
    work  = (Actor205200CtrlWork*)arg1->parent->work;
    if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }
    switch (part->field_72) {
        case 0:
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0x32001400, NULL);
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0x32001400, NULL);
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xF2001400, NULL);
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xF2001400, NULL);
            worldTargetUnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            Gp_ReleaseStateF0Add(arg1, 0x34);
            arg0->recs     = 0;
            work->field_2C = 1;
            work->field_20--;
            gSceneCombatState.pairedEnemySignals |= SCENE_COMBAT_PAIRED_CHARGE_REQUEST;
            switch (work->field_1E) {
                case 1:
                    func_neo_ark_eve_access_tunnel_8017E090((u8)part->field_78, 1);
                    GameFlag_SetNibble(part->field_78 + 0x142, 1);
                    break;
                case 2:
                    func_shelter_b6_corridor_8017EE08((u8)part->field_78, 1);
                    GameFlag_SetNibble(part->field_78 + 0x144, 1);
                    break;
                case 3:
                    func_shelter_b6_training_room_80182A14((u8)part->field_78, 1);
                    GameFlag_SetNibble(part->field_78 + 0x153, 1);
                    break;
                case 0:
                    break;
            }
            part->field_72 = 1;
            part->field_74 = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x3F) + 0x1E;
            part->field_76 = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1F) + 0x1E;
            break;
        case 1:
            if ((s16)--part->field_74 <= 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                part->field_74  = ((gRandomLcgState >> 16) & 0x3F) + 0x1E;
                func_800FDB18(7, coord, NULL, &part->field_68);
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xF2001400, NULL);
                view = Gp_GetStageView(&gGameSession->location.loc);
                d.vx = view->transform.t[0] + coord->coord.t[0];
                d.vy = view->transform.t[1] + coord->coord.t[1];
                d.vz = view->transform.t[2] + coord->coord.t[2];
                dist = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
                snd  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40340005;
                pan  = (s8)worldCoordGetOriginAudioPan(coord);
                vol  = dist - gDisplayState.screenDistance;
                if (vol >= 0x7FFF) {
                    vol = 0x7FFF;
                }
                if (vol < -0x7FFF) {
                    vol = -0x7FFF;
                }
                SndEvt_EnqueueType6(snd, pan, (s16)vol >> 8);
            }
            if ((s16)--part->field_76 <= 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                part->field_76  = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xF2001400, NULL);
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xF2001400, NULL);
            }
            break;
        case 2:
            (Gp_ReleaseStateF0)(arg1, 0x34);
            worldTargetUnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            part->field_72 = 3;
            break;
    }
}

/// Update of the actor's controller task: dispatches on its state to the
/// setup handler `func_actor_205200_8014A72C` (state 0) or the per-frame
/// handler `func_actor_205200_8014A958` (state 1), passing the task's enemy
/// record along with the task.
void func_actor_205200_8014B8C0(Task* task)
{
    EnemyTaskFunc fns[2] = {
        func_actor_205200_8014A72C,
        func_actor_205200_8014A958,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

static s32 func_actor_205200_8014B914(s32 arg0)
{
    s32 delta;

    delta = arg0 - gDisplayState.screenDistance;
    if (delta >= 0x7FFF) {
        delta = 0x7FFF;
    }
    if (delta < -0x7FFF) {
        delta = -0x7FFF;
    }
    return delta >> 8;
}

/// Message 0x7DB handler of the controller, listed in
/// `D_actor_205200_8014CA78`. A non-zero payload halfword raises
/// `Actor205200CtrlWork.field_2E` unless it is already set.
s32 func_actor_205200_8014B94C(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    Actor205200CtrlWork* work;

    work = arg0->work;
    if (request->command != 0 && work->field_2E == 0) {
        work->field_2E = 1;
    }
    return 0;
}

/// State handlers of a part task - spawn, per-frame tick and teardown - that
/// `func_actor_205200_8014B978` dispatches through by state.
static const EnemyTaskFuncTable3 D_actor_205200_80149E24 = {
    func_actor_205200_8014AE0C,
    func_actor_205200_8014B9D4,
    func_actor_205200_8014B484,
};

/// Update of a part task: runs the handler of `D_actor_205200_80149E24` that
/// `Task::state` selects, through a stack copy of the table.
void func_actor_205200_8014B978(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_205200_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Per-frame tick of a live part. `gSceneCombatState.actorControl` gates the body: mode 1 runs
/// none of it, mode 2 marks the node not lockable and returns, mode 0 hides its
/// HP before falling in, and any other mode enters it directly. The body
/// applies the part's hits, ticks its effect timer and, once the controller's
/// 0x7DB flag is up, pushes this task to state 2 and the part to its state 2.
/// The dispatch is written as gotos because that is the shape the switch's
/// binary decision tree leaves behind - mode 0 shares the body with the
/// default path, so its `break` is a jump into it.
static void func_actor_205200_8014B9D4(Enemy* arg0, Task* arg1)
{
    Actor205200Part*     part;
    Actor205200CtrlWork* parentWork;
    s32                  state;
    s32                  one;

    part       = (Actor205200Part*)arg1->work;
    parentWork = (Actor205200CtrlWork*)arg1->parent->work;
    state      = gSceneCombatState.actorControl;
    one        = 1;
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
    arg0->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
    goto default_body;
case2:
    arg0->node.state.parts.flags = one;
    return;
default_body:
    func_actor_205200_8014B048(arg1, one);
    if ((s16)part->field_74 != 0) {
        func_actor_205200_8014BA94(arg1);
    }
    if (parentWork->field_2E == 1) {
        arg1->state    = 2;
        part->field_72 = 2;
    }
case1:
    return;
}

/// Counts a part's effect timer down and queues effect 7 every 0x40 ticks.
static void func_actor_205200_8014BA94(Task* arg0)
{
    Actor205200Part* part;
    u16              timer;

    part           = (Actor205200Part*)arg0->work;
    timer          = part->field_74 - 1;
    part->field_74 = timer;
    if (!(timer & 0x3F)) {
        func_800FDB18(7, arg0->extra.tmd->coords, NULL, &part->field_68);
    }
}
