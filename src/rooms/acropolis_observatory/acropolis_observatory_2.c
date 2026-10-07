#include "acropolis_observatory_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/scene_combat.h"
#include "gameplay/view.h"

#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room.h"

/// Per-frame paths the two streamed scenes walk the player's matrix along,
/// indexed by `gCdCmdQueue.movieFrame + 0xA8`, each with the script pair its
/// scene runs.
extern SVECTOR D_acropolis_observatory_8017E80C[];

extern SVECTOR D_acropolis_observatory_8017F16C[];

void func_acropolis_observatory_8017D9A8(Task*);
void func_acropolis_observatory_8017DD3C(Task*);
void func_acropolis_observatory_8017E0D4(Task*);
void func_acropolis_observatory_8017E134(Task*);

TaskDesc D_acropolis_observatory_8017E7DC[4] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_observatory_8017D9A8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_observatory_8017DD3C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_observatory_8017E0D4, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_observatory_8017E134, { .value = 0 } },
};

SVECTOR D_acropolis_observatory_8017E80C[300] = {
#include "assets/acropolis_observatory_path_0124C.inc"
};

SVECTOR D_acropolis_observatory_8017F16C[300] = {
#include "assets/acropolis_observatory_path_01BAC.inc"
};

static AnimationPackedPose _gAcropolisObservatoryAnimation02878Bank1[7] = {
#include "assets/acropolis_observatory_animation_02878_bank1.inc"
};

static AnimationPackedRotation _gAcropolisObservatoryAnimation02878Bank4[65] = {
#include "assets/acropolis_observatory_animation_02878_bank4.inc"
};

static AnimationRecord _gAcropolisObservatoryAnimation02878Records[123] = {
#include "assets/acropolis_observatory_animation_02878_records.inc"
};

static u16 _gAcropolisObservatoryAnimation02878Indices[20] = {
#include "assets/acropolis_observatory_animation_02878_indices.inc"
};

AnimationSet gAcropolisObservatoryAnimation02878 = {
    _gAcropolisObservatoryAnimation02878Records,
    _gAcropolisObservatoryAnimation02878Indices,
    { NULL, _gAcropolisObservatoryAnimation02878Bank1, NULL, NULL, _gAcropolisObservatoryAnimation02878Bank4, NULL, NULL, NULL },
};

AnimationSet* gAcropolisObservatoryPlayerAnimationSets[2] = { NULL, &gAcropolisObservatoryAnimation02878 };

/// Streamed-scene ride, entry 0 of the room's task table: the same ride as
/// `func_acropolis_observatory_8017DD3C` (entry 1), walking the player's matrix
/// along `D_acropolis_observatory_8017E80C` instead and ending on view index 2.
/// State 0 allocates the `RoomMoviePathWork` block, cues the stream (slot-6 msg
/// 0xFA4), captures slot 3 and the player's coordinate matrix and republishes
/// the player's weapon to slot 3 with a 0x3E8 record. State 1 waits for the
/// stream (`gCdCmdQueue::movieReady`), starts the script pair and adopts its
/// task as a child. State 2 drives the ride, letting the pad skip it once
/// through the fade-out task and warping slot 3 when that task has finished
/// or frame 0xE6 passes.
/// State 3 waits for slot 3 to go idle, releases it and records the view.
/// State 4 stops the stream and kills the task.
void func_acropolis_observatory_8017D9A8(Task* task)
{
    AnimationPlayRequest rec;
    ActorTransform       place;
    s32                  killed;
    RoomMoviePathWork*   work;
    CdCmdQueue*          queue;
    s32                  weaponId;

    queue = &gCdCmdQueue;
    work  = task->work;
    switch (task->state) {
        case 0:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            weaponId                                     = gPlayerStatus.weapon;
            rec.source.index                             = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animationId                              = 1;
            rec.blend                                    = ANIMATION_BLEND_RESET;
            rec.blendFrames                              = 0;
            rec.enableWorldCollision                     = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case 1:
            if (queue->movieReady != 0) {
                work->padScriptTask           = padScriptSpawn(D_acropolis_observatory_80183480,
                                                               D_acropolis_observatory_80183498);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->playerMtx->t[0] = D_acropolis_observatory_8017E80C[queue->movieFrame + 0xA8].vx;
            work->playerMtx->t[1] = D_acropolis_observatory_8017E80C[queue->movieFrame + 0xA8].vy;
            work->playerMtx->t[2] = D_acropolis_observatory_8017E80C[queue->movieFrame + 0xA8].vz;
            if (work->skipFadeStarted != 0) {
                if (taskPollKill(work->skipFadeTask, &killed) != 0) {
                    place.pos.vx = -0x968;
                    place.pos.vy = -0xBAD;
                    place.pos.vz = -0x6D4;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0x400;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3E9, &place, 0);
                    taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 3, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 2, 0, 0);
                work->skipFadeStarted = 1;
            }
            if ((queue->movieFrame + 0xA8) >= 0xE6) {
                place.pos.vx = -0x968;
                place.pos.vy = -0xBAD;
                place.pos.vz = -0x6D4;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3F2, &place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = viewFindLogicalIndex(2);
                task->state                                                = task->state + 1;
            }
            break;

        case 4:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

/// Streamed-scene ride, entry 1 of the room's task table. State 0 allocates the
/// `RoomMoviePathWork` block, cues the stream (slot-6 msg 0xFA4), captures slot 3
/// and the player's coordinate matrix in the block, and republishes the
/// player's weapon to slot 3 with a 0x3E8 record. State 1 waits for the stream
/// to come up (`gCdCmdQueue::movieReady`), then starts the script pair and
/// adopts its task as a child. State 2 drives the ride: every frame it moves
/// the player's matrix to the `field_1EA`th entry of the path table; the first
/// time `padIsStartPressed` reports a Start press it spawns the fade-out task (entry 2
/// of the room's task table), and once that task has finished it warps slot 3
/// with a 0x3E9 placement and spawns the fade-in (entry 3); past frame 0xE6 it
/// sends the same placement as a
/// 0x3F2 and moves on either way. State 3 waits for slot 3 to go idle (msg
/// 0x3F0), releases it (0x3F1) and records the view in the save. State 4 stops
/// the stream (0xFA5), clears the scene flags and kills the task.
void func_acropolis_observatory_8017DD3C(Task* task)
{
    AnimationPlayRequest rec;
    ActorTransform       place;
    s32                  killed;
    RoomMoviePathWork*   work;
    CdCmdQueue*          queue;
    s32                  weaponId;

    queue = &gCdCmdQueue;
    work  = task->work;
    switch (task->state) {
        case 0:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            weaponId                                     = gPlayerStatus.weapon;
            rec.source.index                             = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animationId                              = 1;
            rec.blend                                    = ANIMATION_BLEND_RESET;
            rec.blendFrames                              = 0;
            rec.enableWorldCollision                     = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case 1:
            if (queue->movieReady != 0) {
                work->padScriptTask           = padScriptSpawn(D_acropolis_observatory_801834A0,
                                                               D_acropolis_observatory_801834B8);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->playerMtx->t[0] = D_acropolis_observatory_8017F16C[queue->movieFrame + 0xA8].vx;
            work->playerMtx->t[1] = D_acropolis_observatory_8017F16C[queue->movieFrame + 0xA8].vy;
            work->playerMtx->t[2] = D_acropolis_observatory_8017F16C[queue->movieFrame + 0xA8].vz + 0xC8;
            if (work->skipFadeStarted != 0) {
                if (taskPollKill(work->skipFadeTask, &killed) != 0) {
                    place.pos.vx = -0x8F8;
                    place.pos.vy = -0xBAD;
                    place.pos.vz = -0x2936;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0x400;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3E9, &place, 0);
                    taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 3, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 2, 0, 0);
                work->skipFadeStarted = 1;
            }
            if ((queue->movieFrame + 0xA8) >= 0xE6) {
                place.pos.vx = -0x8F8;
                place.pos.vy = -0xBAD;
                place.pos.vz = -0x2936;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3F2, &place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = viewFindLogicalIndex(4);
                task->state                                                = task->state + 1;
            }
            break;

        case 4:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

/// Fade-out task, entry 2 of the room's task table: subtracts a full-screen
/// grey that grows by 0x20 a frame, counted in `Task::killCountdown`, and asks
/// for its own release once the screen is black. The streamed-scene tasks
/// wait on that release before warping the player.
void func_acropolis_observatory_8017E0D4(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = (u8)arg0->killCountdown;
    fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        taskRequestKill(arg0, 0);
    }
}

/// Fade-in task, entry 3 of the room's task table: the reverse of
/// `func_acropolis_observatory_8017E0D4`, subtracting a grey that shrinks by
/// 0x20 a frame from black, then killing itself.
void func_acropolis_observatory_8017E134(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = ~(u8)arg0->killCountdown;
    fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}
