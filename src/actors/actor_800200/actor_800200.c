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

#include "main/areas.h"
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

/// Task teardown, clearance behavior and native animation selections.
enum {
    ACTOR_800200_TASK_TEARDOWN           = 3,
    ACTOR_800200_STATE_CLEARANCE_ESCAPE  = 10,
    ACTOR_800200_ANIMATION_WALK          = 2,
    ACTOR_800200_ANIMATION_RUN           = 4,
    ACTOR_800200_ANIMATION_TURN_NEGATIVE = 5,
    ACTOR_800200_ANIMATION_TURN_POSITIVE = 6,
    ACTOR_800200_ANIMATION_REST_EXIT     = 8,
    ACTOR_800200_ANIMATION_ACTION_BASE   = 10,
    ACTOR_800200_MOVEMENT_BLEND_FRAMES   = 5
};

/// Normal behavior selectors and animation choices used by the route entries.
enum {
    ACTOR_800200_STATE_PLAYER_FOLLOW       = 1,
    ACTOR_800200_STATE_REST                = 7,
    ACTOR_800200_ANIMATION_REST            = 7,
    ACTOR_800200_ANIMATION_REST_HOLD       = 9,
    ACTOR_800200_ACTION_PLAYER             = 0,
    ACTOR_800200_ACTION_TARGET             = 1,
    ACTOR_800200_STATE_TARGET_TURN         = 2,
    ACTOR_800200_STATE_ACTION_BURST        = 4,
    ACTOR_800200_STATE_APPROACH            = 8,
    ACTOR_800200_STATE_TIMED_WAIT          = 9,
    ACTOR_800200_STATE_ROUTE_ANIMATION     = 11,
    ACTOR_800200_MOVEMENT_STOPPED          = 0,
    ACTOR_800200_MOVEMENT_WALK             = 5,
    ACTOR_800200_MOVEMENT_RUN              = 6,
    ACTOR_800200_TURN_DISABLED             = 0,
    ACTOR_800200_TURN_TARGET               = 2,
    ACTOR_800200_ANIMATION_CONTROLLER_NONE = 0,
    ACTOR_800200_ANIMATION_CONTROLLER_ONCE = 7,
    ACTOR_800200_ANIMATION_IDLE            = 1,
    ACTOR_800200_ANIMATION_ROUTE           = 14,
    ACTOR_800200_ENTRY_BLEND_FRAMES        = 3
};

/// Common destination approach phases; the turn helper advances initial entry.
enum {
    ACTOR_800200_DESTINATION_START_PHASE  = 0,
    ACTOR_800200_DESTINATION_TURN_PHASE   = 1,
    ACTOR_800200_DESTINATION_TRAVEL_PHASE = 2
};

/// Persistent route phases and exclusive X/Z arrival limits in game units.
enum {
    ACTOR_800200_ROUTE_CHECK_END               = 0,
    ACTOR_800200_ROUTE_TRAVEL                  = 1,
    ACTOR_800200_ROUTE_WAIT_PLAYER             = 2,
    ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT  = 0x401,
    ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT = 0x201,
    ACTOR_800200_ROUTE_RANDOM_DELAY_MASK       = 0x7F
};

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

static void _actor800200TeardownTask(Task* task);
static void func_actor_800200_801652EC(Task* arg0);
static void _actor800200EnterPlayerFollow(Task* task);
static void _actor800200EnterTargetTurn(Task* task);
static void _actor800200EnterRest(Task* task);
static void _actor800200EnterApproach(Task* task, s32 movementMode);
static void _actor800200EnterActionBurst(Task* task, s16 targetVariant);
static void _actor800200StartActionBurst(Task* task, s8 repeatsRemaining);
static void _actor800200EnterTimedWait(Task* task, s32 unusedArgument);
static void _actor800200EnterRouteAnimation(Task* task);
static void _actor800200TickSchedule1Route(Task* task);
static void _actor800200TickSchedule7Route(Task* task);
static void _actor800200TickSchedules8To10Route(Task* task);
static void _actor800200TickArea24Route1(Task* task);
static void _actor800200TickArea25Route1(Task* task);
static void _actor800200CompleteArea25Route7(Task* task);
static void _actor800200TickArea3Route(Task* task);
static void _actor800200TickArea24Route10(Task* task);
static void _actor800200TickNormalMode(Task* task);
static void _actor800200TickScheduleIdle(Task* task);
static void _actor800200TickRest(Task* task);
static void _actor800200TickRouteAnimation(Task* task);
static void _actor800200TickDamageMode(Task* task);
static void _actor800200TickDamageRecovery(Task* task);
static void _actor800200HoldStoppedPose(Task* unusedTask);
static void _actor800200TickScriptedMode(Task* task);
static void _actor800200TickScriptedTurn(Task* task);
static s32  _actor800200GetContactDistance(const GfxCoord* coord, const WorldCollisionContact* contact, u16* contactZY);

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
    { ANIMATION_MESSAGE_PLAY, companionPlayScriptedAnimation },
    { 1002, companionPlayScriptedAnimation },
    { 1003, companionPlayScriptedAnimation },
    { 1004, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_PLACE, playerActorPlace },
    { ANIMATION_MESSAGE_IS_PLAYING, playerActorIsAnimationPlaying },
    { GAME_ACTOR_MESSAGE_TURN_TO_YAW, companionTurnToYaw },
    { GAME_ACTOR_MESSAGE_CLIMB_STAIRS, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, playerActorIsScriptedMotionPending },
    { GAME_ACTOR_MESSAGE_END_SCRIPTED, companionEndScriptedMotion },
    { GAME_ACTOR_MESSAGE_MOVE_TO, companionMoveTo },
    { GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, playerActorSetModelDraw },
    { ANIMATION_MESSAGE_INSTALL_AND_PLAY, companionInstallScriptedAnimation },
    { GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, playerActorAttachToCoord },
    { GAME_ACTOR_MESSAGE_WALK_STEPS, playerActorWalkSteps },
    { ANIMATION_MESSAGE_COPY_BANK_EXTENSION, animationCopyCompanionBankExtension },
    { GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_APPLY_DAMAGE, companionPlayScriptedAnimation },
    { 1018, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_RUN_TO, companionRunTo },
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

static void _actor800200InitTask(Task* task);
static void func_actor_800200_801622B0(Task* arg0);
static void _actor800200AdvanceToTeardown(Task* task);
static void _actor800200TickFreeIdle(Task* task);
static void _actor800200TickArea26Route(Task* task);
static void _actor800200TickArea23Route1(Task* task);
static void _actor800200TickSchedule3Route(Task* task);
static void _actor800200TickArea23Route7(Task* task);
static void _actor800200TickArea22Route(Task* task);
static void _actor800200TickArea20Route7(Task* task);
static void _actor800200TickArea4Route(Task* task);
static void _actor800200TickArea2Route(Task* task);
static void _actor800200TickArea5Route(Task* task);
static void _actor800200TickArea1Route(Task* task);
static void _actor800200TickArea20Route8To10(Task* task);
static void _actor800200TickArea19Route(Task* task);
static void _actor800200TickArea15Route(Task* task);
static void _actor800200TickPlayerFollow(Task* task);
static void _actor800200TickTargetTurn(Task* task);
static void _actor800200TickActionBurst(Task* task);
static void _actor800200TickApproach(Task* task);
static void _actor800200TickTimedWait(Task* task);
static void _actor800200TickClearanceEscape(Task* task);
static void _actor800200TickScriptedWalkToDestination(Task* task);
static void _actor800200TickScriptedRunToDestination(Task* task);
static s32  _actor800200PlayFootstepCue(Task* task);

/// Resets a normal stationary action burst while retaining its borrowed focus.
///
/// Requires live actor work; `targetVariant` is `ACTOR_800200_ACTION_PLAYER` (0)
/// or `ACTOR_800200_ACTION_TARGET` (1), selecting animation 10/11 and the paired
/// sound. Stops movement/turning and disables automatic animation control. The
/// action handler starts playback on phase 0. Preserves focus, route progress
/// and the caller's repetition byte; retains no pointer and allocates nothing.
static inline void _actor800200ResetActionBurst(GameActor* actor, u16 targetVariant)
{
    enum { ACTOR_800200_ACTION_BURST_INITIAL_PHASE = 0 };
    actor->mode                        = GAME_ACTOR_MODE_NORMAL;
    actor->state                       = ACTOR_800200_STATE_ACTION_BURST;
    actor->movementMode                = ACTOR_800200_MOVEMENT_STOPPED;
    actor->turnRateIndex               = ACTOR_800200_TURN_DISABLED;
    actor->animationState              = ACTOR_800200_ANIMATION_CONTROLLER_NONE;
    actor->statePhase                  = ACTOR_800200_ACTION_BURST_INITIAL_PHASE;
    actor->attackControl.targetVariant = targetVariant;
}

/// Starts a normal target turn without replacing the focus or repetition byte.
///
/// Requires live actor work. Selects the target-turn rate and phase 0; the turn
/// handler later starts movement and a turning clip. Movement settings, target,
/// repetition count and route progress are retained. Borrows work only.
static inline void _actor800200ResetTargetTurn(GameActor* actor)
{
    enum { ACTOR_800200_TARGET_TURN_INITIAL_PHASE = 0 };
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = ACTOR_800200_STATE_TARGET_TURN;
    actor->turnRateIndex  = ACTOR_800200_TURN_TARGET;
    actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = ACTOR_800200_TARGET_TURN_INITIAL_PHASE;
}

/// Initializes the companion task's native animation, movement bodies and forward probe.
///
/// Requires allocated GameActor/CompanionWork work and the 19-part model, with
/// unlinked bodies and a valid initial animation in actionArgument. Publishes
/// the companion registry slot and installs teardown. The root and part-4
/// spheres borrow one contact table and use characterId in the player-body key
/// category. The probe copies its endpoint before the eight-byte reservation
/// is released; only six bytes of that reservation hold the endpoint.
static void _actor800200InitTask(Task* task)
{
    enum { ACTOR_800200_PROBE_SCRATCH_BYTES = 8,
           ACTOR_800200_ROOT_BODY_RADIUS    = 250,
           ACTOR_800200_PART4_BODY_RADIUS   = 200,
           ACTOR_800200_PART4_COORD_INDEX   = 4,
           ACTOR_800200_PROBE_NEAR_Y        = -256,
           ACTOR_800200_PROBE_NEAR_Z        = 512,
           ACTOR_800200_PROBE_FAR_Z         = 1536 };
    GameActor*             actor;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    GfxCoord*              modelCoords;
    GfxCoord**             coordsSlot;
    WorldCollisionBody*    body;
    WorldCollisionContact* contacts;
    const McSaveData*      liveSave;
    SVECTOR3*              nearEndpoint;
    u8*                    scratchEnd;
    s32                    bodyKeyKind;

    actor                    = task->work;
    scratchEnd               = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = scratchEnd - ACTOR_800200_PROBE_SCRATCH_BYTES;
    nearEndpoint             = (SVECTOR3*)(scratchEnd - ACTOR_800200_PROBE_SCRATCH_BYTES);
    model                    = task->extra.tmd;
    coordsSlot               = &model->coords;
    rootCoord                = *coordsSlot;
    task->state++;
    task->msgTable                                 = D_actor_800200_80169EF0;
    task->exitCallback                             = &_actor800200TeardownTask;
    actor->animationSlotCount                      = GAME_ACTOR_NORMAL_ANIMATION_SLOTS;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = task;
    rootCoord->parent                              = &gGfxViewCoord;
    rootCoord->composeStamp                        = GRAPHICS_COORD_DIRTY;
    model->flags                                   = 0;
    RotMatrix(&actor->rotation, &rootCoord->coord);
    companionInitNativeAnimation(task);
    actor->animationRate = ANIMATION_RATE_ONE;
    playerActorResetChildSlots(task, actor->actionArgument);
    playerActorTickChildSlots(task);
    // Both movement spheres share the actor contact table; the probe owns its result.
    contacts                                   = actor->collisionContacts;
    body                                       = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    actor->previousPosition.vx                 = rootCoord->coord.t[0];
    actor->previousPosition.vy                 = rootCoord->coord.t[1];
    actor->previousPosition.vz                 = rootCoord->coord.t[2];
    body->context.motion                       = &actor->collisionMotionContexts[0];
    body->coord                                = rootCoord;
    actor->collisionMotionContexts[0].contacts = contacts;
    liveSave                                   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    body->pos.vx                               = 0;
    body->pos.vy                               = -ACTOR_800200_ROOT_BODY_RADIUS;
    body->pos.vz                               = 0;
    {
        s32 characterId;

        characterId  = liveSave->state.characterId;
        body->radius = ACTOR_800200_ROOT_BODY_RADIUS;
        body->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        bodyKeyKind  = WORLD_COLLISION_CONTACT_PLAYER_BODY;
        body->key    = characterId | bodyKeyKind;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, body);
    }
    worldCollisionInitContacts(actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), 0);
    body->flags                               |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    body                                       = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    modelCoords                                = task->extra.tmd->coords;
    body->context.motion                       = &actor->collisionMotionContexts[1];
    body->coord                                = modelCoords + ACTOR_800200_PART4_COORD_INDEX;
    actor->collisionMotionContexts[1].contacts = contacts;
    body->pos.vx                               = 0;
    body->pos.vy                               = 0;
    body->pos.vz                               = 0;
    {
        s32 characterId;

        characterId  = liveSave->state.characterId;
        body->radius = ACTOR_800200_PART4_BODY_RADIUS;
        body->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        body->key    = characterId | bodyKeyKind;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, body);
    }
    body->flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    actor->collisionEnableMask = GAME_ACTOR_COLLISION_REQUEST_MASK;
    nearEndpoint->vx           = 0;
    nearEndpoint->vy           = ACTOR_800200_PROBE_NEAR_Y;
    nearEndpoint->vz           = ACTOR_800200_PROBE_NEAR_Z;
    companionBindCollisionProbe(task, nearEndpoint, ACTOR_800200_PROBE_FAR_Z);
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_800200_PROBE_SCRATCH_BYTES);
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
            actor->gridResponse = worldCollisionApplyResponsePushback(coord, actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), &actor->surfaceClass);
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
    worldCollisionClearContacts(actor->collisionContacts);
    worldCollisionClearContacts(actor->companionWork->probe.contacts);
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
        effectDrawGroundShadow(MATRIX_TRANS(&coord->workm), 0x200, gRoomEffectState->groundShadowShade);
    }
    SCRATCH_STACK_RELEASE_BLOCK(CompanionMoveScratch);
}

/// Selects the final task state so the next dispatch tears down this companion.
static void _actor800200AdvanceToTeardown(Task* task)
{
    task->state = ACTOR_800200_TASK_TEARDOWN;
}

/// Clears the companion registry, unlinks its two movement bodies and kills the task.
///
/// Runs as the exit callback or final task state while GameActor work is live.
/// The separately allocated companion work and its probe are left intact.
static void _actor800200TeardownTask(Task* task)
{
    GameActor* actor;

    actor                                          = task->work;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = NULL;
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_ROOT]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART4]);
    taskKill(task);
}

/// State handlers of the actor's main task, indexed by its state: set-up, the
/// per-frame update, a step that only advances to the last state, and the
/// teardown.
static const TaskFuncTable4 D_actor_800200_80161E24 = { {
    _actor800200InitTask,
    func_actor_800200_801622B0,
    _actor800200AdvanceToTeardown,
    _actor800200TeardownTask,
} };

/// Handlers `func_actor_800200_801652EC` runs, indexed by `mode`.
static const TaskFuncTable3 D_actor_800200_80161E34 = { {
    _actor800200TickNormalMode,
    _actor800200TickDamageMode,
    _actor800200TickScriptedMode,
} };

void actor800200Task(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800200_80161E24;
    states.funcs[task->state](task);
}

/// Chooses unscripted idle behavior for companion schedules 0, 2, 4, 5 and 6.
///
/// Requires a live companion model and player in the same root-parent frame.
/// During engaged combat, borrows
/// a lock node and chooses approach, an action burst, or player-follow behavior
/// from four distance-weighted rows. Otherwise follows at least 1536 planar game
/// units away, turns at least 512 angle units (4096 per turn), or starts resting at
/// a newly sampled 150..277 idle-update threshold. Reserves sixteen scratch bytes
/// for a twelve-byte XYZ position/delta view; the last four bytes are untouched.
static void _actor800200TickFreeIdle(Task* task)
{
    enum {
        ACTOR_800200_IDLE_KEEP                    = 0,
        ACTOR_800200_IDLE_APPROACH_TARGET         = 1,
        ACTOR_800200_IDLE_ACTION_BURST            = 2,
        ACTOR_800200_IDLE_FOLLOW_PLAYER           = 3,
        ACTOR_800200_IDLE_SCRATCH_BYTES           = 16,
        ACTOR_800200_COMBAT_DISTANCE_BUCKET_UNITS = 1024,
        ACTOR_800200_COMBAT_DISTANCE_BUCKET_COUNT = ARRAY_SIZE(D_actor_800200_80169FD0),
        ACTOR_800200_COMBAT_CHOICE_MASK           = 0xF,
        ACTOR_800200_COMBAT_REPEAT_MASK           = 3,
        ACTOR_800200_IDLE_FOLLOW_DISTANCE         = 0x600,
        ACTOR_800200_IDLE_TURN_ANGLE              = 0x200,
        ACTOR_800200_IDLE_RANDOM_DELAY_MASK       = 0x7F,
        ACTOR_800200_IDLE_REST_MIN_TICKS          = 150
    };
    GameActor*       actor;
    CompanionWork*   companion;
    const GfxCoord*  rootCoord;
    const GfxCoord*  playerCoord;
    VECTOR3*         targetDelta;
    WorldTargetNode* targetNode;
    u32              decision;
    const u8*        decisionWeights;
    s32              distanceBucket;
    s32              playerTurnMagnitude;

    rootCoord           = task->extra.tmd->coords;
    playerCoord         = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    targetDelta         = (VECTOR3*)SCRATCH_STACK_RESERVE_BYTES(ACTOR_800200_IDLE_SCRATCH_BYTES);
    actor               = task->work;
    actor->actionValue += 1;
    companion           = actor->companionWork;
    // Combat samples one of sixteen weighted choices in a clamped distance bucket.
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        decision          = ACTOR_800200_IDLE_KEEP;
        targetNode        = worldTargetFindLockNode(task);
        actor->targetNode = targetNode;
        if (targetNode != NULL) {
            worldTargetGetBodyPosition(targetNode, targetDelta);
            playerActorGetPointDelta(rootCoord, targetDelta, targetDelta);
            distanceBucket  = playerActorPlanarLength(targetDelta->vx, targetDelta->vz);
            distanceBucket /= ACTOR_800200_COMBAT_DISTANCE_BUCKET_UNITS;
            if (distanceBucket >= ACTOR_800200_COMBAT_DISTANCE_BUCKET_COUNT) {
                distanceBucket = ACTOR_800200_COMBAT_DISTANCE_BUCKET_COUNT - 1;
            }
            decisionWeights = D_actor_800200_80169FD0[distanceBucket];
            decision        = decisionWeights[rand() & ACTOR_800200_COMBAT_CHOICE_MASK];
        }
        switch (decision) {
            case ACTOR_800200_IDLE_KEEP:
                break;
            case ACTOR_800200_IDLE_APPROACH_TARGET:
                worldTargetGetBodyPosition(actor->targetNode, &actor->destination);
                _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
                break;
            case ACTOR_800200_IDLE_ACTION_BURST:
                companion->activity.combat.repeatsRemaining = (rand() & ACTOR_800200_COMBAT_REPEAT_MASK) + 1;
                _actor800200EnterActionBurst(task, ACTOR_800200_ACTION_TARGET);
                break;
            case ACTOR_800200_IDLE_FOLLOW_PLAYER:
                _actor800200EnterPlayerFollow(task);
                break;
        }
    } else {
        // Outside combat, follow, face the player, or settle into a resting pose.
        if (companionGetPlayerPlanarDistance(rootCoord) >= ACTOR_800200_IDLE_FOLLOW_DISTANCE) {
            _actor800200EnterPlayerFollow(task);
        } else {
            playerTurnMagnitude = playerActorGetTurnToPoint(task, MATRIX_TRANS(&playerCoord->coord));
            if (playerTurnMagnitude < 0) {
                playerTurnMagnitude = -playerTurnMagnitude;
            }
            if (playerTurnMagnitude >= ACTOR_800200_IDLE_TURN_ANGLE) {
                actor->targetNode = NULL;
                _actor800200EnterTargetTurn(task);
            } else if (actor->actionValue >= ((rand() & ACTOR_800200_IDLE_RANDOM_DELAY_MASK) + ACTOR_800200_IDLE_REST_MIN_TICKS)) {
                _actor800200EnterRest(task);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_800200_IDLE_SCRATCH_BYTES);
}

/// Sets a route stop in X/Z while retaining the root's current translation Y.
///
/// All inputs must be side-effect-free expressions: actor is evaluated three
/// times, and routeTable and stopIndex twice. The selected stop must be in
/// bounds. The root and destination share the root node's parent coordinates.
/// Expands to a compound statement; call inside a braced block, never as
/// an unbraced if/else arm.
#define ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, routeTable, stopIndex) \
    {                                                                               \
        (actor)->destination.vx = (routeTable)[(stopIndex)].x;                      \
        (actor)->destination.vy = (rootCoord)->coord.t[1];                          \
        (actor)->destination.vz = (routeTable)[(stopIndex)].z;                      \
    }

/// Advances the four-stop area-26 route selected by spawn mode 1.
///
/// Requires a live companion task, model and player; `waypointIndex` must be 0..3.
/// Tests the final stop on entry, then follows X/Z stops at the current Y.
/// Waits for the player at intermediate stops and for the wire rope at stop 2;
/// waiting periodically starts one target-dependent action. Distances are in
/// root-parent game-coordinate units; the route phase survives behavior changes.
static void _actor800200TickArea26Route(Task* task)
{
    enum { ACTOR_800200_ROUTE_ROPE_WAYPOINT         = 2,
           ACTOR_800200_ROUTE_LAST_WAYPOINT         = ARRAY_SIZE(D_actor_800200_80169FF8) - 1,
           ACTOR_800200_ROUTE_WAIT_DISTANCE         = 0xe00,
           ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT = 0xc01 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    u16             routePhase;
    s32             movementMode;
    s32             actionDelay;

    actor      = task->work;
    coord      = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            actor->stateAux = ACTOR_800200_ROUTE_TRAVEL;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_80169FF8, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                _actor800200EnterTimedWait(task, 0);
                return;
            }
            // Fall through to select the current waypoint.
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_80169FF8, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                if (companionGetPlayerPlanarDistance(coord) >= ACTOR_800200_ROUTE_WAIT_DISTANCE || (companion->waypointIndex == ACTOR_800200_ROUTE_ROPE_WAYPOINT && inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_WIRE_ROPE) == 0)) {
                    actor->stateAux   = ACTOR_800200_ROUTE_WAIT_PLAYER;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    _actor800200EnterTargetTurn(task);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            movementMode = ACTOR_800200_MOVEMENT_RUN;
            if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                movementMode = ACTOR_800200_MOVEMENT_WALK;
            }
            _actor800200EnterApproach(task, movementMode);
            return;
        case ACTOR_800200_ROUTE_WAIT_PLAYER:
            // Stop 2 remains gated by rope collection even when the player is close.
            if ((companionGetPlayerPlanarDistance(coord) < ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT && companion->waypointIndex < ACTOR_800200_ROUTE_ROPE_WAYPOINT) || (companion->waypointIndex == ACTOR_800200_ROUTE_ROPE_WAYPOINT && inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_WIRE_ROPE) != 0)) {
                companion->waypointIndex++;
                actor->stateAux = ACTOR_800200_ROUTE_TRAVEL;
                return;
            }
            actionDelay       = actor->stateTimer - 1;
            actor->stateTimer = actionDelay;
            if (actionDelay <= 0) {
                actor->stateTimer = rand() & ACTOR_800200_ROUTE_RANDOM_DELAY_MASK;
                _actor800200StartActionBurst(task, 1);
            }
            return;
    }
}

/// Advances the four-stop area-23 route selected by spawn mode 1.
///
/// Requires a live companion task, model and player; `waypointIndex` must be 0..3.
/// Keeps the current Y and waits at intermediate stops when the player is far
/// away. The wait resumes only at a positive waypoint index; stop 0 retains
/// that original restriction. The final leg walks rather than runs.
static void _actor800200TickArea23Route1(Task* task)
{
    enum { ACTOR_800200_ROUTE_LAST_WAYPOINT         = ARRAY_SIZE(D_actor_800200_8016A020) - 1,
           ACTOR_800200_ROUTE_WAIT_DISTANCE         = 0xc00,
           ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT = 0xb01 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    u16             routePhase;
    s32             movementMode;
    s32             actionDelay;

    actor      = task->work;
    coord      = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            actor->stateAux = ACTOR_800200_ROUTE_TRAVEL;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A020, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                _actor800200EnterTimedWait(task, 0);
                return;
            }
            // Fall through to select the current waypoint.
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A020, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                if (companionGetPlayerPlanarDistance(coord) >= ACTOR_800200_ROUTE_WAIT_DISTANCE) {
                    actor->stateAux   = ACTOR_800200_ROUTE_WAIT_PLAYER;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    _actor800200EnterTargetTurn(task);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            movementMode = ACTOR_800200_MOVEMENT_RUN;
            if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                movementMode = ACTOR_800200_MOVEMENT_WALK;
            }
            _actor800200EnterApproach(task, movementMode);
            return;
        case ACTOR_800200_ROUTE_WAIT_PLAYER:
            if (companionGetPlayerPlanarDistance(coord) < ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT && companion->waypointIndex > 0) {
                companion->waypointIndex++;
                actor->stateAux = ACTOR_800200_ROUTE_TRAVEL;
                return;
            }
            actionDelay       = actor->stateTimer - 1;
            actor->stateTimer = actionDelay;
            if (actionDelay <= 0) {
                actor->stateTimer = rand() & ACTOR_800200_ROUTE_RANDOM_DELAY_MASK;
                _actor800200StartActionBurst(task, 1);
            }
            return;
    }
}

/// Advances the three-stop Dryfield route selected by companion schedule 3.
///
/// Requires a live model/player and waypoint index 0..2. Retains current Y,
/// runs toward the selected stop, and waits for a distant player until within
/// 2560 planar game units or beyond the companion in X. Waiting starts single
/// player-directed actions at random intervals. The last stop requests rest
/// animation, enters approach, then awaits a nonzero behavior phase before holding
/// pose 9; it does not set the route-completion latch.
static void _actor800200TickSchedule3Route(Task* task)
{
    enum {
        ACTOR_800200_SCHEDULE3_TRAVEL            = 0,
        ACTOR_800200_SCHEDULE3_WAIT_PLAYER       = 1,
        ACTOR_800200_SCHEDULE3_UNUSED_PHASE      = 2,
        ACTOR_800200_SCHEDULE3_AWAIT_PHASE       = 3,
        ACTOR_800200_SCHEDULE3_HOLD_POSE         = 4,
        ACTOR_800200_SCHEDULE3_FINISHED          = 5,
        ACTOR_800200_ROUTE_LAST_WAYPOINT         = ARRAY_SIZE(D_actor_800200_80169FE0) - 1,
        ACTOR_800200_ROUTE_WAIT_DISTANCE         = 0xE00,
        ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT = 0xA01
    };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* rootCoord;
    const GfxCoord* playerCoord;
    s32             actionDelay;

    rootCoord   = task->extra.tmd->coords;
    playerCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor       = task->work;
    companion   = actor->companionWork;
    switch (actor->stateAux) {
        case ACTOR_800200_SCHEDULE3_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, D_actor_800200_80169FE0, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                    // Keep the original animation request followed by approach entry.
                    actor->stateAux       = ACTOR_800200_SCHEDULE3_AWAIT_PHASE;
                    actor->statePhase     = 0;
                    actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_ONCE;
                    playerActorPlayChildSlotsWithBlend(task, ACTOR_800200_ANIMATION_REST, 0, ACTOR_800200_ENTRY_BLEND_FRAMES);
                    _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
                    return;
                }
                if (companionGetPlayerPlanarDistance(rootCoord) >= ACTOR_800200_ROUTE_WAIT_DISTANCE) {
                    actor->stateAux   = ACTOR_800200_SCHEDULE3_WAIT_PLAYER;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    _actor800200EnterTargetTurn(task);
                    return;
                }
                companion->waypointIndex++;
            }
            _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
            return;
        case ACTOR_800200_SCHEDULE3_WAIT_PLAYER:
            if ((companionGetPlayerPlanarDistance(rootCoord) < ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT) || (rootCoord->coord.t[0] < playerCoord->coord.t[0])) {
                companion->waypointIndex++;
                actor->stateAux = ACTOR_800200_SCHEDULE3_TRAVEL;
                return;
            }
            actionDelay       = actor->stateTimer - 1;
            actor->stateTimer = actionDelay;
            if (actionDelay <= 0) {
                companion->activity.combat.repeatsRemaining = 1;
                actor->stateTimer                           = rand() & ACTOR_800200_ROUTE_RANDOM_DELAY_MASK;
                _actor800200EnterActionBurst(task, ACTOR_800200_ACTION_PLAYER);
            }
            return;
        case ACTOR_800200_SCHEDULE3_AWAIT_PHASE:
            if (actor->statePhase != 0) {
                actor->stateAux++;
            }
            return;
        case ACTOR_800200_SCHEDULE3_HOLD_POSE:
            actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_NONE;
            actor->stateAux++;
            playerActorResetChildSlots(task, ACTOR_800200_ANIMATION_REST_HOLD);
            return;
        default:
        case ACTOR_800200_SCHEDULE3_UNUSED_PHASE:
        case ACTOR_800200_SCHEDULE3_FINISHED:
            return;
    }
}

/// Advances the two-stop area-23 route selected by spawn mode 7.
///
/// Requires a live companion task and model; `waypointIndex` must be 0..1.
/// Skips travel when initially within 1024 game units of the final X/Z stop;
/// otherwise runs through the stops at the actor's current Y, completing within
/// 512 units of the final stop.
static void _actor800200TickArea23Route7(Task* task)
{
    enum { ACTOR_800200_ROUTE_LAST_WAYPOINT = ARRAY_SIZE(D_actor_800200_8016A048) - 1 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    u16             routePhase;
    s32             initialRoutePhase;

    actor      = task->work;
    coord      = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            initialRoutePhase = ACTOR_800200_ROUTE_TRAVEL;
            actor->stateAux   = initialRoutePhase;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A048, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                goto arrived;
            }
            // Fall through to select the current waypoint.
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A048, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
            return;
        default:
            return;
    }
}

/// Advances the two-stop area-22 route selected by spawn mode 7.
///
/// Requires a live companion task, model and player; `waypointIndex` must be 0..1.
/// Runs at the current Y, waiting at stop 0 when the player is at least 3072 game
/// units away and resuming within 2048. Waiting periodically starts one
/// target-dependent action without advancing the route.
static void _actor800200TickArea22Route(Task* task)
{
    enum { ACTOR_800200_ROUTE_LAST_WAYPOINT         = ARRAY_SIZE(D_actor_800200_8016A058) - 1,
           ACTOR_800200_ROUTE_WAIT_DISTANCE         = 0xc00,
           ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT = 0x801 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    u16             routePhase;
    s32             actionDelay;

    actor      = task->work;
    coord      = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            actor->stateAux = ACTOR_800200_ROUTE_TRAVEL;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A058, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                goto arrived;
            }
            // Fall through to select the current waypoint.
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A058, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                if (companionGetPlayerPlanarDistance(coord) >= ACTOR_800200_ROUTE_WAIT_DISTANCE) {
                    actor->stateAux   = ACTOR_800200_ROUTE_WAIT_PLAYER;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    _actor800200EnterTargetTurn(task);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
            return;
        case ACTOR_800200_ROUTE_WAIT_PLAYER:
            if (companionGetPlayerPlanarDistance(coord) < ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT) {
                companion->waypointIndex++;
                actor->stateAux = ACTOR_800200_ROUTE_TRAVEL;
                return;
            }
            actionDelay       = actor->stateTimer - 1;
            actor->stateTimer = actionDelay;
            if (actionDelay <= 0) {
                actor->stateTimer = rand() & ACTOR_800200_ROUTE_RANDOM_DELAY_MASK;
                _actor800200StartActionBurst(task, 1);
            }
            return;
    }
}

/// Advances the three-stop area-20 route selected by spawn mode 7.
///
/// Requires a live companion task, model and player; `waypointIndex` must be 0..2.
/// Waits at intermediate stops when the player is at least 3072 game units away
/// and resumes within 2304. Keeps the current Y and walks the final leg;
/// waiting periodically starts one target-dependent action.
static void _actor800200TickArea20Route7(Task* task)
{
    enum { ACTOR_800200_ROUTE_LAST_WAYPOINT         = ARRAY_SIZE(D_actor_800200_8016A068) - 1,
           ACTOR_800200_ROUTE_WAIT_DISTANCE         = 0xc00,
           ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT = 0x901 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    u16             routePhase;
    s32             movementMode;
    s32             actionDelay;

    actor      = task->work;
    coord      = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            actor->stateAux = ACTOR_800200_ROUTE_TRAVEL;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A068, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                _actor800200EnterTimedWait(task, 0);
                return;
            }
            // Fall through to select the current waypoint.
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A068, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                if (companionGetPlayerPlanarDistance(coord) >= ACTOR_800200_ROUTE_WAIT_DISTANCE) {
                    actor->stateAux   = ACTOR_800200_ROUTE_WAIT_PLAYER;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    _actor800200EnterTargetTurn(task);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            movementMode = ACTOR_800200_MOVEMENT_RUN;
            if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                movementMode = ACTOR_800200_MOVEMENT_WALK;
            }
            _actor800200EnterApproach(task, movementMode);
            return;
        case ACTOR_800200_ROUTE_WAIT_PLAYER:
            if (companionGetPlayerPlanarDistance(coord) < ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT) {
                companion->waypointIndex++;
                actor->stateAux = ACTOR_800200_ROUTE_TRAVEL;
                return;
            }
            actionDelay       = actor->stateTimer - 1;
            actor->stateTimer = actionDelay;
            if (actionDelay <= 0) {
                actor->stateTimer = rand() & ACTOR_800200_ROUTE_RANDOM_DELAY_MASK;
                _actor800200StartActionBurst(task, 1);
            }
            return;
    }
}

/// Advances the two-stop area-4 route selected by spawn modes 8 through 10.
///
/// Requires a live companion task, model and player; `waypointIndex` must be 0..1.
/// Plays route animation 14 before initial travel and after waiting for the
/// player at stop 0, then walks the final leg at the current Y. The four route
/// phases survive returns to idle and other normal-mode behavior states.
static void _actor800200TickArea4Route(Task* task)
{
    enum { ACTOR_800200_ROUTE_PLAY_ANIMATION         = 1,
           ACTOR_800200_ROUTE_TRAVEL_AFTER_ANIMATION = 2,
           ACTOR_800200_ROUTE_WAIT_AFTER_ANIMATION   = 3,
           ACTOR_800200_ROUTE_LAST_WAYPOINT          = ARRAY_SIZE(D_actor_800200_8016A080) - 1,
           ACTOR_800200_ROUTE_WAIT_DISTANCE          = 0xc00,
           ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT  = 0x901 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    u16             routePhase;
    s32             movementMode;
    s32             actionDelay;

    actor      = task->work;
    coord      = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            actor->stateAux = ACTOR_800200_ROUTE_PLAY_ANIMATION;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A080, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                goto arrived;
            }
        case ACTOR_800200_ROUTE_PLAY_ANIMATION:
            actor->stateAux++;
            _actor800200EnterRouteAnimation(task);
            return;
        case ACTOR_800200_ROUTE_TRAVEL_AFTER_ANIMATION:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A080, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                if (companionGetPlayerPlanarDistance(coord) >= ACTOR_800200_ROUTE_WAIT_DISTANCE) {
                    actor->stateAux   = ACTOR_800200_ROUTE_WAIT_AFTER_ANIMATION;
                    actor->stateTimer = 0;
                    actor->targetNode = NULL;
                    _actor800200EnterTargetTurn(task);
                    return;
                }
                companion->waypointIndex++;
                return;
            }
            movementMode = ACTOR_800200_MOVEMENT_RUN;
            if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                movementMode = ACTOR_800200_MOVEMENT_WALK;
            }
            _actor800200EnterApproach(task, movementMode);
            return;
        case ACTOR_800200_ROUTE_WAIT_AFTER_ANIMATION:
            if (companionGetPlayerPlanarDistance(coord) < ACTOR_800200_ROUTE_RESUME_DISTANCE_LIMIT) {
                companion->waypointIndex++;
                actor->stateAux = ACTOR_800200_ROUTE_PLAY_ANIMATION;
                return;
            }
            actionDelay       = actor->stateTimer - 1;
            actor->stateTimer = actionDelay;
            if (actionDelay <= 0) {
                actor->stateTimer = rand() & ACTOR_800200_ROUTE_RANDOM_DELAY_MASK;
                _actor800200StartActionBurst(task, 1);
            }
            return;
    }
}

/// Advances the three-stop area-2 route selected by spawn modes 8 through 10.
///
/// Requires a live companion task and model; `waypointIndex` must be 0..2.
/// Keeps the current Y, plays route animation 14 after intermediate arrivals,
/// and walks to stop 1 while running to the other stops. Completion selects timed wait.
static void _actor800200TickArea2Route(Task* task)
{
    enum { ACTOR_800200_ROUTE_WALK_WAYPOINT = 1,
           ACTOR_800200_ROUTE_LAST_WAYPOINT = ARRAY_SIZE(D_actor_800200_8016A098) - 1 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    u16             routePhase;
    s32             initialRoutePhase;
    s32             movementMode;

    actor      = task->work;
    coord      = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            initialRoutePhase = ACTOR_800200_ROUTE_TRAVEL;
            actor->stateAux   = initialRoutePhase;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A098, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                goto arrived;
            }
            // Fall through to select the current waypoint.
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A098, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                companion->waypointIndex++;
                _actor800200EnterRouteAnimation(task);
                return;
            }
            movementMode = ACTOR_800200_MOVEMENT_RUN;
            if (companion->waypointIndex == ACTOR_800200_ROUTE_WALK_WAYPOINT) {
                movementMode = ACTOR_800200_MOVEMENT_WALK;
            }
            _actor800200EnterApproach(task, movementMode);
            return;
        default:
            return;
    }
}

/// Advances the three-stop area-5 route selected by spawn modes 8 through 10.
///
/// Requires a live companion task and model; `waypointIndex` must be 0..2.
/// Runs at the current Y and plays route animation 14 after each intermediate
/// arrival. Completion selects timed wait.
static void _actor800200TickArea5Route(Task* task)
{
    enum { ACTOR_800200_ROUTE_LAST_WAYPOINT = ARRAY_SIZE(D_actor_800200_8016A0B0) - 1 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    u16             routePhase;
    s32             initialRoutePhase;

    actor      = task->work;
    coord      = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            initialRoutePhase = ACTOR_800200_ROUTE_TRAVEL;
            actor->stateAux   = initialRoutePhase;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A0B0, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                goto arrived;
            }
            // Fall through to select the current waypoint.
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A0B0, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                companion->waypointIndex++;
                _actor800200EnterRouteAnimation(task);
                return;
            }
            _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
            return;
        default:
            return;
    }
}

/// Advances the three-stop area-1 route selected by spawn modes 8 through 10.
///
/// Requires a live companion task and model; `waypointIndex` must be 0..2.
/// Plays route animation 14 before the first leg and after intermediate arrivals,
/// then runs at the current Y. Completion selects timed wait.
static void _actor800200TickArea1Route(Task* task)
{
    enum { ACTOR_800200_ROUTE_LAST_WAYPOINT = ARRAY_SIZE(D_actor_800200_8016A0C8) - 1 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    s32             initialRoutePhase;

    actor     = task->work;
    coord     = task->extra.tmd->coords;
    companion = actor->companionWork;
    switch (actor->stateAux) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            initialRoutePhase = ACTOR_800200_ROUTE_TRAVEL;
            actor->stateAux   = initialRoutePhase;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A0C8, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                goto arrived;
            }
            _actor800200EnterRouteAnimation(task);
            break;
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A0C8, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    break;
                }
                companion->waypointIndex = companion->waypointIndex + 1;
                _actor800200EnterRouteAnimation(task);
                break;
            }
            _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
            break;
    }
}

/// Advances the five-stop area-20 route selected by spawn modes 8 through 10.
///
/// Requires a live companion task and model; `waypointIndex` must be 0..4.
/// Plays route animation 14 before travel and after intermediate arrivals,
/// then runs at the current Y. Completion selects timed wait.
static void _actor800200TickArea20Route8To10(Task* task)
{
    enum { ACTOR_800200_ROUTE_LAST_WAYPOINT = ARRAY_SIZE(D_actor_800200_8016A0E0) - 1 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* coord;
    u16             routePhase;
    s32             initialRoutePhase;

    actor      = task->work;
    coord      = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            // A spawn near the final stop skips the intermediate route.
            initialRoutePhase = ACTOR_800200_ROUTE_TRAVEL;
            actor->stateAux   = initialRoutePhase;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A0E0, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                goto arrived;
            }
            _actor800200EnterRouteAnimation(task);
            return;
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, coord, D_actor_800200_8016A0E0, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&coord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                companion->waypointIndex++;
                _actor800200EnterRouteAnimation(task);
                return;
            }
            _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
            return;
        default:
            return;
    }
}

/// Advances the four-stop Shelter B1 access-tunnel route of schedule 10.
///
/// Requires a live model and waypoint index 0..3. Tests the final stop on entry,
/// runs between X/Z stops at current Y, and plays a route animation at each
/// intermediate arrival. Final arrival latches completion and enters timed wait.
static void _actor800200TickArea19Route(Task* task)
{
    enum { ACTOR_800200_ROUTE_LAST_WAYPOINT = ARRAY_SIZE(D_actor_800200_8016A108) - 1 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* rootCoord;
    u16             routePhase;
    s32             initialRoutePhase;

    actor      = task->work;
    rootCoord  = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            initialRoutePhase = ACTOR_800200_ROUTE_TRAVEL;
            actor->stateAux   = initialRoutePhase;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, D_actor_800200_8016A108, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                goto arrived;
            }
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, D_actor_800200_8016A108, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterTimedWait(task, 0);
                    return;
                }
                companion->waypointIndex++;
                _actor800200EnterRouteAnimation(task);
                return;
            }
            _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
            return;
        default:
            return;
    }
}

/// Advances the five-stop Shelter B1 main-corridor route of schedule 10.
///
/// Requires a live model and waypoint index 0..4. Tests the final stop on entry,
/// retains current Y, and plays a route animation at every arrival. Walks toward
/// stop 1 and runs on other legs; final arrival also latches route completion.
static void _actor800200TickArea15Route(Task* task)
{
    enum { ACTOR_800200_ROUTE_LAST_WAYPOINT = ARRAY_SIZE(D_actor_800200_8016A130) - 1,
           ACTOR_800200_ROUTE_WALK_WAYPOINT = 1 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* rootCoord;
    u16             routePhase;
    s32             initialRoutePhase;
    s32             movementMode;

    actor      = task->work;
    rootCoord  = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_ROUTE_CHECK_END:
            initialRoutePhase = ACTOR_800200_ROUTE_TRAVEL;
            actor->stateAux   = initialRoutePhase;
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, D_actor_800200_8016A130, ACTOR_800200_ROUTE_LAST_WAYPOINT);
            if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                goto arrived;
            }
        case ACTOR_800200_ROUTE_TRAVEL:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, D_actor_800200_8016A130, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_ROUTE_WAYPOINT_DISTANCE_LIMIT) {
                if (companion->waypointIndex == ACTOR_800200_ROUTE_LAST_WAYPOINT) {
                arrived:
                    companion->routeComplete = COMPANION_ROUTE_COMPLETE;
                    _actor800200EnterRouteAnimation(task);
                    return;
                }
                companion->waypointIndex++;
                _actor800200EnterRouteAnimation(task);
                return;
            }
            movementMode = ACTOR_800200_MOVEMENT_RUN;
            if (companion->waypointIndex == ACTOR_800200_ROUTE_WALK_WAYPOINT) {
                movementMode = ACTOR_800200_MOVEMENT_WALK;
            }
            _actor800200EnterApproach(task, movementMode);
            return;
        default:
            return;
    }
}

/// Follows the player, switching walk/run pace or scanning for an obstruction escape.
///
/// Requires live companion/model/player state in one root-parent frame. Stops
/// within 768 planar game units; running begins at 3584 units and uses randomized
/// hysteresis until 240 follow updates force steady walking. A nonzero probe
/// clearance below 769 units starts the clearance sweep. Body and aim track the
/// player except on the early sweep transition; route progress is retained.
static void _actor800200TickPlayerFollow(Task* task)
{
    enum { ACTOR_800200_FOLLOW_CHOOSE_PACE_PHASE = 0 };
    enum { ACTOR_800200_FOLLOW_OBSTRUCTION_LIMIT   = 769,
           ACTOR_800200_FOLLOW_STOP_DISTANCE_LIMIT = 769,
           ACTOR_800200_FOLLOW_RUN_DISTANCE        = 3584,
           ACTOR_800200_FOLLOW_WALK_PHASE          = 1,
           ACTOR_800200_FOLLOW_RUN_PHASE           = 2,
           ACTOR_800200_FOLLOW_STEADY_WALK_PHASE   = 3,
           ACTOR_800200_FOLLOW_RECHECK_TICKS       = 240,
           ACTOR_800200_FOLLOW_JITTER_MASK         = 0x3FF,
           ACTOR_800200_FOLLOW_WALK_DISTANCE       = 2048,
           ACTOR_800200_FOLLOW_RUN_DISTANCE_BASE   = 3072 };
    GameActor*      actor;
    CompanionWork*  companion;
    GfxCoord*       rootCoord;
    const GfxCoord* playerCoord;
    const VECTOR3*  playerPosition;
    GameActor*      currentActor;
    s32             animationId;
    s32             planarDistance;
    s32             distanceJitter;

    rootCoord      = task->extra.tmd->coords;
    playerCoord    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor          = task->work;
    planarDistance = _actor800200GetContactDistance(rootCoord, actor->companionWork->probe.contacts, NULL);
    // An obstruction interrupts following and starts a fresh clearance sweep.
    if (planarDistance != 0 && planarDistance < ACTOR_800200_FOLLOW_OBSTRUCTION_LIMIT) {
        currentActor                 = task->work;
        companion                    = currentActor->companionWork;
        currentActor->state          = ACTOR_800200_STATE_CLEARANCE_ESCAPE;
        currentActor->turnRateIndex  = ACTOR_800200_TURN_TARGET;
        currentActor->mode           = GAME_ACTOR_MODE_NORMAL;
        currentActor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_NONE;
        currentActor->statePhase     = 0;
        currentActor->movementSign   = 0;
        currentActor->turnSign       = 0;
        companion->scanClearance     = COMPANION_SCAN_UNTESTED;
        companion->scanAngle         = 0;
        playerActorPlayChildSlotsWithBlend(task, ACTOR_800200_ANIMATION_IDLE, 0, ACTOR_800200_ENTRY_BLEND_FRAMES);
        return;
    }
    switch (actor->statePhase) {
        case ACTOR_800200_FOLLOW_CHOOSE_PACE_PHASE:
            actor->stateTimer = 0;
            if (companionGetPlayerPlanarDistance(rootCoord) >= ACTOR_800200_FOLLOW_RUN_DISTANCE) {
                animationId         = ACTOR_800200_ANIMATION_RUN;
                actor->statePhase   = ACTOR_800200_FOLLOW_RUN_PHASE;
                actor->movementMode = ACTOR_800200_MOVEMENT_RUN;
            } else {
            resume:
                if (actor->statePhase != ACTOR_800200_FOLLOW_STEADY_WALK_PHASE) {
                    actor->statePhase = ACTOR_800200_FOLLOW_WALK_PHASE;
                }
                actor->movementMode = ACTOR_800200_MOVEMENT_WALK;
                animationId         = ACTOR_800200_ANIMATION_WALK;
            }
            actor->movementSign = 1;
            playerActorPlayChildSlotsWithBlend(task, animationId, 0, ACTOR_800200_MOVEMENT_BLEND_FRAMES);
        case ACTOR_800200_FOLLOW_WALK_PHASE:
        case ACTOR_800200_FOLLOW_RUN_PHASE:
        case ACTOR_800200_FOLLOW_STEADY_WALK_PHASE:
            planarDistance = companionGetPlayerPlanarDistance(rootCoord);
            if (planarDistance < ACTOR_800200_FOLLOW_STOP_DISTANCE_LIMIT) {
                companionEnterIdle(task, 0);
                break;
            }
            if (actor->statePhase == ACTOR_800200_FOLLOW_STEADY_WALK_PHASE) {
                break;
            }
            actor->stateTimer++;
            if (actor->stateTimer == ACTOR_800200_FOLLOW_RECHECK_TICKS) {
                actor->statePhase = ACTOR_800200_FOLLOW_STEADY_WALK_PHASE;
                goto resume;
            }
            distanceJitter = rand() & ACTOR_800200_FOLLOW_JITTER_MASK;
            if (((ACTOR_800200_FOLLOW_WALK_DISTANCE - distanceJitter) >= planarDistance && actor->statePhase == ACTOR_800200_FOLLOW_RUN_PHASE) || (planarDistance >= distanceJitter + ACTOR_800200_FOLLOW_RUN_DISTANCE_BASE && actor->statePhase == ACTOR_800200_FOLLOW_WALK_PHASE)) {
                actor->statePhase = ACTOR_800200_FOLLOW_CHOOSE_PACE_PHASE;
            }
            break;
        default:
            break;
    }
    playerPosition = MATRIX_TRANS(&playerCoord->coord);
    playerActorTurnBodyTowardPoint(task, playerPosition);
    playerActorTurnAimTowardPoint(task, playerPosition);
}

/// Turns toward a borrowed lock target or the player, then resumes an action burst or idle.
///
/// Angles use 4096 units per turn; the turn completes within 256 angle units.
/// Retains the signed action-repeat byte. Reserves sixteen scratch bytes for a
/// twelve-byte focus position. An un-lockable node forces completion but still
/// queries the uninitialized position, preserving the original behavior.
static void _actor800200TickTargetTurn(Task* task)
{
    enum { ACTOR_800200_TARGET_TURN_START_PHASE   = 0,
           ACTOR_800200_TARGET_TURN_ACTIVE_PHASE  = 1,
           ACTOR_800200_TARGET_TURN_INVALID_PHASE = 2 };
    enum { ACTOR_800200_FOCUS_SCRATCH_BYTES        = 16,
           ACTOR_800200_TARGET_TURN_COMPLETE_LIMIT = 257 };
    GameActor*       actor;
    CompanionWork*   companion;
    const GfxCoord*  playerCoord;
    WorldTargetNode* targetNode;
    VECTOR3*         focusPosition;
    GameActor*       currentActor;
    s32              turnDelta;
    s32              animationId;
    u16              targetVariant;

    actor         = task->work;
    companion     = actor->companionWork;
    playerCoord   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    focusPosition = SCRATCH_STACK_RESERVE_BYTES(ACTOR_800200_FOCUS_SCRATCH_BYTES);
    targetNode    = actor->targetNode;
    if (targetNode != NULL) {
        if (!(targetNode->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            worldTargetGetBodyPosition(targetNode, focusPosition);
        } else {
            actor->statePhase = ACTOR_800200_TARGET_TURN_INVALID_PHASE;
        }
    } else {
        focusPosition->vx = playerCoord->coord.t[0];
        focusPosition->vy = playerCoord->coord.t[1];
        focusPosition->vz = playerCoord->coord.t[2];
    }
    switch (actor->statePhase) {
        case ACTOR_800200_TARGET_TURN_START_PHASE:
            actor->statePhase   = ACTOR_800200_TARGET_TURN_ACTIVE_PHASE;
            actor->movementMode = ACTOR_800200_MOVEMENT_WALK;
            actor->movementSign = 1;
            if (playerActorGetTurnToPoint(task, focusPosition) < 0) {
                actor->turnSign = -1;
                animationId     = ACTOR_800200_ANIMATION_TURN_NEGATIVE;
            } else {
                actor->turnSign = 1;
                animationId     = ACTOR_800200_ANIMATION_TURN_POSITIVE;
            }
            playerActorPlayChildSlotsWithBlend(task, animationId, 1, ACTOR_800200_MOVEMENT_BLEND_FRAMES);
        case ACTOR_800200_TARGET_TURN_ACTIVE_PHASE:
        case ACTOR_800200_TARGET_TURN_INVALID_PHASE:
            turnDelta = playerActorGetTurnToPoint(task, focusPosition);
            if (turnDelta < 0) {
                turnDelta = -turnDelta;
            }
            if ((turnDelta < ACTOR_800200_TARGET_TURN_COMPLETE_LIMIT) || (actor->statePhase == ACTOR_800200_TARGET_TURN_INVALID_PHASE)) {
                if ((s8)companion->activity.combat.repeatsRemaining > 0) {
                    targetVariant = actor->targetNode != 0;
                    currentActor  = task->work;
                    _actor800200ResetActionBurst(currentActor, targetVariant);
                } else {
                    companionEnterIdle(task, 0);
                }
            }
            break;
        default:
            break;
    }
    playerActorTurnAimTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_800200_FOCUS_SCRATCH_BYTES);
}

/// Plays and repeats a player- or target-directed animation and positional sound.
///
/// Variant 0 selects clip 10/script 9; variant 1 selects clip 11/script 10 in
/// this actor's sound bank. Completion decrements the signed repeat byte or
/// turns toward a focus at least 641 angle units and 513 planar game units away.
/// A missing or un-lockable target shortens the burst to one repetition while
/// leaving the focus scratch position uninitialized; that query is retained.
/// Reserves sixteen bytes for a twelve-byte point and releases them each call.
static void _actor800200TickActionBurst(Task* task)
{
    enum { ACTOR_800200_ACTION_REQUEST_PHASE = 0,
           ACTOR_800200_ACTION_PLAY_PHASE    = 1 };
    enum { ACTOR_800200_FOCUS_SCRATCH_BYTES  = 16,
           ACTOR_800200_ACTION_TURN_ANGLE    = 641,
           ACTOR_800200_ACTION_TURN_DISTANCE = 513,
           ACTOR_800200_ACTION_BLEND_FRAMES  = 4,
           ACTOR_800200_ACTION_SOUND_BASE    = SOUND_CHARACTER(SOUND_BANK_ACTOR_800200, 9) };
    GameActor*       actor;
    CompanionWork*   companion;
    const GfxCoord*  playerCoord;
    WorldTargetNode* targetNode;
    GfxCoord*        rootCoord;
    VECTOR3*         focusPosition;
    s8               repeatsRemaining;
    s32              pan;
    s32              turnDelta;
    s32              playingPhase = ACTOR_800200_ACTION_PLAY_PHASE;
    GameActor*       currentActor;

    actor         = task->work;
    companion     = actor->companionWork;
    playerCoord   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    focusPosition = SCRATCH_STACK_RESERVE_BYTES(ACTOR_800200_FOCUS_SCRATCH_BYTES);
    rootCoord     = task->extra.tmd->coords;
    if (actor->targetNode != NULL) {
        targetNode        = worldTargetFindLockNode(task);
        actor->targetNode = targetNode;
        if ((targetNode != NULL) && !(targetNode->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            worldTargetGetBodyPosition(targetNode, focusPosition);
        } else {
            companion->activity.combat.repeatsRemaining = 1;
        }
    } else {
        focusPosition->vx = playerCoord->coord.t[0];
        focusPosition->vy = playerCoord->coord.t[1];
        focusPosition->vz = playerCoord->coord.t[2];
    }
    switch (actor->statePhase) {
        case ACTOR_800200_ACTION_REQUEST_PHASE:
            actor->statePhase = playingPhase;
            playerActorPlayChildSlotsWithBlend(task, actor->attackControl.targetVariant + ACTOR_800200_ANIMATION_ACTION_BASE, 0, ACTOR_800200_ACTION_BLEND_FRAMES);
            pan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(actor->attackControl.targetVariant + ACTOR_800200_ACTION_SOUND_BASE, pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            /* fallthrough */
        case ACTOR_800200_ACTION_PLAY_PHASE:
            if (playerActorIsSlotAdvancingLinearly(task, 1, 0, 0) == 0) {
                turnDelta = playerActorGetTurnToPoint(task, focusPosition);
                if (turnDelta < 0) {
                    turnDelta = -turnDelta;
                }
                if ((turnDelta >= ACTOR_800200_ACTION_TURN_ANGLE) && (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), focusPosition) >= ACTOR_800200_ACTION_TURN_DISTANCE)) {
                    currentActor = task->work;
                    _actor800200ResetTargetTurn(currentActor);
                } else {
                    repeatsRemaining                            = companion->activity.combat.repeatsRemaining - 1;
                    companion->activity.combat.repeatsRemaining = repeatsRemaining;
                    if (repeatsRemaining <= 0) {
                        companionEnterIdle(task, 0);
                    } else {
                        actor->statePhase = ACTOR_800200_ACTION_REQUEST_PHASE;
                    }
                }
            }
            break;
        default:
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_800200_FOCUS_SCRATCH_BYTES);
}

/// Steps root yaw toward the parent-frame destination and records the signed step.
///
/// actor/rootCoord/scratch must be live, turnDelta a writable s32 local, and
/// maxTurnStep a positive angle-unit limit (4096 per turn). Inputs are evaluated
/// repeatedly and must be side-effect-free. No identifiers are captured.
/// Writes XYZ delta, target yaw, clamped step, wrapped root yaw and the initial
/// turning phase. Expands to a compound statement; invoke only in braced blocks.
#define ACTOR_800200_STEP_DESTINATION_TURN(actor, rootCoord, scratch, turnDelta, maxTurnStep)                             \
    {                                                                                                                     \
        (scratch)->targetDelta.vx       = (actor)->destination.vx - (rootCoord)->coord.t[0];                              \
        (scratch)->targetDelta.vy       = (actor)->destination.vy - (rootCoord)->coord.t[1];                              \
        (scratch)->targetDelta.vz       = (actor)->destination.vz - (rootCoord)->coord.t[2];                              \
        (actor)->scriptMotion.targetYaw = ratan2((scratch)->targetDelta.vx, (scratch)->targetDelta.vz);                   \
        (turnDelta)                     = playerActorShortestTurn((actor)->rotation.vy, (actor)->scriptMotion.targetYaw); \
        (scratch)->turnStep             = (turnDelta);                                                                    \
        if ((turnDelta) > (maxTurnStep)) {                                                                                \
            (scratch)->turnStep = (maxTurnStep);                                                                          \
        } else if ((turnDelta) < -(maxTurnStep)) {                                                                        \
            (scratch)->turnStep = -(maxTurnStep);                                                                         \
        } else if ((actor)->statePhase == ACTOR_800200_DESTINATION_START_PHASE) {                                         \
            (actor)->statePhase = ACTOR_800200_DESTINATION_TURN_PHASE;                                                    \
        }                                                                                                                 \
        (actor)->rotation.vy = ((actor)->rotation.vy + (scratch)->turnStep) & ACTOR_TRANSFORM_ANGLE_MASK;                 \
    }

/// Turns and walks or runs toward the stored route or target destination.
///
/// Requires live companion/model state and a destination in the root-parent
/// frame. Turns by at most 48 angle units per update (4096 per turn), then
/// narrows the speed-row argument in stateTimer into movementMode (5 walk,
/// 6 run). Returns to idle within 192 planar game units or on wall contact.
/// Borrows one PlayerActorApproachScratch block and retains route progress.
static void _actor800200TickApproach(Task* task)
{
    enum { ACTOR_800200_APPROACH_TURN_STEP     = 48,
           ACTOR_800200_APPROACH_ARRIVAL_LIMIT = 193 };
    PlayerActorApproachScratch* scratch;
    GfxCoord*                   rootCoord;
    GameActor*                  actor;
    s32                         turnDelta;
    s32                         animationId;
    s32                         movingPhase;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    scratch   = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorApproachScratch);
    ACTOR_800200_STEP_DESTINATION_TURN(actor, rootCoord, scratch, turnDelta, ACTOR_800200_APPROACH_TURN_STEP);
    // Finish the turn before consuming the requested speed row in stateTimer.
    switch (actor->statePhase) {
        case ACTOR_800200_DESTINATION_START_PHASE:
            movingPhase         = ACTOR_800200_DESTINATION_TURN_PHASE;
            actor->statePhase   = movingPhase;
            actor->movementSign = movingPhase;
            animationId         = ACTOR_800200_ANIMATION_TURN_POSITIVE;
            if (scratch->turnStep < 0) {
                animationId = ACTOR_800200_ANIMATION_TURN_NEGATIVE;
            }
            playerActorPlayChildSlots(task, animationId, 1);
        case ACTOR_800200_DESTINATION_TURN_PHASE:
            if (scratch->turnStep == 0) {
                actor->movementMode = actor->stateTimer;
                actor->statePhase++;
                animationId = ACTOR_800200_ANIMATION_RUN;
                if ((u16)actor->stateTimer == ACTOR_800200_MOVEMENT_WALK) {
                    animationId = ACTOR_800200_ANIMATION_WALK;
                }
                playerActorPlayChildSlotsWithBlend(task, animationId, 0, ACTOR_800200_MOVEMENT_BLEND_FRAMES);
            }
            break;
        case ACTOR_800200_DESTINATION_TRAVEL_PHASE:
            if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_APPROACH_ARRIVAL_LIMIT ||
                playerActorHasWallContact(task) != 0) {
                companionEnterIdle(task, 0);
            } else {
                actor->movementSign = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorApproachScratch);
}

/// Initializes a distance-dependent delay before the next action burst.
///
/// Requires live actor/root state and a writable twelve-byte targetPosition.
/// In engaged combat, stores the borrowed lock node and uses distance buckets
/// of 640 game units, clamped to 0..7: 0..127 random ticks plus 32 per bucket.
/// Otherwise uses player distance to subtract 48 per bucket from 0..511 random
/// ticks, with a 96-tick minimum. The caller decrements on this same update.
static inline void _actor800200InitWaitDelay(Task* task, GameActor* actor, const GfxCoord* rootCoord, VECTOR3* targetPosition)
{
    enum { ACTOR_800200_WAIT_BUCKET_UNITS        = 640,
           ACTOR_800200_WAIT_BUCKET_COUNT        = 8,
           ACTOR_800200_WAIT_TARGET_RANDOM_MASK  = 0x7F,
           ACTOR_800200_WAIT_TARGET_BUCKET_SHIFT = 5,
           ACTOR_800200_WAIT_PLAYER_RANDOM_MASK  = 0x1FF,
           ACTOR_800200_WAIT_PLAYER_BUCKET_TICKS = 48,
           ACTOR_800200_WAIT_PLAYER_MIN_TICKS    = 96 };
    WorldTargetNode* targetNode;
    s32              distanceBucket;
    s32              delayTicks;

    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        targetNode        = worldTargetFindLockNode(task);
        actor->targetNode = targetNode;
        if ((targetNode != NULL) && !(targetNode->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            worldTargetGetBodyPosition(targetNode, targetPosition);
            distanceBucket = playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), targetPosition);
            distanceBucket = distanceBucket / ACTOR_800200_WAIT_BUCKET_UNITS;
            if (distanceBucket >= ACTOR_800200_WAIT_BUCKET_COUNT) {
                distanceBucket = ACTOR_800200_WAIT_BUCKET_COUNT - 1;
            }
            actor->stateTimer = (rand() & ACTOR_800200_WAIT_TARGET_RANDOM_MASK) + (distanceBucket << ACTOR_800200_WAIT_TARGET_BUCKET_SHIFT);
            return;
        }
    }
    distanceBucket = companionGetPlayerPlanarDistance(rootCoord);
    distanceBucket = distanceBucket / ACTOR_800200_WAIT_BUCKET_UNITS;
    if (distanceBucket >= ACTOR_800200_WAIT_BUCKET_COUNT) {
        distanceBucket = ACTOR_800200_WAIT_BUCKET_COUNT - 1;
    }
    delayTicks        = (rand() & ACTOR_800200_WAIT_PLAYER_RANDOM_MASK) - (distanceBucket * ACTOR_800200_WAIT_PLAYER_BUCKET_TICKS);
    actor->stateTimer = delayTicks;
    if (delayTicks < ACTOR_800200_WAIT_PLAYER_MIN_TICKS) {
        actor->stateTimer = ACTOR_800200_WAIT_PLAYER_MIN_TICKS;
    }
}

/// Waits for a sampled delay, then selects a focus and starts one action burst.
///
/// Requires live companion/model/player state. Phase 0 initializes the delay
/// and falls through to the first decrement; phase 1 keeps counting active
/// updates. At expiry an engaged battle borrows a lock node, otherwise the
/// focus is the player. Keeps aim toward the player and retains route progress.
/// Reserves sixteen scratch bytes for the delay helper's twelve-byte point.
static void _actor800200TickTimedWait(Task* task)
{
    enum { ACTOR_800200_WAIT_INITIALIZE_PHASE = 0,
           ACTOR_800200_WAIT_COUNT_PHASE      = 1 };
    enum { ACTOR_800200_FOCUS_SCRATCH_BYTES = 16 };
    GameActor*      actor;
    GameActor*      actionActor;
    GameActor*      currentActor;
    GfxCoord*       rootCoord;
    const GfxCoord* playerCoord;
    VECTOR3*        targetPosition;
    s32             ticksRemaining;
    u16             targetVariant;
    u16             waitPhase;
    s32             actionCount;
    s32             countingPhase;

    playerCoord    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    targetPosition = SCRATCH_STACK_RESERVE_BYTES(ACTOR_800200_FOCUS_SCRATCH_BYTES);
    actor          = task->work;
    rootCoord      = task->extra.tmd->coords;
    waitPhase      = actor->statePhase;
    switch (waitPhase) {
        case ACTOR_800200_WAIT_INITIALIZE_PHASE:
            countingPhase     = ACTOR_800200_WAIT_COUNT_PHASE;
            actor->statePhase = countingPhase;
            _actor800200InitWaitDelay(task, actor, rootCoord, targetPosition);
        case ACTOR_800200_WAIT_COUNT_PHASE:
            ticksRemaining    = actor->stateTimer - 1;
            actor->stateTimer = ticksRemaining;
            if (ticksRemaining <= 0) {
                actionActor                                                  = task->work;
                actionCount                                                  = 1;
                actionActor->companionWork->activity.combat.repeatsRemaining = actionCount;
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    actionActor->targetNode = worldTargetFindLockNode(task);
                } else {
                    actionActor->targetNode = NULL;
                }
                targetVariant = actionActor->targetNode != 0;
                currentActor  = task->work;
                _actor800200ResetActionBurst(currentActor, targetVariant);
            }
            break;
    }
    playerActorTurnAimTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_800200_FOCUS_SCRATCH_BYTES);
}

/// Chooses a probe-clearance heading, turns onto it and advances for a bounded interval.
///
/// Requires scanAngle zero and scanClearance -1 on entry. Samples 32 headings
/// at 128 angle units (4096 per turn), preferring clear contact over distance.
/// Converts the selected yaw to absolute root-parent yaw and finishes turning
/// within 63 units using the retained raw heading difference. Moves for 20..51
/// combat updates or 60..187 other updates, stopping on nonzero clearance below
/// 833 game units. This handler advances scanAngle but does not rotate the probe.
static void _actor800200TickClearanceEscape(Task* task)
{
    enum { ACTOR_800200_ESCAPE_SCAN_PHASE = 0,
           ACTOR_800200_ESCAPE_TURN_PHASE = 1,
           ACTOR_800200_ESCAPE_MOVE_PHASE = 2 };
    enum { ACTOR_800200_ESCAPE_TURN_COMPLETE_LIMIT = 64,
           ACTOR_800200_ESCAPE_STOP_DISTANCE_LIMIT = 833,
           ACTOR_800200_ESCAPE_COMBAT_MIN_TICKS    = 20,
           ACTOR_800200_ESCAPE_COMBAT_RANDOM_MASK  = 0x1F,
           ACTOR_800200_ESCAPE_CALM_MIN_TICKS      = 60,
           ACTOR_800200_ESCAPE_CALM_RANDOM_MASK    = 0x7F,
           ACTOR_800200_ESCAPE_SIGN_TEST_SHIFT     = 16,
           ACTOR_800200_ESCAPE_RANDOM_DRAW_SHIFT   = 16 };
    GameActor*     actor;
    CompanionWork* companion;
    s32            distance;
    s32            turningPhase;
    s32            turnAnimationId;
    s32            turnSignTest;
    s32            movementAnimationId;
    s32            headingDistance;
    s32            phaseOrHeading;
    s32            currentHeading;
    u32            delayRandom;
    u16            absoluteHeading;

    actor          = task->work;
    companion      = actor->companionWork;
    distance       = _actor800200GetContactDistance(task->extra.tmd->coords, companion->probe.contacts, NULL);
    phaseOrHeading = actor->statePhase;
    turningPhase   = ACTOR_800200_ESCAPE_TURN_PHASE;
    // Scan 32 headings, then convert the chosen relative heading to absolute yaw.
    switch (phaseOrHeading) {
        case ACTOR_800200_ESCAPE_SCAN_PHASE:
            if (companion->scanAngle < ACTOR_TRANSFORM_ANGLE_TURN) {
                // Retain the best clearance until the sweep has completed.
                if ((companion->scanClearance != COMPANION_SCAN_CLEAR) && ((companion->scanClearance < distance) || (distance == COMPANION_SCAN_CLEAR))) {
                    companion->scanClearance = (s16)distance;
                    companion->targetHeading = (u16)companion->scanAngle;
                }
                companion->scanAngle = (u16)companion->scanAngle + COMPANION_SCAN_ANGLE_STEP;
                return;
            }
            actor->statePhase        = turningPhase;
            absoluteHeading          = ((u16)companion->targetHeading + actor->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
            companion->targetHeading = absoluteHeading;
            turnSignTest             = playerActorShortestTurn((s16)actor->rotation.vy, (s16)absoluteHeading) << ACTOR_800200_ESCAPE_SIGN_TEST_SHIFT;
            turnAnimationId          = ACTOR_800200_ANIMATION_TURN_NEGATIVE;
            if (turnSignTest > 0) {
                turnAnimationId    = ACTOR_800200_ANIMATION_TURN_POSITIVE;
                companion->turnDir = 1;
            } else {
                companion->turnDir = -1;
            }
            playerActorPlayChildSlotsWithBlend(task, turnAnimationId, 0, ACTOR_800200_ENTRY_BLEND_FRAMES);
            return;

        case ACTOR_800200_ESCAPE_TURN_PHASE:
            actor->turnSign = companion->turnDir;
            do {
                currentHeading  = (s16)actor->rotation.vy;
                phaseOrHeading  = companion->targetHeading;
                headingDistance = currentHeading - phaseOrHeading;
            } while (0);
            if (headingDistance < 0) {
                headingDistance = -headingDistance;
            }
            if (headingDistance < ACTOR_800200_ESCAPE_TURN_COMPLETE_LIMIT) {
                actor->statePhase += 1;
                actor->rotation.vy = (u16)companion->targetHeading;
                actor->turnSign    = 0;
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    movementAnimationId = ACTOR_800200_ANIMATION_RUN;
                    actor->movementMode = ACTOR_800200_MOVEMENT_RUN;
                    delayRandom         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = delayRandom;
                    delayRandom         = ((delayRandom >> ACTOR_800200_ESCAPE_RANDOM_DRAW_SHIFT) & ACTOR_800200_ESCAPE_COMBAT_RANDOM_MASK) + ACTOR_800200_ESCAPE_COMBAT_MIN_TICKS;
                    actor->stateTimer   = delayRandom;
                    playerActorPlayChildSlotsWithBlend(task, movementAnimationId, 0, ACTOR_800200_ENTRY_BLEND_FRAMES);
                } else {
                    movementAnimationId = ACTOR_800200_ANIMATION_WALK;
                    actor->movementMode = ACTOR_800200_MOVEMENT_WALK;
                    delayRandom         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = delayRandom;
                    delayRandom         = ((delayRandom >> ACTOR_800200_ESCAPE_RANDOM_DRAW_SHIFT) & ACTOR_800200_ESCAPE_CALM_RANDOM_MASK) + ACTOR_800200_ESCAPE_CALM_MIN_TICKS;
                    actor->stateTimer   = delayRandom;
                    playerActorPlayChildSlotsWithBlend(task, movementAnimationId, 0, ACTOR_800200_ENTRY_BLEND_FRAMES);
                }
                return;
            }
            return;

        // Advance only while clearance and the sampled movement interval permit.
        case ACTOR_800200_ESCAPE_MOVE_PHASE:
            if (((distance < ACTOR_800200_ESCAPE_STOP_DISTANCE_LIMIT) && (distance != 0)) || (--actor->stateTimer <= 0)) {
                companionEnterIdle(task, 0);
            } else {
                actor->movementSign = 1;
            }
            break;
    }
}

/// Turns and walks to a scripted parent-frame destination, then acknowledges arrival.
///
/// Scripted state 4 requires live native actor/model/child slots and approach
/// scratch. Turns by at most 64 angle units (4096 per turn), then selects walk
/// speed row 5. Default clip is 2 with an equipped weapon or 19 without one;
/// actionArgument overrides it. Both X/Z gaps must be below 105 game units to
/// clear pending motion, select state 1 and blend actionValue or idle clip 1.
/// Travel ticks footsteps; every call ticks child slots and releases scratch.
static void _actor800200TickScriptedWalkToDestination(Task* task)
{
    enum { ACTOR_800200_SCRIPT_WALK_TURN_STEP          = 64,
           ACTOR_800200_SCRIPT_WALK_AXIS_ARRIVAL_LIMIT = 105,
           ACTOR_800200_SCRIPT_WALK_FINISHED_STATE     = 1,
           ACTOR_800200_SCRIPT_WALK_NO_WEAPON_CLIP     = 19 };

    PlayerActorApproachScratch* scratch;
    GfxCoord*                   rootCoord;
    GameActor*                  actor;
    s32                         turnDelta;
    s32                         animationId;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    scratch   = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorApproachScratch);
    ACTOR_800200_STEP_DESTINATION_TURN(actor, rootCoord, scratch, turnDelta, ACTOR_800200_SCRIPT_WALK_TURN_STEP);
    // Turn first; only the travel phase advances translation and footsteps.
    switch (actor->statePhase) {
        case ACTOR_800200_DESTINATION_START_PHASE:
            actor->statePhase = ACTOR_800200_DESTINATION_TURN_PHASE;
            animationId       = ACTOR_800200_ANIMATION_TURN_POSITIVE;
            if (scratch->turnStep < 0) {
                animationId = ACTOR_800200_ANIMATION_TURN_NEGATIVE;
            }
            playerActorPlayChildSlots(task, animationId, 1);
        case ACTOR_800200_DESTINATION_TURN_PHASE:
            if (scratch->turnStep == 0) {
                actor->movementMode = ACTOR_800200_MOVEMENT_WALK;
                actor->statePhase++;
                if (actor->actionArgument == 0) {
                    animationId = ACTOR_800200_ANIMATION_WALK;
                    if (actor->equipmentTasks[1] == NULL) {
                        animationId = ACTOR_800200_SCRIPT_WALK_NO_WEAPON_CLIP;
                    }
                } else {
                    animationId = actor->actionArgument;
                }
                playerActorPlayChildSlotsWithBlend(task, animationId, 0, ACTOR_800200_MOVEMENT_BLEND_FRAMES);
            }
            break;
        case ACTOR_800200_DESTINATION_TRAVEL_PHASE:
            if (abs(rootCoord->coord.t[0] - actor->destination.vx) < ACTOR_800200_SCRIPT_WALK_AXIS_ARRIVAL_LIMIT) {
                if (abs(rootCoord->coord.t[2] - actor->destination.vz) < ACTOR_800200_SCRIPT_WALK_AXIS_ARRIVAL_LIMIT) {
                    actor->scriptedMotionPending = 0;
                    actor->state                 = ACTOR_800200_SCRIPT_WALK_FINISHED_STATE;
                    animationId                  = ACTOR_800200_ANIMATION_IDLE;
                    if (actor->actionValue != 0) {
                        animationId = actor->actionValue;
                    }
                    playerActorPlayChildSlotsWithBlend(task, animationId, 0, ACTOR_800200_MOVEMENT_BLEND_FRAMES);
                    break;
                }
            }
            actor->movementSign = 1;
            playerActorStepMovement(task);
            playerActorPlayFootstepCue(task);
            break;
    }
    playerActorTickChildSlots(task);
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorApproachScratch);
}

/// Turns and runs to a scripted destination, then acknowledges completion.
///
/// Scripted-mode state 8 requires live actor/model state and a destination in
/// the root-parent frame. Turns at most 64 angle units per update, selects speed
/// row 6 and default run clip 4, and permits an actionArgument clip override.
/// Arrival requires both X/Z differences below 105 game units; it clears the
/// pending flag, selects scripted state 1 and plays actionValue or idle clip 1.
/// Ticks child slots and releases its approach scratch block before returning.
static void _actor800200TickScriptedRunToDestination(Task* task)
{
    enum { ACTOR_800200_SCRIPT_RUN_TURN_STEP          = 64,
           ACTOR_800200_SCRIPT_RUN_AXIS_ARRIVAL_LIMIT = 105,
           ACTOR_800200_SCRIPT_RUN_FINISHED_STATE     = 1 };
    PlayerActorApproachScratch* scratch;
    GfxCoord*                   rootCoord;
    GameActor*                  actor;
    s32                         turnDelta;
    s32                         animationId;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    scratch   = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorApproachScratch);
    ACTOR_800200_STEP_DESTINATION_TURN(actor, rootCoord, scratch, turnDelta, ACTOR_800200_SCRIPT_RUN_TURN_STEP);
    // Turn first, then run until both parent-frame axes meet their limits.
    switch (actor->statePhase) {
        case ACTOR_800200_DESTINATION_START_PHASE:
            actor->statePhase = ACTOR_800200_DESTINATION_TURN_PHASE;
            animationId       = ACTOR_800200_ANIMATION_TURN_POSITIVE;
            if (scratch->turnStep < 0) {
                animationId = ACTOR_800200_ANIMATION_TURN_NEGATIVE;
            }
            playerActorPlayChildSlots(task, animationId, 1);
        case ACTOR_800200_DESTINATION_TURN_PHASE:
            if (scratch->turnStep == 0) {
                actor->movementMode = ACTOR_800200_MOVEMENT_RUN;
                actor->statePhase++;
                animationId = ACTOR_800200_ANIMATION_RUN;
                if (actor->actionArgument != 0) {
                    animationId = actor->actionArgument;
                }
                playerActorPlayChildSlotsWithBlend(task, animationId, 0, ACTOR_800200_MOVEMENT_BLEND_FRAMES);
            }
            break;
        case ACTOR_800200_DESTINATION_TRAVEL_PHASE:
            if (abs(rootCoord->coord.t[0] - actor->destination.vx) < ACTOR_800200_SCRIPT_RUN_AXIS_ARRIVAL_LIMIT) {
                if (abs(rootCoord->coord.t[2] - actor->destination.vz) < ACTOR_800200_SCRIPT_RUN_AXIS_ARRIVAL_LIMIT) {
                    actor->scriptedMotionPending = 0;
                    actor->state                 = ACTOR_800200_SCRIPT_RUN_FINISHED_STATE;
                    animationId                  = ACTOR_800200_ANIMATION_IDLE;
                    if (actor->actionValue != 0) {
                        animationId = actor->actionValue;
                    }
                    playerActorPlayChildSlotsWithBlend(task, animationId, 0, ACTOR_800200_MOVEMENT_BLEND_FRAMES);
                    break;
                }
            }
            actor->movementSign = 1;
            playerActorStepMovement(task);
            break;
    }
    playerActorTickChildSlots(task);
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorApproachScratch);
}

#undef ACTOR_800200_STEP_DESTINATION_TURN

/// Handlers `_actor800200TickNormalMode` runs, indexed by `state`.
static const TaskFuncTable12 D_actor_800200_80161E5C = { {
    _actor800200TickScheduleIdle,
    _actor800200TickPlayerFollow,
    _actor800200TickTargetTurn,
    _actor800200TickScheduleIdle,
    _actor800200TickActionBurst,
    _actor800200TickScheduleIdle,
    _actor800200TickScheduleIdle,
    _actor800200TickRest,
    _actor800200TickApproach,
    _actor800200TickTimedWait,
    _actor800200TickClearanceEscape,
    _actor800200TickRouteAnimation,
} };

/// Handlers `_actor800200TickScheduleIdle` runs, indexed by the low nibble of
/// the task's `spawnArg1`.
static const TaskFuncTable11 D_actor_800200_80161E8C = { {
    _actor800200TickFreeIdle,
    _actor800200TickSchedule1Route,
    _actor800200TickFreeIdle,
    _actor800200TickSchedule3Route,
    _actor800200TickFreeIdle,
    _actor800200TickFreeIdle,
    _actor800200TickFreeIdle,
    _actor800200TickSchedule7Route,
    _actor800200TickSchedules8To10Route,
    _actor800200TickSchedules8To10Route,
    _actor800200TickSchedules8To10Route,
} };

/// Handlers `_actor800200TickDamageMode` runs, indexed by `hitRegion`.
static const TaskFuncTable4 D_actor_800200_80161EB8 = { {
    _actor800200TickDamageRecovery,
    _actor800200TickDamageRecovery,
    _actor800200TickDamageRecovery,
    _actor800200HoldStoppedPose,
} };

/// Handlers `_actor800200TickScriptedMode` runs, indexed by `state`; the
/// gameplay entries are the player's own mode-2 state handlers.
static const TaskFuncTable9 D_actor_800200_80161EC8 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    _actor800200TickScriptedTurn,
    Gp_PlayerMode2State1,
    _actor800200TickScriptedWalkToDestination,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State1,
    _actor800200TickScriptedRunToDestination,
} };

/// Consumes a new companion animation cue and requests its surface footstep.
///
/// Requires live GameActor/root and room surfaceClass in 0..7 with loaded surface
/// tables. Only cues 1/2 return 1, even when no sound is queued; other or repeated
/// records return 0. Walk/run modes 5/6 map nine room walk bases to four paired
/// actor-bank sounds; cue 1 selects base + 1. An unmapped base retains zero but
/// cue 1 still requests sound 1. Running also latches the footstep action signal
/// when a footstep table exists. Queued sounds use signed-byte pan and depth.
static s32 _actor800200PlayFootstepCue(Task* task)
{
    GameActor*                             actor;
    const AnimationRecord*                 cueRecord;
    GfxCoord*                              rootCoord;
    const WorldCollisionSurfaceProperties* surface;
    const WorldCollisionFootstepSounds*    footstepSounds;
    s32                                    recognizedCue;
    s32                                    soundId;
    s8                                     cueBits;
    s32                                    pan;

    recognizedCue = 0;
    soundId       = WORLD_COLLISION_FOOTSTEP_SILENT;
    actor         = task->work;
    rootCoord     = task->extra.tmd->coords;
    cueRecord     = animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1);
    if (cueRecord != NULL && cueRecord != actor->lastCueRecord) {
        actor->lastCueRecord = cueRecord;
        switch (cueBits = cueRecord->flags & ANIMATION_RECORD_CUE_MASK) {
            case ANIMATION_RECORD_CUE_1:
            case ANIMATION_RECORD_CUE_2:
                surface        = Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][actor->surfaceClass];
                footstepSounds = surface->footstepSounds;
                if (footstepSounds != NULL) {
                    if ((u16)actor->movementMode - ACTOR_800200_MOVEMENT_WALK < 2U) {
                        switch (footstepSounds->walk) {
                            case 0x10000015:
                                soundId = SOUND_CHARACTER(SOUND_BANK_ACTOR_800200, 7);
                                break;
                            case 0x1000002D:
                                soundId = SOUND_CHARACTER(SOUND_BANK_ACTOR_800200, 3);
                                break;
                            case 0x1000001D:
                            case 0x10000049:
                                soundId = SOUND_CHARACTER(SOUND_BANK_ACTOR_800200, 1);
                                break;
                            case 0x1000003D:
                            case 0x10000041:
                            case 0x10000051:
                            case 0x10000059:
                            case 0x1000005D:
                                soundId = SOUND_CHARACTER(SOUND_BANK_ACTOR_800200, 5);
                                break;
                        }
                        // An unmapped room base remains zero; cue 1 still increments it to 1.
                        if (cueBits == ANIMATION_RECORD_CUE_1) {
                            soundId++;
                        }
                        if ((u16)actor->movementMode == ACTOR_800200_MOVEMENT_RUN) {
                            sceneLatchActionSignal(SCENE_COMBAT_ACTION_SIGNAL_FOOTSTEP);
                        }
                    }
                    if (soundId != WORLD_COLLISION_FOOTSTEP_SILENT) {
                        pan = (s8)worldCoordGetOriginAudioPan(rootCoord);
                        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
                        roomEffectRecordAnimationSoundCue(cueBits != ANIMATION_RECORD_CUE_2);
                    }
                }
                recognizedCue = 1;
                break;
        }
    }
    return recognizedCue;
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
    _actor800200PlayFootstepCue(arg0);
    actor->usesPushbackDirection = 0;
}

/// Enters normal behavior that follows the player with distance-dependent speed.
///
/// Requires live companion work/model and player. Resets the follow phase and
/// animation controller but retains movement mode, target and route progress;
/// the following update chooses movement and can switch to obstruction scanning.
static void _actor800200EnterPlayerFollow(Task* task)
{
    GameActor* actor = task->work;

    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = ACTOR_800200_STATE_PLAYER_FOLLOW;
    actor->turnRateIndex  = ACTOR_800200_TURN_DISABLED;
    actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
}

/// Enters normal behavior that turns toward the current target, or the player.
///
/// Requires live `GameActor` work and `CompanionWork`. Preserves `targetNode`, route
/// phase and waypoint index; callers select the target before entry. Resets the
/// turning behavior's phase and selects turn-rate row 2 (40 angle units per tick).
static void _actor800200EnterTargetTurn(Task* task)
{
    GameActor* actor = task->work;

    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = ACTOR_800200_STATE_TARGET_TURN;
    actor->turnRateIndex  = ACTOR_800200_TURN_TARGET;
    actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
}

/// Stops and blends into the companion's resting behavior.
///
/// Requires live model and initialized child animation slots. Plays set 7 with
/// a three-frame blend and one-shot completion tracking. The rest handler holds
/// set 9, then plays set 8 when the player moves away or combat engages. Retains
/// target and route progress.
static void _actor800200EnterRest(Task* task)
{
    GameActor* actor = task->work;

    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = ACTOR_800200_STATE_REST;
    actor->movementMode   = ACTOR_800200_MOVEMENT_STOPPED;
    actor->turnRateIndex  = ACTOR_800200_TURN_DISABLED;
    actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_ONCE;
    actor->statePhase     = 0;
    playerActorPlayChildSlotsWithBlend(task, ACTOR_800200_ANIMATION_REST, 0, ACTOR_800200_ENTRY_BLEND_FRAMES);
}

/// Enters normal behavior that turns and moves toward the stored destination.
///
/// Requires live `GameActor` work, `CompanionWork` and model. `movementMode` is speed
/// row 5 (walk) or 6 (run), staged in `stateTimer` until the turn has finished.
/// The approach ends within 192 planar game units or on wall contact, then
/// returns to idle; it leaves the route phase and waypoint index intact.
static void _actor800200EnterApproach(Task* task, s32 movementMode)
{
    GameActor* actor = task->work;

    actor->state          = ACTOR_800200_STATE_APPROACH;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = ACTOR_800200_MOVEMENT_WALK;
    actor->turnRateIndex  = ACTOR_800200_TURN_DISABLED;
    actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
    actor->stateTimer     = movementMode;
}

/// Initializes a stopped normal behavior while preserving target and route progress.
///
/// `actor` must be live task work. `state` is action burst (4) or timed wait (9)
/// for the current callers. Stops movement/turning, disables animation tracking
/// and resets only the behavior phase; callers initialize their own action data.
static inline void _actor800200EnterStationaryState(GameActor* actor, u16 state)
{
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = state;
    actor->movementMode   = ACTOR_800200_MOVEMENT_STOPPED;
    actor->turnRateIndex  = ACTOR_800200_TURN_DISABLED;
    actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
}

/// Enters a stationary burst of target-dependent animation and sound.
///
/// Requires live `GameActor` work and `CompanionWork`. `targetVariant` is 0 for the
/// player-directed action or 1 for the target-directed action; the handler selects
/// animation 10 + `targetVariant` and sound script 0x40720009 + `targetVariant`.
/// The caller supplies `targetNode` and `repeatsRemaining` before entry. Completion
/// can turn toward the focus or return to idle; route progress is retained.
static void _actor800200EnterActionBurst(Task* task, s16 targetVariant)
{
    GameActor* actor = task->work;

    _actor800200EnterStationaryState(actor, ACTOR_800200_STATE_ACTION_BURST);
    actor->attackControl.targetVariant = targetVariant;
}

/// Selects a battle target and starts a stationary animation/sound burst.
///
/// Requires live `GameActor` work and `CompanionWork`. `repeatsRemaining` is stored
/// in the counter byte; positive requests must fit `s8` (1..127). The action
/// handler consumes that byte as `s8` and can shorten a burst when its target
/// disappears. An engaged battle borrows the current lock node; otherwise
/// the focus is the player. The target remains borrowed while the action runs.
static void _actor800200StartActionBurst(Task* task, s8 repeatsRemaining)
{
    GameActor* actor = task->work;
    GameActor* currentActor;
    u16        targetVariant;

    actor->companionWork->activity.combat.repeatsRemaining = repeatsRemaining;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        actor->targetNode = worldTargetFindLockNode(task);
    } else {
        actor->targetNode = NULL;
    }
    targetVariant = actor->targetNode != NULL;
    currentActor  = task->work;
    _actor800200EnterStationaryState(currentActor, ACTOR_800200_STATE_ACTION_BURST);
    currentActor->attackControl.targetVariant = targetVariant;
}

/// Stops in idle animation before a distance-dependent wait and action burst.
///
/// Requires live `GameActor` work, `CompanionWork` and initialized child animation
/// slots. The wait handler initializes its own random tick count on its first
/// update. Blends idle set 1 for three normal-rate frames, retaining route
/// progress. `unusedArgument` is ignored and retained in the calling convention.
static void _actor800200EnterTimedWait(Task* task, s32 unusedArgument)
{
    GameActor* actor = task->work;

    _actor800200EnterStationaryState(actor, ACTOR_800200_STATE_TIMED_WAIT);
    playerActorPlayChildSlotsWithBlend(task, ACTOR_800200_ANIMATION_IDLE, 0, ACTOR_800200_ENTRY_BLEND_FRAMES);
}

/// Plays one route animation before returning to normal idle behavior.
///
/// Requires live `GameActor` work and initialized child animation slots. Stops
/// movement and turning, blends set 14 for three normal-rate frames, and uses
/// the one-shot animation controller to advance the completion phase. Keeps
/// the route phase and waypoint index for the next route update.
static void _actor800200EnterRouteAnimation(Task* task)
{
    GameActor* actor = task->work;

    actor->state          = ACTOR_800200_STATE_ROUTE_ANIMATION;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = ACTOR_800200_MOVEMENT_STOPPED;
    actor->turnRateIndex  = ACTOR_800200_TURN_DISABLED;
    actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_ONCE;
    actor->statePhase     = 0;
    playerActorPlayChildSlotsWithBlend(task, ACTOR_800200_ANIMATION_ROUTE, 0, ACTOR_800200_ENTRY_BLEND_FRAMES);
}

/// Runs the current Dryfield area route for companion schedule 1.
///
/// Requires a live companion task/model. A completed route enters timed wait;
/// otherwise dispatches the junk yard, garage, factory or driveway route.
/// Area loading supplies fresh, zeroed route progress for each room.
static void _actor800200TickSchedule1Route(Task* task)
{
    GameActor* actor = task->work;
    u8         areaId;

    if (actor->companionWork->routeComplete == COMPANION_ROUTE_COMPLETE) {
        _actor800200EnterTimedWait(task, 0);
        return;
    }
    areaId = gGameSession->location.loc.area;
    switch (areaId) {
        case GAME_AREA_DRYFIELD_JUNK_YARD:
            _actor800200TickArea26Route(task);
            return;
        case GAME_AREA_DRYFIELD_GARAGE:
            _actor800200TickArea24Route1(task);
            return;
        case GAME_AREA_DRYFIELD_FACTORY:
            _actor800200TickArea23Route1(task);
            return;
        case GAME_AREA_DRYFIELD_DRIVEWAY:
            _actor800200TickArea25Route1(task);
            return;
    }
}

/// Runs the current Dryfield-night area route for companion schedule 7.
///
/// Requires a live companion task/model. The driveway completes immediately;
/// factory, breezeway and water tower advance their own routes. A completed
/// route enters timed wait; route progress starts zeroed on each area load.
static void _actor800200TickSchedule7Route(Task* task)
{
    GameActor* actor = task->work;
    u8         areaId;

    if (actor->companionWork->routeComplete == COMPANION_ROUTE_COMPLETE) {
        _actor800200EnterTimedWait(task, 0);
        return;
    }
    areaId = gGameSession->location.loc.area;
    switch (areaId) {
        case GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY:
            _actor800200CompleteArea25Route7(task);
            return;
        case GAME_AREA_DRYFIELD_NIGHT_FACTORY:
            _actor800200TickArea23Route7(task);
            return;
        case GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY:
            _actor800200TickArea22Route(task);
            return;
        case GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER:
            _actor800200TickArea20Route7(task);
            return;
    }
}

/// Runs the current Shelter area route for companion schedules 8, 9 and 10.
///
/// Requires a live companion task/model. Schedule 8 selects the Shelter/Neo Ark
/// stage's heliport; schedule 9 selects its areas 1..5. Schedule 10 selects
/// Mine/Shelter stage areas 15, 19, 20
/// and 24. Area 16 has no route handler. Completed routes enter timed wait;
/// each area load creates fresh route progress.
static void _actor800200TickSchedules8To10Route(Task* task)
{
    GameActor* actor = task->work;
    u8         areaId;

    if (actor->companionWork->routeComplete == COMPANION_ROUTE_COMPLETE) {
        _actor800200EnterTimedWait(task, 0);
        return;
    }
    areaId = gGameSession->location.loc.area;
    switch (areaId) {
        case GAME_AREA_SHELTER_1F_PARKING_GARAGE:
            _actor800200TickArea1Route(task);
            return;
        case GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK:
            _actor800200TickArea2Route(task);
            return;
        case GAME_AREA_SHELTER_1F_BULWARK:
            _actor800200TickArea3Route(task);
            return;
        case GAME_AREA_SHELTER_1F_HELIPORT:
            _actor800200TickArea4Route(task);
            return;
        case GAME_AREA_SHELTER_1F_AIRLOCK:
            _actor800200TickArea5Route(task);
            return;
        case GAME_AREA_SHELTER_B1_MAIN_CORRIDOR:
            _actor800200TickArea15Route(task);
            return;
        case GAME_AREA_SHELTER_B1_ACCESS_TUNNEL:
            _actor800200TickArea19Route(task);
            return;
        case GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING:
            _actor800200TickArea20Route8To10(task);
            return;
        case GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL:
            _actor800200TickArea24Route10(task);
            return;
    }
}

/// Runs to the single Dryfield-garage stop for companion schedule 1.
///
/// Requires a live model and waypoint index zero, supplied by area spawning.
/// Retains current Y and runs until within 1024 planar game units, then latches
/// completion and enters timed wait. Keeps the original index-2 walk override,
/// although normal area initialization and this handler never select index 2.
static void _actor800200TickArea24Route1(Task* task)
{
    enum { ACTOR_800200_ROUTE_WALK_OVERRIDE_INDEX = 2 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* rootCoord;
    s32             movementMode;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    companion = actor->companionWork;
    if (actor->stateAux == ACTOR_800200_ROUTE_CHECK_END) {
        ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, D_actor_800200_8016A018, companion->waypointIndex);
        if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
            companion->routeComplete = COMPANION_ROUTE_COMPLETE;
            _actor800200EnterTimedWait(task, 0);
            return;
        }
        movementMode = ACTOR_800200_MOVEMENT_RUN;
        if (companion->waypointIndex == ACTOR_800200_ROUTE_WALK_OVERRIDE_INDEX) {
            movementMode = ACTOR_800200_MOVEMENT_WALK;
        }
        _actor800200EnterApproach(task, movementMode);
    }
}

/// Runs to the single Dryfield-driveway stop for companion schedule 1.
///
/// Requires a live model and waypoint index zero. Retains current Y and runs
/// until within 1024 planar game units, then latches completion and enters timed
/// wait. Area spawning zeroes the index; this handler never advances it.
static void _actor800200TickArea25Route1(Task* task)
{
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* rootCoord;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    companion = actor->companionWork;
    if (actor->stateAux == ACTOR_800200_ROUTE_CHECK_END) {
        ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, D_actor_800200_8016A040, companion->waypointIndex);
        if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
            companion->routeComplete = COMPANION_ROUTE_COMPLETE;
            _actor800200EnterTimedWait(task, 0);
            return;
        }
        _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
    }
}

/// Completes the schedule-7 route immediately in the Dryfield-night driveway.
///
/// Requires live companion work/model and initialized child slots. No waypoint
/// is read or moved to; latches completion and starts the stationary timed wait.
static void _actor800200CompleteArea25Route7(Task* task)
{
    GameActor* actor = task->work;

    actor->companionWork->routeComplete = COMPANION_ROUTE_COMPLETE;
    _actor800200EnterTimedWait(task, 0);
}

/// Runs to the single Shelter 1F bulwark stop for companion schedule 9.
///
/// Requires a live model and waypoint index zero, supplied by area spawning.
/// Retains current Y and runs until within 1024 planar game units. Arrival plays
/// a route animation unless already complete; the next idle update latches
/// completion and enters timed wait.
static void _actor800200TickArea3Route(Task* task)
{
    enum { ACTOR_800200_BULWARK_APPROACH = 0,
           ACTOR_800200_BULWARK_COMPLETE = 1 };
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* rootCoord;
    u32             routePhase;

    actor      = task->work;
    rootCoord  = task->extra.tmd->coords;
    routePhase = actor->stateAux;
    companion  = actor->companionWork;
    switch (routePhase) {
        case ACTOR_800200_BULWARK_APPROACH:
            ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, D_actor_800200_8016A090, companion->waypointIndex);
            if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
                actor->stateAux++;
                if (companion->routeComplete != COMPANION_ROUTE_COMPLETE) {
                    _actor800200EnterRouteAnimation(task);
                }
                return;
            }
            _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
            return;
        case ACTOR_800200_BULWARK_COMPLETE:
            companion->routeComplete = routePhase;
            _actor800200EnterTimedWait(task, 0);
            break;
    }
}

/// Runs to the single Shelter B1 transfer-tunnel stop for schedule 10.
///
/// Requires a live model and waypoint index zero. Retains current Y and runs
/// until within 1024 planar game units, then latches completion and plays one
/// route animation. Area spawning zeroes the index; this handler never advances it.
static void _actor800200TickArea24Route10(Task* task)
{
    GameActor*      actor;
    CompanionWork*  companion;
    const GfxCoord* rootCoord;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    companion = actor->companionWork;
    if (actor->stateAux == ACTOR_800200_ROUTE_CHECK_END) {
        ACTOR_800200_SET_ROUTE_DESTINATION(actor, rootCoord, D_actor_800200_8016A128, companion->waypointIndex);
        if (playerActorPlanarDistance(MATRIX_TRANS(&rootCoord->coord), &actor->destination) < ACTOR_800200_ROUTE_INITIAL_DISTANCE_LIMIT) {
            companion->routeComplete = COMPANION_ROUTE_COMPLETE;
            _actor800200EnterRouteAnimation(task);
            return;
        }
        _actor800200EnterApproach(task, ACTOR_800200_MOVEMENT_RUN);
    }
}

#undef ACTOR_800200_SET_ROUTE_DESTINATION

/// Advances the companion's normal behavior, contact reaction and movement.
///
/// Requires live actor/companion work, model root and loaded state handlers;
/// `GameActor.state` is an unchecked index in 0..11. Counts down a positive
/// decision timer before dispatch. A zero signed recovery byte permits body
/// contacts; a nonzero unsigned hit-region halfword enters damage mode and
/// requests the spatial hurt cue. Animation, child tracks, facing and movement
/// then update even if that reaction changed mode.
static void _actor800200TickNormalMode(Task* task)
{
    GameActor*            actor;
    CompanionWork*        companion;
    GfxCoord*             rootCoord;
    const TaskFuncTable12 stateHandlers = D_actor_800200_80161E5C;
    s32                   audioPan;

    actor     = task->work;
    companion = actor->companionWork;
    rootCoord = task->extra.tmd->coords;
    if (companion->decisionTimer > 0) {
        companion->decisionTimer--;
    }
    // State selection precedes contact reactions and the common movement update.
    stateHandlers.funcs[actor->state](task);
    if ((s8)actor->recoveryTicks == 0) {
        playerActorResolveBodyContacts(task, actor->collisionContacts);
        if ((u16)actor->hitRegion != 0) {
            companionEnterDamageReaction(task);
            audioPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(SOUND_ACTOR_800200_HURT, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
    }
    playerActorTickAnimationState(task);
    playerActorTickChildSlots(task);
    playerActorUpdateFacing(task);
    playerActorStepMovement(task);
}

/// Dispatches idle behavior for the companion-2 schedule captured when the task spawned.
///
/// The spawn argument's low nibble must be 0..10, as established by the schedule
/// flag writers and companion setup; the mask does not admit table slots 11..15.
/// Idle states 0, 3, 5 and 6 share this dispatcher. Schedule changes take effect
/// when a new task is spawned, rather than by rereading the flag each update.
static void _actor800200TickScheduleIdle(Task* task)
{
    enum { ACTOR_800200_SPAWN_SCHEDULE_MASK = 0xF };
    TaskFuncTable11 handlers;

    handlers = D_actor_800200_80161E8C;
    handlers.funcs[task->spawnArg1.value & ACTOR_800200_SPAWN_SCHEDULE_MASK](task);
}

/// Holds the resting pose, then gets up when the player moves away or combat engages.
///
/// Requires live companion/model/player state. Animation completion advances
/// phase 0 to 1; phase 1 installs hold clip 9 and checks phase 2 immediately.
/// At 1280 planar game units or engaged combat it plays exit clip 8; controller
/// completion advances phase 3 to 4 and returns to idle. Aim tracks the player.
static void _actor800200TickRest(Task* task)
{
    enum { ACTOR_800200_REST_ENTERED_PHASE   = 1,
           ACTOR_800200_REST_HOLD_PHASE      = 2,
           ACTOR_800200_REST_EXIT_PHASE      = 3,
           ACTOR_800200_REST_FINISHED_PHASE  = 4,
           ACTOR_800200_REST_PLAYER_DISTANCE = 1280 };
    GameActor*      actor;
    GfxCoord*       rootCoord;
    const GfxCoord* playerCoord;

    rootCoord   = task->extra.tmd->coords;
    playerCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor       = task->work;
    switch (actor->statePhase) {
        case ACTOR_800200_REST_ENTERED_PHASE:
            actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_NONE;
            actor->statePhase    += 1;
            playerActorResetChildSlots(task, ACTOR_800200_ANIMATION_REST_HOLD);
        case ACTOR_800200_REST_HOLD_PHASE:
            if ((companionGetPlayerPlanarDistance(rootCoord) >= ACTOR_800200_REST_PLAYER_DISTANCE) || (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED)) {
                actor->animationState = ACTOR_800200_ANIMATION_CONTROLLER_ONCE;
                actor->statePhase    += 1;
                playerActorPlayChildSlotsWithBlend(task, ACTOR_800200_ANIMATION_REST_EXIT, 0, ACTOR_800200_ENTRY_BLEND_FRAMES);
            }
            break;
        case ACTOR_800200_REST_FINISHED_PHASE:
            companionEnterIdle(task, 0);
            break;
        default:
        case 0:
        case ACTOR_800200_REST_EXIT_PHASE:
            break;
    }
    playerActorTurnAimTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
}

/// Returns to companion idle when the one-shot route animation completes.
///
/// Animation controller 7 advances phase 0 to 1. Other phases leave behavior
/// intact; completion preserves the route phase and waypoint index.
static void _actor800200TickRouteAnimation(Task* task)
{
    enum { ACTOR_800200_ROUTE_ANIMATION_PLAYING_PHASE  = 0,
           ACTOR_800200_ROUTE_ANIMATION_FINISHED_PHASE = 1 };
    const GameActor* actor = task->work;
    u16              phase = actor->statePhase;

    if (phase != ACTOR_800200_ROUTE_ANIMATION_PLAYING_PHASE) {
        if (phase == ACTOR_800200_ROUTE_ANIMATION_FINISHED_PHASE) {
            companionEnterIdle(task, 0);
        }
    }
}

/// Advances Flint's animation, damage response, facing and movement in that order.
///
/// Requires live GameActor/model/native child slots and damage selector 0..3.
/// Selectors 0..2 finish damage recovery; selector 3 holds the stopped pose.
/// Animation can advance the response phase before the selected callback runs.
static void _actor800200TickDamageMode(Task* task)
{
    TaskFuncTable4 handlers;
    GameActor*     actor;

    handlers = D_actor_800200_80161EB8;
    actor    = task->work;
    playerActorTickAnimationState(task);
    playerActorTickChildSlots(task);
    handlers.funcs[(u16)actor->hitRegion](task);
    playerActorUpdateFacing(task);
    playerActorStepMovement(task);
}

/// Finishes ordinary damage reactions through the shared player-side recovery handler.
///
/// Damage selectors 0..2 share this callback. Requires live native actor/model
/// state; phase 1 clears the pending hit and arms 18 recovery ticks. A retained
/// zero state enters normal idle; a nonzero state enters player-follow state 1
/// through the shared aim entry with its twelve-frame blend. Other phases leave
/// the reaction active.
static void _actor800200TickDamageRecovery(Task* task)
{
    playerActorFinishDamageReaction(task);
}

/// Holds the stopped damage pose without recovery; the task parameter is unused.
///
/// The stopped-pose entry selects damage selector 3 and native clip 18. Normal
/// damage dispatch keeps animation/facing updates running around this no-op.
static void _actor800200HoldStoppedPose(Task* unusedTask)
{
}

/// Dispatches Flint's scripted state and rebuilds the root from its Euler angles.
///
/// Requires live actor/model state and a valid scripted state index 0..8.
/// States 2, 4 and 8 handle turn, walk and run commands; other slots use resident
/// player handlers. Rotation uses 4096 units per turn. The outer frame update
/// invalidates/composes the root after dispatch; translation stays intact here.
static void _actor800200TickScriptedMode(Task* task)
{
    TaskFuncTable9 states;
    GameActor*     actor;
    GfxCoord*      rootCoord;

    states    = D_actor_800200_80161EC8;
    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    states.funcs[actor->state](task);
    RotMatrix(&actor->rotation, &rootCoord->coord);
}

/// Turns Flint toward a scripted yaw by up to 48 angle units per active call.
///
/// Scripted state 2 requires live actor/model/native child slots. Angles use
/// 4096 units per turn. Within 48 units of the stored target or its lower
/// one-turn image, snaps to the stored halfword, clears pending motion and
/// selects idle state/clip 1 with a five-frame blend. Otherwise enables forward
/// walk-speed movement while stepping wrapped yaw. Always ticks child slots;
/// this handler itself does not advance translation.
static void _actor800200TickScriptedTurn(Task* task)
{
    enum { ACTOR_800200_SCRIPTED_TURN_STEP = 48 };

    GameActor* actor;
    s16        currentYaw;
    s16        targetYaw;
    u16        storedTargetYaw;
    s32        yawDistance;
    s32        wrappedTargetYaw;
    s32        turnDelta;
    s32        idleStateAndClip;

    actor           = task->work;
    currentYaw      = actor->rotation.vy;
    targetYaw       = actor->scriptMotion.targetYaw;
    storedTargetYaw = actor->scriptMotion.targetYaw;
    yawDistance     = currentYaw - targetYaw;
    if (yawDistance < 0) {
        yawDistance = -yawDistance;
    }
    // Compare the stored target and its lower one-turn image before stepping.
    if (yawDistance < ACTOR_800200_SCRIPTED_TURN_STEP + 1 || (wrappedTargetYaw = targetYaw - ACTOR_TRANSFORM_ANGLE_TURN, yawDistance = currentYaw - wrappedTargetYaw, yawDistance = ABS(yawDistance), yawDistance < ACTOR_800200_SCRIPTED_TURN_STEP + 1)) {
        idleStateAndClip             = ACTOR_800200_ANIMATION_IDLE;
        actor->rotation.vy           = storedTargetYaw;
        actor->scriptedMotionPending = 0;
        actor->state                 = idleStateAndClip;
        playerActorPlayChildSlotsWithBlend(task, idleStateAndClip, 0, ACTOR_800200_MOVEMENT_BLEND_FRAMES);
    } else {
        turnDelta = playerActorShortestTurn(currentYaw, targetYaw);
        if (turnDelta > ACTOR_800200_SCRIPTED_TURN_STEP) {
            turnDelta = ACTOR_800200_SCRIPTED_TURN_STEP;
        } else if (turnDelta < -ACTOR_800200_SCRIPTED_TURN_STEP) {
            turnDelta = -ACTOR_800200_SCRIPTED_TURN_STEP;
        }
        actor->movementMode = ACTOR_800200_MOVEMENT_WALK;
        actor->movementSign = 1;
        actor->rotation.vy  = ((u16)actor->rotation.vy + turnDelta) & ACTOR_TRANSFORM_ANGLE_MASK;
    }
    playerActorTickChildSlots(task);
}

/// Measures a recorded probe contact from an already composed coordinate origin.
///
/// Borrows one contact and reads the cached world translation without composing
/// it. Returns planar world-game units, or zero for an empty key; a coincident
/// contact also returns zero. Horizontal differences, their squares and their
/// sum must fit s32. Optional `contactZY` must provide two writable
/// halfwords and receives the signed coordinate bits as (Z, Y). The original
/// X, Y, Z write order is retained, so Z overwrites X in the first halfword.
static s32 _actor800200GetContactDistance(const GfxCoord* coord, const WorldCollisionContact* contact, u16* contactZY)
{
    s32 distance;

    if (contact->key.value != 0) {
        distance = playerActorPlanarLength(coord->workm.t[0] - contact->point.vx, coord->workm.t[2] - contact->point.vz);
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
