#include "rooms/shelter_b3_dumping_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "shelter_b3_dumping_hole_private.h"

#include "actors/actor_403200.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
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

s32 func_shelter_b3_dumping_hole_8017D758(Task*, s32, s32, s32);
s32 func_shelter_b3_dumping_hole_8017D760(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b3_dumping_hole_8017D82C(Task*, s32, s32, s32);
s32 func_shelter_b3_dumping_hole_8017D868(Task*, s32, s32, s32);
s32 func_shelter_b3_dumping_hole_8017D870(Task*, s32, s32, s32);

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
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b3_dumping_hole_8017D760 },
    { 5105, func_shelter_b3_dumping_hole_8017D758 },
    { 5103, func_shelter_b3_dumping_hole_8017D868 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b3_dumping_hole_8017D82C },
    { ROOM_MESSAGE_ACTOR_EVENT, func_shelter_b3_dumping_hole_8017D870 },
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
}; /// Message-table handler that accepts every message without acting on it.

static void func_shelter_b3_dumping_hole_8017D8A0(Task* arg0);
static void func_shelter_b3_dumping_hole_8017D998(Task* task);

s32 func_shelter_b3_dumping_hole_8017D758(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b3_dumping_hole_8017D760(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR) {
        if (func_shelter_b3_dumping_hole_8017FB70() != 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(0x16);
            }
            return 0;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->room = gGameSession->eventRoomIndex + 1;
        }
        return 1;
    }
    return 1;
}

s32 func_shelter_b3_dumping_hole_8017D82C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0x12) {
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_11D) != 0 ? 0x12 : 0x17, CAP_EVENT_PAUSE_ACTORS);
    }
    return 0;
}

/// Message-table handler that accepts every message without acting on it.
s32 func_shelter_b3_dumping_hole_8017D868(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b3_dumping_hole_8017D870(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    taskSpawnFromTable(D_shelter_b3_dumping_hole_80189ADC, 0, 0, 0);
    return 0;
}

static void func_shelter_b3_dumping_hole_8017D8A0(Task* arg0)
{
    arg0->msgTable = D_shelter_b3_dumping_hole_80187574;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    func_shelter_b3_dumping_hole_80183198(0x180, 0, 0);
    if (gameFlagGetNibble(GAME_FLAG_DUMPING_HOLE_ARRIVAL_SEEN) == 0) {
        if (gGameSession->location.loc.variant == 1) {
            if (gGameSession->location.loc.warp == 3) {
                evsStartScriptWithSkip(D_shelter_b3_dumping_hole_8018B080, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                       D_shelter_b3_dumping_hole_8018B428);
            }
            gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x21);
            gameFlagSetNibble(GAME_FLAG_DUMPING_HOLE_ARRIVAL_SEEN, 1);
        }
    }
    if (gGameSession->location.loc.room >= 2) {
        taskSpawnFromTable(D_actor_342100_80164B78, 0, 0, 0);
    }
    arg0->state                       += 1;
    D_shelter_b3_dumping_hole_8018F4A4 = 0;
}

/// Empty function; the unused local reserves the 0x10-byte stack frame the
/// original carries.
static void func_shelter_b3_dumping_hole_8017D998(Task* task)
{
    char pad[0x10];
}

/// State handlers of the room's controller task, run by
/// `func_shelter_b3_dumping_hole_8017D9A8`: set-up, an idle state, and the
/// kill.
static const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D5C4 = { {
    func_shelter_b3_dumping_hole_8017D8A0,
    func_shelter_b3_dumping_hole_8017D998,
    taskKill,
} };

void func_shelter_b3_dumping_hole_8017D9A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D5C4;
    sp.funcs[task->state](task);
}
