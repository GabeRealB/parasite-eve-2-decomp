#include "rooms/mine_gorge.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

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

void func_mine_gorge_8017D828(Task* arg0);

/// The room's message table, installed by the room task's first state.
extern TaskMessageEntry D_mine_gorge_8017E280[];

/// The cutscene task `func_mine_gorge_8017D5F8` spawns: one descriptor and a
/// terminator.
extern TaskDesc D_mine_gorge_8017E2B0[];

extern EvsCommand D_mine_gorge_8017E2F0[];
extern EvsCommand D_mine_gorge_8017E500[];
extern EvsCommand D_mine_gorge_8017E610[];

static void func_mine_gorge_8017D8D4(Task* arg0);
static void func_mine_gorge_8017D998(Task* task);

s32 func_mine_gorge_8017D5F8(Task*, s32, s32, s32);
s32 func_mine_gorge_8017D6E8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_mine_gorge_8017D77C(Task*, s32, s32, s32);
s32 func_mine_gorge_8017D784(Task* task, s32 msgId, const void* firstArg, s32 arg3);
s32 func_mine_gorge_8017D7F4(Task*, s32, s32, s32);

void func_mine_gorge_8017D8BC(u8);

static AnimationSet _gMineGorgeAnimation00C98;

extern AnimationPlayRequest     D_mine_gorge_8017E2DC;
extern AnimationPlayRequest     D_mine_gorge_8017E5E8;
extern AnimationBankCopyRequest D_mine_gorge_8017E5E0;
void                            func_mine_gorge_8017D8C8(s32);

static AnimationPackedPose _gMineGorgeAnimation00C98Bank1[10] = {
#include "assets/mine_gorge_animation_00C98_bank1.inc"
};

static AnimationPackedRotation _gMineGorgeAnimation00C98Bank4[95] = {
#include "assets/mine_gorge_animation_00C98_bank4.inc"
};

static AnimationRecord _gMineGorgeAnimation00C98Records[139] = {
#include "assets/mine_gorge_animation_00C98_records.inc"
};

static u16 _gMineGorgeAnimation00C98Indices[20] = {
#include "assets/mine_gorge_animation_00C98_indices.inc"

};

static AnimationSet _gMineGorgeAnimation00C98 = {
    _gMineGorgeAnimation00C98Records,
    _gMineGorgeAnimation00C98Indices,
    { NULL, _gMineGorgeAnimation00C98Bank1, NULL, NULL, _gMineGorgeAnimation00C98Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_mine_gorge_8017E280[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_mine_gorge_8017D6E8 },
    { 5105, func_mine_gorge_8017D5F8 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_mine_gorge_8017D784 },
    { ROOM_MESSAGE_COMMAND, func_mine_gorge_8017D77C },
    { ROOM_MESSAGE_SOUND, func_mine_gorge_8017D7F4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_mine_gorge_8017E2B0[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_mine_gorge_8017D828, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationPlayRequest D_mine_gorge_8017E2C8 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mine_gorge_8017E2DC = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_mine_gorge_8017E2F0[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_mine_gorge_8017D8BC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E2DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54050009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E2C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_gorge_8017E500[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationSet* D_mine_gorge_8017E5D8[2] = {
    &_gMineGorgeAnimation00C98,
    NULL,
};

AnimationBankCopyRequest D_mine_gorge_8017E5E0 = { { .sets = D_mine_gorge_8017E5D8 }, ARRAY_SIZE(D_mine_gorge_8017E5D8) };

AnimationPlayRequest D_mine_gorge_8017E5E8 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 7, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_mine_gorge_8017E5FC = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 7, ANIMATION_WORLD_COLLISION_ENABLE };

EvsCommand D_mine_gorge_8017E610[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_gorge_8017E5E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E2DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54050007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mine_gorge_8017D8C8 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E5E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Answers message `0x13F1` with argument `0x11F`: while flag nibble `0xA4` is
/// clear and a room-action `WorldCollisionTrigger` with `parameter0 == 0xFF` and a
/// non-zero `hit` is queued, raises the nibble, spawns the cutscene task
/// `D_mine_gorge_8017E2B0`, moves the session to room 2 with the HUD hidden and
/// the room objects dirty, and starts the session event. Returns 1 when it
/// did so, 0 otherwise.
s32 func_mine_gorge_8017D5F8(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    WorldCollisionTrigger* node;
    s32                    found;

    if (arg2 == 0x11F) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_GORGE_TRIGGER_EVENT_DONE) == 0) {
            node = Gp_PendingObj4C;
            while (node != NULL) {
                if (node->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && node->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && node->hit != 0) {
                    found = 1;
                    goto check;
                }
                node = node->next;
            }
            found = 0;
        check:
            if (found != 0) {
                gameFlagSetNibble(GAME_FLAG_MINE_GORGE_TRIGGER_EVENT_DONE, 1);
                Task_SpawnOnDefaultList(D_mine_gorge_8017E2B0, 0, 0, 0);
                gGameSession->location.loc.room = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2);
                gGameSession->hideHud           = (gGameSession->roomObjsDirty = 1);
                gGameSession->eventState        = 1;
                return 1;
            }
        }
    }
    return 0;
}

/// Answers message `0x13EE`: copies the event message to `out` and passes both
/// to `func_map_shelter_80179A04`. A message of id 2 arriving while flag nibble `0xB5` is
/// clear and `queryOnly` is zero sets nibble `flagId` to 2, runs cap command 3
/// and returns 0; every other case returns 1, except that a set `queryOnly`
/// returns 0 without acting.
s32 func_mine_gorge_8017D6E8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId != GAME_AREA_MINE_CAVERN) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    Gp_SetNibbleIf(in->flagId, 2);
    Gp_RunCapCmd1(3);
    return 0;
}

/// Answers message `0x13F0` by doing nothing.
s32 func_mine_gorge_8017D77C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Cutscene gate on the `0x13EF` direction message: when the payload's
/// action ID is 1, flag nibble `0xC5` is still clear and the session is in
/// place 1, raises the nibble and starts the script blob at
/// `D_mine_gorge_8017E610`.
s32 func_mine_gorge_8017D784(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    u8 actionId = request->actionId;

    if (actionId == 1 && gameFlagGetNibble(GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN) == 0 && gGameSession->location.loc.variant == actionId) {
        gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN, 1);
        func_800E8614(D_mine_gorge_8017E610, 0);
    }
    return 0;
}

/// Answers message `0x13F2`: argument `0xA` queues event sound `0x5405000A`.
s32 func_mine_gorge_8017D7F4(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0xA) {
        SndEvt_EnqueueType6(0x54050000 | arg2, 0, 0);
    }
    return 0;
}

/// The cutscene task: the first pass raises `D_80115768` and the session's
/// `hideHud`, hides the display and starts the script blob pair
/// `D_mine_gorge_8017E2F0` / `D_mine_gorge_8017E500`; the next pass kills the
/// task and clears collection bit `0x11F`.
void func_mine_gorge_8017D828(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115768 = 1;
        SetDispMask(0);
        gGameSession->hideHud = 1;
        func_800E8634(D_mine_gorge_8017E2F0, 0, D_mine_gorge_8017E500);
    } else {
        taskKill(arg0);
        Gp_ClearCollectedBit(0x11F);
    }
    arg0->state = arg0->state + 1;
}

/// Script callback: stores its argument in `D_80115768`.
void func_mine_gorge_8017D8BC(u8 arg0)
{
    D_80115768 = arg0;
}

/// Script callback: stores its argument in `gSceneCombatState.actor03700Wave`.
void func_mine_gorge_8017D8C8(s32 arg0)
{
    gSceneCombatState.actor03700Wave = arg0;
}

/// Room task setup state: installs the message table and pointer slot 7, sets
/// `gSceneCombatState.actor03700Wave` to `0x15` in place 1 once flag nibble `0xC5` is set, and on the
/// first pass with flag nibble `0xBE == 2` arms nibble `0x166`, clears nibble
/// `0xB5` and calls `Gp_SpawnIfCapIdle(8, 0)`. Then selects scene music entry 1 and
/// advances state.
static void func_mine_gorge_8017D8D4(Task* arg0)
{
    arg0->msgTable = D_mine_gorge_8017E280;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == 1) && (gameFlagGetNibble(GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN) != 0)) {
        gSceneCombatState.actor03700Wave = 0x15;
    }
    if ((gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 2) && (gameFlagGetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE) == 0)) {
        gameFlagSetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE, 1);
        gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 0);
        Gp_SpawnIfCapIdle(8, 0);
    }
    arg0->state           = arg0->state + 1;
    gStageSceneMusicEntry = 1;
}

/// The room task's idle state.
static void func_mine_gorge_8017D998(Task* task)
{
}

/// State handlers of the room task `func_mine_gorge_8017D9A0` runs: the room's
/// setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_mine_gorge_8017D5C4 = {
    { func_mine_gorge_8017D8D4, func_mine_gorge_8017D998, taskKill }
};

/// Runs one tick of the room task through the three-state table
/// `D_mine_gorge_8017D5C4`, copying the table onto the stack and calling the
/// entry for the task's current state.
void func_mine_gorge_8017D9A0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_gorge_8017D5C4;
    sp.funcs[task->state](task);
}
