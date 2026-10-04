#include "actor_342400_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/mad_chaser_waves.h"
#include "../../shared/mad_chaser.h"

/// Which coordinate a `_Actor342400CullZone` bounds, and from which side.
enum {
    ACTOR_342400_CULL_ZONE_AXIS_X = 0, // The zone is every x below the limit
    ACTOR_342400_CULL_ZONE_AXIS_Z = 1, // The zone is every z at or above the limit (any nonzero value)
};

/// One region of the room in which the wave spawners kill their enemies.
///
/// `GameSession::enemyCullZone` selects the region in force, 1..16, or 0 for
/// none. A region is everything to one side of a line across the room: a
/// spawner tests its living enemy's position against the line, and once the
/// enemy is inside orders it to die and stops watching it.
typedef struct {
    s16 axis;  // Coordinate the line crosses (ACTOR_342400_CULL_ZONE_AXIS_*)
    s16 limit; // Where the line crosses it, in world units
} _Actor342400CullZone;
STATIC_ASSERT_SIZEOF(_Actor342400CullZone, 0x4);

extern TaskDesc D_actor_207000_801575F0; // absolute, spawned by func_actor_342400_80162DA0
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry     D_actor_342400_8016BF48[2]; // stored into `Task::msgTable` by func_actor_342400_801628F0
extern OverlayEncounterSlot gMadChaserWaveSlots[];
extern TaskDesc             D_actor_342400_8016BFE0[];
extern _Actor342400CullZone D_actor_342400_8016C010[];
extern s16                  D_actor_342400_8016C054[][4]; // spawn variant per player-position band, 4 random picks

                                                          // spawn counter, `<< 12` into `Enemy::placeKey`

static s16  func_actor_342400_801624A4(void);
static s16  func_actor_342400_801626CC(s16 arg0, s16 arg1, s16 arg2);
static void func_actor_342400_801628F0(Task* arg0);
static void func_actor_342400_80162A34(Task* arg0);
static void func_actor_342400_80162AB0(Task* arg0);
static void func_actor_342400_80162B60(Task* arg0);
static void func_actor_342400_80162C10(Task* arg0);
static void func_actor_342400_80162CA8(Task* arg0);
static void func_actor_342400_80162CBC(Task* arg0);
static void func_actor_342400_80162DA0(Task* arg0);
static void func_actor_342400_80162F08(Task* arg0);
static void func_actor_342400_80162F1C(Task* arg0);
static void func_actor_342400_80162FFC(Task* arg0);

void func_actor_342400_80162748(Task*);

void func_actor_342400_801626AC(Task* task, s32 msgId, ActorCommand* request, s32 arg3);

TaskMessageEntry D_actor_342400_8016BF48[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_342400_801626AC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

OverlayEncounterSlot gMadChaserWaveSlots[17] = {
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 1), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 2), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 1), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 2), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
};

void func_actor_342400_80162748(Task*);
void func_actor_342400_801627C0(Task*);
void func_actor_342400_80162824(Task*);
void func_actor_342400_80162888(Task*);

TaskDesc D_actor_342400_8016BFE0[4] = {
    { { { TASK_BODY_NONE, 32 } }, func_actor_342400_80162748, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_actor_342400_801627C0, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_actor_342400_80162824, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_actor_342400_80162888, { .value = 0 } },
};

_Actor342400CullZone D_actor_342400_8016C010[17] = {
    { ACTOR_342400_CULL_ZONE_AXIS_X, 0 }, // 0 selects no zone, so this row is never read
    { ACTOR_342400_CULL_ZONE_AXIS_X, 1295 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 2700 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 4300 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 5700 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 7295 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 8700 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -7800 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -9210 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -10805 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -12210 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -15760 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -17170 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -18825 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -20235 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -23800 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -25205 },
};

s16 D_actor_342400_8016C054[6][4] = {
    { 2, 3, 11, 12 },
    { 3, 4, 12, 5 },
    { 5, 6, 7, 13 },
    { 7, 8, 13, 14 },
    { 8, 9, 14, 15 },
    { 9, 15, 9, 15 },
};

static TmdBone _gActor342400MadChaserBurstHeadSkeleton[1] = {
#include "assets/mad_chaser_burst_head_skeleton.inc"
};

static u32 _gActor342400MadChaserBurstHeadPartVerts[1] = {
#include "assets/mad_chaser_burst_head_partVerts.inc"
};

static SVECTOR _gActor342400MadChaserBurstHeadVerts[48] = {
#include "assets/mad_chaser_burst_head_verts.inc"
};

static SVECTOR _gActor342400MadChaserBurstHeadNormals[53] = {
#include "assets/mad_chaser_burst_head_normals.inc"
};

static u32 _gActor342400MadChaserBurstHeadStream[486] = {
#include "assets/mad_chaser_burst_head_stream.inc"
};

TmdSource gMadChaserChunkModel0 = {
    0,
    3260,
    0,
    1,
    _gActor342400MadChaserBurstHeadPartVerts,
    _gActor342400MadChaserBurstHeadVerts,
    _gActor342400MadChaserBurstHeadNormals,
    _gActor342400MadChaserBurstHeadSkeleton,
    _gActor342400MadChaserBurstHeadStream,
};

static TmdBone _gActor342400MadChaserBurstArmSkeleton[1] = {
#include "assets/mad_chaser_burst_arm_skeleton.inc"
};

static u32 _gActor342400MadChaserBurstArmPartVerts[1] = {
#include "assets/mad_chaser_burst_arm_partVerts.inc"
};

static SVECTOR _gActor342400MadChaserBurstArmVerts[28] = {
#include "assets/mad_chaser_burst_arm_verts.inc"
};

static SVECTOR _gActor342400MadChaserBurstArmNormals[37] = {
#include "assets/mad_chaser_burst_arm_normals.inc"
};

static u32 _gActor342400MadChaserBurstArmStream[276] = {
#include "assets/mad_chaser_burst_arm_stream.inc"
};

TmdSource gMadChaserChunkModel1 = {
    0,
    1828,
    0,
    1,
    _gActor342400MadChaserBurstArmPartVerts,
    _gActor342400MadChaserBurstArmVerts,
    _gActor342400MadChaserBurstArmNormals,
    _gActor342400MadChaserBurstArmSkeleton,
    _gActor342400MadChaserBurstArmStream,
};

static TmdBone _gActor342400MadChaserBurstTailSkeleton[1] = {
#include "assets/mad_chaser_burst_tail_skeleton.inc"
};

static u32 _gActor342400MadChaserBurstTailPartVerts[1] = {
#include "assets/mad_chaser_burst_tail_partVerts.inc"
};

static SVECTOR _gActor342400MadChaserBurstTailVerts[25] = {
#include "assets/mad_chaser_burst_tail_verts.inc"
};

static SVECTOR _gActor342400MadChaserBurstTailNormals[32] = {
#include "assets/mad_chaser_burst_tail_normals.inc"
};

static u32 _gActor342400MadChaserBurstTailStream[215] = {
#include "assets/mad_chaser_burst_tail_stream.inc"
};

TmdSource gMadChaserChunkModel2 = {
    0,
    1448,
    0,
    1,
    _gActor342400MadChaserBurstTailPartVerts,
    _gActor342400MadChaserBurstTailVerts,
    _gActor342400MadChaserBurstTailNormals,
    _gActor342400MadChaserBurstTailSkeleton,
    _gActor342400MadChaserBurstTailStream,
};

static TmdBone _gActor342400MadChaserBodySkeleton[9] = {
#include "assets/mad_chaser_body_skeleton.inc"
};

static u32 _gActor342400MadChaserBodyPartVerts[9] = {
#include "assets/mad_chaser_body_partVerts.inc"
};

static SVECTOR _gActor342400MadChaserBodyVerts[160] = {
#include "assets/mad_chaser_body_verts.inc"
};

static SVECTOR _gActor342400MadChaserBodyNormals[206] = {
#include "assets/mad_chaser_body_normals.inc"
};

static u32 _gActor342400MadChaserBodyStream[2105] = {
#include "assets/mad_chaser_body_stream.inc"
};

TmdSource gActor342400MadChaserBody = {
    0,
    11212,
    3088,
    9,
    _gActor342400MadChaserBodyPartVerts,
    _gActor342400MadChaserBodyVerts,
    _gActor342400MadChaserBodyNormals,
    _gActor342400MadChaserBodySkeleton,
    _gActor342400MadChaserBodyStream,
};

DamageAttack D_actor_342400_80170584[1] = {
    { 22, 0 },
};

EnemyParams gMadChaserEnemyParams = { D_actor_342400_80170584, 110, 20, 40, 1, 100, 10, 100, 0 };

static void func_actor_342400_80162324(Task* arg0);
static void func_actor_342400_801631DC(s16 arg0);

#include "../../shared/mad_chaser_waves_pair_spawn.inc.c"

void madChaserWavePairCull(Task* arg0)
{
    OverlayEncounterPairWork* work = arg0->work;
    Enemy*                    enemy;
    Task*                     task;
    GfxCoord*                 coord;
    ActorCommand              msg;

    if (work->enemy0 != NULL) {
        enemy = work->enemy0;
        task  = enemy->task;
        coord = task->extra.tmd->coords;
        if (enemy->hp <= 0) {
            work->enemy0 = NULL;
        } else if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
            msg.context.loc.stage = 0;
            msg.context.loc.area  = 0;
            msg.command           = 5;
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
            work->enemy0 = NULL;
        }
    } else {
        work->goneMask |= OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY0;
    }
    if (work->enemy1 != NULL) {
        enemy = work->enemy1;
        task  = enemy->task;
        coord = task->extra.tmd->coords;
        if (enemy->hp <= 0) {
            work->enemy1 = NULL;
        } else if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
            msg.context.loc.stage = 0;
            msg.context.loc.area  = 0;
            msg.command           = 5;
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
            work->enemy1 = NULL;
        }
    } else {
        work->goneMask |= OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY1;
    }
}

static void func_actor_342400_80162324(Task* arg0)
{
    OverlayEncounterControllerWork* work = arg0->work;
    s16                             count;
    s16                             i;
    s16                             idx;
    s16                             type;
    s16                             arg;

    count = 0;
    for (i = 0; i < 17; i++) {
        if (gMadChaserWaveSlots[i].status == OVERLAY_ENCOUNTER_SLOT_LIVE) {
            count++;
        }
    }
    if (count < 3) {
        idx = work->nextSlot;
        if (idx < 17 && gGameSession->sceneClock >= 0x3D) {
            type = gMadChaserWaveSlots[idx].kind;
            arg  = gMadChaserWaveSlots[idx].command;
            switch (type) {
                case 0:
                    Task_SpawnFromTable(D_actor_342400_8016BFE0, 1, (idx << 16) + arg + (func_actor_342400_801624A4() << 16 >> 8), 0);
                    break;
                case 1:
                    Task_SpawnFromTable(D_actor_342400_8016BFE0, 2, (idx << 16) + arg + (func_actor_342400_801624A4() << 16 >> 8), 0);
                    break;
                case 2:
                    Task_SpawnFromTable(D_actor_342400_8016BFE0, 3, (idx << 16) + arg + (func_actor_342400_801624A4() << 16 >> 8), 0);
                    break;
            }
            work->nextSlot++;
        }
    }
}

static s16 func_actor_342400_801624A4(void)
{
    GfxCoord* coord = (*gPlayerActorTasks)->extra.tmd->coords;
    s16       x     = coord->coord.t[0];
    s16       z     = coord->coord.t[2];

    if (x <= 5000) {
        return D_actor_342400_8016C054[0][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 3];
    }
    if (x <= 8000) {
        return D_actor_342400_8016C054[1][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 3];
    }
    if (x <= 11000) {
        return D_actor_342400_8016C054[2][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 3];
    }
    if (z >= -5500) {
        return D_actor_342400_8016C054[3][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 3];
    }
    if (z >= -8500) {
        return D_actor_342400_8016C054[4][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 3];
    }
    if (z >= -11500) {
        return D_actor_342400_8016C054[5][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 3];
    }
    if (z >= -24500) {
        return D_actor_342400_8016C054[5][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 3];
    }
    return D_actor_342400_8016C054[5][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 3];
}

void func_actor_342400_801626AC(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    OverlayEncounterControllerWork* work = arg0->work;
    u16                             id   = request->command;

    if (id == OVERLAY_ENCOUNTER_COMMAND_STOP) {
        work->stop = id;
    }
}

static s16 func_actor_342400_801626CC(s16 arg0, s16 arg1, s16 arg2)
{
    if (arg0 == 0 || arg0 > 0x10) {
        return 0;
    }
    if (D_actor_342400_8016C010[arg0].axis == ACTOR_342400_CULL_ZONE_AXIS_X) {
        if (arg1 < D_actor_342400_8016C010[arg0].limit) {
            return 1;
        }
    } else {
        if (arg2 >= D_actor_342400_8016C010[arg0].limit) {
            return 1;
        }
    }
    return 0;
}

/// The controller task's four state handlers, dispatched by
/// `func_actor_342400_80162748` on `Task::state`.
static const TaskFuncTable4 D_actor_342400_80161E24 = { {
    func_actor_342400_801628F0,
    madChaserWaveOpen,
    func_actor_342400_80162A34,
    func_actor_342400_80162AB0,
} };

void func_actor_342400_80162748(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_342400_80161E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[arg0->state](arg0);
    }
}

/// The first spawner task's four state handlers, dispatched by
/// `func_actor_342400_801627C0`.
static const TaskFuncTable4 D_actor_342400_80161E34 = { {
    func_actor_342400_80162B60,
    func_actor_342400_80162C10,
    func_actor_342400_80162CA8,
    func_actor_342400_80162CBC,
} };

/// Runs the first spawner task's handler for its `Task::state`.
void func_actor_342400_801627C0(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_342400_80161E34;
    handlers.funcs[arg0->state](arg0);
}

/// The second spawner task's four state handlers, dispatched by
/// `func_actor_342400_80162824`.
static const TaskFuncTable4 D_actor_342400_80161E44 = { {
    func_actor_342400_80162DA0,
    madChaserWaveRevealSecond,
    func_actor_342400_80162F08,
    func_actor_342400_80162F1C,
} };

/// Runs the second spawner task's handler for its `Task::state`.
void func_actor_342400_80162824(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_342400_80161E44;
    handlers.funcs[arg0->state](arg0);
}

/// The five state handlers `func_actor_342400_80162888` dispatches through by
/// `Task::state`.
static const TaskFuncTable5 D_actor_342400_80161E54 = { {
    madChaserWavePairSpawn,
    func_actor_342400_80162FFC,
    madChaserWavePairRevealFirst,
    madChaserWavePairRevealSecond,
    madChaserWavePairWatch,
} };

void func_actor_342400_80162888(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_342400_80161E54;
    sp.funcs[arg0->state](arg0);
}

static void func_actor_342400_801628F0(Task* arg0)
{
    OverlayEncounterControllerWork* work;
    s32                             i;

    if (gGameSession->spawnPhase[1] == GAME_SESSION_SPAWN_COMPLETE || gGameSession->spawnPhase[0] == GAME_SESSION_SPAWN_IDLE ||
        (work = memCalloc(sizeof(*work), 0)) == NULL) {
        taskKill(arg0);
        return;
    }
    for (i = 16; i >= 0; i--) {
        gMadChaserWaveSlots[i].status = OVERLAY_ENCOUNTER_SLOT_WAITING;
    }
    gMadChaserWaveEnemyCount = 0;
    arg0->work               = work;
    arg0->msgTable           = D_actor_342400_8016BF48;
    arg0->state++;
}

#include "../../shared/mad_chaser_waves_open.inc.c"

static void func_actor_342400_80162A34(Task* arg0)
{
    OverlayEncounterControllerWork* work = arg0->work;

    if (++work->frames == 15) {
        (Gp_IncStateF0Ref)(0);
        gGameSession->spawnPhase[1] = GAME_SESSION_SPAWN_ARMED;
        Gp_ArmStateF0(1);
        arg0->state++;
    }
}

static void func_actor_342400_80162AB0(Task* arg0)
{
    OverlayEncounterControllerWork* work = arg0->work;
    s16                             count;
    s32                             i;

    count = 0;
    if (work->stop != OVERLAY_ENCOUNTER_COMMAND_STOP) {
        func_actor_342400_80162324(arg0);
        for (i = 0; i < 17; i++) {
            if (gMadChaserWaveSlots[i].status == OVERLAY_ENCOUNTER_SLOT_DONE) {
                count++;
            }
        }
        if (count == 17) {
            Gp_ReleaseStateF0Clear(arg0, 0);
            gGameSession->spawnPhase[1] = GAME_SESSION_SPAWN_COMPLETE;
            taskKill(arg0);
        }
    }
}

static void func_actor_342400_80162B60(Task* arg0)
{
    OverlayEncounterSingleWork* work;
    Enemy*                      enemy;

    work = memCalloc(8, 0);
    if (work != NULL) {
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(D_actor_342400_80173A54, 1, 0, 0);
        if (enemy != NULL) {
            gMadChaserWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
            work->enemy                                                    = enemy;
            enemy->placeKey                                                = gMadChaserWaveEnemyCount << 12;
            gMadChaserWaveEnemyCount++;
            arg0->state++;
            return;
        }
    }
    taskKill(arg0);
}

static void func_actor_342400_80162C10(Task* arg0)
{
    OverlayEncounterSingleWork* work = arg0->work;
    Enemy*                      enemy;
    Task*                       task;
    TmdObject*                  obj;
    ActorCommand                msg;

    enemy = work->enemy;
    task  = enemy->task;
    if (++work->frames > 60) {
        obj                    = task->extra.tmd;
        obj->clutRowOffset     = 2;
        obj->texturePageOffset = 0;
        enemy->workType        = ENEMY_WORK_PLAIN;
        msg.context.loc.stage  = 0;
        msg.context.loc.area   = 0x2C;
        msg.command            = arg0->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        arg0->state++;
    }
}

static void func_actor_342400_80162CA8(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

static void func_actor_342400_80162CBC(Task* arg0)
{
    OverlayEncounterSingleWork* work = arg0->work;
    Enemy*                      enemy;
    Task*                       task;
    GfxCoord*                   coord;
    ActorCommand                msg;

    enemy = work->enemy;
    task  = enemy->task;
    coord = task->extra.tmd->coords;
    if (enemy->hp <= 0) {
        gMadChaserWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(arg0);
        return;
    }
    if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
        msg.context.loc.stage = 0;
        msg.context.loc.area  = 0x2C;
        msg.command           = 5;
        TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        gMadChaserWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(arg0);
    }
}

static void func_actor_342400_80162DA0(Task* arg0)
{
    OverlayEncounterSingleWork* work;
    Enemy*                      enemy;
    TmdObject*                  obj;

    work = memCalloc(8, 0);
    if (work != NULL) {
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&D_actor_207000_801575F0, 2, 0, 0);
        if (enemy != NULL) {
            gMadChaserWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
            work->enemy                                                    = enemy;
            enemy->placeKey                                                = gMadChaserWaveEnemyCount << 12;
            gMadChaserWaveEnemyCount++;
            obj                    = enemy->task->extra.tmd;
            obj->texturePageOffset = 2;
            obj->clutRowOffset     = 4;
            arg0->state++;
            return;
        }
    }
    taskKill(arg0);
}

#include "../../shared/mad_chaser_waves_reveal_second.inc.c"

static void func_actor_342400_80162F08(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

static void func_actor_342400_80162F1C(Task* arg0)
{
    OverlayEncounterSingleWork* work = arg0->work;
    Enemy*                      enemy;
    Task*                       task;
    GfxCoord*                   coord;
    ActorCommand                msg;

    enemy = work->enemy;
    task  = enemy->task;
    coord = task->extra.tmd->coords;
    if (enemy->hp <= 0) {
        gMadChaserWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(arg0);
        return;
    }
    if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
        msg.context.loc.stage = 0;
        msg.context.loc.area  = 0;
        msg.command           = 5;
        TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        gMadChaserWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(arg0);
    }
}

static void func_actor_342400_80162FFC(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

#include "../../shared/mad_chaser_waves_pair_reveal_first.inc.c"

#include "../../shared/mad_chaser_waves_pair_reveal_second.inc.c"

#include "../../shared/mad_chaser_waves_pair_watch.inc.c"

static void func_actor_342400_801631DC(s16 arg0)
{
    if (arg0 == 0) {
        D_shelter_b3_garbage_incinerator_80187328[1] = 2;
        return;
    }
    D_shelter_b3_garbage_incinerator_80187328[1] = 0x11;
}

void madChaserWaveSpawnSlot(s16 arg0, s16 arg1, s16 arg2)
{
    switch (arg1) {
        case 0:
            Task_SpawnFromTable(D_actor_342400_8016BFE0, 1, (arg0 << 16) + arg2 + (func_actor_342400_801624A4() << 8), 0);
            break;
        case 1:
            Task_SpawnFromTable(D_actor_342400_8016BFE0, 2, (arg0 << 16) + arg2 + (func_actor_342400_801624A4() << 8), 0);
            break;
        case 2:
            Task_SpawnFromTable(D_actor_342400_8016BFE0, 3, (arg0 << 16) + arg2 + (func_actor_342400_801624A4() << 8), 0);
            break;
    }
}

#include "../../shared/mad_chaser_waves_pair_drop_dead.inc.c"
