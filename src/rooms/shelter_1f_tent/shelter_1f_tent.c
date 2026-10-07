#include "rooms/shelter_1f_tent.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "shelter_1f_tent_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
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

#include "mapui/map_neo_ark.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_cutscene.h"

extern void func_actor_460200_80132210(void);
extern void func_actor_460200_801322B8(void);
extern void func_actor_460200_80132390(void);

extern UiObjectDesc D_800611E4;

extern EvsCommand D_actor_460200_801362B8[];
extern EvsCommand D_actor_460200_80137890[];

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
extern TaskMessageEntry D_shelter_1f_tent_80181CDC[];

#define TELEPHONE_TITLE_BYTES "Telephone\0\x0C-"
#include "../../shared/telephone.h"

s32 func_shelter_1f_tent_8017FC54(Task*, s32, s32, s32);
s32 func_shelter_1f_tent_8017FC5C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_1f_tent_8017FCA0(Task*, s32, s32, s32);
s32 func_shelter_1f_tent_8017FD54(Task*, s32, RoomEventMsg*, s32);

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_shelter_1f_tent_80181CDC[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_tent_8017FC5C },
    { 5105, func_shelter_1f_tent_8017FC54 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_1f_tent_8017FD54 },
    { ROOM_MESSAGE_COMMAND, func_shelter_1f_tent_8017FCA0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_shelter_1f_tent_8017F9F0(Task* task);
static void func_shelter_1f_tent_8017FDA8(Task* task);

#include "../../shared/telephone.inc.c"

void func_shelter_1f_tent_8017EA60(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

static void func_shelter_1f_tent_8017F9F0(Task* task)
{
    s32 idx;
    s32 val;

    task->msgTable = D_shelter_1f_tent_80181CDC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 1) {
        func_actor_460200_80132210();
    }
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_1F_TENT_ARRIVED) == 0) {
        gameFlagSetNibble(GAME_FLAG_SHELTER_1F_TENT_ARRIVED, 1);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1B3, 2);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1B0, 2);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1AE, 2);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1CD, 2);
        gameFlagSetNibble(GAME_FLAG_0FC, 0);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, 0);
        gameFlagSetNibble(GAME_FLAG_SHELTER_ELEVATOR_ENABLED, 0);
        gameFlagSetNibble(GAME_FLAG_SHELTER_1F_TENT_1BA, 2);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1BB, 2);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x36);
        Gp_FillPlayerHpMp();
        Gp_ApplyAreaRecs(D_shelter_1f_tent_801842D4);
        evsStartScriptWithSkip(D_actor_460200_801362B8, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_460200_80137890);
        if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) != 0) {
            Gp_ApplyAreaRecs(D_shelter_1f_tent_801843B0);
            Gp_ApplyAreaRecs(D_shelter_1f_tent_801843B8);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            idx = 0x155;
            val = 0xB;
        } else {
            Gp_ApplyAreaRecs(D_shelter_1f_tent_801843A8);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            idx = 0x155;
            val = 0xA;
        }
        gameFlagSetNibble(idx, val);
        if (gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) != 0) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 8);
        }
    } else {
        sndEvtRequestScriptStart(SOUND_SHELTER_1F_TENT_AMBIENCE_1, 0, 0);
        sndEvtRequestScriptStart(SOUND_SHELTER_1F_TENT_AMBIENCE_2, 0, 0);
    }
    task->state = task->state + 1;
}

#include "../../shared/room_cutscene_sound_task.inc.c"

s32 func_shelter_1f_tent_8017FC54(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Copies the incoming `RoomEventMsg` onto the outgoing one, hands both to
/// `mapNeoArkResolveRoomVariant` and returns 1.
s32 func_shelter_1f_tent_8017FC5C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapNeoArkResolveRoomVariant(in, out);
    return 1;
}

s32 func_shelter_1f_tent_8017FCA0(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_TENT_FIRST_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_TENT_FIRST_SCENE, 1);
            capRunCommandWithTransition(0x18);
            return 0;
        }
        D_shelter_1f_tent_801843C4.view            = 5;
        D_shelter_1f_tent_801843C4.capSlot         = arg2;
        D_shelter_1f_tent_801843C4.capFile         = arg2;
        D_shelter_1f_tent_801843C4.skipScene       = 0;
        D_shelter_1f_tent_801843C4.startSound      = 0x551C0003;
        D_shelter_1f_tent_801843C4.endSound        = 0x551C0006;
        D_shelter_1f_tent_801843C4.sceneSound      = 0x551C0004;
        D_shelter_1f_tent_801843C4.afterSceneSound = 0x551C0005;
        taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 0xC, &D_shelter_1f_tent_801843C4);
    }
    return 0;
}

s32 func_shelter_1f_tent_8017FD54(Task* arg0, s32 arg1, RoomEventMsg* arg2, s32 arg3)
{
    if (arg2->warp == 1) {
        func_actor_460200_801322B8();
    }
    if (arg2->warp == 2) {
        func_actor_460200_80132390();
    }
    return 0;
}

static void func_shelter_1f_tent_8017FDA8(Task* task)
{
    char pad[0x10];
}

/// States of the room's message task, run by
/// `func_shelter_1f_tent_8017FDB8`: install the message table and apply the
/// room's first-visit setup, idle, die.
static const TaskFuncTable3 D_shelter_1f_tent_8017D6A4 = {
    {
        func_shelter_1f_tent_8017F9F0,
        func_shelter_1f_tent_8017FDA8,
        taskKill,
    },
};

/// Runs the handler for the task's current state, from a local copy of
/// `D_shelter_1f_tent_8017D6A4`.
void func_shelter_1f_tent_8017FDB8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_tent_8017D6A4;
    sp.funcs[task->state](task);
}
