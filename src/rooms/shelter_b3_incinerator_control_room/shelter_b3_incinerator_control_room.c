#include "rooms/shelter_b3_incinerator_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "shelter_b3_incinerator_control_room_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
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

s32 func_shelter_b3_incinerator_control_room_8017FA84(Task*, s32, s32, s32);
s32 func_shelter_b3_incinerator_control_room_8017FA8C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b3_incinerator_control_room_8017FB20(Task*, s32, s32, s32);
s32 func_shelter_b3_incinerator_control_room_8017FBE0(Task*, s32, s32, s32);
s32 func_shelter_b3_incinerator_control_room_8017FBE8(Task*, s32, s32, s32);

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_shelter_b3_incinerator_control_room_80181838[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b3_incinerator_control_room_8017FA8C },
    { 5105, func_shelter_b3_incinerator_control_room_8017FA84 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b3_incinerator_control_room_8017FBE0 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b3_incinerator_control_room_8017FB20 },
    { ROOM_MESSAGE_SOUND, func_shelter_b3_incinerator_control_room_8017FBE8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_shelter_b3_incinerator_control_room_8017FC1C(Task* task);
static void func_shelter_b3_incinerator_control_room_8017FCA8(Task* task);

#include "../../shared/telephone.inc.c"

void func_shelter_b3_incinerator_control_room_8017EA64(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

#include "../../shared/room_cutscene_sound_task.inc.c"

s32 func_shelter_b3_incinerator_control_room_8017FA84(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b3_incinerator_control_room_8017FA8C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId != GAME_AREA_SHELTER_B3_ELEVATOR_HALL) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_B3_INCINERATOR_CONTROL_DOOR_UNLOCKED) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    Gp_SetNibbleIf(in->flagId, 2);
    Gp_RunCapCmd1(3);
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
            Gp_SpawnIfCapIdle(6, 1);
        }
    }
    return 0;
}

s32 func_shelter_b3_incinerator_control_room_8017FBE0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b3_incinerator_control_room_8017FBE8(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0x63) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 9), 0, 0);
    }
    return 0;
}

static void func_shelter_b3_incinerator_control_room_8017FC1C(Task* task)
{
    task->msgTable = D_shelter_b3_incinerator_control_room_80181838;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
    if (gGameSession->location.loc.warp == 4) {
        func_800E3FAC(0xA2, 0x23);
        Gp_ApplyAreaRecs(D_shelter_b3_incinerator_control_room_80182A40);
        Gp_FillAllyHp();
        func_800E8634(D_actor_142600_801360E4, 0, D_actor_142600_80136804);
    }
}

static void func_shelter_b3_incinerator_control_room_8017FCA8(Task* task)
{
    char pad[0x10];
}

/// States of the room's message task, run by
/// `func_shelter_b3_incinerator_control_room_8017FCB8`: install the message
/// table and apply the warp-4 entry setup, idle, die.
static const TaskFuncTable3 D_shelter_b3_incinerator_control_room_8017D6A4 = {
    {
        func_shelter_b3_incinerator_control_room_8017FC1C,
        func_shelter_b3_incinerator_control_room_8017FCA8,
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
