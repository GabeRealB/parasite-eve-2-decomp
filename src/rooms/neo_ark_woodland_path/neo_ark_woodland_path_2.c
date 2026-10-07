#include "rooms/neo_ark_woodland_path.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_woodland_path_private.h"

#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"
#include "../../shared/falling_leaves.h"
#include "../../shared/roaming_enemies.h"

#define ABS_DIFF(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))

/// The distance between `a` and `b`, spelled as a conditional subtraction.

/// The room's frame countdown at `gRoamerCooldown`. Signed,
/// although the arithmetic reads compile as `lhu` (`func_...8018154C` adds to
/// it, `func_...80180DDC` counts it down): a load whose result is truncated by
/// the following `sh` only has to supply the low half, so GCC picks the
/// unsigned form by itself, while the `lh` comparisons and the -1 the 0x7DB
/// handler stores need the signed declaration. A union offering both views
/// compiles the same instructions but marks every access `in_struct`, and that
/// flag decides the scheduler's dependence analysis - it pinned a load after a
/// store in `roamerArmPoolB`. See
/// `DECOMPILATION_LEARNINGS.md`, "A union that only names a view costs
/// `in_struct`".
extern s16 gRoamerCooldown;

extern ActorCommand gRoamerCommand;

/// The room's five spawn slots: `roamerBankRetreat` fills the
/// first free one with a countdown and `roamerAmbushMsg`
/// hands slot 0 to the spawn it triggers and clears it. Read as `lhu` by the
/// handler and as `lh` by the slot filler, so each site names the view it uses
/// (`[0]` here, an `s16*` cast there).
extern u16 gRoamerReserveHp[5];

/// Ceiling `roamerBankRetreat` clamps a spawn slot to
/// (0x1A4, 420 frames). Only the first halfword is this unit's; the run
/// continues into the room's parameter block, so the extent is splat's.

/// The same run reached through its leading label, which is how
/// `roamerArmPoolB` reads the ceiling: element 2 is
/// `D_...8494C[0]`, 420 frames. splat names both addresses because the compiled
/// code names both, and the two are different code - an index keeps this
/// symbol in a register and takes the offset as the load's displacement, while
/// naming `D_...8494C` addresses it directly.
extern EnemyParams  gRoamerParams;
extern DamageAttack D_neo_ark_woodland_path_80184930[6];

/// The room's arming count, packed into game flag 0x10A as a nibble:
/// `roamerArmPoolB` adds the slot's spawn count to it and
/// then caps it at 5, the number of slots `D_...84A60` has. Signed, though the
/// add reads it as `lhu` - the result is truncated by the following `sh`, so
/// only the low half matters and GCC picks the unsigned load by itself.
extern s16 gRoamerReserveCount;

/// How many spawns each slot arms, indexed by `gGameSession->location.loc.variant` (the
/// slot the session is in): the byte `func_...80180C6C` adds to
/// `D_...80184990`, and the gate `func_...80180DDC` tests against zero.
extern u8 gRoamerArmCountsB[];

/// The message-handler table `roamerArmPoolB` parks in
/// `Task::msgTable`: a placement request (0x13EF,
/// `_roamerLatchSpawnRequestPoolB`), a countdown bump (0x13F4) and the
/// 0x7DB command handler `roamerAmbushMsg`.
extern TaskMessageEntry gRoamerMsgTableB[];

/// The same gate for the arm-state one step earlier: `func_...80180568` tests
/// it against zero and `func_...801806D8` reads the slot's count from it. One
/// byte per session slot, indexed by `gGameSession->location.loc.variant`, like
/// `D_...84970` above.
extern u8 gRoamerArmCountsA[];

/// `func_...80180568`'s own message-handler table, parked in `Task::msgTable`
/// as `D_...849F4` is by `func_...80180C6C`: the same three ids, answered by
/// the placement request `roamerLatchRequest`, the spawn-slot
/// filler `roamerBankRetreat` and a 0x7DB handler that
/// ignores the message.
extern TaskMessageEntry gRoamerMsgTableA[];

/// Set once a spawn slot has been armed, read by the room's other states.
extern s16 gRoamerReleasePending;

/// Spawn point requested by the room's 0x13EF handlers (the message's third
/// byte, taken only while `D_...8498E` has run out and the byte differs from
/// the previous request). One-based index into the placement table of the
/// running sequence; zero means no request, and the per-frame states clear it
/// every frame whether or not they placed a spawn.
extern s16 gRoamerSpawnRequest;

/// The byte the last 0x13EF message carried, kept so that a repeated request
/// is dropped rather than placed again.
extern s16 gRoamerLastRequest;

/// The room's spawn placements, indexed by `D_...80184992 - 1`.
extern RoamerSpawnPoint gRoamerSpawnPointsA[];

/// The second arming sequence's spawn placements, which
/// `func_...80180DDC` picks from by `D_...80184992 - 1`. Five of them; any
/// request past the fourth takes the last.
extern RoamerSpawnPoint D_neo_ark_woodland_path_80184A14[5];

/// `gSceneCombatState.battleRefs` as `func_...801806D8` saw it on the previous frame, so
/// that it can tell the reference count was non-zero before the frame began.
extern s16 gRoamerPrevBattleRefs;

static s32 _neoArkWoodlandPathIgnoreRoamerCommand(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg);
s32        func_neo_ark_woodland_path_8018154C(Task*, s32, s32, s32);
static s32 _roamerLatchSpawnRequestPoolB(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

void func_neo_ark_woodland_path_801814E8(Task*);
void func_neo_ark_woodland_path_801815D4(Task*);

DamageAttack D_neo_ark_woodland_path_80184930[6] = {
    { 30, 7 },
    { 30, 7 },
    { 50, 7 },
    { 50, 7 },
    { 40, 0 },
    { 40, 0 },
};

EnemyParams gRoamerParams = { D_neo_ark_woodland_path_80184930, 420, 115, 200, 5, 100, 10, 100, 10 };

// Retained numeric records following the enemy parameters.
u16 D_neo_ark_woodland_path_80184958[3][4] = {
    { 0, 900, 3, 0 },
    { 0, 800, 5, 0 },
    { 0, 500, 7, 0 },
};

u8 gRoamerArmCountsB[16] = {
    0,
    1,
    3,
    2,
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

u8 gRoamerArmCountsA[14] = {
    0,
    3,
    2,
    4,
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

s16 gRoamerCooldown = 30;

s16 gRoamerReserveCount = 0;

s16 gRoamerSpawnRequest = 0;

s16 gRoamerLastRequest = 0;

s16 gRoamerReleasePending = 0;

TaskMessageEntry gRoamerMsgTableA[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, roamerLatchRequest },
    { ROOM_MESSAGE_ACTOR_EVENT, roamerBankRetreat },
    { ACTOR_COMMAND_MESSAGE_APPLY, _neoArkWoodlandPathIgnoreRoamerCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

RoamerSpawnPoint gRoamerSpawnPointsA[7] = {
    { -2000, 0, 8977, -1024 },
    { 379, 0, 7700, 2048 },
    { 7950, 0, 4650, -1024 },
    { 7950, 0, -4650, -1024 },
    { -633, 0, -1000, 2048 },
    { -2280, 0, -6378, -1024 },
    { 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
};

s16 gRoamerPrevBattleRefs = 0;

TaskMessageEntry gRoamerMsgTableB[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, _roamerLatchSpawnRequestPoolB },
    { ROOM_MESSAGE_ACTOR_EVENT, func_neo_ark_woodland_path_8018154C },
    { ACTOR_COMMAND_MESSAGE_APPLY, roamerAmbushMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

RoamerSpawnPoint D_neo_ark_woodland_path_80184A14[5] = {
    { 8884, 0, 2200, 2048 },
    { 0, 0, -2000, 200 },
    { -4200, 0, -3000, 0 },
    { -4100, 0, 2000, 1900 },
    { -6500, 0, 2000, 2200 },
};

s16 D_neo_ark_woodland_path_80184A3C[4] = {
    0x7FFF,
    0x7FFF,
    0x7FFF,
    0x7FFF,
};

TaskDesc D_neo_ark_woodland_path_80184A44[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_woodland_path_801815D4, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_woodland_path_801814E8, { .value = 0 } },
};

ActorCommand gRoamerCommand = { { .loc = { 0, 0 } }, 0 };

u16 gRoamerReserveHp[5];

static void _neoArkWoodlandPathAdvanceRoamerPoolAState(Task* task);

static void _neoArkWoodlandPathAdvanceRoamerPoolBState(Task* task);

static void func_neo_ark_woodland_path_80180DDC(Task* task);

/// Scatters effects around the slot-3 task's model while its root coordinate
/// is at a y of 0x12C or more (y grows downward) and no event is running.
/// Once per frame, the spawn chance is set from how far model parts 15 and 18
/// moved since the previous frame. Two effects are rolled at the model's x and
/// z with y fixed at 0xC8: effect `gRoomEffectWaterRippleId` against that chance, then effect
/// `gRoomEffectWaterSprayId` against the chance less 0x20. The same function also sets the
/// room effect mode to 2 while the root y is below 0x11. On its first run it
/// stores the two effect ids and the starting part positions.
void func_neo_ark_woodland_path_8017EA08(Task* task)
{
    EffectWork* work;
    Task*       owner;
    GfxCoord*   root;
    GfxCoord*   part;
    GfxCoord    coord;
    s32         i;

    work  = task->spawnArg2.pointer;
    owner = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    root  = owner->extra.tmd->coords;
    if (task->state == 0) {
        gRoomEffectWaterRippleId = EFFECT_NEO_ARK_WOODLAND_PATH_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_NEO_ARK_WOODLAND_PATH_WATER_SPRAY;
        task->state              = 1;
        for (i = 0; i < 2; i++) {
            part                                   = &owner->extra.tmd->coords[i * 3 + 15];
            D_neo_ark_woodland_path_80181684[i].vx = part->workm.t[0];
            D_neo_ark_woodland_path_80181684[i].vy = part->workm.t[1];
            D_neo_ark_woodland_path_80181684[i].vz = part->workm.t[2];
        }
    }
    gRoomEffectState->roomEffectMode = (root->coord.t[1] < 0x11) * ROOM_EFFECT_VIEW_ENABLED;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && root->coord.t[1] >= 0x12C) {
        for (i = 0; i < 2; i++) {
            // This task keeps its spawn odds, out of 0x200, in its work block's `angle`.
            part               = &owner->extra.tmd->coords[i * 3 + 15];
            work->angle        = ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vx, part->workm.t[0]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vy, part->workm.t[1]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vz, part->workm.t[2]) + 0x20;
            coord.parent       = root->parent;
            coord.coord        = root->coord;
            coord.coord.t[0]   = root->coord.t[0];
            coord.coord.t[1]   = 0xC8;
            coord.coord.t[2]   = root->coord.t[2];
            coord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&coord);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((s32)((gRandomLcgState >> 16) & 0x1FF) < work->angle) {
                effectSpawn(gRoomEffectWaterRippleId, &coord, 0x40, 0);
            }
            work->angle    -= 0x20;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((s32)((gRandomLcgState >> 16) & 0x1FF) < work->angle) {
                effectSpawn(gRoomEffectWaterSprayId, &coord, 0x1202180, 0);
            }
            D_neo_ark_woodland_path_80181684[i].vx = part->workm.t[0];
            D_neo_ark_woodland_path_80181684[i].vy = part->workm.t[1];
            D_neo_ark_woodland_path_80181684[i].vz = part->workm.t[2];
        }
    }
}

#include "../../shared/falling_leaves_task.inc.c"

void neoArkWoodlandPathLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/falling_leaves_draw_neo_ark.inc.c"

#include "../../shared/water_ripple_task.inc.c"

void neoArkWoodlandPathWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16.inc.c"

void neoArkWoodlandPathWaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

#include "../../shared/roaming_enemies_bank_retreat.inc.c"

#include "../../shared/roaming_enemies_arm_pool_a.inc.c"

#include "../../shared/roaming_enemies_tick_pool_a.inc.c"

/// State handlers of the first arming sequence's entry task
/// `func_neo_ark_woodland_path_801814E8`: arm, run, advance, then kill.
static const TaskFuncTable4 D_neo_ark_woodland_path_8017D638 = {
    { roamerArmPoolA, roamerTickPoolA,
      _neoArkWoodlandPathAdvanceRoamerPoolAState, taskKill }
};

#include "../../shared/roaming_enemies_ambush_msg.inc.c"

#include "../../shared/roaming_enemies_arm_pool_b.inc.c"

/// Per-frame state of the arming sequence `func_...80180C6C` sets up, the
/// sibling of `func_...801806D8`: counts the room's countdown down, and once the
/// reference count on `gSceneCombatState` has dropped to zero folds the still-pending
/// spawn slots back into game flags 0x167 and 0x10A. When a spawn point has
/// been requested it hands the first pending slot to a waiting placed actor,
/// sends it the 0x7DB message and places it at one of five fixed points.
static void func_neo_ark_woodland_path_80180DDC(Task* task)
{
    s16    i;
    s16    count;
    s32    a;
    s32    b;
    Enemy* obj;
    s16    j;
    s16    k;

    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gRoamerArmCountsB[gGameSession->location.loc.variant] == 0) {
        return;
    }
    if (gRoamerCooldown > 0) {
        gRoamerCooldown--;
    }
    if (gSceneCombatState.battleRefs == 0 && gRoamerPrevBattleRefs > 0) {
        b     = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        printf("(get_flag(266)-get_total()) = %d\n", b - count);
        a     = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B);
        b     = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE, count);
        areaSyncLocationVariant(&gGameSession->location.loc);
        gRoamerCooldown = 0x96;
    }
    gRoamerPrevBattleRefs = gSceneCombatState.battleRefs;
    if (gGameSession->battleResetPending == 1 && gRoamerCooldown == 0) {
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.peTargetCount             = 0;
        gSceneCombatState.battleRefs                = 0;
        gSceneCombatState.expReward                 = 0;
        gSceneCombatState.bpReward                  = 0;
        gSceneCombatState.mpReward                  = 0;
        gGameSession->battleResetPending            = 0;
    }
    if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_FINISHED && gRoamerSpawnRequest != 0) {
        gRoamerCommand.context.loc.stage = 5;
        gRoamerCommand.context.loc.area  = 0xB;
        gRoamerCommand.command           = 0xB;
        for (i = 0; i < 2; i++) {
            if (sceneFindPlacedActor(i) == 0) {
                break;
            }
            obj = sceneFindPlacedActor(i)->spawnArg2.pointer;
            if (obj == NULL) {
                break;
            }
            if (obj->hp == -999) {
                for (j = 0; j < gRoamerReserveCount; j++) {
                    if (((s16*)gRoamerReserveHp)[j] > 0) {
                        obj->hp             = gRoamerReserveHp[j];
                        obj->reactionFlags  = 0;
                        gRoamerReserveHp[j] = 0;
                        break;
                    }
                }
                if (obj->hp > 0) {
                    sceneAcquireBattleRef(0);
                    gRoamerCooldown += 0x5A;
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(i), ACTOR_COMMAND_MESSAGE_APPLY, &gRoamerCommand, 0);
                    switch ((s16)(gRoamerSpawnRequest - 1)) {
                        case 0:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0]   = D_neo_ark_woodland_path_80184A14[0].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1]   = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2]   = D_neo_ark_woodland_path_80184A14[0].z;
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_woodland_path_80184A14[0].yaw, 1);
                            break;
                        case 1:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0]   = D_neo_ark_woodland_path_80184A14[1].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1]   = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2]   = D_neo_ark_woodland_path_80184A14[1].z;
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_woodland_path_80184A14[1].yaw, 1);
                            break;
                        case 2:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[2].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1] = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[2].z;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_woodland_path_80184A14[2].yaw, 1);
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                        case 3:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[3].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1] = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[3].z;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_woodland_path_80184A14[3].yaw, 1);
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                        case 4:
                        default:
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[4].x;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[1] = 0;
                            sceneFindPlacedActor(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[4].z;
                            gfxRotMatrixY(&sceneFindPlacedActor(i)->extra.tmd->coords->coord,
                                          D_neo_ark_woodland_path_80184A14[4].yaw, 1);
                            sceneFindPlacedActor(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                    }
                }
                break;
            }
        }
    }
    gRoamerSpawnRequest = 0;
}

/// State handlers of the second arming sequence's entry task
/// `func_neo_ark_woodland_path_801815D4`: arm, run, advance, then kill.
static const TaskFuncTable4 D_neo_ark_woodland_path_8017D684 = {
    { roamerArmPoolB, func_neo_ark_woodland_path_80180DDC,
      _neoArkWoodlandPathAdvanceRoamerPoolBState, taskKill }
};

/// Ignores actor commands delivered to the pool-A controller and returns zero.
///
/// Serves `ACTOR_COMMAND_MESSAGE_APPLY` in `gRoamerMsgTableA`. All arguments
/// are unused: the command is neither read, modified nor retained, and the
/// controller's task state is unchanged.
static s32 _neoArkWoodlandPathIgnoreRoamerCommand(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg)
{
    return 0;
}

/// Selects the pool-A room-action latch emitted by the shared fragment.
///
/// The callback type and inclusion requirements are documented in
/// `roaming_enemies_latch_request.inc.c`.
#define ROAMER_LATCH_SPAWN_REQUEST roamerLatchRequest
#include "../../shared/roaming_enemies_latch_request.inc.c"
#undef ROAMER_LATCH_SPAWN_REQUEST

/// Advances the live pool-A controller from its pre-kill state 2 to state 3.
///
/// The following controller tick runs `taskKill`; this tick only increments
/// the state and leaves the task and its message table live.
static void _neoArkWoodlandPathAdvanceRoamerPoolAState(Task* task)
{
    task->state++;
}

/// Entry task of the first arming sequence: runs the state handler
/// `D_neo_ark_woodland_path_8017D638` holds for the task's state, copying the table
/// to the stack first.
void func_neo_ark_woodland_path_801814E8(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_neo_ark_woodland_path_8017D638;
    handlers.funcs[task->state](task);
}

s32 func_neo_ark_woodland_path_8018154C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    gRoamerCooldown += 0x5A;
    return 1;
}

/// Selects the private pool-B room-action latch for this fragment inclusion.
#define ROAMER_LATCH_SPAWN_REQUEST _roamerLatchSpawnRequestPoolB
#include "../../shared/roaming_enemies_latch_request.inc.c"
#undef ROAMER_LATCH_SPAWN_REQUEST

/// Advances the live pool-B controller from its pre-kill state 2 to state 3.
///
/// The following controller tick runs `taskKill`; this tick only increments
/// the state and leaves the task and its message table live.
static void _neoArkWoodlandPathAdvanceRoamerPoolBState(Task* task)
{
    task->state++;
}

/// Entry task of the second arming sequence: runs the state handler
/// `D_neo_ark_woodland_path_8017D684` holds for the task's state, copying the table
/// to the stack first.
void func_neo_ark_woodland_path_801815D4(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_neo_ark_woodland_path_8017D684;
    handlers.funcs[task->state](task);
}
