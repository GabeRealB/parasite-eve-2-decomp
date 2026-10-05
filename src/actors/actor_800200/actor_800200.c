#include "actors/actor_800200.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

enum { COMPANION_ROUTE_COMPLETE = 1 };

/// One X/Z stop on a scripted route for this companion.
///
/// A route is a table of these stops. Handlers select an entry with
/// `CompanionWork.waypointIndex`, copy `x` and `z` into
/// `GameActor.destination`, and take Y from the actor's current translation.
/// Both components are signed game coordinates in the root coordinate's
/// parent space.
typedef struct {
    s32 x; // Signed game X of this stop
    s32 z; // Signed game Z of this stop
} _Actor800200Waypoint;
STATIC_ASSERT_SIZEOF(_Actor800200Waypoint, 8);

extern TaskMessageEntry     D_actor_800200_80169EF0[20];
extern u8*                  D_actor_800200_80169FD0[4];
extern _Actor800200Waypoint D_actor_800200_80169FE0[];
extern _Actor800200Waypoint D_actor_800200_80169FF8[];
extern _Actor800200Waypoint D_actor_800200_8016A018[];
extern _Actor800200Waypoint D_actor_800200_8016A020[];
extern _Actor800200Waypoint D_actor_800200_8016A040[];
extern _Actor800200Waypoint D_actor_800200_8016A048[];
extern _Actor800200Waypoint D_actor_800200_8016A058[];
extern _Actor800200Waypoint D_actor_800200_8016A068[];
extern _Actor800200Waypoint D_actor_800200_8016A080[];
extern _Actor800200Waypoint D_actor_800200_8016A090[];
extern _Actor800200Waypoint D_actor_800200_8016A098[];
extern _Actor800200Waypoint D_actor_800200_8016A0B0[];
extern _Actor800200Waypoint D_actor_800200_8016A0C8[];
extern _Actor800200Waypoint D_actor_800200_8016A0E0[];
extern _Actor800200Waypoint D_actor_800200_8016A108[];
extern _Actor800200Waypoint D_actor_800200_8016A128[];
extern _Actor800200Waypoint D_actor_800200_8016A130[];

static void func_actor_800200_801626A0(Task* task);
static void func_actor_800200_801652EC(Task* arg0);
static void func_actor_800200_80165380(Task* arg0);
static void func_actor_800200_801653A0(Task* arg0);
static void func_actor_800200_801653C0(Task* arg0);
static void func_actor_800200_80165408(Task* arg0, s32 arg1);
static void func_actor_800200_80165434(Task* arg0, s16 arg1);
static void func_actor_800200_8016545C(Task* arg0, s8 arg1);
static void func_actor_800200_801654EC(Task* arg0, s32 arg1);
static void func_actor_800200_80165534(Task* arg0);
static void func_actor_800200_80165580(Task* arg0);
static void func_actor_800200_80165644(Task* arg0);
static void func_actor_800200_80165708(Task* arg0);
static void func_actor_800200_80165814(Task* arg0);
static void func_actor_800200_801658E0(Task* arg0);
static void func_actor_800200_8016599C(Task* arg0);
static void func_actor_800200_801659CC(Task* arg0);
static void func_actor_800200_80165ACC(Task* arg0);
static void func_actor_800200_80165B84(Task* arg0);
static void func_actor_800200_80165CB4(Task* arg0);
static void func_actor_800200_80165D44(Task* arg0);
static void func_actor_800200_80165E50(Task* arg0);
static void func_actor_800200_80165E90(Task* arg0);
static void func_actor_800200_80165F28(Task* arg0);
static void func_actor_800200_80165F48(Task* arg0);
static void func_actor_800200_80165F50(Task* arg0);
static void func_actor_800200_80165FF0(Task* arg0);
static s32  _actor800200GetContactDistance(GfxCoord* coord, WorldCollisionContact* contact, u16* contactZY);

static AnimationSet _gActor800200Animation08644;
static AnimationSet _gActor800200Animation08CC0;
static AnimationSet _gActor800200Animation09504;
static AnimationSet _gActor800200Animation09AB8;
static AnimationSet _gActor800200Animation09D48;
static AnimationSet _gActor800200Animation0A0C4;
static AnimationSet _gActor800200Animation0A354;
static AnimationSet _gActor800200Animation0A734;
static AnimationSet _gActor800200Animation0AC24;
static AnimationSet _gActor800200Animation0B080;
static AnimationSet _gActor800200Animation0B328;
static AnimationSet _gActor800200Animation0B90C;
static AnimationSet _gActor800200Animation0BF24;
static AnimationSet _gActor800200Animation0C530;
static AnimationSet _gActor800200Animation0CC70;
static AnimationSet _gActor800200Animation0CE00;
static AnimationSet _gActor800200Animation0CFA8;
static AnimationSet _gActor800200Animation0D168;
static AnimationSet _gActor800200Animation0D3C0;

static TmdBone _gActor800200FlintBodySkeleton[19] = {
#include "assets/flint_body_skeleton.inc"
};

static u32 _gActor800200FlintBodyPartVerts[19] = {
#include "assets/flint_body_partVerts.inc"
};

static SVECTOR _gActor800200FlintBodyVerts[238] = {
#include "assets/flint_body_verts.inc"
};

static SVECTOR _gActor800200FlintBodyNormals[238] = {
#include "assets/flint_body_normals.inc"
};

static u32 _gActor800200FlintBodyStream[2784] = {
#include "assets/flint_body_stream.inc"
};

TmdSource gActor800200FlintBody = {
    0,
    13636,
    6016,
    19,
    _gActor800200FlintBodyPartVerts,
    _gActor800200FlintBodyVerts,
    _gActor800200FlintBodyNormals,
    _gActor800200FlintBodySkeleton,
    _gActor800200FlintBodyStream,
};

TaskMessageEntry D_actor_800200_80169EF0[20] = {
    { ANIMATION_MESSAGE_PLAY, func_8010C4F0 },
    { 1002, func_8010C4F0 },
    { 1003, func_8010C4F0 },
    { 1004, func_8010C4F0 },
    { GAME_ACTOR_MESSAGE_PLACE, func_80104D68 },
    { ANIMATION_MESSAGE_IS_PLAYING, func_8010583C },
    { GAME_ACTOR_MESSAGE_TURN_TO_YAW, func_8010C688 },
    { GAME_ACTOR_MESSAGE_CLIMB_STAIRS, func_8010C4F0 },
    { GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, func_80105828 },
    { GAME_ACTOR_MESSAGE_END_SCRIPTED, func_8010C30C },
    { GAME_ACTOR_MESSAGE_MOVE_TO, func_8010C6C8 },
    { GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, func_80104684 },
    { ANIMATION_MESSAGE_INSTALL_AND_PLAY, func_8010C648 },
    { GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, func_80105A60 },
    { GAME_ACTOR_MESSAGE_WALK_STEPS, func_801052B8 },
    { ANIMATION_MESSAGE_COPY_BANK_EXTENSION, Gp_CopyAllyAnim },
    { GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, func_8010C4F0 },
    { GAME_ACTOR_MESSAGE_APPLY_DAMAGE, func_8010C4F0 },
    { 1018, func_8010C4F0 },
    { 1019, func_8010C708 },
};

u8 D_actor_800200_80169F90[16] = {
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800200_80169FA0[16] = {
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800200_80169FB0[16] = {
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800200_80169FC0[16] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    3,
    3,
    2,
    2,
    2,
};

u8* D_actor_800200_80169FD0[4] = {
    D_actor_800200_80169F90,
    D_actor_800200_80169FA0,
    D_actor_800200_80169FB0,
    D_actor_800200_80169FC0,
};

_Actor800200Waypoint D_actor_800200_80169FE0[3] = {
    { 0x3C28, 3850 },
    { 0x52DA, 3390 },
    { 0x5488, 3000 },
};

_Actor800200Waypoint D_actor_800200_80169FF8[4] = {
    { 0x32C8, 3740 },
    { 7530, 2720 },
    { 1720, 1480 },
    { 5100, 1250 },
};

_Actor800200Waypoint D_actor_800200_8016A018[1] = {
    { 940, 2320 },
};

_Actor800200Waypoint D_actor_800200_8016A020[4] = {
    { 5000, 3760 },
    { 2970, 3020 },
    { 3400, 1640 },
    { 4530, 1500 },
};

_Actor800200Waypoint D_actor_800200_8016A040[1] = {
    { 200, 1650 },
};

_Actor800200Waypoint D_actor_800200_8016A048[2] = {
    { 1530, 2560 },
    { 1100, 7060 },
};

_Actor800200Waypoint D_actor_800200_8016A058[2] = {
    { 0x2C4C, 1450 },
    { 7940, 1590 },
};

_Actor800200Waypoint D_actor_800200_8016A068[3] = {
    { 4100, -350 },
    { 200, -1050 },
    { -1900, 600 },
};

_Actor800200Waypoint D_actor_800200_8016A080[2] = {
    { 8512, 2880 },
    { 0x2792, 2880 },
};

_Actor800200Waypoint D_actor_800200_8016A090[1] = {
    { 3130, 0 },
};

_Actor800200Waypoint D_actor_800200_8016A098[3] = {
    { -9350, -1248 },
    { -5070, 230 },
    { -4960, 1380 },
};

_Actor800200Waypoint D_actor_800200_8016A0B0[3] = {
    { -4740, 4740 },
    { 2170, 4660 },
    { 2100, 3390 },
};

_Actor800200Waypoint D_actor_800200_8016A0C8[3] = {
    { 4600, -320 },
    { 5200, -1540 },
    { 9100, -1560 },
};

_Actor800200Waypoint D_actor_800200_8016A0E0[5] = {
    { -480, -3700 },
    { 1940, -3640 },
    { 3000, -320 },
    { 1860, -4720 },
    { 1980, -6660 },
};

_Actor800200Waypoint D_actor_800200_8016A108[4] = {
    { 1780, 7950 },
    { 9850, 7980 },
    { 9710, 6210 },
    { 0x288C, 6240 },
};

_Actor800200Waypoint D_actor_800200_8016A128[1] = {
    { 5570, 18 },
};

_Actor800200Waypoint D_actor_800200_8016A130[5] = {
    { -800, -2050 },
    { 10, -2950 },
    { 10, -6900 },
    { 10, -1900 },
    { 10, -390 },
};

static AnimationPackedPose _gActor800200Animation08644Bank1[5] = {
#include "assets/actor_800200_animation_08644_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation08644Bank4[46] = {
#include "assets/actor_800200_animation_08644_bank4.inc"
};

static AnimationRecord _gActor800200Animation08644Records[124] = {
#include "assets/actor_800200_animation_08644_records.inc"
};

static u16 _gActor800200Animation08644Indices[20] = {
#include "assets/actor_800200_animation_08644_indices.inc"
};

static AnimationSet _gActor800200Animation08644 = {
    _gActor800200Animation08644Records,
    _gActor800200Animation08644Indices,
    { NULL, _gActor800200Animation08644Bank1, NULL, NULL, _gActor800200Animation08644Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation08CC0Bank1[9] = {
#include "assets/actor_800200_animation_08CC0_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation08CC0Bank4[139] = {
#include "assets/actor_800200_animation_08CC0_bank4.inc"
};

static AnimationRecord _gActor800200Animation08CC0Records[229] = {
#include "assets/actor_800200_animation_08CC0_records.inc"
};

static u16 _gActor800200Animation08CC0Indices[20] = {
#include "assets/actor_800200_animation_08CC0_indices.inc"
};

static AnimationSet _gActor800200Animation08CC0 = {
    _gActor800200Animation08CC0Records,
    _gActor800200Animation08CC0Indices,
    { NULL, _gActor800200Animation08CC0Bank1, NULL, NULL, _gActor800200Animation08CC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation09504Bank1[16] = {
#include "assets/actor_800200_animation_09504_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation09504Bank4[182] = {
#include "assets/actor_800200_animation_09504_bank4.inc"
};

static AnimationRecord _gActor800200Animation09504Records[279] = {
#include "assets/actor_800200_animation_09504_records.inc"
};

static u16 _gActor800200Animation09504Indices[20] = {
#include "assets/actor_800200_animation_09504_indices.inc"
};

static AnimationSet _gActor800200Animation09504 = {
    _gActor800200Animation09504Records,
    _gActor800200Animation09504Indices,
    { NULL, _gActor800200Animation09504Bank1, NULL, NULL, _gActor800200Animation09504Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation09AB8Bank1[11] = {
#include "assets/actor_800200_animation_09AB8_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation09AB8Bank4[134] = {
#include "assets/actor_800200_animation_09AB8_bank4.inc"
};

static AnimationRecord _gActor800200Animation09AB8Records[178] = {
#include "assets/actor_800200_animation_09AB8_records.inc"
};

static u16 _gActor800200Animation09AB8Indices[20] = {
#include "assets/actor_800200_animation_09AB8_indices.inc"
};

static AnimationSet _gActor800200Animation09AB8 = {
    _gActor800200Animation09AB8Records,
    _gActor800200Animation09AB8Indices,
    { NULL, _gActor800200Animation09AB8Bank1, NULL, NULL, _gActor800200Animation09AB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation09D48Bank1[2] = {
#include "assets/actor_800200_animation_09D48_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation09D48Bank4[36] = {
#include "assets/actor_800200_animation_09D48_bank4.inc"
};

static AnimationRecord _gActor800200Animation09D48Records[102] = {
#include "assets/actor_800200_animation_09D48_records.inc"
};

static u16 _gActor800200Animation09D48Indices[20] = {
#include "assets/actor_800200_animation_09D48_indices.inc"
};

static AnimationSet _gActor800200Animation09D48 = {
    _gActor800200Animation09D48Records,
    _gActor800200Animation09D48Indices,
    { NULL, _gActor800200Animation09D48Bank1, NULL, NULL, _gActor800200Animation09D48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0A0C4Bank1[6] = {
#include "assets/actor_800200_animation_0A0C4_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0A0C4Bank4[74] = {
#include "assets/actor_800200_animation_0A0C4_bank4.inc"
};

static AnimationRecord _gActor800200Animation0A0C4Records[111] = {
#include "assets/actor_800200_animation_0A0C4_records.inc"
};

static u16 _gActor800200Animation0A0C4Indices[20] = {
#include "assets/actor_800200_animation_0A0C4_indices.inc"
};

static AnimationSet _gActor800200Animation0A0C4 = {
    _gActor800200Animation0A0C4Records,
    _gActor800200Animation0A0C4Indices,
    { NULL, _gActor800200Animation0A0C4Bank1, NULL, NULL, _gActor800200Animation0A0C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0A354Bank1[2] = {
#include "assets/actor_800200_animation_0A354_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0A354Bank4[39] = {
#include "assets/actor_800200_animation_0A354_bank4.inc"
};

static AnimationRecord _gActor800200Animation0A354Records[99] = {
#include "assets/actor_800200_animation_0A354_records.inc"
};

static u16 _gActor800200Animation0A354Indices[20] = {
#include "assets/actor_800200_animation_0A354_indices.inc"
};

static AnimationSet _gActor800200Animation0A354 = {
    _gActor800200Animation0A354Records,
    _gActor800200Animation0A354Indices,
    { NULL, _gActor800200Animation0A354Bank1, NULL, NULL, _gActor800200Animation0A354Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0A734Bank1[7] = {
#include "assets/actor_800200_animation_0A734_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0A734Bank4[89] = {
#include "assets/actor_800200_animation_0A734_bank4.inc"
};

static AnimationRecord _gActor800200Animation0A734Records[118] = {
#include "assets/actor_800200_animation_0A734_records.inc"
};

static u16 _gActor800200Animation0A734Indices[20] = {
#include "assets/actor_800200_animation_0A734_indices.inc"
};

static AnimationSet _gActor800200Animation0A734 = {
    _gActor800200Animation0A734Records,
    _gActor800200Animation0A734Indices,
    { NULL, _gActor800200Animation0A734Bank1, NULL, NULL, _gActor800200Animation0A734Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0AC24Bank1[5] = {
#include "assets/actor_800200_animation_0AC24_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0AC24Bank4[112] = {
#include "assets/actor_800200_animation_0AC24_bank4.inc"
};

static AnimationRecord _gActor800200Animation0AC24Records[169] = {
#include "assets/actor_800200_animation_0AC24_records.inc"
};

static u16 _gActor800200Animation0AC24Indices[20] = {
#include "assets/actor_800200_animation_0AC24_indices.inc"
};

static AnimationSet _gActor800200Animation0AC24 = {
    _gActor800200Animation0AC24Records,
    _gActor800200Animation0AC24Indices,
    { NULL, _gActor800200Animation0AC24Bank1, NULL, NULL, _gActor800200Animation0AC24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0B080Bank1[7] = {
#include "assets/actor_800200_animation_0B080_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0B080Bank4[92] = {
#include "assets/actor_800200_animation_0B080_bank4.inc"
};

static AnimationRecord _gActor800200Animation0B080Records[146] = {
#include "assets/actor_800200_animation_0B080_records.inc"
};

static u16 _gActor800200Animation0B080Indices[20] = {
#include "assets/actor_800200_animation_0B080_indices.inc"
};

static AnimationSet _gActor800200Animation0B080 = {
    _gActor800200Animation0B080Records,
    _gActor800200Animation0B080Indices,
    { NULL, _gActor800200Animation0B080Bank1, NULL, NULL, _gActor800200Animation0B080Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0B328Bank1[4] = {
#include "assets/actor_800200_animation_0B328_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0B328Bank4[45] = {
#include "assets/actor_800200_animation_0B328_bank4.inc"
};

static AnimationRecord _gActor800200Animation0B328Records[93] = {
#include "assets/actor_800200_animation_0B328_records.inc"
};

static u16 _gActor800200Animation0B328Indices[20] = {
#include "assets/actor_800200_animation_0B328_indices.inc"
};

static AnimationSet _gActor800200Animation0B328 = {
    _gActor800200Animation0B328Records,
    _gActor800200Animation0B328Indices,
    { NULL, _gActor800200Animation0B328Bank1, NULL, NULL, _gActor800200Animation0B328Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0B90CBank1[7] = {
#include "assets/actor_800200_animation_0B90C_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0B90CBank4[128] = {
#include "assets/actor_800200_animation_0B90C_bank4.inc"
};

static AnimationRecord _gActor800200Animation0B90CRecords[208] = {
#include "assets/actor_800200_animation_0B90C_records.inc"
};

static u16 _gActor800200Animation0B90CIndices[20] = {
#include "assets/actor_800200_animation_0B90C_indices.inc"
};

static AnimationSet _gActor800200Animation0B90C = {
    _gActor800200Animation0B90CRecords,
    _gActor800200Animation0B90CIndices,
    { NULL, _gActor800200Animation0B90CBank1, NULL, NULL, _gActor800200Animation0B90CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0BF24Bank1[3] = {
#include "assets/actor_800200_animation_0BF24_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0BF24Bank4[144] = {
#include "assets/actor_800200_animation_0BF24_bank4.inc"
};

static AnimationRecord _gActor800200Animation0BF24Records[217] = {
#include "assets/actor_800200_animation_0BF24_records.inc"
};

static u16 _gActor800200Animation0BF24Indices[20] = {
#include "assets/actor_800200_animation_0BF24_indices.inc"
};

static AnimationSet _gActor800200Animation0BF24 = {
    _gActor800200Animation0BF24Records,
    _gActor800200Animation0BF24Indices,
    { NULL, _gActor800200Animation0BF24Bank1, NULL, NULL, _gActor800200Animation0BF24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0C530Bank1[3] = {
#include "assets/actor_800200_animation_0C530_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0C530Bank4[141] = {
#include "assets/actor_800200_animation_0C530_bank4.inc"
};

static AnimationRecord _gActor800200Animation0C530Records[217] = {
#include "assets/actor_800200_animation_0C530_records.inc"
};

static u16 _gActor800200Animation0C530Indices[20] = {
#include "assets/actor_800200_animation_0C530_indices.inc"
};

static AnimationSet _gActor800200Animation0C530 = {
    _gActor800200Animation0C530Records,
    _gActor800200Animation0C530Indices,
    { NULL, _gActor800200Animation0C530Bank1, NULL, NULL, _gActor800200Animation0C530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0CC70Bank1[8] = {
#include "assets/actor_800200_animation_0CC70_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0CC70Bank4[158] = {
#include "assets/actor_800200_animation_0CC70_bank4.inc"
};

static AnimationRecord _gActor800200Animation0CC70Records[262] = {
#include "assets/actor_800200_animation_0CC70_records.inc"
};

static u16 _gActor800200Animation0CC70Indices[20] = {
#include "assets/actor_800200_animation_0CC70_indices.inc"
};

static AnimationSet _gActor800200Animation0CC70 = {
    _gActor800200Animation0CC70Records,
    _gActor800200Animation0CC70Indices,
    { NULL, _gActor800200Animation0CC70Bank1, NULL, NULL, _gActor800200Animation0CC70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0CE00Bank1[2] = {
#include "assets/actor_800200_animation_0CE00_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0CE00Bank4[17] = {
#include "assets/actor_800200_animation_0CE00_bank4.inc"
};

static AnimationRecord _gActor800200Animation0CE00Records[57] = {
#include "assets/actor_800200_animation_0CE00_records.inc"
};

static u16 _gActor800200Animation0CE00Indices[20] = {
#include "assets/actor_800200_animation_0CE00_indices.inc"
};

static AnimationSet _gActor800200Animation0CE00 = {
    _gActor800200Animation0CE00Records,
    _gActor800200Animation0CE00Indices,
    { NULL, _gActor800200Animation0CE00Bank1, NULL, NULL, _gActor800200Animation0CE00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0CFA8Bank1[2] = {
#include "assets/actor_800200_animation_0CFA8_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0CFA8Bank4[23] = {
#include "assets/actor_800200_animation_0CFA8_bank4.inc"
};

static AnimationRecord _gActor800200Animation0CFA8Records[57] = {
#include "assets/actor_800200_animation_0CFA8_records.inc"
};

static u16 _gActor800200Animation0CFA8Indices[20] = {
#include "assets/actor_800200_animation_0CFA8_indices.inc"
};

static AnimationSet _gActor800200Animation0CFA8 = {
    _gActor800200Animation0CFA8Records,
    _gActor800200Animation0CFA8Indices,
    { NULL, _gActor800200Animation0CFA8Bank1, NULL, NULL, _gActor800200Animation0CFA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0D168Bank1[2] = {
#include "assets/actor_800200_animation_0D168_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0D168Bank4[26] = {
#include "assets/actor_800200_animation_0D168_bank4.inc"
};

static AnimationRecord _gActor800200Animation0D168Records[60] = {
#include "assets/actor_800200_animation_0D168_records.inc"
};

static u16 _gActor800200Animation0D168Indices[20] = {
#include "assets/actor_800200_animation_0D168_indices.inc"
};

static AnimationSet _gActor800200Animation0D168 = {
    _gActor800200Animation0D168Records,
    _gActor800200Animation0D168Indices,
    { NULL, _gActor800200Animation0D168Bank1, NULL, NULL, _gActor800200Animation0D168Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800200Animation0D3C0Bank1[2] = {
#include "assets/actor_800200_animation_0D3C0_bank1.inc"
};

static AnimationPackedRotation _gActor800200Animation0D3C0Bank4[40] = {
#include "assets/actor_800200_animation_0D3C0_bank4.inc"
};

static AnimationRecord _gActor800200Animation0D3C0Records[84] = {
#include "assets/actor_800200_animation_0D3C0_records.inc"
};

static u16 _gActor800200Animation0D3C0Indices[20] = {
#include "assets/actor_800200_animation_0D3C0_indices.inc"
};

static AnimationSet _gActor800200Animation0D3C0 = {
    _gActor800200Animation0D3C0Records,
    _gActor800200Animation0D3C0Indices,
    { NULL, _gActor800200Animation0D3C0Bank1, NULL, NULL, _gActor800200Animation0D3C0Bank4, NULL, NULL, NULL },
};

AnimationBank D_actor_800200_8016F208 = { { {
    NULL,
    &_gActor800200Animation08644,
    &_gActor800200Animation08CC0,
    &_gActor800200Animation08CC0,
    &_gActor800200Animation09504,
    &_gActor800200Animation0C530,
    &_gActor800200Animation0BF24,
    &_gActor800200Animation09AB8,
    &_gActor800200Animation0A0C4,
    &_gActor800200Animation09D48,
    &_gActor800200Animation0B328,
    &_gActor800200Animation0B90C,
    &_gActor800200Animation08644,
    &_gActor800200Animation08644,
    &_gActor800200Animation0CC70,
    &_gActor800200Animation08644,
    &_gActor800200Animation0AC24,
    &_gActor800200Animation0B080,
    &_gActor800200Animation0CE00,
    &_gActor800200Animation08CC0,
    &_gActor800200Animation08644,
    &_gActor800200Animation08644,
    &_gActor800200Animation08644,
    &_gActor800200Animation08644,
    &_gActor800200Animation08644,
    &_gActor800200Animation08644,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor800200Animation08644,
    &_gActor800200Animation08644,
    &_gActor800200Animation0A354,
    &_gActor800200Animation0A734,
    &_gActor800200Animation0CFA8,
    &_gActor800200Animation0D168,
    &_gActor800200Animation0D3C0,
    &_gActor800200Animation08644,
    &_gActor800200Animation08644,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

static void func_actor_800200_80162088(Task* arg0);
static void func_actor_800200_801622B0(Task* arg0);
static void func_actor_800200_80162694(Task* arg0);
static void func_actor_800200_80162750(Task* arg0);
static void func_actor_800200_80162990(Task* arg0);
static void func_actor_800200_80162BFC(Task* arg0);
static void func_actor_800200_80162E0C(Task* arg0);
static void func_actor_800200_80163044(Task* arg0);
static void func_actor_800200_80163180(Task* arg0);
static void func_actor_800200_8016337C(Task* arg0);
static void func_actor_800200_80163584(Task* arg0);
static void func_actor_800200_801637B4(Task* arg0);
static void func_actor_800200_8016390C(Task* arg0);
static void func_actor_800200_80163A54(Task* arg0);
static void func_actor_800200_80163B90(Task* arg0);
static void func_actor_800200_80163CCC(Task* arg0);
static void func_actor_800200_80163E14(Task* arg0);
static void func_actor_800200_80163F5C(Task* arg0);
static void func_actor_800200_80164180(Task* arg0);
static void func_actor_800200_8016436C(Task* arg0);
static void func_actor_800200_80164598(Task* arg0);
static void func_actor_800200_801647A8(Task* arg0);
static void func_actor_800200_801649D8(Task* arg0);
static void func_actor_800200_80164C54(Task* arg0);
static void func_actor_800200_80164EBC(Task* arg0);
static s32  func_actor_800200_80165104(Task* arg0);

static void func_actor_800200_80162088(Task* arg0)
{
    GameActor*             actor;
    TmdObject*             extra;
    GfxCoord*              coord;
    GfxCoord*              next;
    GfxCoord**             addr;
    WorldCollisionBody*    obj;
    WorldCollisionContact* recs;
    McSaveData*            save;
    SVECTOR3*              scratch;
    void*                  head;
    s32                    packed;

    actor                      = arg0->work;
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = head - 8;
    scratch                    = (SVECTOR3*)(head - 8);
    extra                      = arg0->extra.tmd;
    addr                       = &extra->coords;
    coord                      = *addr;
    arg0->state++;
    arg0->msgTable                                 = D_actor_800200_80169EF0;
    arg0->exitCallback                             = &func_actor_800200_801626A0;
    actor->animationSlotCount                      = GAME_ACTOR_NORMAL_ANIMATION_SLOTS;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = arg0;
    coord->parent                                  = &gGfxViewCoord;
    coord->composeStamp                            = GRAPHICS_COORD_DIRTY;
    extra->flags                                   = 0;
    RotMatrix(&actor->rotation, &coord->coord);
    func_8010BFCC(arg0);
    actor->animationRate = ANIMATION_RATE_ONE;
    Gp_AnimResetChildSlots(arg0, actor->actionArgument);
    Gp_AnimTickChildSlots(arg0);
    recs                                       = actor->collisionContacts;
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    actor->previousPosition.vx                 = coord->coord.t[0];
    actor->previousPosition.vy                 = coord->coord.t[1];
    actor->previousPosition.vz                 = coord->coord.t[2];
    obj->context.motion                        = &actor->collisionMotionContexts[0];
    obj->coord                                 = coord;
    actor->collisionMotionContexts[0].contacts = recs;
    save                                       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    obj->pos.vx                                = 0;
    obj->pos.vy                                = -0xFA;
    obj->pos.vz                                = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xFA;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        packed      = 0x10000;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    worldCollisionInitContacts(actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), 0);
    obj->flags                                |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    next                                       = arg0->extra.tmd->coords;
    obj->context.motion                        = &actor->collisionMotionContexts[1];
    obj->coord                                 = next + 4;
    actor->collisionMotionContexts[1].contacts = recs;
    obj->pos.vx                                = 0;
    obj->pos.vy                                = 0;
    obj->pos.vz                                = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xC8;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    obj->flags                 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    actor->collisionEnableMask  = GAME_ACTOR_COLLISION_REQUEST_MASK;
    ((SVECTOR3*)(head - 8))->vx = 0;
    scratch->vy                 = -0x100;
    scratch->vz                 = 0x200;
    Gp_BindActorD4(arg0, scratch, 0x600);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_actor_800200_801622B0(Task* arg0)
{
    void**                cursorSlot;
    CompanionMoveScratch* blockEnd;
    CompanionMoveScratch* scratch;
    GameActor*            actor;
    TmdObject*            obj;
    TmdObject*            extra;
    GfxCoord*             coord;
    CompanionWork*        companion;
    WorldCollisionBody*   objs[2];
    s32                   dy;
    s32                   i;
    s8                    bits;

    // Reserve the block: the cursor on entry is the address one past its end.
    cursorSlot                                        = SCRATCH_HEAD_ADDR;
    blockEnd                                          = SCRATCH_HEAD_AT(cursorSlot, CompanionMoveScratch);
    obj                                               = arg0->extra.tmd;
    SCRATCH_HEAD_AT(cursorSlot, CompanionMoveScratch) = blockEnd - 1;
    extra                                             = obj;
    scratch                                           = blockEnd - 1;

    coord     = extra->coords;
    actor     = arg0->work;
    companion = actor->companionWork;
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
        (dy = coord->coord.t[1], dy = dy - actor->previousPosition.vy, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->previousPosition.vx;
        coord->coord.t[1] = actor->previousPosition.vy;
        coord->coord.t[2] = actor->previousPosition.vz;
    } else {
        if (actor->collisionEnableMask & 1) {
            actor->gridResponse = func_801011D0(coord, actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), &actor->surfaceClass);
            if ((s8)actor->gridResponse == 2) {
                coord->coord.t[0] = actor->previousPosition.vx;
                coord->coord.t[1] = actor->previousPosition.vy;
                coord->coord.t[2] = actor->previousPosition.vz;
            }
        } else {
            actor->gridResponse = 0;
        }
        actor->previousPosition.vx = coord->coord.t[0];
        actor->previousPosition.vy = coord->coord.t[1];
        actor->previousPosition.vz = coord->coord.t[2];
    }
    companion->probe.coord = *arg0->extra.tmd->coords;
    objs[0]                = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    objs[1]                = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    for (i = 0; i < 2; i++) {
        bits = actor->pendingCollisionUpdates;
        if ((bits >> i) & 1) {
            actor->collisionEnableMask |= 1 << i;
            objs[i]->flags             |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (bits & (8 << i)) {
            actor->collisionEnableMask &= ~(1 << i);
            objs[i]->flags             &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->pendingCollisionUpdates = 0;
    if (D_80115768 == 0 && gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        func_actor_800200_801652EC(arg0);
    }
    Gp_ClearRec18Occupied(actor->collisionContacts);
    Gp_ClearRec18Occupied(actor->companionWork->probe.contacts);
    if (actor->collisionEnableMask & 1) {
        coord->coord.t[1] = actor->previousPosition.vy + 8;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    // Stage this frame's collision heading, then give it to every motion context.
    if ((s8)actor->usesPushbackDirection != 0) {
        scratch->motionDirection.vx = actor->pushbackDirection.vx;
        scratch->motionDirection.vy = actor->pushbackDirection.vy;
        scratch->motionDirection.vz = actor->pushbackDirection.vz;
    } else {
        scratch->motionDirection.vx = (u16)coord->workm.m[0][2] * (s8) * (volatile u8*)&actor->movementSign;
        scratch->motionDirection.vy = (u16)coord->workm.m[1][2] * (s8) * (volatile u8*)&actor->movementSign;
        scratch->motionDirection.vz = (u16)coord->workm.m[2][2] * (s8) * (volatile u8*)&actor->movementSign;
    }
    actor->collisionMotionContexts[0].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[0].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[0].motionDirection.vz = scratch->motionDirection.vz;
    actor->collisionMotionContexts[1].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[1].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[1].motionDirection.vz = scratch->motionDirection.vz;
    actor->collisionMotionContexts[2].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[2].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[2].motionDirection.vz = scratch->motionDirection.vz;
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        Gp_DrawEffGroundQuad(MATRIX_TRANS(&coord->workm), 0x200, gRoomEffectState->groundShadowShade);
    }
    SCRATCH_STACK_RELEASE_BLOCK(CompanionMoveScratch);
}

static void func_actor_800200_80162694(Task* arg0)
{
    arg0->state = 3;
}

/// Teardown of the actor's main task, run both as its exit callback and as
/// the last entry of its state table: clears the second `gPlayerActorTasks` slot,
/// unlinks the two collision objects the set-up state linked, and kills the
/// task.
static void func_actor_800200_801626A0(Task* task)
{
    GameActor* actor;

    actor                                          = (GameActor*)task->work;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = NULL;
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_ROOT]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART4]);
    taskKill(task);
}

/// State handlers of the actor's main task, indexed by its state: set-up, the
/// per-frame update, a step that only advances to the last state, and the
/// teardown.
static const TaskFuncTable4 D_actor_800200_80161E24 = { {
    func_actor_800200_80162088,
    func_actor_800200_801622B0,
    func_actor_800200_80162694,
    func_actor_800200_801626A0,
} };

/// Handlers `func_actor_800200_801652EC` runs, indexed by `mode`.
static const TaskFuncTable3 D_actor_800200_80161E34 = { {
    func_actor_800200_80165B84,
    func_actor_800200_80165E90,
    func_actor_800200_80165F50,
} };

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects. The table is a local, so it is copied from `.rodata` onto the
/// stack on every call.
void func_actor_800200_801626EC(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800200_80161E24;
    states.funcs[task->state](task);
}

static void func_actor_800200_80162750(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      target;
    u8*            head;
    VECTOR3*       vec;
    void*          lock;
    u32            state;
    u8*            tbl;
    s32            dist;
    s32            diff;

    coord                    = arg0->extra.tmd->coords;
    target                   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    vec                      = (VECTOR3*)(head - 0x10);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    actor                    = arg0->work;
    actor->actionValue      += 1;
    companion                = actor->companionWork;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        state             = 0;
        lock              = Gp_FindLockNode(arg0);
        actor->targetNode = lock;
        if (lock != NULL) {
            Gp_GetLockPos(lock, vec);
            func_80103C74(coord, vec, vec);
            dist  = func_80103D8C(((VECTOR3*)(head - 0x10))->vx, vec->vz);
            dist /= 1024;
            if (dist >= 4) {
                dist = 3;
            }
            tbl   = D_actor_800200_80169FD0[dist];
            state = *(tbl + (rand() & 0xF));
        }
        switch (state) {
            case 0:
                break;
            case 1:
                Gp_GetLockPos(actor->targetNode, &actor->destination);
                func_actor_800200_80165408(arg0, 6);
                break;
            case 2:
                companion->activity.combat.repeatsRemaining = (rand() & 3) + 1;
                func_actor_800200_80165434(arg0, 1);
                break;
            case 3:
                goto do_65380;
        }
    } else {
        if (func_8010BC70(coord) >= 0x600) {
        do_65380:
            func_actor_800200_80165380(arg0);
        } else {
            diff = func_8010BCF4(arg0, MATRIX_TRANS(&target->coord));
            if (diff < 0) {
                diff = -diff;
            }
            if (diff >= 0x200) {
                actor->targetNode = NULL;
                func_actor_800200_801653A0(arg0);
            } else if (actor->actionValue >= ((rand() & 0x7F) + 0x96)) {
                func_actor_800200_801653C0(arg0);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800200_80162990(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            mode;
    s32            delay;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            actor->stateAux       = 1;
            actor->destination.vx = D_actor_800200_80169FF8[3].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_80169FF8[3].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->destination.vx = D_actor_800200_80169FF8[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_80169FF8[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 3) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xE00 || (companion->waypointIndex == 2 && Gp_HasCollectedBit(0x114) == 0)) {
                    actor->stateAux   = 2;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            mode = 6;
            if (companion->waypointIndex == 3) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 2:
            if ((func_8010BC70(coord) < 0xC01 && companion->waypointIndex < 2) || (companion->waypointIndex == state && Gp_HasCollectedBit(0x114) != 0)) {
                companion->waypointIndex++;
                actor->stateAux = 1;
                return;
            }
            delay             = actor->stateTimer - 1;
            actor->stateTimer = delay;
            if (delay <= 0) {
                actor->stateTimer = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_80162BFC(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            mode;
    s32            delay;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            actor->stateAux       = 1;
            actor->destination.vx = D_actor_800200_8016A020[3].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A020[3].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->destination.vx = D_actor_800200_8016A020[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A020[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 3) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->stateAux   = 2;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            mode = 6;
            if (companion->waypointIndex == 3) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 2:
            if (func_8010BC70(coord) < 0xB01 && companion->waypointIndex > 0) {
                companion->waypointIndex++;
                actor->stateAux = 1;
                return;
            }
            delay             = actor->stateTimer - 1;
            actor->stateTimer = delay;
            if (delay <= 0) {
                actor->stateTimer = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_80162E0C(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      target;
    s32            delay;

    coord     = arg0->extra.tmd->coords;
    target    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor     = arg0->work;
    companion = actor->companionWork;
    switch (actor->stateAux) {
        case 0:
            actor->destination.vx = D_actor_800200_80169FE0[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_80169FE0[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 2) {
                    actor->stateAux       = 3;
                    actor->statePhase     = 0;
                    actor->animationState = 7;
                    playerActorPlayChildSlotsWithBlend(arg0, 7, 0, 3);
                    func_actor_800200_80165408(arg0, 6);
                    return;
                }
                if (func_8010BC70(coord) >= 0xE00) {
                    actor->stateAux   = 1;
                    actor->stateTimer = 0;
                    actor->targetNode = 0;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                companion->waypointIndex++;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 1:
            if ((func_8010BC70(coord) < 0xA01) || (coord->coord.t[0] < target->coord.t[0])) {
                companion->waypointIndex++;
                actor->stateAux = 0;
                return;
            }
            delay             = actor->stateTimer - 1;
            actor->stateTimer = delay;
            if (delay <= 0) {
                companion->activity.combat.repeatsRemaining = 1;
                actor->stateTimer                           = rand() & 0x7F;
                func_actor_800200_80165434(arg0, 0);
            }
            return;
        case 3:
            if (actor->statePhase != 0) {
                actor->stateAux++;
            }
            return;
        case 4:
            actor->animationState = 0;
            actor->stateAux++;
            Gp_AnimResetChildSlots(arg0, 9);
            return;
        default:
        case 2:
        case 5:
            return;
    }
}

static void func_actor_800200_80163044(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            flag;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            flag                  = 1;
            actor->stateAux       = flag;
            actor->destination.vx = D_actor_800200_8016A048[1].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A048[1].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->destination.vx = D_actor_800200_8016A048[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A048[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 1) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163180(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            delay;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            actor->stateAux       = 1;
            actor->destination.vx = D_actor_800200_8016A058[1].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A058[1].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->destination.vx = D_actor_800200_8016A058[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A058[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 1) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->stateAux   = 2;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 2:
            if (func_8010BC70(coord) < 0x801) {
                companion->waypointIndex++;
                actor->stateAux = 1;
                return;
            }
            delay             = actor->stateTimer - 1;
            actor->stateTimer = delay;
            if (delay <= 0) {
                actor->stateTimer = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_8016337C(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            mode;
    s32            delay;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            actor->stateAux       = 1;
            actor->destination.vx = D_actor_800200_8016A068[2].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A068[2].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->destination.vx = D_actor_800200_8016A068[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A068[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 2) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->stateAux   = 2;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            mode = 6;
            if (companion->waypointIndex == 2) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 2:
            if (func_8010BC70(coord) < 0x901) {
                companion->waypointIndex++;
                actor->stateAux = 1;
                return;
            }
            delay             = actor->stateTimer - 1;
            actor->stateTimer = delay;
            if (delay <= 0) {
                actor->stateTimer = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_80163584(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            mode;
    s32            delay;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            actor->stateAux       = 1;
            actor->destination.vx = D_actor_800200_8016A080[1].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A080[1].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->stateAux++;
            func_actor_800200_80165534(arg0);
            return;
        case 2:
            actor->destination.vx = D_actor_800200_8016A080[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A080[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 1) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->stateAux   = 3;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            mode = 6;
            if (companion->waypointIndex == 1) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 3:
            if (func_8010BC70(coord) < 0x901) {
                companion->waypointIndex++;
                actor->stateAux = 1;
                return;
            }
            delay             = actor->stateTimer - 1;
            actor->stateTimer = delay;
            if (delay <= 0) {
                actor->stateTimer = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

static void func_actor_800200_801637B4(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            flag;
    s32            mode;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            flag                  = 1;
            actor->stateAux       = flag;
            actor->destination.vx = D_actor_800200_8016A098[2].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A098[2].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->destination.vx = D_actor_800200_8016A098[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A098[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 2) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                companion->waypointIndex++;
                func_actor_800200_80165534(arg0);
                return;
            }
            mode = 6;
            if (companion->waypointIndex == 1) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        default:
            return;
    }
}

static void func_actor_800200_8016390C(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            flag;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            flag                  = 1;
            actor->stateAux       = flag;
            actor->destination.vx = D_actor_800200_8016A0B0[2].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A0B0[2].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->destination.vx = D_actor_800200_8016A0B0[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A0B0[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 2) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                companion->waypointIndex++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163A54(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    s32            flag;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    companion = actor->companionWork;
    switch (actor->stateAux) {
        case 0:
            flag                  = 1;
            actor->stateAux       = flag;
            actor->destination.vx = D_actor_800200_8016A0C8[2].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A0C8[2].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
            func_actor_800200_80165534(arg0);
            break;
        case 1:
            actor->destination.vx = D_actor_800200_8016A0C8[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A0C8[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 2) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    break;
                }
                companion->waypointIndex = companion->waypointIndex + 1;
                func_actor_800200_80165534(arg0);
                break;
            }
            func_actor_800200_80165408(arg0, 6);
            break;
    }
}

static void func_actor_800200_80163B90(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            flag;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            flag                  = 1;
            actor->stateAux       = flag;
            actor->destination.vx = D_actor_800200_8016A0E0[4].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A0E0[4].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
            func_actor_800200_80165534(arg0);
            return;
        case 1:
            actor->destination.vx = D_actor_800200_8016A0E0[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A0E0[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 4) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                companion->waypointIndex++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163CCC(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            flag;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            flag                  = 1;
            actor->stateAux       = flag;
            actor->destination.vx = D_actor_800200_8016A108[3].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A108[3].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->destination.vx = D_actor_800200_8016A108[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A108[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 3) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                companion->waypointIndex++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163E14(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u16            state;
    s32            flag;
    s32            mode;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            flag                  = 1;
            actor->stateAux       = flag;
            actor->destination.vx = D_actor_800200_8016A130[4].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A130[4].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->destination.vx = D_actor_800200_8016A130[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A130[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x201) {
                if (companion->waypointIndex == 4) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    func_actor_800200_80165534(arg0);
                    return;
                }
                companion->waypointIndex++;
                func_actor_800200_80165534(arg0);
                return;
            }
            mode = 6;
            if (companion->waypointIndex == 1) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        default:
            return;
    }
}

static void func_actor_800200_80163F5C(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      target;
    VECTOR3*       vec;
    GameActor*     hit;
    s32            mode;
    s32            dist;
    s32            angle;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor  = arg0->work;
    dist   = _actor800200GetContactDistance(coord, actor->companionWork->probe.contacts, NULL);
    if (dist != 0 && dist < 0x301) {
        hit                      = arg0->work;
        companion                = hit->companionWork;
        hit->state               = 0xA;
        hit->turnRateIndex       = 2;
        hit->mode                = GAME_ACTOR_MODE_NORMAL;
        hit->animationState      = 0;
        hit->statePhase          = 0;
        hit->movementSign        = 0;
        hit->turnSign            = 0;
        companion->scanClearance = COMPANION_SCAN_UNTESTED;
        companion->scanAngle     = 0;
        playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 3);
        return;
    }
    switch (actor->statePhase) {
        case 0:
            actor->stateTimer = 0;
            if (func_8010BC70(coord) >= 0xE00) {
                mode                = 4;
                actor->statePhase   = 2;
                actor->movementMode = 6;
            } else {
            resume:
                if (actor->statePhase != 3) {
                    actor->statePhase = 1;
                }
                actor->movementMode = 5;
                mode                = 2;
            }
            actor->movementSign = 1;
            playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
        case 1:
        case 2:
        case 3:
            dist = func_8010BC70(coord);
            if (dist < 0x301) {
                Gp_ResetActorMove(arg0, 0);
                break;
            }
            if (actor->statePhase == 3) {
                break;
            }
            actor->stateTimer++;
            if (actor->stateTimer == 0xF0) {
                actor->statePhase = 3;
                goto resume;
            }
            angle = rand() & 0x3FF;
            if ((0x800 - angle) < dist) {
                goto in_range;
            }
            if (actor->statePhase == 2) {
                goto reset;
            }
        in_range:
            if (dist < angle + 0xC00) {
                break;
            }
            if (actor->statePhase != 1) {
                break;
            }
        reset:
            actor->statePhase = 0;
            break;
        default:
            break;
    }
    vec = (VECTOR3*)&target->coord.t[0];
    func_8010BD88(arg0, vec);
    func_8010BE5C(arg0, vec);
}

static void func_actor_800200_80164180(Task* arg0)
{
    GameActor*       actor;
    CompanionWork*   companion;
    GfxCoord*        target;
    WorldTargetNode* node;
    u8*              head;
    u8*              tmp;
    VECTOR3*         vec;
    GameActor*       actor2;
    s32              dist;
    s32              anim;
    u16              flag;

    actor                    = arg0->work;
    companion                = actor->companionWork;
    target                   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    tmp                      = head - 0x10;
    SCRATCH_STACK_CURSOR(u8) = tmp;
    vec                      = (VECTOR3*)tmp;
    node                     = actor->targetNode;
    if (node != NULL) {
        if (!(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            Gp_GetLockPos(node, vec);
        } else {
            actor->statePhase = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    switch (actor->statePhase) {
        case 0:
            actor->statePhase   = 1;
            actor->movementMode = 5;
            actor->movementSign = 1;
            if (func_8010BCF4(arg0, vec) < 0) {
                actor->turnSign = -1;
                anim            = 5;
            } else {
                actor->turnSign = 1;
                anim            = 6;
            }
            playerActorPlayChildSlotsWithBlend(arg0, anim, 1, 5);
        case 1:
        case 2:
            dist = func_8010BCF4(arg0, vec);
            if (dist < 0) {
                dist = -dist;
            }
            if ((dist < 0x101) || (actor->statePhase == 2)) {
                if ((s8)companion->activity.combat.repeatsRemaining > 0) {
                    flag                                = actor->targetNode != 0;
                    actor2                              = arg0->work;
                    actor2->mode                        = GAME_ACTOR_MODE_NORMAL;
                    actor2->state                       = 4;
                    actor2->movementMode                = 0;
                    actor2->turnRateIndex               = 0;
                    actor2->animationState              = 0;
                    actor2->statePhase                  = 0;
                    actor2->attackControl.targetVariant = flag;
                } else {
                    Gp_ResetActorMove(arg0, 0);
                }
            }
            break;
        default:
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)&target->coord.t[0]);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800200_8016436C(Task* arg0)
{
    GameActor*       actor;
    CompanionWork*   companion;
    GfxCoord*        target;
    WorldTargetNode* node;
    u8*              tmp;
    GfxCoord*        coord;
    VECTOR3*         vec;
    u8*              head;
    void**           scratch;
    s8               count;
    s32              pan;
    s32              dist;
    u16              state;
    s32              next = 1;
    GameActor*       actor2;

    actor                    = arg0->work;
    companion                = actor->companionWork;
    target                   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    tmp                      = head - 0x10;
    SCRATCH_STACK_CURSOR(u8) = tmp;
    vec                      = (VECTOR3*)tmp;
    coord                    = arg0->extra.tmd->coords;
    if (actor->targetNode != NULL) {
        node              = Gp_FindLockNode(arg0);
        actor->targetNode = node;
        if ((node != NULL) && !(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            Gp_GetLockPos(node, vec);
        } else {
            companion->activity.combat.repeatsRemaining = 1;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    state = actor->statePhase;
    if (state != 0) {
        if (state != 1) {
            scratch = SCRATCH_HEAD_ADDR;
        } else {
            goto tick;
        }
    } else {
        actor->statePhase = next;
        playerActorPlayChildSlotsWithBlend(arg0, actor->attackControl.targetVariant + 0xA, 0, 4);
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(actor->attackControl.targetVariant + 0x40720009, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    tick:
        if (func_80105894(arg0, 1, 0, 0) == 0) {
            dist = func_8010BCF4(arg0, vec);
            if (dist < 0) {
                dist = -dist;
            }
            if ((dist >= 0x281) && (func_80103DD4(MATRIX_TRANS(&coord->coord), vec) >= 0x201)) {
                actor2                 = arg0->work;
                actor2->mode           = GAME_ACTOR_MODE_NORMAL;
                actor2->state          = 2;
                actor2->turnRateIndex  = 2;
                actor2->animationState = 0;
                actor2->statePhase     = 0;
            } else {
                count                                       = companion->activity.combat.repeatsRemaining - 1;
                companion->activity.combat.repeatsRemaining = count;
                if (count <= 0) {
                    Gp_ResetActorMove(arg0, 0);
                } else {
                    actor->statePhase = 0;
                }
            }
        }
        scratch = SCRATCH_HEAD_ADDR;
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

static void func_actor_800200_80164598(Task* arg0)
{
    PlayerActorApproachScratch* block;
    GfxCoord*                   coord;
    GameActor*                  actor;
    s32                         val;
    s32                         mode;
    s32                         flag;

    actor                         = arg0->work;
    coord                         = arg0->extra.tmd->coords;
    block                         = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorApproachScratch);
    block->targetDelta.vx         = actor->destination.vx - coord->coord.t[0];
    block->targetDelta.vy         = actor->destination.vy - coord->coord.t[1];
    block->targetDelta.vz         = actor->destination.vz - coord->coord.t[2];
    actor->scriptMotion.targetYaw = ratan2(block->targetDelta.vx, block->targetDelta.vz);
    val                           = func_80103E7C(actor->rotation.vy, actor->scriptMotion.targetYaw);
    block->turnStep               = val;
    if (val > 0x30) {
        block->turnStep = 0x30;
    } else if (val < -0x30) {
        block->turnStep = -0x30;
    } else if (actor->statePhase == 0) {
        actor->statePhase = 1;
    }
    actor->rotation.vy = (actor->rotation.vy + block->turnStep) & 0xFFF;
    switch (actor->statePhase) {
        case 0:
            flag                = 1;
            actor->statePhase   = flag;
            actor->movementSign = flag;
            mode                = 6;
            if (block->turnStep < 0) {
                mode = 5;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            if (block->turnStep == 0) {
                actor->movementMode = actor->stateTimer;
                actor->statePhase++;
                mode = 4;
                if ((u16)actor->stateTimer == 5) {
                    mode = 2;
                }
                playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
            }
            break;
        case 2:
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0xC1 ||
                func_801041B4(arg0) != 0) {
                Gp_ResetActorMove(arg0, 0);
            } else {
                actor->movementSign = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorApproachScratch);
}

static void func_actor_800200_801647A8(Task* arg0)
{
    GameActor*       actor;
    GameActor*       actor2;
    GameActor*       actor3;
    GfxCoord*        coord;
    GfxCoord*        target;
    WorldTargetNode* node;
    VECTOR3*         vec;
    u8*              head;
    u8*              tmp;
    s32              dist;
    s32              value;
    u16              flag;
    u16              state;
    s32              next;
    s32              initialState;

    target                   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    tmp                      = head - 0x10;
    SCRATCH_STACK_CURSOR(u8) = tmp;
    vec                      = (VECTOR3*)tmp;
    actor                    = arg0->work;
    coord                    = arg0->extra.tmd->coords;
    state                    = actor->statePhase;
    switch (state) {
        case 0:
            initialState      = 1;
            actor->statePhase = initialState;
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                node              = Gp_FindLockNode(arg0);
                actor->targetNode = node;
                if ((node != NULL) && !(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
                    Gp_GetLockPos(node, vec);
                    dist = func_80103DD4(MATRIX_TRANS(&coord->coord), vec);
                    dist = dist / 640;
                    if (dist >= 8) {
                        dist = 7;
                    }
                    value = (rand() & 0x7F) + (dist << 5);
                    goto store;
                }
            }
            dist = func_8010BC70(coord);
            dist = dist / 640;
            if (dist >= 8) {
                dist = 7;
            }
            value             = (rand() & 0x1FF) - (dist * 0x30);
            actor->stateTimer = value;
            if (value < 0x60) {
                value = 0x60;
            store:
                actor->stateTimer = value;
            }
        case 1:
            value             = actor->stateTimer - 1;
            actor->stateTimer = value;
            if (value <= 0) {
                actor2                                                  = arg0->work;
                next                                                    = 1;
                actor2->companionWork->activity.combat.repeatsRemaining = next;
                if (gSceneCombatState.signals.bytes.battlePhase == next) {
                    actor2->targetNode = Gp_FindLockNode(arg0);
                } else {
                    actor2->targetNode = NULL;
                }
                flag                                = actor2->targetNode != 0;
                actor3                              = arg0->work;
                actor3->mode                        = GAME_ACTOR_MODE_NORMAL;
                actor3->state                       = 4;
                actor3->movementMode                = 0;
                actor3->turnRateIndex               = 0;
                actor3->animationState              = 0;
                actor3->statePhase                  = 0;
                actor3->attackControl.targetVariant = flag;
            }
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)&target->coord.t[0]);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800200_801649D8(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    s32            distance;
    s32            one;
    s32            turnAnim;
    s32            turn;
    s32            idleAnim;
    s32            delta;
    s32            value;
    s32            tick;
    s32            heading;
    u32            random;
    u16            target;

    actor     = arg0->work;
    companion = actor->companionWork;
    distance  = _actor800200GetContactDistance(arg0->extra.tmd->coords, companion->probe.contacts, 0);
    value     = actor->statePhase;
    one       = 1;
    switch (value) {
        case 0:
            if (companion->scanAngle < ACTOR_TRANSFORM_ANGLE_TURN) {
                // Retain the best clearance until the sweep has completed.
                if ((companion->scanClearance != COMPANION_SCAN_CLEAR) && ((companion->scanClearance < distance) || (distance == COMPANION_SCAN_CLEAR))) {
                    companion->scanClearance = (s16)distance;
                    companion->targetHeading = (u16)companion->scanAngle;
                }
                companion->scanAngle = (u16)companion->scanAngle + COMPANION_SCAN_ANGLE_STEP;
                return;
            }
            actor->statePhase        = one;
            target                   = ((u16)companion->targetHeading + actor->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
            companion->targetHeading = target;
            turn                     = func_80103E7C((s16)actor->rotation.vy, (s16)target) << 0x10;
            turnAnim                 = 5;
            if (turn > 0) {
                turnAnim           = 6;
                companion->turnDir = 1;
            } else {
                companion->turnDir = -1;
            }
            playerActorPlayChildSlotsWithBlend(arg0, turnAnim, 0, 3);
            return;

        case 1:
            actor->turnSign = companion->turnDir;
            do {
                heading = (s16)actor->rotation.vy;
                value   = companion->targetHeading;
                delta   = heading - value;
            } while (0);
            if (delta < 0) {
                delta = -delta;
            }
            if (delta < 0x40) {
                actor->statePhase += 1;
                actor->rotation.vy = (u16)companion->targetHeading;
                actor->turnSign    = 0;
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    idleAnim            = 4;
                    actor->movementMode = 6;
                    random              = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = random;
                    random              = ((random >> 0x10) & 0x1F) + 0x14;
                    actor->stateTimer   = random;
                    playerActorPlayChildSlotsWithBlend(arg0, idleAnim, 0, 3);
                } else {
                    idleAnim            = 2;
                    actor->movementMode = 5;
                    random              = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = random;
                    random              = ((random >> 0x10) & 0x7F) + 0x3C;
                    actor->stateTimer   = random;
                    playerActorPlayChildSlotsWithBlend(arg0, idleAnim, 0, 3);
                }
                return;
            }
            return;

        case 2:
            if ((distance >= 0x341) || (distance == 0)) {
                tick              = actor->stateTimer - 1;
                actor->stateTimer = tick;
                if (tick <= 0) {
                reset:
                    Gp_ResetActorMove(arg0, 0);
                    break;
                }
                actor->movementSign = 1;
                break;
            }
            goto reset;
    }
}

static void func_actor_800200_80164C54(Task* arg0)
{
    PlayerActorApproachScratch* block;
    GfxCoord*                   coord;
    GameActor*                  actor;
    s32                         val;
    s32                         mode;

    actor                         = arg0->work;
    coord                         = arg0->extra.tmd->coords;
    block                         = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorApproachScratch);
    block->targetDelta.vx         = actor->destination.vx - coord->coord.t[0];
    block->targetDelta.vy         = actor->destination.vy - coord->coord.t[1];
    block->targetDelta.vz         = actor->destination.vz - coord->coord.t[2];
    actor->scriptMotion.targetYaw = ratan2(block->targetDelta.vx, block->targetDelta.vz);
    val                           = func_80103E7C(actor->rotation.vy, actor->scriptMotion.targetYaw);
    block->turnStep               = val;
    if (val > 0x40) {
        block->turnStep = 0x40;
    } else if (val < -0x40) {
        block->turnStep = -0x40;
    } else if (actor->statePhase == 0) {
        actor->statePhase = 1;
    }
    actor->rotation.vy = (actor->rotation.vy + block->turnStep) & 0xFFF;
    switch (actor->statePhase) {
        case 0:
            actor->statePhase = 1;
            mode              = 6;
            if (block->turnStep < 0) {
                mode = 5;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            if (block->turnStep == 0) {
                actor->movementMode = 5;
                actor->statePhase++;
                if (actor->actionArgument == 0) {
                    mode = 2;
                    if (actor->equipmentTasks[1] == NULL) {
                        mode = 0x13;
                    }
                } else {
                    mode = actor->actionArgument;
                }
                playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
            }
            break;
        case 2:
            if (abs(coord->coord.t[0] - actor->destination.vx) < 0x69) {
                if (abs(coord->coord.t[2] - actor->destination.vz) < 0x69) {
                    actor->scriptedMotionPending = 0;
                    actor->state                 = 1;
                    mode                         = 1;
                    if (actor->actionValue != 0) {
                        mode = actor->actionValue;
                    }
                    playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
                    break;
                }
            }
            actor->movementSign = 1;
            Gp_StepPlayerMove(arg0);
            func_80105ED4(arg0);
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorApproachScratch);
}

static void func_actor_800200_80164EBC(Task* arg0)
{
    PlayerActorApproachScratch* block;
    GfxCoord*                   coord;
    GameActor*                  actor;
    s32                         val;
    s32                         mode;

    actor                         = arg0->work;
    coord                         = arg0->extra.tmd->coords;
    block                         = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorApproachScratch);
    block->targetDelta.vx         = actor->destination.vx - coord->coord.t[0];
    block->targetDelta.vy         = actor->destination.vy - coord->coord.t[1];
    block->targetDelta.vz         = actor->destination.vz - coord->coord.t[2];
    actor->scriptMotion.targetYaw = ratan2(block->targetDelta.vx, block->targetDelta.vz);
    val                           = func_80103E7C(actor->rotation.vy, actor->scriptMotion.targetYaw);
    block->turnStep               = val;
    if (val > 0x40) {
        block->turnStep = 0x40;
    } else if (val < -0x40) {
        block->turnStep = -0x40;
    } else if (actor->statePhase == 0) {
        actor->statePhase = 1;
    }
    actor->rotation.vy = (actor->rotation.vy + block->turnStep) & 0xFFF;
    switch (actor->statePhase) {
        case 0:
            actor->statePhase = 1;
            mode              = 6;
            if (block->turnStep < 0) {
                mode = 5;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            if (block->turnStep == 0) {
                actor->movementMode = 6;
                actor->statePhase++;
                mode = 4;
                if (actor->actionArgument != 0) {
                    mode = actor->actionArgument;
                }
                playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
            }
            break;
        case 2:
            if (abs(coord->coord.t[0] - actor->destination.vx) < 0x69) {
                if (abs(coord->coord.t[2] - actor->destination.vz) < 0x69) {
                    actor->scriptedMotionPending = 0;
                    actor->state                 = 1;
                    mode                         = 1;
                    if (actor->actionValue != 0) {
                        mode = actor->actionValue;
                    }
                    playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
                    break;
                }
            }
            actor->movementSign = 1;
            Gp_StepPlayerMove(arg0);
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorApproachScratch);
}

/// Handlers `func_actor_800200_80165B84` runs, indexed by `state`.
static const TaskFuncTable12 D_actor_800200_80161E5C = { {
    func_actor_800200_80165CB4,
    func_actor_800200_80163F5C,
    func_actor_800200_80164180,
    func_actor_800200_80165CB4,
    func_actor_800200_8016436C,
    func_actor_800200_80165CB4,
    func_actor_800200_80165CB4,
    func_actor_800200_80165D44,
    func_actor_800200_80164598,
    func_actor_800200_801647A8,
    func_actor_800200_801649D8,
    func_actor_800200_80165E50,
} };

/// Handlers `func_actor_800200_80165CB4` runs, indexed by the low nibble of
/// the task's `spawnArg1`.
static const TaskFuncTable11 D_actor_800200_80161E8C = { {
    func_actor_800200_80162750,
    func_actor_800200_80165580,
    func_actor_800200_80162750,
    func_actor_800200_80162E0C,
    func_actor_800200_80162750,
    func_actor_800200_80162750,
    func_actor_800200_80162750,
    func_actor_800200_80165644,
    func_actor_800200_80165708,
    func_actor_800200_80165708,
    func_actor_800200_80165708,
} };

/// Handlers `func_actor_800200_80165E90` runs, indexed by `hitRegion`.
static const TaskFuncTable4 D_actor_800200_80161EB8 = { {
    func_actor_800200_80165F28,
    func_actor_800200_80165F28,
    func_actor_800200_80165F28,
    func_actor_800200_80165F48,
} };

/// Handlers `func_actor_800200_80165F50` runs, indexed by `state`; the
/// gameplay entries are the player's own mode-2 state handlers.
static const TaskFuncTable9 D_actor_800200_80161EC8 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    func_actor_800200_80165FF0,
    Gp_PlayerMode2State1,
    func_actor_800200_80164C54,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State1,
    func_actor_800200_80164EBC,
} };

static s32 func_actor_800200_80165104(Task* arg0)
{
    GameActor*                          actor;
    const AnimationRecord*              rec;
    GfxCoord*                           obj;
    WorldCollisionSurfaceProperties*    surface;
    const WorldCollisionFootstepSounds* footstepSounds;
    s32                                 ret;
    s32                                 sound;
    s8                                  cueBits;
    s32                                 pan;

    ret   = 0;
    sound = WORLD_COLLISION_FOOTSTEP_SILENT;
    actor = arg0->work;
    obj   = arg0->extra.tmd->coords;
    rec   = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
    if (rec != NULL && rec != actor->lastCueRecord) {
        actor->lastCueRecord = rec;
        switch (cueBits = rec->flags & ANIMATION_RECORD_CUE_MASK) {
            case ANIMATION_RECORD_CUE_1:
            case ANIMATION_RECORD_CUE_2:
                surface        = Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][actor->surfaceClass];
                footstepSounds = surface->footstepSounds;
                if (footstepSounds != NULL) {
                    if ((u16)actor->movementMode - 5 < 2U) {
                        switch (footstepSounds->walk) {
                            case 0x10000015:
                                sound = 0x40720007;
                                break;
                            case 0x1000002D:
                                sound = 0x40720003;
                                break;
                            case 0x1000001D:
                            case 0x10000049:
                                sound = 0x40720001;
                                break;
                            case 0x1000003D:
                            case 0x10000041:
                            case 0x10000051:
                            case 0x10000059:
                            case 0x1000005D:
                                sound = 0x40720005;
                                break;
                        }
                        if (cueBits == ANIMATION_RECORD_CUE_1) {
                            sound++;
                        }
                        if ((u16)actor->movementMode == 6) {
                            Gp_SetStateF0Bit(5);
                        }
                    }
                    if (sound != WORLD_COLLISION_FOOTSTEP_SILENT) {
                        pan = (s8)worldCoordGetOriginAudioPan(obj);
                        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(obj));
                        func_800EA3A0(cueBits != ANIMATION_RECORD_CUE_2);
                    }
                }
                ret = 1;
                break;
        }
    }
    return ret;
}

static void func_actor_800200_801652EC(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable3 sp;

    sp    = D_actor_800200_80161E34;
    actor = arg0->work;
    if ((s8)actor->recoveryTicks > 0) {
        actor->recoveryTicks--;
    }
    sp.funcs[actor->mode](arg0);
    func_actor_800200_80165104(arg0);
    actor->usesPushbackDirection = 0;
}

static void func_actor_800200_80165380(Task* arg0)
{
    GameActor* actor = arg0->work;

    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = 1;
    actor->turnRateIndex  = 0;
    actor->animationState = 0;
    actor->statePhase     = 0;
}

static void func_actor_800200_801653A0(Task* arg0)
{
    GameActor* actor = arg0->work;

    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = 2;
    actor->turnRateIndex  = 2;
    actor->animationState = 0;
    actor->statePhase     = 0;
}

static void func_actor_800200_801653C0(Task* arg0)
{
    GameActor* actor = arg0->work;

    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = 7;
    actor->movementMode   = 0;
    actor->turnRateIndex  = 0;
    actor->animationState = 7;
    actor->statePhase     = 0;
    playerActorPlayChildSlotsWithBlend(arg0, 7, 0, 3);
}

static void func_actor_800200_80165408(Task* arg0, s32 arg1)
{
    GameActor* actor = arg0->work;

    actor->state          = 8;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = 5;
    actor->turnRateIndex  = 0;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->stateTimer     = arg1;
}

static void func_actor_800200_80165434(Task* arg0, s16 arg1)
{
    GameActor* actor = arg0->work;

    actor->mode                        = GAME_ACTOR_MODE_NORMAL;
    actor->state                       = 4;
    actor->movementMode                = 0;
    actor->turnRateIndex               = 0;
    actor->animationState              = 0;
    actor->statePhase                  = 0;
    actor->attackControl.targetVariant = arg1;
}

static void func_actor_800200_8016545C(Task* arg0, s8 arg1)
{
    GameActor* actor = arg0->work;
    GameActor* actor2;
    u16        flag;

    actor->companionWork->activity.combat.repeatsRemaining = arg1;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        actor->targetNode = Gp_FindLockNode(arg0);
    } else {
        actor->targetNode = 0;
    }
    flag                                = actor->targetNode != 0;
    actor2                              = arg0->work;
    actor2->mode                        = GAME_ACTOR_MODE_NORMAL;
    actor2->state                       = 4;
    actor2->movementMode                = 0;
    actor2->turnRateIndex               = 0;
    actor2->animationState              = 0;
    actor2->statePhase                  = 0;
    actor2->attackControl.targetVariant = flag;
}

static void func_actor_800200_801654EC(Task* arg0, s32 arg1)
{
    GameActor* actor = arg0->work;

    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = 9;
    actor->movementMode   = 0;
    actor->turnRateIndex  = 0;
    actor->animationState = 0;
    actor->statePhase     = 0;
    playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 3);
}

static void func_actor_800200_80165534(Task* arg0)
{
    GameActor* actor = arg0->work;

    actor->state          = 0xB;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = 0;
    actor->turnRateIndex  = 0;
    actor->animationState = 7;
    actor->statePhase     = 0;
    playerActorPlayChildSlotsWithBlend(arg0, 0xE, 0, 3);
}

static void func_actor_800200_80165580(Task* arg0)
{
    u8 temp_v1;

    if (((GameActor*)arg0->work)->companionWork->routeComplete == COMPANION_ROUTE_COMPLETE) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v1 = gGameSession->location.loc.area;
    switch (temp_v1) {
        case 26:
            func_actor_800200_80162990(arg0);
            return;
        case 24:
            func_actor_800200_80165814(arg0);
            return;
        case 23:
            func_actor_800200_80162BFC(arg0);
            return;
        case 25:
            func_actor_800200_801658E0(arg0);
            return;
    }
}

static void func_actor_800200_80165644(Task* arg0)
{
    u8 temp_v1;

    if (((GameActor*)arg0->work)->companionWork->routeComplete == COMPANION_ROUTE_COMPLETE) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v1 = gGameSession->location.loc.area;
    switch (temp_v1) {
        case 25:
            func_actor_800200_8016599C(arg0);
            return;
        case 23:
            func_actor_800200_80163044(arg0);
            return;
        case 22:
            func_actor_800200_80163180(arg0);
            return;
        case 20:
            func_actor_800200_8016337C(arg0);
            return;
    }
}

static void func_actor_800200_80165708(Task* arg0)
{
    u8 temp_v0;

    if (((GameActor*)arg0->work)->companionWork->routeComplete == COMPANION_ROUTE_COMPLETE) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v0 = gGameSession->location.loc.area;
    switch (temp_v0) {
        case 1:
            func_actor_800200_80163A54(arg0);
            return;
        case 2:
            func_actor_800200_801637B4(arg0);
            return;
        case 3:
            func_actor_800200_801659CC(arg0);
            return;
        case 4:
            func_actor_800200_80163584(arg0);
            return;
        case 5:
            func_actor_800200_8016390C(arg0);
            return;
        case 15:
            func_actor_800200_80163E14(arg0);
            return;
        case 19:
            func_actor_800200_80163CCC(arg0);
            return;
        case 20:
            func_actor_800200_80163B90(arg0);
            return;
        case 24:
            func_actor_800200_80165ACC(arg0);
            return;
    }
}

static void func_actor_800200_80165814(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    s32            arg;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    companion = actor->companionWork;
    if (actor->stateAux == 0) {
        actor->destination.vx = D_actor_800200_8016A018[companion->waypointIndex].x;
        actor->destination.vy = coord->coord.t[1];
        actor->destination.vz = D_actor_800200_8016A018[companion->waypointIndex].z;
        if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
            companion->routeComplete = COMPANION_ROUTE_COMPLETE;
            func_actor_800200_801654EC(arg0, 0);
            return;
        }
        arg = 6;
        if (companion->waypointIndex == 2) {
            arg = 5;
        }
        func_actor_800200_80165408(arg0, arg);
    }
}

static void func_actor_800200_801658E0(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    companion = actor->companionWork;
    if (actor->stateAux == 0) {
        actor->destination.vx = D_actor_800200_8016A040[companion->waypointIndex].x;
        actor->destination.vy = coord->coord.t[1];
        actor->destination.vz = D_actor_800200_8016A040[companion->waypointIndex].z;
        if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
            companion->routeComplete = COMPANION_ROUTE_COMPLETE;
            func_actor_800200_801654EC(arg0, 0);
            return;
        }
        func_actor_800200_80165408(arg0, 6);
    }
}

static void func_actor_800200_8016599C(Task* arg0)
{
    ((GameActor*)arg0->work)->companionWork->routeComplete = COMPANION_ROUTE_COMPLETE;
    func_actor_800200_801654EC(arg0, 0);
}

static void func_actor_800200_801659CC(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    u32            state;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    state     = actor->stateAux;
    companion = actor->companionWork;
    switch (state) {
        case 0:
            actor->destination.vx = D_actor_800200_8016A090[companion->waypointIndex].x;
            actor->destination.vy = coord->coord.t[1];
            actor->destination.vz = D_actor_800200_8016A090[companion->waypointIndex].z;
            if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
                actor->stateAux++;
                if (companion->routeComplete != COMPANION_ROUTE_COMPLETE) {
                    func_actor_800200_80165534(arg0);
                }
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 1:
            companion->routeComplete = state;
            func_actor_800200_801654EC(arg0, 0);
            break;
    }
}

static void func_actor_800200_80165ACC(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;

    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    companion = actor->companionWork;
    if (actor->stateAux == 0) {
        actor->destination.vx = D_actor_800200_8016A128[companion->waypointIndex].x;
        actor->destination.vy = coord->coord.t[1];
        actor->destination.vz = D_actor_800200_8016A128[companion->waypointIndex].z;
        if (func_80103DD4(MATRIX_TRANS(&coord->coord), &actor->destination) < 0x401) {
            companion->routeComplete = COMPANION_ROUTE_COMPLETE;
            func_actor_800200_80165534(arg0);
            return;
        }
        func_actor_800200_80165408(arg0, 6);
    }
}

static void func_actor_800200_80165B84(Task* arg0)
{
    GameActor*      actor;
    CompanionWork*  companion;
    GfxCoord*       coord;
    TaskFuncTable12 sp;
    s32             pan;

    sp        = D_actor_800200_80161E5C;
    actor     = arg0->work;
    companion = actor->companionWork;
    coord     = arg0->extra.tmd->coords;
    if (companion->decisionTimer > 0) {
        companion->decisionTimer--;
    }
    sp.funcs[actor->state](arg0);
    if ((s8)actor->recoveryTicks == 0) {
        func_80109BB4(arg0, actor->collisionContacts);
        if ((u16)actor->hitRegion != 0) {
            func_8010B9A4(arg0);
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(SOUND_ACTOR_800200_HURT, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
    }
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

static void func_actor_800200_80165CB4(Task* arg0)
{
    TaskFuncTable11 sp;

    sp = D_actor_800200_80161E8C;
    sp.funcs[arg0->spawnArg1.value & 0xF](arg0);
}

static void func_actor_800200_80165D44(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  target;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor  = arg0->work;
    switch (actor->statePhase) {
        case 1:
            actor->animationState = 0;
            actor->statePhase    += 1;
            Gp_AnimResetChildSlots(arg0, 9);
        case 2:
            if ((func_8010BC70(coord) >= 0x500) || (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED)) {
                actor->animationState = 7;
                actor->statePhase    += 1;
                playerActorPlayChildSlotsWithBlend(arg0, 8, 0, 3);
            }
            break;
        case 4:
            Gp_ResetActorMove(arg0, 0);
            break;
        default:
        case 0:
        case 3:
            break;
    }
    func_8010BE5C(arg0, MATRIX_TRANS(&target->coord));
}

static void func_actor_800200_80165E50(Task* arg0)
{
    u16 state = ((GameActor*)arg0->work)->statePhase;

    if (state != 0) {
        if (state == 1) {
            Gp_ResetActorMove(arg0, 0);
        }
    }
}

static void func_actor_800200_80165E90(Task* arg0)
{
    TaskFuncTable4 handlers;
    GameActor*     actor;

    handlers = D_actor_800200_80161EB8;
    actor    = arg0->work;
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    handlers.funcs[(u16)actor->hitRegion](arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

static void func_actor_800200_80165F28(Task* arg0)
{
    func_8010ABD4(arg0);
}

static void func_actor_800200_80165F48(Task* arg0)
{
}

static void func_actor_800200_80165F50(Task* arg0)
{
    TaskFuncTable9 sp;
    GameActor*     actor;
    GfxCoord*      coord;

    sp    = D_actor_800200_80161EC8;
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    sp.funcs[actor->state](arg0);
    RotMatrix(&actor->rotation, &coord->coord);
}

static void func_actor_800200_80165FF0(Task* arg0)
{
    GameActor* actor;
    s16        cur;
    s16        tgt;
    u16        raw;
    s32        temp;
    s32        wrap;
    s32        delta;
    s32        flag;

    actor = arg0->work;
    cur   = actor->rotation.vy;
    tgt   = actor->scriptMotion.targetYaw;
    raw   = actor->scriptMotion.targetYaw;
    temp  = cur - tgt;
    if (temp < 0) {
        temp = -temp;
    }
    if (temp < 0x31 || (wrap = tgt - 0x1000, temp = cur - wrap, temp = ABS(temp), temp < 0x31)) {
        flag                         = 1;
        actor->rotation.vy           = raw;
        actor->scriptedMotionPending = 0;
        actor->state                 = flag;
        playerActorPlayChildSlotsWithBlend(arg0, flag, 0, 5);
    } else {
        delta = func_80103E7C(cur, tgt);
        if (delta > 0x30) {
            delta = 0x30;
        } else if (delta < -0x30) {
            delta = -0x30;
        }
        actor->movementMode = 5;
        actor->movementSign = 1;
        actor->rotation.vy  = ((u16)actor->rotation.vy + delta) & 0xFFF;
    }
    Gp_AnimTickChildSlots(arg0);
}

/// Planar distance from the coordinate origin to a nonempty contact, or 0.
///
/// Distances use world units. Optional `contactZY` needs two halfwords and
/// receives the signed coordinate bits (Z, Y). The original X, Y, Z stores
/// deliberately retain their order, with Z overwriting X.
static s32 _actor800200GetContactDistance(GfxCoord* coord, WorldCollisionContact* contact, u16* contactZY)
{
    s32 distance;

    if (contact->key.value != 0) {
        distance = func_80103D8C(coord->workm.t[0] - contact->point.vx, coord->workm.t[2] - contact->point.vz);
        if (contactZY != NULL) {
            // Retain the original repeated first-halfword write.
            contactZY[0] = contact->point.vx;
            contactZY[1] = contact->point.vy;
            contactZY[0] = contact->point.vz;
        }
    } else {
        distance = 0;
    }
    return distance;
}
