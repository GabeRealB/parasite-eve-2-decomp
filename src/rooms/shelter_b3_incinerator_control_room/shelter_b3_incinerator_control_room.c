#include "rooms/shelter_b3_incinerator_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "shelter_b3_incinerator_control_room_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/player_state.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_shelter.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_cutscene.h"

static void _roomCutsceneSoundTask(Task* task);

extern UiObjectDesc D_800611E4;

extern EvsCommand D_actor_142600_801360E4[];
extern EvsCommand D_actor_142600_80136804[];

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `gMcSaveData`.

/// View saved when the cutscene starts and restored when it ends.

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row captions of the play-data panel, one per row.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// Suffix appended after a plain count on rows 1, 2, 3 and 6 of the play-data
/// panel.
static u8 Telephone_Data_80181A70[];

/// Suffix appended after a percentage.
static u8 Telephone_Data_80181A78[];

/// Help strings handed to the UI holder while the cursor rests on a row of
/// the play-data panel, one per row.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// Lists of the play-data panel, the usage panel and the telephone menu, and
/// the descriptors of the panels they spawn.
static UiList       Telephone_Data_80181C44;
static UiList       Telephone_Data_80181C6C;
static UiObjectDesc Telephone_Data_80181C90;
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;
static UiList       Telephone_Data_80181CF4;

/// Task table the cutscene and its sound task are spawned from.
extern TaskDesc gRoomCutsceneTaskDescs[];

/// Message table of the room's message task.
extern TaskMessageEntry D_shelter_b3_incinerator_control_room_80181838[];

#define TELEPHONE_TITLE_BYTES "Telephone\0\x14\xCF"
#include "../../shared/telephone.h"

static s32 _shelterB3IncineratorControlRoomRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
s32        func_shelter_b3_incinerator_control_room_8017FA8C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_shelter_b3_incinerator_control_room_8017FB20(Task*, s32, s32, s32);
static s32 _shelterB3IncineratorControlRoomIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);
static s32 _shelterB3IncineratorControlRoomHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unused);

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_shelter_b3_incinerator_control_room_80181838[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b3_incinerator_control_room_8017FA8C },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB3IncineratorControlRoomRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB3IncineratorControlRoomIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, func_shelter_b3_incinerator_control_room_8017FB20 },
    { ROOM_MESSAGE_SOUND, _shelterB3IncineratorControlRoomHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_shelter_b3_incinerator_control_room_8017FC1C(Task* task);
static void _shelterB3IncineratorControlRoomIdleRoom(Task* task);

#include "../../shared/telephone.inc.c"

void func_shelter_b3_incinerator_control_room_8017EA64(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Refuses key-item use with the inventory menu's refused reply.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored and the
/// room state remains unchanged.
static s32 _shelterB3IncineratorControlRoomRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 func_shelter_b3_incinerator_control_room_8017FA8C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId != GAME_AREA_SHELTER_B3_ELEVATOR_HALL) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_B3_INCINERATOR_CONTROL_DOOR_UNLOCKED) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    gameFlagSetNibbleIfPresent(in->flagId, 2);
    capRunCommandWithTransition(3);
    return 0;
}

s32 func_shelter_b3_incinerator_control_room_8017FB20(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_INCINERATOR_CONTROL_FIRST_USE) != 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp     = arg2;
            D_shelter_b3_incinerator_control_room_80182A58.view            = 8;
            D_shelter_b3_incinerator_control_room_80182A58.capSlot         = arg2;
            D_shelter_b3_incinerator_control_room_80182A58.capFile         = arg2;
            D_shelter_b3_incinerator_control_room_80182A58.skipScene       = 0;
            D_shelter_b3_incinerator_control_room_80182A58.startSound      = 0x54290001;
            D_shelter_b3_incinerator_control_room_80182A58.endSound        = 0x54290004;
            D_shelter_b3_incinerator_control_room_80182A58.sceneSound      = 0x54290002;
            D_shelter_b3_incinerator_control_room_80182A58.afterSceneSound = 0x54290003;
            taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 7, &D_shelter_b3_incinerator_control_room_80182A58);
        } else {
            gameFlagSetNibble(GAME_FLAG_INCINERATOR_CONTROL_FIRST_USE, 1);
            capSpawnEventIfIdle(6, CAP_EVENT_PAUSE_ACTORS);
        }
    }
    return 0;
}

/// Ignores direction-triggered room actions and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`; the borrowed four-byte request
/// and unused second word are neither read nor retained.
static s32 _shelterB3IncineratorControlRoomIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    return 0;
}

/// Maps room sound cue 99 to control-room sound-bank entry 9.
///
/// Handles `ROOM_MESSAGE_SOUND` with an integer cue and unused second word.
/// Requires the room sound bank loaded. Other cues do nothing; returns zero.
static s32 _shelterB3IncineratorControlRoomHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unused)
{
    enum {
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_SOUND_CUE_99  = 99,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_SOUND_ENTRY_9 = 9
    };
    if (cueId == SHELTER_B3_INCINERATOR_CONTROL_ROOM_SOUND_CUE_99) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SOUND_ENTRY_9), 0, 0);
    }
    return 0;
}

static void func_shelter_b3_incinerator_control_room_8017FC1C(Task* task)
{
    task->msgTable = D_shelter_b3_incinerator_control_room_80181838;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
    if (gGameSession->location.loc.warp == 4) {
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x23);
        areaApplySavedUpdates(D_shelter_b3_incinerator_control_room_80182A40);
        companionRestoreFullHp();
        evsStartScriptWithSkip(D_actor_142600_801360E4, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_142600_80136804);
    }
}

/// Leaves the initialized room task idle while its message table remains active.
///
/// The room controller's state 1 performs no per-frame work or state change.
static void _shelterB3IncineratorControlRoomIdleRoom(Task* task)
{
    // Retained unused storage preserves the original 16-byte stack frame.
    char unusedStackBytes[0x10];
}

/// States of the room's message task, run by
/// `func_shelter_b3_incinerator_control_room_8017FCB8`: install the message
/// table and apply the warp-4 entry setup, idle, die.
static const TaskFuncTable3 D_shelter_b3_incinerator_control_room_8017D6A4 = {
    {
        func_shelter_b3_incinerator_control_room_8017FC1C,
        _shelterB3IncineratorControlRoomIdleRoom,
        taskKill,
    },
};

/// Runs the handler for the task's current state, from a local copy of
/// `D_shelter_b3_incinerator_control_room_8017D6A4`.
void func_shelter_b3_incinerator_control_room_8017FCB8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_incinerator_control_room_8017D6A4;
    sp.funcs[task->state](task);
}
