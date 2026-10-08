#include "rooms/shelter_1f_guardroom.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

/// The room's message table, installed on its event task in state 0.
extern TaskMessageEntry D_shelter_1f_guardroom_8017DA30[];
extern TaskDesc         D_shelter_1f_guardroom_8017DA60;
extern TaskDesc         D_shelter_1f_guardroom_8017DA6C;
extern Task*            D_shelter_1f_guardroom_8017E014;

static void _shelter1fGuardroomInitializeRoom(Task* task);
static void _shelter1fGuardroomRoomIdleState(Task* unusedTask);
static void _shelter1fGuardroomSetUnlockOverlayVisible(u8 visible);
static s32  _shelter1fGuardroomRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32  _shelter1fGuardroomIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Key-item use request dispatched to this room's message table.
enum { SHELTER_1F_GUARDROOM_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// The event task's three states: set-up, idle, and kill.
static const TaskFuncTable3 D_shelter_1f_guardroom_8017D5C4 = {
    {
        _shelter1fGuardroomInitializeRoom,
        _shelter1fGuardroomRoomIdleState,
        taskKill,
    },
};

extern WorldCollisionGrid         D_shelter_1f_guardroom_8017DBF0[1];
extern WorldCollisionTrigger      D_shelter_1f_guardroom_8017DE3C[2];
extern WorldCollisionTrigger      D_shelter_1f_guardroom_8017DED4[3];
extern WorldCoordRoomAmbientEntry D_shelter_1f_guardroom_8017DFB8[4];
extern WorldCoordRoomLights       D_shelter_1f_guardroom_8017DE24[1];
static s32                        _shelter1fGuardroomResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32                        _shelter1fGuardroomCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32                        _shelter1fGuardroomPlaySoundCueMessage(Task* task, s32 messageId, s32 cueId, s32 secondArg);
static void                       _shelter1fGuardroomUnlockBulwarkTask(Task* task);
static void                       _shelter1fGuardroomUnlockMovieTask(Task* task);

TaskMessageEntry D_shelter_1f_guardroom_8017DA30[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fGuardroomResolveRoomEvent },
    { SHELTER_1F_GUARDROOM_MESSAGE_USE_KEY_ITEM, _shelter1fGuardroomRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fGuardroomIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fGuardroomCommandMessage },
    { ROOM_MESSAGE_SOUND, _shelter1fGuardroomPlaySoundCueMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_1f_guardroom_8017DA60 = { { { TASK_BODY_NONE, 32 } }, _shelter1fGuardroomUnlockBulwarkTask, { .value = 0 } };

TaskDesc D_shelter_1f_guardroom_8017DA6C = { { { TASK_BODY_NONE, 192 } }, _shelter1fGuardroomUnlockMovieTask, { .value = 0 } };

WorldCollisionRoomResources D_shelter_1f_guardroom_8017DA78[1] = {
    { D_shelter_1f_guardroom_8017DBF0, D_shelter_1f_guardroom_8017DE3C, D_shelter_1f_guardroom_8017DED4, NULL },
};

WorldCoordRoomLighting D_shelter_1f_guardroom_8017DA88[1] = {
    { D_shelter_1f_guardroom_8017DE24, D_shelter_1f_guardroom_8017DFB8 },
};

u8* D_shelter_1f_guardroom_8017DA90[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_1f_guardroom_8017DA94[1] = { 3 };

DirectionWarpEntry D_shelter_1f_guardroom_8017DA98[1] = {
    { { { .word = 2048 }, -9000, 0, -3620 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -9000, 0, -3620 }, { 0, 0, 0, 0 }, 0x55060002, 0x55060001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelter1fGuardroomCollision00630Normals[7] = {
#include "assets/shelter_1f_guardroom_collision_00630_normals.inc"
};

static SVECTOR _gShelter1fGuardroomCollision00630Verts[14] = {
#include "assets/shelter_1f_guardroom_collision_00630_verts.inc"
};

static WorldCollisionGridFace _gShelter1fGuardroomCollision00630Faces[8] = {
#include "assets/shelter_1f_guardroom_collision_00630_faces.inc"
};

static s16 _gShelter1fGuardroomCollision00630Cells[10] = {
#include "assets/shelter_1f_guardroom_collision_00630_cells.inc"
};

#define GRID_CELL(i) (&_gShelter1fGuardroomCollision00630Cells[i])
static s16* _gShelter1fGuardroomCollision00630Table[1] = {
#include "assets/shelter_1f_guardroom_collision_00630_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_1f_guardroom_8017DBF0[1] = {
    { NULL, _gShelter1fGuardroomCollision00630Normals, _gShelter1fGuardroomCollision00630Verts, _gShelter1fGuardroomCollision00630Faces, _gShelter1fGuardroomCollision00630Table, 0x2904, 4500, 1, 1, 4000, 8 },
};

ViewCamera D_shelter_1f_guardroom_8017DC14[3] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5334, 0 } }, 207 },
    { { { { 597, 0, 4052 }, { 2175, 3455, -320 }, { -3418, 2198, 504 } }, { 6020, 2440, 4040 } }, 207 },
    { { { { 756, 0, -4025 }, { -1977, 3567, -371 }, { 3506, 2012, 658 } }, { 9590, 2480, 4320 } }, 257 },
};

SpriteBatch D_shelter_1f_guardroom_8017DC80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_guardroom_8017DC90[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_1f_guardroom_8017DCA0[2] = {
    { 142, 0x3FC0, { .fields = { 72, 160 } }, -72, -120, 2500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 160 } }, 0, -120, 2500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_1f_guardroom_8017DCC8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_1f_guardroom_8017DCE0[3] = {
    { { .empty = D_shelter_1f_guardroom_8017DC80 }, D_shelter_1f_guardroom_8017DC80, NULL },
    { { .empty = D_shelter_1f_guardroom_8017DC90 }, D_shelter_1f_guardroom_8017DC90, NULL },
    { { .elements = D_shelter_1f_guardroom_8017DCA0 }, D_shelter_1f_guardroom_8017DCC8, NULL },
};

WorldCoordPointLight D_shelter_1f_guardroom_8017DD04[3] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9420, -2000, -3790 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2867, 2457 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7690, -2000, -3790 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2867, 2457 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9000, -2110, -3400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 0, 0 }, { 0, 0 } }, 1000, 1500 },
};

WorldCoordRoomLights D_shelter_1f_guardroom_8017DE24[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_1f_guardroom_8017DD04), D_shelter_1f_guardroom_8017DD04, 0, NULL },
};

WorldCollisionTrigger D_shelter_1f_guardroom_8017DE3C[2] = {
    { NULL, NULL, NULL, { -7645, -1408, -3616, 0 }, { { 75, -1872, -1898, 0 }, { -87, -1872, 1885, 0 }, { 75, 1872, -1898, 0 }, { -87, 1872, 1885, 0 } }, { 4098, 0, 175, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7472, -1440, -3600, 0 }, { { -78, -1840, 1851, 0 }, { 62, -1840, -1863, 0 }, { -78, 1840, 1851, 0 }, { 62, 1840, -1863, 0 } }, { -4095, 0, -155, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_1f_guardroom_8017DED4[3] = {
    { NULL, NULL, NULL, { -9360, -48, -3520, 0 }, { { -656, 0, -224, 0 }, { 656, 0, -224, 0 }, { -656, 0, 224, 0 }, { 656, 0, 224, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 692, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7488, -64, -3136, 0 }, { { -656, 0, -224, 0 }, { 656, 0, -224, 0 }, { -656, 0, 224, 0 }, { 656, 0, 224, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 692, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6960, -64, -3728, 0 }, { { -288, 0, -816, 0 }, { 288, 0, -816, 0 }, { -288, 0, 816, 0 }, { 288, 0, 816, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 863, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_1f_guardroom_8017DFB8[4] = {
    { .viewCount = ARRAY_SIZE(D_shelter_1f_guardroom_8017DFB8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 300, 300, 300, 300 } },
};

WorldCollisionFootstepSounds D_shelter_1f_guardroom_8017DFD8 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_1f_guardroom_8017DFE4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_1f_guardroom_8017DFEC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_1f_guardroom_8017DFD8 },
};

WorldCollisionSurfaceProperties* D_shelter_1f_guardroom_8017DFF4[8] = {
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFEC,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
};

Task* D_shelter_1f_guardroom_8017E014 = NULL;

/// Commits the completed movie's unlock and restores ordinary player presentation.
static inline void _shelter1fGuardroomFinishBulwarkUnlock(Task* task)
{
    gGameSession->hideHud = 0;
    _shelter1fGuardroomSetUnlockOverlayVisible(1);
    gameFlagSetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED, 1);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    taskKill(task);
}

/// Runs the dialogue and confirmed movie sequence that unlocks the bulwark.
///
/// Starts at state 0 with scripted player control already held. CAP command 2
/// must finish before its variant key is tested; key 11 starts the movie while
/// other keys release control and exit. Movie teardown precedes restoring the
/// HUD, showing the unlocked overlay and setting the unlock flag. Requires the
/// guardroom resources, CAP slot 2 and session to remain live through completion.
static void _shelter1fGuardroomUnlockBulwarkTask(Task* task)
{
    enum {
        SHELTER_1F_GUARDROOM_UNLOCK_START_DIALOGUE = 0,
        SHELTER_1F_GUARDROOM_UNLOCK_WAIT_DIALOGUE  = 1,
        SHELTER_1F_GUARDROOM_UNLOCK_CHECK_CHOICE   = 2,
        SHELTER_1F_GUARDROOM_UNLOCK_WAIT_MOVIE     = 3,
        SHELTER_1F_GUARDROOM_UNLOCK_FINISH         = 4,
        SHELTER_1F_GUARDROOM_UNLOCK_CAP_COMMAND    = 2,
        SHELTER_1F_GUARDROOM_UNLOCK_CONFIRMED_KEY  = 0xB
    };
    s32 movieKillStatus;

    switch (task->state) {
        case SHELTER_1F_GUARDROOM_UNLOCK_START_DIALOGUE:
            capRunCommandWithTransition(SHELTER_1F_GUARDROOM_UNLOCK_CAP_COMMAND);
            task->state++;
            break;
        case SHELTER_1F_GUARDROOM_UNLOCK_WAIT_DIALOGUE:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_1F_GUARDROOM_UNLOCK_CHECK_CHOICE:
            Gp_CapCmds[SHELTER_1F_GUARDROOM_UNLOCK_CAP_COMMAND].command->counter = 1;
            if (capGetVariantKey() != SHELTER_1F_GUARDROOM_UNLOCK_CONFIRMED_KEY) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                break;
            }
            // Keep the HUD hidden until the independent movie task has exited.
            gGameSession->hideHud           = 1;
            D_shelter_1f_guardroom_8017E014 = taskSpawnFromTable(&D_shelter_1f_guardroom_8017DA6C, 0, 0, 0);
            task->state++;
            break;
        case SHELTER_1F_GUARDROOM_UNLOCK_WAIT_MOVIE:
            if (taskPollKill(D_shelter_1f_guardroom_8017E014, &movieKillStatus) == 0) {
                break;
            }
            task->state++;
            break;
        case SHELTER_1F_GUARDROOM_UNLOCK_FINISH:
            _shelter1fGuardroomFinishBulwarkUnlock(task);
            break;
    }
}

/// Refuses every key-item use request in the guardroom without consuming the item.
///
/// `itemId` is the inventory item ID; all arguments are ignored. The zero
/// result makes the item menu report that the item cannot be used here.
static s32 _shelter1fGuardroomRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    enum { SHELTER_1F_GUARDROOM_KEY_ITEM_USE_REFUSED = 0 };

    return SHELTER_1F_GUARDROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition after resolving its Neo Ark destination variant.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Borrows a complete eight-byte request
/// and writable reply during synchronous dispatch; they may alias. Copies the
/// request before resolving the reply room. Query mode preserves the selectors.
/// Neither pointer is retained; the map overlay must remain loaded. Always
/// returns 1 (passage allowed); the receiving task and message ID are unused.
static s32 _shelter1fGuardroomResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_1F_GUARDROOM_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return SHELTER_1F_GUARDROOM_TRANSITION_ALLOWED;
}

/// Handles the bulwark unlock command from the guardroom's room-message table.
///
/// `ROOM_MESSAGE_COMMAND` payload 2 holds player control and starts the unlock
/// sequence while the flag is clear, or runs CAP command 3 once unlocked. Other
/// payloads do nothing. Always returns zero; receiver, message ID and second
/// payload are unused. The room and CAP resources must remain loaded.
static s32 _shelter1fGuardroomCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        SHELTER_1F_GUARDROOM_COMMAND_UNLOCK_BULWARK = 2,
        SHELTER_1F_GUARDROOM_CAP_ALREADY_UNLOCKED   = 3,
        SHELTER_1F_GUARDROOM_COMMAND_HANDLED        = 0
    };

    if (commandId == SHELTER_1F_GUARDROOM_COMMAND_UNLOCK_BULWARK) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED) == 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&D_shelter_1f_guardroom_8017DA60, 0, 0, 0);
        } else {
            capRunCommandWithTransition(SHELTER_1F_GUARDROOM_CAP_ALREADY_UNLOCKED);
        }
    }
    return SHELTER_1F_GUARDROOM_COMMAND_HANDLED;
}

/// Ignores trigger action requests in the guardroom and returns zero.
///
/// `request` borrows the direction system's action record during synchronous
/// dispatch. No argument is read, no action starts, and no storage is retained.
static s32 _shelter1fGuardroomIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Queues the guardroom sound script selected by CAP cue 3.
///
/// Handles `ROOM_MESSAGE_SOUND`: cue 3 starts local area-bank script 3; other
/// cues do nothing. Requires the current stage sound bank and sound event queue.
/// Always returns zero; the receiving task, message ID and second payload are unused.
static s32 _shelter1fGuardroomPlaySoundCueMessage(Task* task, s32 messageId, s32 cueId, s32 secondArg)
{
    enum { SHELTER_1F_GUARDROOM_SOUND_CUE_SCRIPT_3 = 3 };

    if (cueId == SHELTER_1F_GUARDROOM_SOUND_CUE_SCRIPT_3) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_GUARDROOM, 3), 0, 0);
    }
    return 0;
}

/// Registers the room receiver and restores the unlocked-bulwark overlay.
///
/// State 0 installs the message table and publishes the borrowed task in the
/// room slot. The unlock flag selects overlay visibility before state 1 begins.
/// Requires the guardroom's loaded sprite tables and current session.
static void _shelter1fGuardroomInitializeRoom(Task* task)
{
    task->msgTable = D_shelter_1f_guardroom_8017DA30;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _shelter1fGuardroomSetUnlockOverlayVisible(gameFlagGetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED));
    task->state++;
}

/// Keeps the initialized room event task available for messages in state 1.
///
/// The frame callback leaves the task, its message table and its state intact;
/// teardown remains the responsibility of the room's task owner.
static void _shelter1fGuardroomRoomIdleState(Task* unusedTask)
{
}

void shelter1fGuardroomRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_1f_guardroom_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

/// Plays the current room's bulwark-unlock movie with its overlay hidden.
///
/// State 0 selects movie sub-ID 0 and one-based frame 1. State 1 waits for
/// `movieReady` (started or skipped); state 2 waits for the CD queue to become
/// idle before requesting teardown. The surrounding unlock task restores the
/// overlay and HUD after this task exits. Requires a loaded guardroom movie.
/// The queue copies the four-byte stack arguments synchronously; the three
/// opcode-unused bytes remain unwritten, and no stack pointer is retained.
static void _shelter1fGuardroomUnlockMovieTask(Task* task)
{
    enum {
        SHELTER_1F_GUARDROOM_MOVIE_START       = 0,
        SHELTER_1F_GUARDROOM_MOVIE_WAIT_READY  = 1,
        SHELTER_1F_GUARDROOM_MOVIE_WAIT_IDLE   = 2,
        SHELTER_1F_GUARDROOM_MOVIE_FIRST_FRAME = 1,
        SHELTER_1F_GUARDROOM_MOVIE_SUB_ID      = 0
    };
    u8          streamArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case SHELTER_1F_GUARDROOM_MOVIE_START:
            _shelter1fGuardroomSetUnlockOverlayVisible(0);
            queue->movieFrame = SHELTER_1F_GUARDROOM_MOVIE_FIRST_FRAME;
            streamArgs[0]     = streamFindMovieSlot(&gGameSession->location.loc, SHELTER_1F_GUARDROOM_MOVIE_SUB_ID, 0);
            // The queue copies four bytes; this opcode uses only the slot byte.
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state++;
            break;
        case SHELTER_1F_GUARDROOM_MOVIE_WAIT_READY:
            if (queue->movieReady != 0) {
                task->state = SHELTER_1F_GUARDROOM_MOVIE_WAIT_IDLE;
            }
            break;
        case SHELTER_1F_GUARDROOM_MOVIE_WAIT_IDLE:
            if (cdCmdIsIdle()) {
                taskRequestKill(task, 0);
            }
            break;
    }
}

/// Shows or hides the two-sprite overlay for the unlocked bulwark in guardroom view 3.
///
/// `visible` is a byte (0 hidden, nonzero visible). Requires the current session
/// to select the guardroom and its map and room resources to remain loaded.
/// The overlay is hidden during the unlock movie and restored when it completes.
static void _shelter1fGuardroomSetUnlockOverlayVisible(u8 visible)
{
    enum {
        SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_VIEW_INDEX  = 2,
        SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_BATCH_INDEX = 1,
    };
    const GameLocationKey* location = &gGameSession->location.loc;
    SpriteBatch*           batches;

    batches = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1][SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_VIEW_INDEX].batches;
    if (visible == 0) {
        batches[SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_BATCH_INDEX].hidden = 1;
    } else {
        batches[SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_BATCH_INDEX].hidden = 0;
    }
}

void shelter1fGuardroomEffectNoopTask(Task* unusedTask)
{
}
