#include "rooms/dryfield_toilet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_toilet_private.h"

#include "gameplay/animation.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "../../shared/room_variants.h"
static s32 _toiletSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

/// The room's message table, installed in `Task::msgTable` by the room task's
/// entry state.
extern TaskMessageEntry D_dryfield_toilet_801802A4[6];

/// The four-byte payload the room task's entry state broadcasts to the scene's
/// actors.
extern s32 D_dryfield_toilet_801802D4;

/// The template the room's collision grid is restored from, and the grid
/// itself.
extern WorldCollisionGrid D_dryfield_toilet_80180314;

static void _dryfieldToiletInitRoom(Task* task);
static void _dryfieldToiletIdleRoom(Task* unusedTask);

/// The room task's three states: entry, idle and `taskKill`.
static const TaskFuncTable3 D_dryfield_toilet_8017D5C4 = {
    { _dryfieldToiletInitRoom, _dryfieldToiletIdleRoom, taskKill },
};

static s32 _dryfieldToiletRefuseKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _dryfieldToiletIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg);
static s32 _dryfieldToiletHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Action and area variant selecting the room's once-only event.
enum {
    DRYFIELD_TOILET_ACTION_START_EVENT      = 1,
    DRYFIELD_TOILET_EVENT_VARIANT           = 1,
    DRYFIELD_TOILET_EVENT_COLLISION_SHIFT_X = 2000, // Whole room-coordinate units toward negative X
};

TaskMessageEntry D_dryfield_toilet_801802A4[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantParkingLotMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldToiletRefuseKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldToiletHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldToiletIgnoreRoomCommand },
    { ROOM_MESSAGE_SOUND, _toiletSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_dryfield_toilet_801802D4 = 4098;

static SVECTOR _gDryfieldToiletCollision02D54Normals[1] = {
#include "assets/dryfield_toilet_collision_02D54_normals.inc"
};

static SVECTOR _gDryfieldToiletCollision02D54Verts[4] = {
#include "assets/dryfield_toilet_collision_02D54_verts.inc"
};

static WorldCollisionGridFace _gDryfieldToiletCollision02D54Faces[1] = {
#include "assets/dryfield_toilet_collision_02D54_faces.inc"
};

static s16 _gDryfieldToiletCollision02D54Cells[2] = {
#include "assets/dryfield_toilet_collision_02D54_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldToiletCollision02D54Cells[i])
static s16* _gDryfieldToiletCollision02D54Table[1] = {
#include "assets/dryfield_toilet_collision_02D54_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_toilet_80180314 = { NULL, _gDryfieldToiletCollision02D54Normals, _gDryfieldToiletCollision02D54Verts, _gDryfieldToiletCollision02D54Faces, _gDryfieldToiletCollision02D54Table, 2250, 590, 1, 1, 4000, 1 };

static AnimationPackedPose _gDryfieldToiletAnimation03054Bank1[6] = {
#include "assets/dryfield_toilet_animation_03054_bank1.inc"
};

static AnimationPackedRotation _gDryfieldToiletAnimation03054Bank4[46] = {
#include "assets/dryfield_toilet_animation_03054_bank4.inc"
};

static AnimationRecord _gDryfieldToiletAnimation03054Records[109] = {
#include "assets/dryfield_toilet_animation_03054_records.inc"
};

static u16 _gDryfieldToiletAnimation03054Indices[20] = {
#include "assets/dryfield_toilet_animation_03054_indices.inc"
};

AnimationSet gDryfieldToiletAnimation03054 = {
    _gDryfieldToiletAnimation03054Records,
    _gDryfieldToiletAnimation03054Indices,
    { NULL, _gDryfieldToiletAnimation03054Bank1, NULL, NULL, _gDryfieldToiletAnimation03054Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldToiletAnimation035A4Bank1[9] = {
#include "assets/dryfield_toilet_animation_035A4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldToiletAnimation035A4Bank4[131] = {
#include "assets/dryfield_toilet_animation_035A4_bank4.inc"
};

static AnimationRecord _gDryfieldToiletAnimation035A4Records[162] = {
#include "assets/dryfield_toilet_animation_035A4_records.inc"
};

static u16 _gDryfieldToiletAnimation035A4Indices[20] = {
#include "assets/dryfield_toilet_animation_035A4_indices.inc"
};

AnimationSet gDryfieldToiletAnimation035A4 = {
    _gDryfieldToiletAnimation035A4Records,
    _gDryfieldToiletAnimation035A4Indices,
    { NULL, _gDryfieldToiletAnimation035A4Bank1, NULL, NULL, _gDryfieldToiletAnimation035A4Bank4, NULL, NULL, NULL },
};

/// Initializes the room grid's reserved event face from its collision template.
///
/// Restores normal 0, vertices 0..3 and face 0, preserving the vectors' fourth
/// components and all other grid data. A seen event shifts those vertices
/// 2000 whole room-coordinate units toward negative X after restoration.
static void _dryfieldToiletInitEventCollision(void)
{
    // Copy XYZ while retaining the fourth component. Each argument is evaluated
    // three times and must be a stable, side-effect-free SVECTOR lvalue.
#define DRYFIELD_TOILET_COPY_INITIAL_COLLISION_XYZ(destination, source) \
    {                                                                   \
        (destination).vx = (source).vx;                                 \
        (destination).vy = (source).vy;                                 \
        (destination).vz = (source).vz;                                 \
    }

    WorldCollisionGrid*       liveGrid          = &D_dryfield_toilet_80181404;
    const WorldCollisionGrid* collisionTemplate = &D_dryfield_toilet_80180314;
    s32                       index;

    for (index = 0; index < (s32)ARRAY_SIZE(_gDryfieldToiletCollision02D54Faces); index++) {
        DRYFIELD_TOILET_COPY_INITIAL_COLLISION_XYZ(liveGrid->normals[index], collisionTemplate->normals[index]);
        DRYFIELD_TOILET_COPY_INITIAL_COLLISION_XYZ(liveGrid->vertices[index * 4 + 0], collisionTemplate->vertices[index * 4 + 0]);
        DRYFIELD_TOILET_COPY_INITIAL_COLLISION_XYZ(liveGrid->vertices[index * 4 + 1], collisionTemplate->vertices[index * 4 + 1]);
        DRYFIELD_TOILET_COPY_INITIAL_COLLISION_XYZ(liveGrid->vertices[index * 4 + 2], collisionTemplate->vertices[index * 4 + 2]);
        DRYFIELD_TOILET_COPY_INITIAL_COLLISION_XYZ(liveGrid->vertices[index * 4 + 3], collisionTemplate->vertices[index * 4 + 3]);
        liveGrid->faces[index] = collisionTemplate->faces[index];
    }
    if (gameFlagGetNibble(GAME_FLAG_TOILET_EVENT_SEEN) != 0) {
        for (index = 0; index < (s32)ARRAY_SIZE(_gDryfieldToiletCollision02D54Verts); index++) {
            liveGrid->vertices[index].vx -= DRYFIELD_TOILET_EVENT_COLLISION_SHIFT_X;
        }
    }
#undef DRYFIELD_TOILET_COPY_INITIAL_COLLISION_XYZ
}

#include "../../shared/room_variants_parking_lot.inc.c"

#include "../../shared/toilet_sound_msg.inc.c"

/// Refuses every key-item-use request without consuming the selected item.
static s32 _dryfieldToiletRefuseKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Ignores room commands and returns zero without changing room state.
static s32 _dryfieldToiletIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg)
{
    return 0;
}

/// Starts the room event once when action 1 arrives in area variant 1.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows `request` through synchronous
/// dispatch; only its action byte is read. The second payload is unused.
/// Starts the event with its skip script, keeps the HUD setting, then latches
/// `GAME_FLAG_TOILET_EVENT_SEEN`. Returns zero whether or not the event starts.
static s32 _dryfieldToiletHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    u8 actionId = request->actionId;

    if (actionId == DRYFIELD_TOILET_ACTION_START_EVENT && gameFlagGetNibble(GAME_FLAG_TOILET_EVENT_SEEN) == 0 && gGameSession->location.loc.variant == actionId) {
        evsStartScriptWithSkip(D_dryfield_toilet_80180C58, EVENT_SCRIPT_HUD_KEEP, D_dryfield_toilet_80180F40);
        gameFlagSetNibble(GAME_FLAG_TOILET_EVENT_SEEN, 1);
    }
    return 0;
}

/// Registers the room task and prepares the actors and collision for its unseen event.
///
/// State 0 installs the message table and claims `GAME_TASK_SLOT_ROOM`. An
/// unseen event in area variant 1 requires a live scene task for the borrowed
/// actor-command broadcast. Advances the task to idle state 1 in either case.
static void _dryfieldToiletInitRoom(Task* task)
{
    task->msgTable = D_dryfield_toilet_801802A4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_TOILET_EVENT_SEEN) == 0 && gGameSession->location.loc.variant == DRYFIELD_TOILET_EVENT_VARIANT) {
        // Prepare placed actors before installing the event's collision face.
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_dryfield_toilet_801802D4, ACTOR_COMMAND_MESSAGE_APPLY);
        _dryfieldToiletInitEventCollision();
    }
    task->state = task->state + 1;
}

/// Keeps the room task alive in idle state 1 to receive messages.
static void _dryfieldToiletIdleRoom(Task* unusedTask)
{
    // The binary retains this otherwise unused sixteen-byte stack frame.
    char unusedStackFrame[0x10];
}

void dryfieldToiletRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_dryfield_toilet_8017D5C4;
    states.funcs[task->state](task);
}

void dryfieldToiletMoveEventCollision(s32 moveAside)
{
    // Copy XYZ while retaining the fourth component. Each argument is evaluated
    // three times and must be a stable, side-effect-free SVECTOR lvalue.
#define DRYFIELD_TOILET_COPY_SCRIPT_COLLISION_XYZ(destination, source) \
    {                                                                  \
        (destination).vx = (source).vx;                                \
        (destination).vy = (source).vy;                                \
        (destination).vz = (source).vz;                                \
    }

    WorldCollisionGrid*       liveGrid          = &D_dryfield_toilet_80181404;
    const WorldCollisionGrid* collisionTemplate = &D_dryfield_toilet_80180314;
    s32                       index;

    if (moveAside == DRYFIELD_TOILET_EVENT_COLLISION_RESTORE) {
        for (index = 0; index < (s32)ARRAY_SIZE(_gDryfieldToiletCollision02D54Faces); index++) {
            DRYFIELD_TOILET_COPY_SCRIPT_COLLISION_XYZ(liveGrid->normals[index], collisionTemplate->normals[index]);
            DRYFIELD_TOILET_COPY_SCRIPT_COLLISION_XYZ(liveGrid->vertices[index * 4 + 0], collisionTemplate->vertices[index * 4 + 0]);
            DRYFIELD_TOILET_COPY_SCRIPT_COLLISION_XYZ(liveGrid->vertices[index * 4 + 1], collisionTemplate->vertices[index * 4 + 1]);
            DRYFIELD_TOILET_COPY_SCRIPT_COLLISION_XYZ(liveGrid->vertices[index * 4 + 2], collisionTemplate->vertices[index * 4 + 2]);
            DRYFIELD_TOILET_COPY_SCRIPT_COLLISION_XYZ(liveGrid->vertices[index * 4 + 3], collisionTemplate->vertices[index * 4 + 3]);
            liveGrid->faces[index] = collisionTemplate->faces[index];
        }
    } else {
        for (index = 0; index < (s32)ARRAY_SIZE(_gDryfieldToiletCollision02D54Verts); index++) {
            liveGrid->vertices[index].vx -= DRYFIELD_TOILET_EVENT_COLLISION_SHIFT_X;
        }
    }
#undef DRYFIELD_TOILET_COPY_SCRIPT_COLLISION_XYZ
}

void dryfieldToiletStageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

void dryfieldToiletStartScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

void dryfieldToiletFinishScene(void)
{
    streamFinishScene();
}

void dryfieldToiletCancelScene(void)
{
    cdCmdCancelScene();
}

void dryfieldToiletEngageBattle(s32 unusedArg)
{
    sceneEngageBattle(unusedArg);
}
