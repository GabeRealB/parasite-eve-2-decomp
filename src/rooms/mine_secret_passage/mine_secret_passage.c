#include "rooms/mine_secret_passage.h"

#include "types.h"

#include "mine_secret_passage_private.h"

#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"

/// Staging save location this room's warp handler latches: area /
/// `field_4` / `field_1` take the three bytes the outgoing location carries.
extern RoomEventMsg D_mine_secret_passage_80183448;

static void func_mine_secret_passage_8017D8C8(Task* arg0);
static void func_mine_secret_passage_8017D914(Task* arg0);
static void func_mine_secret_passage_8017D968(Task* task);

/// State handlers of the room task `func_mine_secret_passage_8017D970` drives:
/// set-up, the one-shot state, the idle state and `taskKill`.
static const TaskFuncTable4 D_mine_secret_passage_8017D5C4 = {
    func_mine_secret_passage_8017D8C8,
    func_mine_secret_passage_8017D914,
    func_mine_secret_passage_8017D968,
    taskKill,
};

RoomEventMsg D_mine_secret_passage_80183448;

/// Runs the room's save sequence. State 0 asks for the caption, state 1 waits
/// for it and drops the periscope overlay, state 2 takes the confirm key or
/// backs out, state 3 counts the armed-shot window down before raising the PE
/// prompt, state 4 raises the helper task 0x31 and queues the sound event,
/// state 5 waits for that voice, and state 6 - the commit - copies the staged
/// location into `gMcSaveData` and reloads. Every state but the commit advances
/// through the shared `advance` tail; a confirmed cancel stops without it.
void func_mine_secret_passage_8017D60C(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            Gp_RunCapCmd(2, 0);
            arg0->state++;
            break;
        case 1:
            if (capIsBusy() != 0) {
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            arg0->state++;
            break;
        case 2:
            if (capGetVariantKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            arg0->killCountdown            = 3;
            arg0->state++;
            break;
        case 3:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if ((temp_v0 << 0x10) != 0) {
                break;
            }
            Gp_TriggerPeIfArmed();
            arg0->state++;
            break;
        case 4:
            D_mine_secret_passage_80183440.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_mine_secret_passage_80183440.fade.phase      = SCREEN_FADE_RUNNING;
            D_mine_secret_passage_80183440.fade.rampFrames = 0x1E;
            Task_Spawn(1, 0x31, 0, &D_mine_secret_passage_80183440.fade);
            sndEvtRequestScriptStart(SOUND_MINE_SECRET_PASSAGE_EXIT_TRANSIT, 0, 0);
            arg0->state++;
            break;
        case 5:
            if (SndVoice_HasActiveId(SOUND_MINE_SECRET_PASSAGE_EXIT_TRANSIT) != 0) {
                break;
            }
            arg0->state++;
            break;
        case 6:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_mine_secret_passage_80183448.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_mine_secret_passage_80183448.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_mine_secret_passage_80183448.areaId)[1];
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_mine_secret_passage_8017D7C4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler id 0x13EE of the room's `TaskMessageEntry` table
/// `D_mine_secret_passage_80180E8C`: copies the
/// requested `RoomEventMsg` to `dst` and forwards both to `func_map_shelter_80179A04`. A
/// area-9 request latches the outgoing location's three bytes into the room's
/// staging save location and starts the cutscene task; `queryOnly` set only
/// suppresses that side effect. Returns 2 for a area-9 request and 1 for
/// every other one.
s32 func_mine_secret_passage_8017D7CC(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    func_map_shelter_80179A04(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_B1_ELEVATOR_HALL) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_mine_secret_passage_80183448.warp              = (u8)dst->areaId;
            D_mine_secret_passage_80183448.field_4           = dst->warp;
            ((u8*)&D_mine_secret_passage_80183448.areaId)[1] = dst->room;
            Gp_MsgPlayerWeapon(0);
            taskSpawnFromTable(&D_mine_secret_passage_80180EBC, 0, 0, 0);
        }
        return 2;
    }
    return 1;
}

s32 func_mine_secret_passage_8017D888(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_mine_secret_passage_8017D890(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler id 0x13F2 of the room's `TaskMessageEntry` table
/// `D_mine_secret_passage_80180E8C`: cues sound event 0x16 when the message's
/// `arg2` is 3. No `Task` is spawned, so the room owns this cue rather than a
/// child task.
s32 func_mine_secret_passage_8017D898(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
    }
    return 0;
}

/// Set-up state of the room task: points the task at the passage's message
/// table, publishes it in pointer slot 7, selects scene music entry 1 and
/// advances to the next state.
static void func_mine_secret_passage_8017D8C8(Task* arg0)
{
    arg0->msgTable = D_mine_secret_passage_80180E8C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    arg0->state           = (s32)(arg0->state + 1);
    gStageSceneMusicEntry = 1;
}

/// One-shot state of the room task: the first time through (game flag nibble
/// 0x172 still clear) it sets the flag and calls `Gp_SpawnIfCapIdle(3, 1)`;
/// either way it advances to the idle state.
static void func_mine_secret_passage_8017D914(Task* arg0)
{
    if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_INTRO_SEEN) == 0) {
        gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_INTRO_SEEN, 1);
        Gp_SpawnIfCapIdle(3, 1);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Idle state of the room task.
static void func_mine_secret_passage_8017D968(Task* task)
{
}

/// Per-frame entry point of the room task: runs the handler of
/// `D_mine_secret_passage_8017D5C4` its state selects. The table is a local
/// copy, so it is copied from `.rodata` onto the stack every frame.
void func_mine_secret_passage_8017D970(Task* task)
{
    TaskFuncTable4 states;

    states = D_mine_secret_passage_8017D5C4;
    states.funcs[task->state](task);
}
