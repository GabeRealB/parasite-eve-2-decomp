#include "rooms/dryfield_night_motel_balcony.h"

#include "types.h"

#include "dryfield_night_motel_balcony_private.h"

#include "gameplay/area_flags.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
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
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"

extern RoomEventActiveBytes gRoomEventActive;

/// A gameplay state byte; the one-shot balcony event waits while it is 1.

/// Gameplay-resident script data the room task starts: the pair handed to
/// `evsStartScriptWithSkip` on the first visit, and the one handed to `evsStartScript`
/// by the one-shot event.
extern EvsCommand D_actor_335800_80165060[];
extern EvsCommand D_actor_335800_80165798[];
extern EvsCommand D_actor_335800_80165720[];

/// The message and request the event gate latched for the event task.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.

AreaApplyRec D_dryfield_night_motel_balcony_8018F2CC[2] = {
    { 3, 29, 4, 0 },
    { 255, 0, 0, 0 },
};

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 115, 55, 136 } };

RoomEventReq gRoomEventReq;

static void func_dryfield_night_motel_balcony_8017DC30(Task* task);
static void func_dryfield_night_motel_balcony_8017DD0C(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#define ROOM_VARIANT_MOTEL_BALCONY_DOORS_MSG roomVariantMotelBalconyDoorsMsg
#include "../../shared/room_variants_motel_balcony_doors.inc.c"

#define MOTEL_BALCONY_CUE_SOUND_MSG motelBalconyCueSoundMsg
#include "../../shared/room_variants_motel_balcony_sound.inc.c"

s32 func_dryfield_night_motel_balcony_8017DC18(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC20(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC28(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Room task state 0: installs the room's message table, registers the task
/// in pointer slot 7 and reapplies the nine saved sprite-command states. On
/// place 2, room 2 with flag nibble 0x61 still clear, it also starts the
/// script pair, sets nibbles 0x61, 0x10E (arming state 1) and 0x155, clears
/// nibble 3 and sets `flowFlags` to 0x85. Then advances to the next state.
static void func_dryfield_night_motel_balcony_8017DC30(Task* task)
{
    u8 field9;

    task->msgTable = D_dryfield_night_motel_balcony_80182804;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    func_dryfield_night_motel_balcony_8017E3C8();
    field9 = gGameSession->location.loc.variant;
    if (field9 == 2 && gGameSession->location.loc.room == field9 && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) == 0) {
        evsStartScriptWithSkip(D_actor_335800_80165060, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_335800_80165798);
        gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN, 1);
        gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCRIPT_STATE, 1);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 1);
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_LOAD_ENDING_MUSIC_ONLY | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    }
    task->state = task->state + 1;
}

/// Room task state 1: once the event state is idle, `Gp_StateC08.mode` is not 1 and
/// flag nibble 0x10E is 1, runs the one-shot script and moves the nibble to 2.
static void func_dryfield_night_motel_balcony_8017DD0C(Task* task)
{
    if (gGameSession->eventState == 0 && Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCRIPT_STATE) == 1) {
        evsStartScript(D_actor_335800_80165720, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCRIPT_STATE, 2);
    }
}

/// The room task's three states: setup, the per-tick balcony event check,
/// and exit.
static const TaskFuncTable3 D_dryfield_night_motel_balcony_8017D5DC = {
    func_dryfield_night_motel_balcony_8017DC30,
    func_dryfield_night_motel_balcony_8017DD0C,
    taskKill,
};

/// Runs the room task's current state from its state table, dispatching
/// through a copy of the table taken onto the stack.
void func_dryfield_night_motel_balcony_8017DD78(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_balcony_8017D5DC;
    sp.funcs[task->state](task);
}
