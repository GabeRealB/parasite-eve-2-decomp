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

static void func_dryfield_toilet_8017D940(Task* arg0);
static void func_dryfield_toilet_8017D9D4(Task* task);

/// The room task's three states: entry, idle and `taskKill`.
static const TaskFuncTable3 D_dryfield_toilet_8017D5C4 = {
    { func_dryfield_toilet_8017D940, func_dryfield_toilet_8017D9D4, taskKill },
};

s32 func_dryfield_toilet_8017D8B8(Task*, s32, s32, s32);
s32 func_dryfield_toilet_8017D8C0(Task*, s32, s32, s32);
s32 func_dryfield_toilet_8017D8C8(Task*, s32, RoomEventMsg*, RoomEventMsg*);

TaskMessageEntry D_dryfield_toilet_801802A4[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantParkingLotMsg },
    { 5105, func_dryfield_toilet_8017D8B8 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_toilet_8017D8C8 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_toilet_8017D8C0 },
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

static void func_dryfield_toilet_8017D5E4(void);

/// Restores one face of the room's collision grid (its normal, four corners and
/// face record) from the template, then slides the four corners 2000 units toward
/// negative x once game flag nibble 0x60 is set.
static void func_dryfield_toilet_8017D5E4(void)
{
    WorldCollisionGrid* geom = &D_dryfield_toilet_80181404;
    WorldCollisionGrid* src  = &D_dryfield_toilet_80180314;
    s32                 i;

    for (i = 0; i < 1; i++) {
        geom->normals[i].vx          = src->normals[i].vx;
        geom->normals[i].vy          = src->normals[i].vy;
        geom->normals[i].vz          = src->normals[i].vz;
        geom->vertices[i * 4 + 0].vx = src->vertices[i * 4 + 0].vx;
        geom->vertices[i * 4 + 0].vy = src->vertices[i * 4 + 0].vy;
        geom->vertices[i * 4 + 0].vz = src->vertices[i * 4 + 0].vz;
        geom->vertices[i * 4 + 1].vx = src->vertices[i * 4 + 1].vx;
        geom->vertices[i * 4 + 1].vy = src->vertices[i * 4 + 1].vy;
        geom->vertices[i * 4 + 1].vz = src->vertices[i * 4 + 1].vz;
        geom->vertices[i * 4 + 2].vx = src->vertices[i * 4 + 2].vx;
        geom->vertices[i * 4 + 2].vy = src->vertices[i * 4 + 2].vy;
        geom->vertices[i * 4 + 2].vz = src->vertices[i * 4 + 2].vz;
        geom->vertices[i * 4 + 3].vx = src->vertices[i * 4 + 3].vx;
        geom->vertices[i * 4 + 3].vy = src->vertices[i * 4 + 3].vy;
        geom->vertices[i * 4 + 3].vz = src->vertices[i * 4 + 3].vz;
        geom->faces[i]               = src->faces[i];
    }
    if (gameFlagGetNibble(GAME_FLAG_TOILET_EVENT_SEEN) != 0) {
        for (i = 0; i < 4; i++) {
            geom->vertices[i].vx -= 2000;
        }
    }
}

#include "../../shared/room_variants_parking_lot.inc.c"

#include "../../shared/toilet_sound_msg.inc.c"

s32 func_dryfield_toilet_8017D8B8(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

s32 func_dryfield_toilet_8017D8C0(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

/// Handler for message `0x13EF` in the room's `(msgId, handler)` table - the
/// direction record `_directionDispatchRoomAction` posts. On the visit whose sub-id
/// (`warp`) is 1, that agrees with the session's own sub-id
/// (`gGameSession::location.loc.variant`) and that has not yet latched nibble 0x60, the
/// toilet starts its cutscene pair and latches the nibble. The outgoing record
/// is never written: this handler only consumes the message.
s32 func_dryfield_toilet_8017D8C8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 subId = in->warp;

    if (subId == 1 && gameFlagGetNibble(GAME_FLAG_TOILET_EVENT_SEEN) == 0 && gGameSession->location.loc.variant == subId) {
        evsStartScriptWithSkip(D_dryfield_toilet_80180C58, EVENT_SCRIPT_HUD_KEEP, D_dryfield_toilet_80180F40);
        gameFlagSetNibble(GAME_FLAG_TOILET_EVENT_SEEN, 1);
    }
    return 0;
}

/// The room task's entry state: publish the message table, claim game pointer
/// slot 7, and on the visit that agrees with the session's sub-id
/// (`gGameSession::location.loc.variant` == 1) and has not yet latched nibble 0x60, post
/// message `0x7DA` with the room's payload and run the scene setup. Advance to
/// the next state either way.
static void func_dryfield_toilet_8017D940(Task* arg0)
{
    arg0->msgTable = D_dryfield_toilet_801802A4;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_TOILET_EVENT_SEEN) == 0 && gGameSession->location.loc.variant == 1) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_dryfield_toilet_801802D4, ACTOR_COMMAND_MESSAGE_APPLY);
        func_dryfield_toilet_8017D5E4();
    }
    arg0->state = arg0->state + 1;
}

/// The room task's idle state, entry 1 of its state table: does nothing but
/// open and close a stack frame.
static void func_dryfield_toilet_8017D9D4(Task* task)
{
    char pad[0x10];
}

/// The room task's update: runs the handler for its current state from a stack
/// copy of the room's state table.
void func_dryfield_toilet_8017D9E4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_toilet_8017D5C4;
    sp.funcs[task->state](task);
}

/// With a zero argument, restores one face of the room's collision grid (its
/// normal, four corners and face record) from the template; otherwise slides
/// the grid's four corners 2000 units toward negative x.
void func_dryfield_toilet_8017DA3C(s32 arg0)
{
    WorldCollisionGrid* geom = &D_dryfield_toilet_80181404;
    WorldCollisionGrid* src  = &D_dryfield_toilet_80180314;
    s32                 i;

    if (arg0 == 0) {
        for (i = 0; i < 1; i++) {
            geom->normals[i].vx          = src->normals[i].vx;
            geom->normals[i].vy          = src->normals[i].vy;
            geom->normals[i].vz          = src->normals[i].vz;
            geom->vertices[i * 4 + 0].vx = src->vertices[i * 4 + 0].vx;
            geom->vertices[i * 4 + 0].vy = src->vertices[i * 4 + 0].vy;
            geom->vertices[i * 4 + 0].vz = src->vertices[i * 4 + 0].vz;
            geom->vertices[i * 4 + 1].vx = src->vertices[i * 4 + 1].vx;
            geom->vertices[i * 4 + 1].vy = src->vertices[i * 4 + 1].vy;
            geom->vertices[i * 4 + 1].vz = src->vertices[i * 4 + 1].vz;
            geom->vertices[i * 4 + 2].vx = src->vertices[i * 4 + 2].vx;
            geom->vertices[i * 4 + 2].vy = src->vertices[i * 4 + 2].vy;
            geom->vertices[i * 4 + 2].vz = src->vertices[i * 4 + 2].vz;
            geom->vertices[i * 4 + 3].vx = src->vertices[i * 4 + 3].vx;
            geom->vertices[i * 4 + 3].vy = src->vertices[i * 4 + 3].vy;
            geom->vertices[i * 4 + 3].vz = src->vertices[i * 4 + 3].vz;
            geom->faces[i]               = src->faces[i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            geom->vertices[i].vx -= 2000;
        }
    }
}

void func_dryfield_toilet_8017DC50(void)
{
    cdCmdStageSceneAudioStart();
}

void func_dryfield_toilet_8017DC70(void)
{
    cdCmdEnqueueScenePlayback();
}

void func_dryfield_toilet_8017DC90(void)
{
    streamFinishScene();
}

void func_dryfield_toilet_8017DCB0(void)
{
    cdCmdCancelScene();
}

void func_dryfield_toilet_8017DCD0(s32 arg0)
{
    sceneEngageBattle(arg0);
}
