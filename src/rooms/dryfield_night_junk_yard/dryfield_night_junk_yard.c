#include "rooms/dryfield_night_junk_yard.h"

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/sprites.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "../../shared/junk_yard.h"

/// The room's message table, published in `Task::msgTable` for
/// `taskMessageDispatch` to walk.
extern TaskMessageEntry D_dryfield_night_junk_yard_8018055C[];
/// Payload of the 0x7DA message the entry task sends to the slot-4 task.
extern s32        D_dryfield_night_junk_yard_801805A0;
extern EvsCommand D_dryfield_night_junk_yard_801805A4[];

static s32 _dryfieldNightJunkYardRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
s32        func_dryfield_night_junk_yard_8017D6AC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_dryfield_night_junk_yard_8017D82C(Task*, s32, RoomEventMsg*, s32);

extern AnimationPlayRequest D_dryfield_night_junk_yard_80180584;
extern ActorCommand         D_dryfield_night_junk_yard_80180598;
extern ActorCommand         D_dryfield_night_junk_yard_8018059C;
static void                 _dryfieldNightJunkYardSetRoomNumber(u8 roomNumber);

TaskMessageEntry D_dryfield_night_junk_yard_8018055C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_junk_yard_8017D6AC },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightJunkYardRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_junk_yard_8017D82C },
    { ROOM_MESSAGE_COMMAND, junkYardCapMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationPlayRequest D_dryfield_night_junk_yard_80180584 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_dryfield_night_junk_yard_80180598 = { { .loc = { 3, 26 } }, 0 };

ActorCommand D_dryfield_night_junk_yard_8018059C = { { .loc = { 3, 26 } }, 1 };

s32 D_dryfield_night_junk_yard_801805A0 = 0x21A03;

EvsCommand D_dryfield_night_junk_yard_801805A4[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_junk_yard_80180584 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_dryfield_night_junk_yard_80180598 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _dryfieldNightJunkYardSetRoomNumber }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_dryfield_night_junk_yard_8018059C } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static void func_dryfield_night_junk_yard_8017D8B0(Task* task);
static void _dryfieldNightJunkYardRoomIdle(Task* unusedTask);

#include "../../shared/junk_yard_cap_msg.inc.c"

/// Refuses key-item use in the night Junk Yard without changing inventory.
///
/// The item menu supplies the collected-item ID and a zero second payload.
/// All arguments are ignored; the refused reply requests the cannot-use notice.
static s32 _dryfieldNightJunkYardRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Handler for message 0x13EE in the room's message table. Copies the incoming
/// record to the outgoing one, then edits the copy according to the message id
/// and the game's progress nibbles.
///
/// Message 0x18 first picks the copy's `room` answer from nibble 0x7A (2
/// once it has reached 4, else 1). Message 0x1B, while nibble 0x46 is 1, runs
/// the CAP command 4 and arms the nibble in `flagId`, consuming the message
/// (returns 0); otherwise, with nibble 0x64 still clear, it answers 2 and
/// latches that nibble. It then walks the 0x73 / 0x61 / 0x8F chain - only while
/// 0x73 is 1, 0x61 agrees with it and 0x8F is clear - and answers 3 when nibble
/// 0x7A has reached it, latching 0x8F.
///
/// `queryOnly` non-zero means "report only", which suppresses every side effect.
s32 func_dryfield_night_junk_yard_8017D6AC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventMsg unused;
    s32          state;
    s32          value;

    if (in->areaId == GAME_AREA_DRYFIELD_NIGHT_GARAGE && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 4) {
            out->room = 2;
        } else {
            out->room = 1;
        }
    }
    *out = *in;
    if (in->areaId != GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_046) == 1) {
        if (in->queryOnly != ROOM_EVENT_EXECUTE) {
            return 0;
        }
        capRunCommandWithTransition(4);
        gameFlagSetNibbleIfPresent(in->flagId, 2);
        return 0;
    }
    if (gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_FIRST_ENTRY) == 0 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        out->warp = 2;
        gameFlagSetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_FIRST_ENTRY, 1);
    }
    state = gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED);
    if (state != 1) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) != state) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_NIGHT_EVENT_ENTRY) != 0) {
        return 1;
    }
    value = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
    if (value != 3) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 1;
    }
    out->warp = value;
    gameFlagSetNibble(GAME_FLAG_TRAILER_COACH_NIGHT_EVENT_ENTRY, 1);
    return 1;
}

/// Handler for message 0x13EF in the room's message table. When the record's
/// `field_2` is 3 on the visit whose `place` is 1, it latches nibble 0x9F once
/// and passes `D_dryfield_night_junk_yard_801805A4` to `evsStartScript`. Always
/// returns 0.
s32 func_dryfield_night_junk_yard_8017D82C(Task* arg0, s32 arg1, RoomEventMsg* in, s32 arg3)
{
    if ((in->warp == 3) && (gGameSession->location.loc.variant == 1) && (gameFlagGetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN, 1);
        evsStartScript(D_dryfield_night_junk_yard_801805A4, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Sets the room number in both the active session and the live save.
///
/// The event script passes room number 2 after engaging the battle. The byte
/// is stored unchanged, without loading resources or validating the room number.
static void _dryfieldNightJunkYardSetRoomNumber(u8 roomNumber)
{
    gGameSession->location.loc.room                            = roomNumber;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = roomNumber;
}

/// Room entry task tick: publish the message table, claim game pointer slot 7,
/// and, on the visit whose sub-id (`gGameSession::location.loc.variant`) is 1 and that has
/// already latched nibble 0x9F, announce the room to the slot-4 task with
/// message 0x7DA. The nibble is then applied to the current sprite-table entry
/// either way, and the state advances.
static void func_dryfield_night_junk_yard_8017D8B0(Task* task)
{
    u8 subId;

    task->msgTable = D_dryfield_night_junk_yard_8018055C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    subId = gGameSession->location.loc.variant;
    if (subId == 1 && gameFlagGetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN) == subId) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_dryfield_night_junk_yard_801805A0, ACTOR_COMMAND_MESSAGE_APPLY);
    }
    func_dryfield_night_junk_yard_8017D9B8(gameFlagGetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN));
    task->state = task->state + 1;
}

/// Keeps the Junk Yard room task alive to receive messages.
static void _dryfieldNightJunkYardRoomIdle(Task* unusedTask)
{
}

/// The room entry task's states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_junk_yard_8017D5C4 = {
    { func_dryfield_night_junk_yard_8017D8B0, _dryfieldNightJunkYardRoomIdle, taskKill },
};

/// The room entry task: copies the three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_night_junk_yard_8017D960(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_junk_yard_8017D5C4;
    sp.funcs[task->state](task);
}

/// Applies game flag nibble 0x9F to the sixth sprite command of view 0 in the
/// current room's sprite record: a zero nibble draws the command, a nonzero one
/// hides it (`spriteLinkViewCachedPackets` skips OT-linking when `field_4` is set).
void func_dryfield_night_junk_yard_8017D9B8(u8 arg0)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    SpriteBatch*     batches;

    batches = gSpriteAreaTables[sess->stage - 1][0].areaViews[sess->area - 1][6].batches;
    if (arg0 == 0) {
        batches[5].hidden = 0;
    } else {
        batches[5].hidden = 1;
    }
}
