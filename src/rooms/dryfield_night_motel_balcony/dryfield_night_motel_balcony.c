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

static void _dryfieldNightMotelBalconyInitRoom(Task* task);
static void _dryfieldNightMotelBalconyCheckFollowUpScene(Task* unusedTask);

enum {
    DRYFIELD_NIGHT_MOTEL_BALCONY_SCENE_VARIANT     = 2,
    DRYFIELD_NIGHT_MOTEL_BALCONY_FOLLOW_UP_PENDING = 1,
    DRYFIELD_NIGHT_MOTEL_BALCONY_FOLLOW_UP_STARTED = 2,
};

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#define ROOM_VARIANT_MOTEL_BALCONY_DOORS_MSG roomVariantMotelBalconyDoorsMsg
#include "../../shared/room_variants_motel_balcony_doors.inc.c"

#define MOTEL_BALCONY_CUE_SOUND_MSG motelBalconyCueSoundMsg
#include "../../shared/room_variants_motel_balcony_sound.inc.c"

s32 dryfieldNightMotelBalconyRefuseKeyItemMsg(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 dryfieldNightMotelBalconyIgnoreCommandMsg(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandKey, s32 unusedSecondArg)
{
    return 0;
}

s32 dryfieldNightMotelBalconyIgnoreRoomActionMsg(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room, restores damaged scenery and starts its unseen entry scene.
///
/// State 0 borrows the live room task/session and loaded sprite tables. Variant
/// 2, room 2 starts the script pair once and arms the follow-up scene. Advances
/// to state 1 after setup; the message table stays live with the room overlay.
static void _dryfieldNightMotelBalconyInitRoom(Task* task)
{
    u8 variant;

    task->msgTable = D_dryfield_night_motel_balcony_80182804;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    dryfieldNightMotelBalconyRestoreSectionStates();
    variant = gGameSession->location.loc.variant;
    if (variant == DRYFIELD_NIGHT_MOTEL_BALCONY_SCENE_VARIANT && gGameSession->location.loc.room == variant && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) == 0) {
        evsStartScriptWithSkip(D_actor_335800_80165060, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_335800_80165798);
        gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN, 1);
        gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCRIPT_STATE, DRYFIELD_NIGHT_MOTEL_BALCONY_FOLLOW_UP_PENDING);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 1);
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_LOAD_ENDING_MUSIC_ONLY | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    }
    task->state = task->state + 1;
}

/// Starts the armed follow-up scene when event and attachment control allow it.
///
/// State 1 requires the live session, attachment state and loaded event script.
/// The task argument is unused. Marks the saved script state as started without
/// advancing the task, so subsequent ticks leave the scene alone.
static void _dryfieldNightMotelBalconyCheckFollowUpScene(Task* unusedTask)
{
    if (gGameSession->eventState == 0 && Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCRIPT_STATE) == DRYFIELD_NIGHT_MOTEL_BALCONY_FOLLOW_UP_PENDING) {
        evsStartScript(D_actor_335800_80165720, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCRIPT_STATE, DRYFIELD_NIGHT_MOTEL_BALCONY_FOLLOW_UP_STARTED);
    }
}

/// The room task's three states: setup, the per-tick balcony event check,
/// and exit.
static const TaskFuncTable3 D_dryfield_night_motel_balcony_8017D5DC = {
    _dryfieldNightMotelBalconyInitRoom,
    _dryfieldNightMotelBalconyCheckFollowUpScene,
    taskKill,
};

void dryfieldNightMotelBalconyRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_dryfield_night_motel_balcony_8017D5DC;
    states.funcs[task->state](task);
}
