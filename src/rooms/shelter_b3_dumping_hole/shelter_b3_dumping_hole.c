#include "rooms/shelter_b3_dumping_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "shelter_b3_dumping_hole_private.h"

#include "actors/actor_403200.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/gameflag.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

extern u8 D_shelter_b3_dumping_hole_8018F4A4;

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_shelter_b3_dumping_hole_80187574[6];

extern TaskDesc D_actor_342100_80164B78[];

static s32  _shelterB3DumpingHoleRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _shelterB3DumpingHoleResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB3DumpingHoleHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _shelterB3DumpingHoleIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);
static s32  _shelterB3DumpingHoleStartCollapseOnActorEvent(Task* task, s32 messageId, s32 eventId, s32 unusedArg);
static void _shelterB3DumpingHoleInitializeRoomTask(Task* task);
static void _shelterB3DumpingHoleIdleRoom(Task* task);

static AnimationSet _gShelterB3DumpingHoleAnimation0AAB4;

static TmdBone _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gShelterB3DumpingHoleAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0PartVerts,
    _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0Verts,
    _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0Normals,
    _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0Skeleton,
    _gShelterB3DumpingHoleAcropolisSanctuaryModel090F0Stream,
};

TaskMessageEntry D_shelter_b3_dumping_hole_80187574[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB3DumpingHoleResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB3DumpingHoleRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB3DumpingHoleIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB3DumpingHoleHandleRoomCommand },
    { ROOM_MESSAGE_ACTOR_EVENT, _shelterB3DumpingHoleStartCollapseOnActorEvent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static TmdBone _gShelterB3DumpingHoleModel0A0CCSkeleton[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A0CC_skeleton.inc"
};

static u32 _gShelterB3DumpingHoleModel0A0CCPartVerts[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A0CC_partVerts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0A0CCVerts[9] = {
#include "assets/shelter_b3_dumping_hole_model_0A0CC_verts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0A0CCNormals[15] = {
#include "assets/shelter_b3_dumping_hole_model_0A0CC_normals.inc"
};

static u32 _gShelterB3DumpingHoleModel0A0CCStream[90] = {
#include "assets/shelter_b3_dumping_hole_model_0A0CC_stream.inc"
};

TmdSource gShelterB3DumpingHoleModel0A0CC = {
    0,
    560,
    0,
    1,
    _gShelterB3DumpingHoleModel0A0CCPartVerts,
    _gShelterB3DumpingHoleModel0A0CCVerts,
    _gShelterB3DumpingHoleModel0A0CCNormals,
    _gShelterB3DumpingHoleModel0A0CCSkeleton,
    _gShelterB3DumpingHoleModel0A0CCStream,
};

static TmdBone _gShelterB3DumpingHoleModel0A348Skeleton[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A348_skeleton.inc"
};

static u32 _gShelterB3DumpingHoleModel0A348PartVerts[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A348_partVerts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0A348Verts[9] = {
#include "assets/shelter_b3_dumping_hole_model_0A348_verts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0A348Normals[16] = {
#include "assets/shelter_b3_dumping_hole_model_0A348_normals.inc"
};

static u32 _gShelterB3DumpingHoleModel0A348Stream[90] = {
#include "assets/shelter_b3_dumping_hole_model_0A348_stream.inc"
};

TmdSource gShelterB3DumpingHoleModel0A348 = {
    0,
    560,
    0,
    1,
    _gShelterB3DumpingHoleModel0A348PartVerts,
    _gShelterB3DumpingHoleModel0A348Verts,
    _gShelterB3DumpingHoleModel0A348Normals,
    _gShelterB3DumpingHoleModel0A348Skeleton,
    _gShelterB3DumpingHoleModel0A348Stream,
};

static TmdBone _gShelterB3DumpingHoleModel0A5ECSkeleton[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A5EC_skeleton.inc"
};

static u32 _gShelterB3DumpingHoleModel0A5ECPartVerts[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A5EC_partVerts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0A5ECVerts[11] = {
#include "assets/shelter_b3_dumping_hole_model_0A5EC_verts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0A5ECNormals[19] = {
#include "assets/shelter_b3_dumping_hole_model_0A5EC_normals.inc"
};

static u32 _gShelterB3DumpingHoleModel0A5ECStream[114] = {
#include "assets/shelter_b3_dumping_hole_model_0A5EC_stream.inc"
};

TmdSource gShelterB3DumpingHoleModel0A5EC = {
    0,
    720,
    0,
    1,
    _gShelterB3DumpingHoleModel0A5ECPartVerts,
    _gShelterB3DumpingHoleModel0A5ECVerts,
    _gShelterB3DumpingHoleModel0A5ECNormals,
    _gShelterB3DumpingHoleModel0A5ECSkeleton,
    _gShelterB3DumpingHoleModel0A5ECStream,
};

static AnimationPackedPose _gShelterB3DumpingHoleAnimation0AAB4Bank1[6] = {
#include "assets/shelter_b3_dumping_hole_animation_0AAB4_bank1.inc"
};

static AnimationPackedRotation _gShelterB3DumpingHoleAnimation0AAB4Bank4[46] = {
#include "assets/shelter_b3_dumping_hole_animation_0AAB4_bank4.inc"
};

static AnimationRecord _gShelterB3DumpingHoleAnimation0AAB4Records[109] = {
#include "assets/shelter_b3_dumping_hole_animation_0AAB4_records.inc"
};

static u16 _gShelterB3DumpingHoleAnimation0AAB4Indices[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0AAB4_indices.inc"
};

static AnimationSet _gShelterB3DumpingHoleAnimation0AAB4 = {
    _gShelterB3DumpingHoleAnimation0AAB4Records,
    _gShelterB3DumpingHoleAnimation0AAB4Indices,
    { NULL, _gShelterB3DumpingHoleAnimation0AAB4Bank1, NULL, NULL, _gShelterB3DumpingHoleAnimation0AAB4Bank4, NULL, NULL, NULL },
};

s16 D_shelter_b3_dumping_hole_8018809C = 1;

AnimationSet* D_shelter_b3_dumping_hole_801880A0[6] = {
    &_gShelterB3DumpingHoleAnimation0AAB4,
    &gActor403200Animation2CF64,
    &gActor403200Animation2D1D0,
    &gActor403200Animation2D928,
    &gActor403200Animation2CDE0,
    NULL,
};

/// Refuses key-item use with the inventory menu's refused reply.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; the selected collected-item ID and
/// unused second word are ignored, with no changes to room state.
static s32 _shelterB3DumpingHoleRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves departures and refuses the incinerator exit while it is blocked.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`, borrowing complete eight-byte records
/// for this call; request and reply may alias. Returns 1 for ordinary passage
/// and 0 for a blocked exit. Execute requests play the refusal CAP or select
/// the incinerator room from the zero-based event room index; queries do neither.
/// The receiver and message ID are unused.
static s32 _shelterB3DumpingHoleResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { INCINERATOR_EXIT_BLOCKED_CAP_COMMAND = 0x16,
           ROOM_TRANSITION_HANDLED              = 0,
           ROOM_TRANSITION_ALLOWED              = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR) {
        if (shelterB3DumpingHoleIsIncineratorExitBlocked() != 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(INCINERATOR_EXIT_BLOCKED_CAP_COMMAND);
            }
            return ROOM_TRANSITION_HANDLED;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->room = gGameSession->eventRoomIndex + 1;
        }
        return ROOM_TRANSITION_ALLOWED;
    }
    return ROOM_TRANSITION_ALLOWED;
}

/// Starts the progress-selected CAP event for room command 18 when CAP is idle.
///
/// Handles `ROOM_MESSAGE_COMMAND`: command 18 selects CAP 23 while flag 0x11D
/// is clear and CAP 18 otherwise, holding actors for the event. Other commands
/// have no effect. The receiver and other words are unused; every call returns 0.
static s32 _shelterB3DumpingHoleHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum { DUMPING_HOLE_PROGRESS_COMMAND     = 0x12,
           DUMPING_HOLE_PROGRESS_CAP_COMMAND = 0x12,
           DUMPING_HOLE_INITIAL_CAP_COMMAND  = 0x17,
           DUMPING_HOLE_COMMAND_REPLY        = 0 };

    if (commandId == DUMPING_HOLE_PROGRESS_COMMAND) {
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_11D) != 0 ? DUMPING_HOLE_PROGRESS_CAP_COMMAND : DUMPING_HOLE_INITIAL_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
    }
    return DUMPING_HOLE_COMMAND_REPLY;
}

/// Ignores direction-triggered room actions and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`; the borrowed four-byte request
/// and unused second word are neither read nor retained.
static s32 _shelterB3DumpingHoleIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    return 0;
}

/// Starts the room collapse sequence when the Glutton reports its death handoff.
///
/// Handles `ROOM_MESSAGE_ACTOR_EVENT` without filtering the event payload.
/// All arguments are unused, and even a failed task spawn returns 0.
static s32 _shelterB3DumpingHoleStartCollapseOnActorEvent(Task* task, s32 messageId, s32 eventId, s32 unusedArg)
{
    enum { DUMPING_HOLE_COLLAPSE_TASK_INDEX = 0,
           DUMPING_HOLE_ACTOR_EVENT_REPLY   = 0 };

    taskSpawnFromTable(D_shelter_b3_dumping_hole_80189ADC, DUMPING_HOLE_COLLAPSE_TASK_INDEX, 0, 0);
    return DUMPING_HOLE_ACTOR_EVENT_REPLY;
}

/// Registers the room receiver and selects its arrival scene and encounter.
///
/// Runs at state 0 with loaded captions and event-script resources. The first
/// variant-1 arrival records the objective and arrival flag; warp 3 also starts
/// the skippable entry script. Room selectors 2 and above start the controller
/// that schedules the enemy waves and the later burn scene. Advances to idle.
static void _shelterB3DumpingHoleInitializeRoomTask(Task* task)
{
    enum { DUMPING_HOLE_CAPTION_TEXTURE_X_WORDS = 0x180,
           DUMPING_HOLE_CAPTION_TEXTURE_Y_ROWS  = 0,
           DUMPING_HOLE_CAPTION_DATA_INDEX      = 0,
           DUMPING_HOLE_ARRIVAL_VARIANT         = 1,
           DUMPING_HOLE_ARRIVAL_WARP            = 3,
           DUMPING_HOLE_ARRIVAL_OBJECTIVE       = 0x21,
           DUMPING_HOLE_ARRIVAL_SEEN            = 1,
           DUMPING_HOLE_ENCOUNTER_FIRST_ROOM    = 2,
           DUMPING_HOLE_ENCOUNTER_TASK_INDEX    = 0 };

    task->msgTable = D_shelter_b3_dumping_hole_80187574;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    shelterB3DumpingHoleSelectCaptionResource(DUMPING_HOLE_CAPTION_TEXTURE_X_WORDS, DUMPING_HOLE_CAPTION_TEXTURE_Y_ROWS, DUMPING_HOLE_CAPTION_DATA_INDEX);
    // Record first arrival independently of whether this warp runs the entry scene.
    if (gameFlagGetNibble(GAME_FLAG_DUMPING_HOLE_ARRIVAL_SEEN) == 0) {
        if (gGameSession->location.loc.variant == DUMPING_HOLE_ARRIVAL_VARIANT) {
            if (gGameSession->location.loc.warp == DUMPING_HOLE_ARRIVAL_WARP) {
                evsStartScriptWithSkip(D_shelter_b3_dumping_hole_8018B080, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                       D_shelter_b3_dumping_hole_8018B428);
            }
            gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, DUMPING_HOLE_ARRIVAL_OBJECTIVE);
            gameFlagSetNibble(GAME_FLAG_DUMPING_HOLE_ARRIVAL_SEEN, DUMPING_HOLE_ARRIVAL_SEEN);
        }
    }
    if (gGameSession->location.loc.room >= DUMPING_HOLE_ENCOUNTER_FIRST_ROOM) {
        taskSpawnFromTable(D_actor_342100_80164B78, DUMPING_HOLE_ENCOUNTER_TASK_INDEX, 0, 0);
    }
    task->state                       += 1;
    D_shelter_b3_dumping_hole_8018F4A4 = 0;
}

/// Leaves the initialized room task idle while its message table remains active.
///
/// The room controller's state 1 performs no per-frame work or state change.
static void _shelterB3DumpingHoleIdleRoom(Task* task)
{
    // Retained unused storage preserves the original 16-byte stack frame.
    char unusedStackBytes[0x10];
}

/// State handlers of the room's controller task, run by
/// `shelterB3DumpingHoleRoomTask`: set-up, an idle state, and the
/// kill.
static const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D5C4 = { {
    _shelterB3DumpingHoleInitializeRoomTask,
    _shelterB3DumpingHoleIdleRoom,
    taskKill,
} };

void shelterB3DumpingHoleRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_shelter_b3_dumping_hole_8017D5C4;
    handlers.funcs[task->state](task);
}
