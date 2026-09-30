#include "actor_342400_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gamemain.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/hopper_waves.h"
#include "../../shared/hopping_enemy.h"

/// 4-byte record in the table at `D_actor_342400_8016C010`, indexed (1..16)
/// by `gGameSession->enemyCullZone`. `func_actor_342400_801626CC` compares
/// an enemy's x against `limit` when `axis` is 0 and its z otherwise.
typedef struct Actor342400Limit {
    /* 0x0 */ s16 axis;
    /* 0x2 */ s16 limit;
} Actor342400Limit;
STATIC_ASSERT_SIZEOF(Actor342400Limit, 0x4);

extern TaskDesc D_801575F0; // absolute, spawned by func_actor_342400_80162DA0
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, ActorCommand* request);
    } handler;
} Actor342400MessageEntry;
STATIC_ASSERT_SIZEOF(Actor342400MessageEntry, 8);

extern Actor342400MessageEntry D_actor_342400_8016BF48[2]; // stored into `Task::msgTable` by func_actor_342400_801628F0
extern OverlayEncounterSlot    gHopperWaveSlots[];
extern TaskDesc                D_actor_342400_8016BFE0[];
extern Actor342400Limit        D_actor_342400_8016C010[];
extern s16                     D_actor_342400_8016C054[][4]; // spawn variant per player-position band, 4 random picks

                                                             // spawn counter, `<< 12` into `GpEnemy::placeKey`

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

void func_actor_342400_801626AC(Task*, s32, ActorCommand* request);

Actor342400MessageEntry D_actor_342400_8016BF48[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_342400_801626AC } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

OverlayEncounterSlot gHopperWaveSlots[17] = {
    { 1, 1, { 0, 0 }, 0 },
    { 0, 1, { 0, 0 }, 0 },
    { 2, 1, { 0, 0 }, 0 },
    { 2, 1, { 0, 0 }, 0 },
    { 1, 1, { 0, 0 }, 0 },
    { 0, 17, { 0, 0 }, 0 },
    { 0, 33, { 0, 0 }, 0 },
    { 2, 1, { 0, 0 }, 0 },
    { 0, 1, { 0, 0 }, 0 },
    { 2, 1, { 0, 0 }, 0 },
    { 0, 1, { 0, 0 }, 0 },
    { 1, 1, { 0, 0 }, 0 },
    { 2, 1, { 0, 0 }, 0 },
    { 1, 1, { 0, 0 }, 0 },
    { 0, 17, { 0, 0 }, 0 },
    { 0, 33, { 0, 0 }, 0 },
    { 2, 1, { 0, 0 }, 0 },
};

void func_actor_342400_80162748(Task*);
void func_actor_342400_801627C0(Task*);
void func_actor_342400_80162824(Task*);
void func_actor_342400_80162888(Task*);

TaskDesc D_actor_342400_8016BFE0[4] = {
    { 0, 32, func_actor_342400_80162748, { .model = NULL } },
    { 0, 97, func_actor_342400_801627C0, { .model = NULL } },
    { 0, 97, func_actor_342400_80162824, { .model = NULL } },
    { 0, 97, func_actor_342400_80162888, { .model = NULL } },
};

Actor342400Limit D_actor_342400_8016C010[17] = {
    { 0, 0 },
    { 0, 1295 },
    { 0, 2700 },
    { 0, 4300 },
    { 0, 5700 },
    { 0, 7295 },
    { 0, 8700 },
    { 1, -7800 },
    { 1, -9210 },
    { 1, -0x2A35 },
    { 1, -0x2FB2 },
    { 1, -0x3D90 },
    { 1, -0x4312 },
    { 1, -0x4989 },
    { 1, -0x4F0B },
    { 1, -0x5CF8 },
    { 1, -0x6275 },
};

s16 D_actor_342400_8016C054[6][4] = {
    { 2, 3, 11, 12 },
    { 3, 4, 12, 5 },
    { 5, 6, 7, 13 },
    { 7, 8, 13, 14 },
    { 8, 9, 14, 15 },
    { 9, 15, 9, 15 },
};

TmdBone D_actor_342400_8016C084[1] = {
#include "assets/actor_342400_model_0AD4C_skeleton.inc"
};

u32 D_actor_342400_8016C0A8[1] = {
#include "assets/actor_342400_model_0AD4C_partVerts.inc"
};

SVECTOR D_actor_342400_8016C0AC[48] = {
#include "assets/actor_342400_model_0AD4C_verts.inc"
};

SVECTOR D_actor_342400_8016C22C[53] = {
#include "assets/actor_342400_model_0AD4C_normals.inc"
};

u32 D_actor_342400_8016C3D4[486] = {
#include "assets/actor_342400_model_0AD4C_stream.inc"
};

TmdSource gHopperChunkModel0 = {
    0,
    3260,
    0,
    1,
    D_actor_342400_8016C0A8,
    D_actor_342400_8016C0AC,
    D_actor_342400_8016C22C,
    D_actor_342400_8016C084,
    D_actor_342400_8016C3D4,
};

TmdBone D_actor_342400_8016CB90[1] = {
#include "assets/actor_342400_model_0B3F0_skeleton.inc"
};

u32 D_actor_342400_8016CBB4[1] = {
#include "assets/actor_342400_model_0B3F0_partVerts.inc"
};

SVECTOR D_actor_342400_8016CBB8[28] = {
#include "assets/actor_342400_model_0B3F0_verts.inc"
};

SVECTOR D_actor_342400_8016CC98[37] = {
#include "assets/actor_342400_model_0B3F0_normals.inc"
};

u32 D_actor_342400_8016CDC0[276] = {
#include "assets/actor_342400_model_0B3F0_stream.inc"
};

TmdSource gHopperChunkModel1 = {
    0,
    1828,
    0,
    1,
    D_actor_342400_8016CBB4,
    D_actor_342400_8016CBB8,
    D_actor_342400_8016CC98,
    D_actor_342400_8016CB90,
    D_actor_342400_8016CDC0,
};

TmdBone D_actor_342400_8016D234[1] = {
#include "assets/actor_342400_model_0B960_skeleton.inc"
};

u32 D_actor_342400_8016D258[1] = {
#include "assets/actor_342400_model_0B960_partVerts.inc"
};

SVECTOR D_actor_342400_8016D25C[25] = {
#include "assets/actor_342400_model_0B960_verts.inc"
};

SVECTOR D_actor_342400_8016D324[32] = {
#include "assets/actor_342400_model_0B960_normals.inc"
};

u32 D_actor_342400_8016D424[215] = {
#include "assets/actor_342400_model_0B960_stream.inc"
};

TmdSource gHopperChunkModel2 = {
    0,
    1448,
    0,
    1,
    D_actor_342400_8016D258,
    D_actor_342400_8016D25C,
    D_actor_342400_8016D324,
    D_actor_342400_8016D234,
    D_actor_342400_8016D424,
};

TmdBone D_actor_342400_8016D7A4[9] = {
#include "assets/actor_342400_model_0E740_skeleton.inc"
};

u32 D_actor_342400_8016D8E8[9] = {
#include "assets/actor_342400_model_0E740_partVerts.inc"
};

SVECTOR D_actor_342400_8016D90C[160] = {
#include "assets/actor_342400_model_0E740_verts.inc"
};

SVECTOR D_actor_342400_8016DE0C[206] = {
#include "assets/actor_342400_model_0E740_normals.inc"
};

u32 D_actor_342400_8016E47C[2105] = {
#include "assets/actor_342400_model_0E740_stream.inc"
};

TmdSource D_actor_342400_80170560 = {
    0,
    11212,
    3088,
    9,
    D_actor_342400_8016D8E8,
    D_actor_342400_8016D90C,
    D_actor_342400_8016DE0C,
    D_actor_342400_8016D7A4,
    D_actor_342400_8016E47C,
};

DamageAttack D_actor_342400_80170584[1] = {
    { 22, 0 },
};

EnemyParams gHopperEnemyParams = { D_actor_342400_80170584, 110, 20, 40, 1, 100, 10, 100, 0 };

static void func_actor_342400_80162324(Task* arg0);
static void func_actor_342400_801631DC(s16 arg0);

#include "../../shared/hopper_waves_pair_spawn.inc.c"

void hopperWavePairCull(Task* arg0)
{
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;
    GpEnemy*                  enemy;
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
            Gp_DispatchMsgPtr(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
            work->enemy0 = NULL;
        }
    } else {
        work->goneMask |= 1;
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
            Gp_DispatchMsgPtr(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
            work->enemy1 = NULL;
        }
    } else {
        work->goneMask |= 2;
    }
}

static void func_actor_342400_80162324(Task* arg0)
{
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;
    s16                       count;
    s16                       i;
    s16                       idx;
    s16                       type;
    s16                       arg;

    count = 0;
    for (i = 0; i < 17; i++) {
        if (gHopperWaveSlots[i].status == 1) {
            count++;
        }
    }
    if (count < 3) {
        idx = work->nextSlot;
        if (idx < 17 && gGameSession->sceneClock >= 0x3D) {
            type = gHopperWaveSlots[idx].kind;
            arg  = gHopperWaveSlots[idx].command;
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
    GfxCoord* coord = (*Gp_ActorSlots)->extra.tmd->coords;
    s16       x     = coord->coord.t[0];
    s16       z     = coord->coord.t[2];

    if (x <= 5000) {
        return D_actor_342400_8016C054[0][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (x <= 8000) {
        return D_actor_342400_8016C054[1][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (x <= 11000) {
        return D_actor_342400_8016C054[2][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (z >= -5500) {
        return D_actor_342400_8016C054[3][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (z >= -8500) {
        return D_actor_342400_8016C054[4][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (z >= -11500) {
        return D_actor_342400_8016C054[5][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (z >= -24500) {
        return D_actor_342400_8016C054[5][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    return D_actor_342400_8016C054[5][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
}

void func_actor_342400_801626AC(Task* arg0, s32 arg1, ActorCommand* request)
{
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;
    u16                       id   = request->command;

    if (id == 4) {
        work->stop = id;
    }
}

static s16 func_actor_342400_801626CC(s16 arg0, s16 arg1, s16 arg2)
{
    if (arg0 == 0 || arg0 > 0x10) {
        return 0;
    }
    if (D_actor_342400_8016C010[arg0].axis == 0) {
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
    hopperWaveOpen,
    func_actor_342400_80162A34,
    func_actor_342400_80162AB0,
} };

void func_actor_342400_80162748(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_342400_80161E24;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[arg0->state](arg0);
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
    TaskFuncTable4 sp;

    sp = D_actor_342400_80161E34;
    sp.funcs[arg0->state](arg0);
}

/// The second spawner task's four state handlers, dispatched by
/// `func_actor_342400_80162824`.
static const TaskFuncTable4 D_actor_342400_80161E44 = { {
    func_actor_342400_80162DA0,
    hopperWaveRevealSecond,
    func_actor_342400_80162F08,
    func_actor_342400_80162F1C,
} };

/// Runs the second spawner task's handler for its `Task::state`.
void func_actor_342400_80162824(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_342400_80161E44;
    sp.funcs[arg0->state](arg0);
}

/// The five state handlers `func_actor_342400_80162888` dispatches through by
/// `Task::state`.
static const TaskFuncTable5 D_actor_342400_80161E54 = { {
    hopperWavePairSpawn,
    func_actor_342400_80162FFC,
    hopperWavePairRevealFirst,
    hopperWavePairRevealSecond,
    hopperWavePairWatch,
} };

void func_actor_342400_80162888(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_342400_80161E54;
    sp.funcs[arg0->state](arg0);
}

static void func_actor_342400_801628F0(Task* arg0)
{
    OverlayEncounterCtrlWork* work;
    s32                       i;

    if (gGameSession->spawnPhase[1] == GAME_SESSION_SPAWN_COMPLETE || gGameSession->spawnPhase[0] == GAME_SESSION_SPAWN_IDLE ||
        (work = memCalloc(6, 0)) == NULL) {
        taskKill(arg0);
        return;
    }
    for (i = 16; i >= 0; i--) {
        gHopperWaveSlots[i].status = 0;
    }
    gHopperWaveEnemyCount = 0;
    arg0->work            = work;
    arg0->msgTable        = D_actor_342400_8016BF48;
    arg0->state++;
}

#include "../../shared/hopper_waves_open.inc.c"

static void func_actor_342400_80162A34(Task* arg0)
{
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;

    if (++work->frames == 15) {
        (Gp_IncStateF0Ref)(0);
        gGameSession->spawnPhase[1] = GAME_SESSION_SPAWN_ARMED;
        Gp_ArmStateF0(1);
        arg0->state++;
    }
}

static void func_actor_342400_80162AB0(Task* arg0)
{
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;
    s16                       count;
    s32                       i;

    count = 0;
    if (work->stop != 4) {
        func_actor_342400_80162324(arg0);
        for (i = 0; i < 17; i++) {
            if (gHopperWaveSlots[i].status == 2) {
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
    GpEnemy*                    enemy;

    work = memCalloc(8, 0);
    if (work != NULL) {
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(D_actor_342400_80173A54, 1, 0, 0);
        if (enemy != NULL) {
            gHopperWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = 1;
            work->enemy                                                 = enemy;
            enemy->placeKey                                             = gHopperWaveEnemyCount << 12;
            gHopperWaveEnemyCount++;
            arg0->state++;
            return;
        }
    }
    taskKill(arg0);
}

static void func_actor_342400_80162C10(Task* arg0)
{
    OverlayEncounterSingleWork* work = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    enemy;
    Task*                       task;
    TmdObject*                  obj;
    ActorCommand                msg;

    enemy = work->enemy;
    task  = enemy->task;
    if (++work->frames > 60) {
        obj                    = task->extra.tmd;
        obj->clutRowOffset     = 2;
        obj->texturePageOffset = 0;
        enemy->workType        = 0x900;
        msg.context.loc.stage  = 0;
        msg.context.loc.area   = 0x2C;
        msg.command            = arg0->spawnArg1.value;
        Gp_DispatchMsgPtr(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        arg0->state++;
    }
}

static void func_actor_342400_80162CA8(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

static void func_actor_342400_80162CBC(Task* arg0)
{
    OverlayEncounterSingleWork* work = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    enemy;
    Task*                       task;
    GfxCoord*                   coord;
    ActorCommand                msg;

    enemy = work->enemy;
    task  = enemy->task;
    coord = task->extra.tmd->coords;
    if (enemy->hp <= 0) {
        gHopperWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = 2;
        taskKill(arg0);
        return;
    }
    if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
        msg.context.loc.stage = 0;
        msg.context.loc.area  = 0x2C;
        msg.command           = 5;
        Gp_DispatchMsgPtr(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        gHopperWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = 2;
        taskKill(arg0);
    }
}

static void func_actor_342400_80162DA0(Task* arg0)
{
    OverlayEncounterSingleWork* work;
    GpEnemy*                    enemy;
    TmdObject*                  obj;

    work = memCalloc(8, 0);
    if (work != NULL) {
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&D_801575F0, 2, 0, 0);
        if (enemy != NULL) {
            gHopperWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = 1;
            work->enemy                                                 = enemy;
            enemy->placeKey                                             = gHopperWaveEnemyCount << 12;
            gHopperWaveEnemyCount++;
            obj                    = enemy->task->extra.tmd;
            obj->texturePageOffset = 2;
            obj->clutRowOffset     = 4;
            arg0->state++;
            return;
        }
    }
    taskKill(arg0);
}

#include "../../shared/hopper_waves_reveal_second.inc.c"

static void func_actor_342400_80162F08(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

static void func_actor_342400_80162F1C(Task* arg0)
{
    OverlayEncounterSingleWork* work = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    enemy;
    Task*                       task;
    GfxCoord*                   coord;
    ActorCommand                msg;

    enemy = work->enemy;
    task  = enemy->task;
    coord = task->extra.tmd->coords;
    if (enemy->hp <= 0) {
        gHopperWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = 2;
        taskKill(arg0);
        return;
    }
    if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
        msg.context.loc.stage = 0;
        msg.context.loc.area  = 0;
        msg.command           = 5;
        Gp_DispatchMsgPtr(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        gHopperWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = 2;
        taskKill(arg0);
    }
}

static void func_actor_342400_80162FFC(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

#include "../../shared/hopper_waves_pair_reveal_first.inc.c"

#include "../../shared/hopper_waves_pair_reveal_second.inc.c"

#include "../../shared/hopper_waves_pair_watch.inc.c"

static void func_actor_342400_801631DC(s16 arg0)
{
    if (arg0 == 0) {
        D_shelter_b3_garbage_incinerator_80187328[1] = 2;
        return;
    }
    D_shelter_b3_garbage_incinerator_80187328[1] = 0x11;
}

void hopperWaveSpawnSlot(s16 arg0, s16 arg1, s16 arg2)
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

#include "../../shared/hopper_waves_pair_drop_dead.inc.c"
