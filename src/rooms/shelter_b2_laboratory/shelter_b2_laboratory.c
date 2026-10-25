#include "rooms/shelter_b2_laboratory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_143000.h"
#include "actors/actor_143000.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stage_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
// Symbol is `RoomCutsceneSoundTaskStorage`; the runner reads `task`.
#define ROOM_CUTSCENE_SOUND_TASK gRoomCutsceneSoundTask.task
#include "../../shared/room/room_cutscene.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// Scratch block this room's diamond glow drawer takes from the scratch stack
/// for one projected centre.
///
/// One perspective transform of a world point through `gGfxViewCoord.workm`
/// writes the screen position and the GTE flag word. A negative flag word
/// means the transform reported an error, and the drawer links nothing.
/// Otherwise it stores the ordering-table depth and the on-screen half-extent,
/// a size divided by that depth, and builds the diamonds around the centre.
/// `sx` and `sy` are written by one screen-XY store, so they stay adjacent.
///
/// `GlowCentreScratch` holds the same four values in 16 bytes. This record
/// keeps eight more bytes between the half-extent and the screen position;
/// the drawer reserves and releases them with the rest of the block and never
/// reads or writes them, so the records stay separate.
typedef struct {
    s32 otz;        // Ordering-table depth of the centre; also the divisor for the half-extent
    s32 flag;       // GTE flag word; negative means the transform reported an error
    s32 radius;     // On-screen half-extent of the diamonds around the centre
    u8  unknown[8]; // Reserved with the block and never accessed; role and grouping unproven
    u16 sx;         // Projected centre, x
    u16 sy;         // Projected centre, y
} _ShelterB2LaboratoryGlowDiamondScratch;
STATIC_ASSERT_SIZEOF(_ShelterB2LaboratoryGlowDiamondScratch, 0x18);

/// View saved when the cutscene starts and restored when it ends.

/// Task descriptor for the flag-gated exit's transition.

/// This room's cutscene sound-task slot. The runner uses `task`.
extern RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask;

/// Copy of the request that started the pending exit
/// transition, read back by `roomEventTask`.
extern RoomEventReq gRoomEventReq;

/// Set when `_roomEventGate` started a transition,
/// cleared on every other call.
extern u8 gRoomEventActive;

// Keep this first declaration before included telephone storage for BSS order.
static u16 _gShelterB2LaboratoryFastGlowPulse;

#include "../../shared/room/telephone.h"

static s32  _shelterB2LaboratoryRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
static void _shelterB2LaboratoryInitRoomTask(Task* task);
static void _shelterB2LaboratoryIdleMessageTask(Task* task);
static void _shelterB2LaboratoryDrawGlowDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _shelterB2LaboratorySetFastGlowPulse(s16 enabled);

/// Values used by room events to select the cyan glow's pulse rate.
enum {
    SHELTER_B2_LABORATORY_GLOW_PULSE_SLOW = 0,
    SHELTER_B2_LABORATORY_GLOW_PULSE_FAST = 1,
};

static s32  _shelterB2LaboratoryCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _shelterB2LaboratoryResolveRoomTransition(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB2LaboratoryConsoleActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _shelterB2LaboratoryCueSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueId, s32 unusedSecondArg);
static void _shelterB2LaboratoryAmbienceTask(Task* task);
static void _shelterB2LaboratoryConsoleKeypadTask(Task* task);
static void _shelterB2LaboratoryFinishConsoleSceneTask(Task* task);

#include "../../shared/room/telephone_data.inc.c"

#include "assets/rooms/item_box/model.inc"
DEFINE_ALIAS(_gItemBoxModel, gShelterB2LaboratoryItemBoxModel);

/// This carrier's bodyless scene runner and timed sound task, followed by END.
///
/// Entry SCENE borrows a RoomCutsceneRec in spawnArg2 and takes the follow-up
/// CAP command in spawnArg1; SOUND takes a packed sound request in spawnArg2.
/// Both run at priority 32. Keep callback code and the scene record loaded
/// until completion; spawning copies the recipe and retains no table pointer.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// Laboratory room-message routes, with five handlers followed by the end marker.
///
/// Transition requests borrow request/reply records; key-item requests carry
/// an item ID, directed actions a borrowed request, and commands/sounds an
/// integer selector. The room task borrows this table and callback code until
/// teardown. Unsupported IDs return zero through the final sentinel.
static TaskMessageEntry _gShelterB2LaboratoryRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB2LaboratoryResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB2LaboratoryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2LaboratoryConsoleActionMessage },
    { ROOM_MESSAGE_COMMAND, _shelterB2LaboratoryCommandMessage },
    { ROOM_MESSAGE_SOUND, _shelterB2LaboratoryCueSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Live console keypad task borrowed until its requested exit is collected.
///
/// NULL before spawning and after a successful poll releases the keypad.
/// Only one console waiter may use this slot; keep the room and keypad actor
/// loaded until it is cleared. Spawn failure is not handled by the waiter.
static Task* _gShelterB2LaboratoryKeypadTask = NULL;

/// Descriptor indices of the laboratory's background tasks.
enum {
    SHELTER_B2_LABORATORY_TASK_CONSOLE_KEYPAD       = 0,
    SHELTER_B2_LABORATORY_TASK_AMBIENCE             = 1,
    SHELTER_B2_LABORATORY_TASK_FINISH_CONSOLE_SCENE = 2,
    SHELTER_B2_LABORATORY_TASK_COUNT                = 3,
};

/// Spawn recipes for the console keypad waiter, ambience and post-scene follow-up.
///
/// Three bodyless tasks at priority `TASK_PRIORITY_SCENE`, with unused spawn payloads and models.
/// No end marker: callers select only the named descriptor indices. Spawning
/// copies the recipe; this overlay's callback code must outlive each task.
static TaskDesc _gShelterB2LaboratoryTaskDescriptors[SHELTER_B2_LABORATORY_TASK_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB2LaboratoryConsoleKeypadTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB2LaboratoryAmbienceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB2LaboratoryFinishConsoleSceneTask, { .value = 0 } },
};

/// Looping ambience gain percentages indexed by the active logical view, 0..15.
///
/// Entry 0 is silent; 0 means silence and 100 full gain. Retuning converts gain
/// to attenuation as 127 - gain * 127 / 100, with signed integer division.
/// This table is not used for the initial spatial mix. No terminator.
static s8 _gShelterB2LaboratoryAmbienceGainPercentByView[SHELTER_B2_LABORATORY_VIEW_COUNT + 1] = {
    0,
    0,
    60,
    30,
    45,
    100,
    75,
    40,
    30,
    0,
    0,
    0,
    0,
    0,
    0,
    100,
};

/// World-space endpoint pairs and the laboratory's pulsing cyan centre.
///
/// Each capsule borrows exactly two consecutive signed-halfword positions in
/// room-coordinate units. The centre is one point shared by the diamond and
/// disc drawers; the SVECTOR fourth halfwords are unused by projection.
typedef struct {
    SVECTOR capsuleEndpoints[22][2]; // Two endpoints for each authored capsule, in room units
    SVECTOR cyanCentre;              // Centre of the pulsing cyan diamond or disc, in room units
} _ShelterB2LaboratoryGlowGeometry;
STATIC_ASSERT_SIZEOF(_ShelterB2LaboratoryGlowGeometry, 0x168);

/// Laboratory glow geometry selected by the current mapped view.
///
/// Twenty-two endpoint pairs followed by one cyan centre. Drawing borrows the
/// points only during each call; this room overlay owns their persistent storage.
static _ShelterB2LaboratoryGlowGeometry _gShelterB2LaboratoryGlowGeometry = {
    .capsuleEndpoints = {
        { { 182, -1966, 5669, 0 }, { 823, -1966, 5669, 0 } },
        { { 3202, -1966, 5669, 0 }, { 3843, -1966, 5669, 0 } },
        { { 1642, -2807, 4491, 0 }, { 2462, -2807, 4491, 0 } },
        { { 1642, -2807, 4351, 0 }, { 2462, -2807, 4351, 0 } },
        { { 1709, -2807, 1442, 0 }, { 2529, -2807, 1442, 0 } },
        { { 1709, -2807, 1302, 0 }, { 2529, -2807, 1302, 0 } },
        { { 1709, -2807, -1369, 0 }, { 2529, -2807, -1369, 0 } },
        { { 1709, -2807, -1511, 0 }, { 2529, -2807, -1511, 0 } },
        { { 8935, -3150, 1448, 0 }, { 9762, -3150, 1448, 0 } },
        { { 8935, -3150, 1306, 0 }, { 9762, -3150, 1306, 0 } },
        { { 8902, -3150, -1343, 0 }, { 9721, -3150, -1343, 0 } },
        { { 8902, -3150, -1483, 0 }, { 9721, -3150, -1483, 0 } },
        { { 12886, -2946, -1615, 0 }, { 12886, -2946, -2278, 0 } },
        { { -339, -2195, 2398, 0 }, { -339, -2195, 1547, 0 } },
        { { 6018, -2111, 3328, 0 }, { 6806, -2111, 3328, 0 } },
        { { -278, -2407, -1452, 0 }, { -278, -2407, -1091, 0 } },
        { { -278, -2407, -780, 0 }, { -278, -2407, -418, 0 } },
        { { -278, -2407, -97, 0 }, { -278, -2407, 262, 0 } },
        { { 11312, -3085, -362, 0 }, { 11312, -3085, -1, 0 } },
        { { 11312, -3085, 1054, 0 }, { 11312, -3085, 1418, 0 } },
        { { 11312, -3085, 2483, 0 }, { 11312, -3085, 2846, 0 } },
        { { 3100, -1163, -966, 0 }, { 3472, -1163, -966, 0 } },
    },
    .cyanCentre = { 3181, -1161, -3312, 0 },
};

u8* gShelterB2LaboratoryRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB2LaboratoryRoomViewCounts[1] = { SHELTER_B2_LABORATORY_VIEW_COUNT };

/// Optional map-marker nibble associated with the corridor warp endpoint.
enum { SHELTER_B2_LABORATORY_CORRIDOR_ENDPOINT_MAP_FLAG = 0x1C3 };

DirectionWarpEntry gShelterB2LaboratoryWarpEntries[2] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 100, 0, 1800 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 100, 0, 1800 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 4), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 3), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, SHELTER_B2_LABORATORY_CORRIDOR_ENDPOINT_MAP_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 6350, 0, 2900 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 6350, 0, 2900 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 2), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 1), DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gShelterB2LaboratoryCollisionGridNormals[23] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_normals.inc"
};

/// Collision mesh vertices in signed room-coordinate units.
static SVECTOR _gShelterB2LaboratoryCollisionGridVerts[123] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_verts.inc"
};

/// Indexed collision faces and their room-local surface classes.
static WorldCollisionGridFace _gShelterB2LaboratoryCollisionGridFaces[51] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_faces.inc"
};

/// Cell face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB2LaboratoryCollisionGridCells[220] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB2LaboratoryCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..219
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B2_LABORATORY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB2LaboratoryCollisionGridCells[(halfwordOffset)])
/// X-major 4-by-3 directory of face-index lists in the cell pool.
static s16* _gShelterB2LaboratoryCollisionGridTable[12] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_table.inc"
};
#undef SHELTER_B2_LABORATORY_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB2LaboratoryCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB2LaboratoryCollisionGridNormals,
    .vertices    = _gShelterB2LaboratoryCollisionGridVerts,
    .faces       = _gShelterB2LaboratoryCollisionGridFaces,
    .cellFaceIds = _gShelterB2LaboratoryCollisionGridTable,
    .xBias       = 1411,
    .zBias       = 4736,
    .cellCountX  = 4,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB2LaboratoryCollisionGridFaces),
};

ViewCamera gShelterB2LaboratoryViewCameras[SHELTER_B2_LABORATORY_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -6200, 0x7530, -1480 } }, 564 },
    { { { { 1318, 0, 3878 }, { 1296, 3860, -440 }, { -3655, 1369, 1242 } }, { -7992, 1967, -470 } }, 230 },
    { { { { 3662, 0, 1834 }, { 408, 3992, -816 }, { -1788, 912, 3570 } }, { -4133, 1597, -776 } }, 230 },
    { { { { 1559, 0, -3787 }, { -761, 4012, -313 }, { 3709, 823, 1527 } }, { -3665, 1388, -448 } }, 230 },
    { { { { 922, 0, 3990 }, { 799, 4012, -184 }, { -3909, 820, 904 } }, { -8742, 1568, 3074 } }, 230 },
    { { { { 988, 0, -3974 }, { -1089, 3939, -270 }, { 3822, 1122, 950 } }, { -4027, 2154, 3224 } }, 230 },
    { { { { 1535, 0, -3797 }, { -1082, 3926, -437 }, { 3639, 1167, 1471 } }, { -8309, 3056, 3056 } }, 230 },
    { { { { 3861, 0, -1367 }, { -277, 4010, -783 }, { 1338, 830, 3780 } }, { -9759, 2782, 2019 } }, 230 },
    { { { { -3525, 0, -2084 }, { 56, 4094, -95 }, { 2083, -110, -3524 } }, { -2270, 1220, -572 } }, 380 },
    { { { { 3982, 0, -958 }, { 27, 4094, 112 }, { 958, -115, 3980 } }, { -2860, 1220, 3450 } }, 541 },
    { { { { 4095, 0, 0 }, { 0, 4091, -197 }, { 0, 197, 4091 } }, { 0x2E72, 1215, 1870 } }, 541 },
    { { { { -794, 0, 4018 }, { -178, 4091, -35 }, { -4014, -182, -793 } }, { -6219, 895, 1568 } }, 269 },
    { { { { -4036, 0, 695 }, { 136, 4016, 793 }, { -682, 804, -3957 } }, { -3499, 1404, 2437 } }, 269 },
    { { { { 4095, 0, 0 }, { 0, 4091, -197 }, { 0, 197, 4091 } }, { 0x2E72, 1215, 1870 } }, 541 },
    { { { { 862, 0, 4004 }, { 1204, 3906, -259 }, { -3818, 1231, 822 } }, { -6593, 1887, 2833 } }, 246 },
};

/// View 1's empty sprite list; its zero first count keeps decoded image strips.
static SpriteBatch _gShelterB2LaboratoryView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 51 texture rectangles, positioned in view pixels with unscaled OT depths.
static SpriteSource _gShelterB2LaboratoryView2SpriteSources[51] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 40, 0, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 16, 0, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 24, 0, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 32, 0, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 16, 535, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 24, 521, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 40, 519, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 48, 474, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 48, 504, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 64, 471, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 64, 423, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 80, 402, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 80, 510, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 48, 0, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 88, 507, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 104, 501, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 80, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 112, 504, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 96, 351, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -8, 0, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, -48, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 0, 0, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 8, 0, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 16, 0, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, -72, 743, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, -72, 783, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -32, 826, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, -72, 856, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -40, 923, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -72, 965, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, -24, 994, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -72, 1011, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -32, 1042, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -32, 1096, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -72, 1065, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -72, 1110, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -32, 1159, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 16, 1140, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 8, 1212, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -8, 1194, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 1147, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 0, 1036, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -72, 1065, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -72, 1100, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -40, 1144, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -24, 1162, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -72, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -24, 0, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -16, 1447, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -24, 1456, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -24, 1480, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
};

/// View 2's image-strip selector and sprite ranges [0,19), [19,47), [47,51).
static SpriteBatch _gShelterB2LaboratoryView2SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 28, 0, 0, { 2, 0 } },
    { 47, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 20 texture rectangles, positioned in view pixels with unscaled OT depths.
static SpriteSource _gShelterB2LaboratoryView3SpriteSources[20] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -80, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -24, 0, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -24, 0, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 16, 0, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -80, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -80, 375, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 112, -56, 375, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 112, -32, 375, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, 0, 375, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 104, 24, 375, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, 56, 0, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 64, 0, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 88, 0, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 96, 375, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 88, 375, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 80, 375, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 56, 375, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 64, 375, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 88, 375, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 104, 0, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
};

/// View 3's image-strip selector and one range covering its complete sprite array.
static SpriteBatch _gShelterB2LaboratoryView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB2LaboratoryView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 30 texture rectangles, positioned in view pixels with unscaled OT depths.
static SpriteSource _gShelterB2LaboratoryView4SpriteSources[30] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -40, 1397, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -40, 1311, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 80, -40, 1267, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -40, 1048, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 16, 0, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 40, 0, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 72, 609, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 136, 48, 564, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 136, 16, 555, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 0, 560, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -88, 0, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -128, -64, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 0, 0, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, 24, 0, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 56, 678, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 64, 0, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 56, 709, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 48, 730, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 32, 736, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 32, 738, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 16, 727, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 0, 690, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 0, 643, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -88, 638, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -64, 636, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -40, 630, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, -16, 637, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -64, 636, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -40, 642, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -16, 648, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
};

/// View 4's image-strip selector and sprite ranges [0,4), [4,10), [10,30).
static SpriteBatch _gShelterB2LaboratoryView4SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 6, 0, 0, { 2, 0 } },
    { 10, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 23 authored sprite rectangles in view pixels with unscaled OT depths.
///
/// Its batch selects 24 records; the intended storage boundary is unproven.
static SpriteSource _gShelterB2LaboratoryView5SpriteSources[23] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -32, 0, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -24, 0, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -16, 0, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 48, 0, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 104, 72, 0, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 64, 0, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 48, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, -32, 698, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -32, 523, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, -32, 767, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, -32, 0, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -8, 903, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 0, 778, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 0, 717, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 0, 711, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 0, 606, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 0, 529, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 120, 48, 536, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 32, 574, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 16, 727, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, 48, 813, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 48, 578, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 16, 619, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
};

/// View 5's image-strip selector and retained range [0,24).
///
/// The associated source array declares 23 records; the extra record's role
/// and intended storage boundary are unproven.
static SpriteBatch _gShelterB2LaboratoryView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's empty sprite list; its zero first count keeps decoded image strips.
static SpriteBatch _gShelterB2LaboratoryView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's 99 view-pixel sprite rectangles, including semitransparent sources.
static SpriteSource _gShelterB2LaboratoryView7SpriteSources[99] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 64, 0, { .fields = { 64, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 64, 0, { .fields = { 80, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 72, 0, { .fields = { 32, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -96, 80, 0, { .fields = { 104, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, 88, 0, { .fields = { 112, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 96, 0, { .fields = { 56, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 8, 96, 0, { .fields = { 48, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 88, 0, { .fields = { 56, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, 72, 0, { .fields = { 104, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 0, { .fields = { 64, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 0, { .fields = { 80, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -16, -8, 0, { .fields = { 32, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, -8, 0, { .fields = { 104, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -136, -8, 0, { .fields = { 88, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -96, -8, 0, { .fields = { 8, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -16, 0, 0, { .fields = { 48, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -16, 8, 0, { .fields = { 56, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 16, 0, { .fields = { 56, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 24, 0, { .fields = { 64, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 32, 0, { .fields = { 80, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, -8, 899, { .fields = { 120, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 16, 899, { .fields = { 0, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 0, 826, { .fields = { 56, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 32, 859, { .fields = { 0, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 8, 707, { .fields = { 104, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 40, 734, { .fields = { 96, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 16, 627, { .fields = { 80, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 56, 665, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 32, 558, { .fields = { 72, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 64, 570, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 40, 507, { .fields = { 56, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 72, 514, { .fields = { 64, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, 32, 503, { .fields = { 64, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, 64, 527, { .fields = { 80, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 32, 520, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 64, 545, { .fields = { 64, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -40, -8, 0, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -8, 0, { .fields = { 32, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, 24, 536, { .fields = { 32, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, 56, 563, { .fields = { 8, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 24, 570, { .fields = { 0, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -96, 56, 597, { .fields = { 96, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, 16, 602, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 40, 632, { .fields = { 32, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 16, 653, { .fields = { 0, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 40, 683, { .fields = { 0, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 8, 647, { .fields = { 72, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 16 } }, 40, 0, 811, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, 32, 856, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 40, 16, 827, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 32, 8, 0, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, 32, 56, 787, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, 32, 64, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, 24, 16, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, 16, 24, 0, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -32, -16, 0, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -96, -16, 0, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -160, -16, 0, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -24, -8, 0, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -24, 0, 0, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -160, 8, 0, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -160, 16, 0, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -112, 16, 0, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -72, 24, 0, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -112, 24, 0, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -160, 24, 0, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 24 } }, -160, 32, 0, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -128, 56, 639, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 64 } }, -160, 56, 0, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -128, 72, 0, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 24 } }, -112, 32, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -72, 32, 0, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 56, 0, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 88, 0, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 64 } }, -64, 56, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -40, 32, 0, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -40, 40, 0, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -40, 72, 0, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -8, 40, 490, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -8, 64, 508, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -8, 88, 526, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, 0, 24, 491, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, 32, -16, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, 24, -8, 707, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, 16, 0, 713, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 8, 8, 688, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 0, 16, 480, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -8, 16, 467, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -24, 8, 0, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -16, 16, 474, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -24, 16, 481, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -32, 8, 487, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -40, 8, 503, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 16 } }, -80, 8, 516, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -80, -8, 0, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -80, 0, 0, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 16 } }, -128, 0, 562, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -128, -8, 0, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -160, -8, 619, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

/// View 7's image-strip selector and sprite ranges [0,47), [47,99).
static SpriteBatch _gShelterB2LaboratoryView7SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 1, 0 } },
    { 47, 52, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's 78 view-pixel sprite rectangles, including semitransparent sources.
static SpriteSource _gShelterB2LaboratoryView8SpriteSources[78] = {
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -64, -32, 0, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -56, -16, 0, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -48, 0, 0, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -40, 8, 0, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -32, 24, 0, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 40, 0, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, 48, 0, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 64, 0, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 80, 0, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 80, 0, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 64, 0, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 48, 0, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 40, 0, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -8, 0, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -24, 104, 225, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 104, 315, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 96, 238, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -32, 80, 253, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 96, 309, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 80, 324, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 64, 278, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, 48, 311, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 40, 343, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 24, 385, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 48, 661, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 16, 644, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 16, 442, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 8, 476, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 16, 1014, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 0, 960, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 541, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 631, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, -16, 779, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -32, 1112, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -80, 0, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -64, 0, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -56, 0, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 0, 0, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 16, 1879, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -80, 1718, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 32, -64, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -80, 1714, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 8, 1869, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -8, 1862, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -24, 1795, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -64, 1750, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -40, 1750, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 24, 0, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -56, 0, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -80, 713, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -56, 713, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -32, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, -8, 750, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 8, 776, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 32, 803, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 24 } }, -64, -24, 0, { .fields = { 48, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 40, 16 } }, -56, 0, 0, { .fields = { 16, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, -48, 16, 0, { .fields = { 56, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, 40, 0, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, -32, 64, 0, { .fields = { 64, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 24 } }, -24, 80, 0, { .fields = { 96, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -80, 112, 0, { .fields = { 0, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, -80, 72, 0, { .fields = { 88, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, -80, 40, 0, { .fields = { 88, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, -80, 8, 0, { .fields = { 104, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -48, 112, 269, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 8 } }, -56, 104, 294, { .fields = { 112, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 16 } }, -56, 88, 322, { .fields = { 24, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -56, 80, 313, { .fields = { 0, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, 72, 348, { .fields = { 8, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -64, 64, 392, { .fields = { 0, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -64, 48, 442, { .fields = { 24, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -64, 40, 427, { .fields = { 0, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -72, 24, 596, { .fields = { 24, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -72, 16, 568, { .fields = { 0, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, -72, 8, 694, { .fields = { 8, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -80, 0, 890, { .fields = { 0, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, -80, -24, 1164, { .fields = { 72, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// View 8's image-strip selector and ranges [0,34), [34,47), [47,55), [55,78).
static SpriteBatch _gShelterB2LaboratoryView8SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 3, 0 } },
    { 34, 13, 0, 0, { 0, 0 } },
    { 47, 8, 0, 0, { 2, 0 } },
    { 55, 23, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's 16 texture rectangles, positioned in view pixels with unscaled OT depths.
static SpriteSource _gShelterB2LaboratoryView9SpriteSources[16] = {
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -56, -16, 375, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -16, -16, 375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 56, 403, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 56, 375, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -120, 96, 375, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -80, 96, 375, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, 56, 375, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, 56, 438, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 48, 375, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 48, 375, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -40, 96, 375, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 0, 96, 375, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, 64, 393, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, 64, 393, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 40, 96, 375, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, 96, 375, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
};

/// View 9's image-strip selector and one range covering its complete sprite array.
static SpriteBatch _gShelterB2LaboratoryView9SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB2LaboratoryView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's empty sprite list; the zero first count retains decoded image strips.
static SpriteBatch _gShelterB2LaboratoryView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 11's empty sprite list; the zero first count retains decoded image strips.
static SpriteBatch _gShelterB2LaboratoryView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 12's empty sprite list; the zero first count retains decoded image strips.
static SpriteBatch _gShelterB2LaboratoryView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 13's empty sprite list; the zero first count retains decoded image strips.
static SpriteBatch _gShelterB2LaboratoryView13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 14's empty sprite list; the zero first count retains decoded image strips.
static SpriteBatch _gShelterB2LaboratoryView14SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored 8-bit texture page, CLUT and neutral colour modulation for view 15.
enum {
    SHELTER_B2_LABORATORY_VIEW15_TEXTURE_8BIT       = 1,
    SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE       = getTPage(SHELTER_B2_LABORATORY_VIEW15_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    SHELTER_B2_LABORATORY_VIEW15_CLUT               = getClut(0, 255),
    SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION = 128,
};

/// View 15's 37 texture rectangles in view pixels with unscaled OT depths.
///
/// The two included batch ranges partition this array. Cached allocation copies
/// geometry, texture and command bits into two buffers; linking reads the live
/// source depths. Storage stays mutable and borrowed while the room is loaded.
static SpriteSource _gShelterB2LaboratoryView15SpriteSources[37] = {
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 24, 8 } }, -120, 48, 825, { .fields = { 88, 64 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 48 } }, -120, -24, 787, { .fields = { 120, 0 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 40 } }, -88, -88, 825, { .fields = { 120, 48 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 16 } }, -120, -72, 800, { .fields = { 104, 136 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 24 } }, -88, 32, 928, { .fields = { 112, 0 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 24 } }, -96, 32, 920, { .fields = { 112, 48 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 16 } }, -104, -64, 844, { .fields = { 96, 152 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 24 } }, -104, -88, 825, { .fields = { 104, 72 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 16 } }, -120, -88, 772, { .fields = { 96, 120 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 24 } }, -112, -72, 804, { .fields = { 120, 120 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 32 } }, -120, -56, 787, { .fields = { 120, 88 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 16 } }, -96, -48, 825, { .fields = { 96, 32 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 16 } }, -96, -32, 825, { .fields = { 96, 48 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 24 } }, -104, 24, 825, { .fields = { 120, 168 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, -96, -16, 825, { .fields = { 88, 144 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, -96, -8, 864, { .fields = { 88, 136 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, -96, 0, 864, { .fields = { 88, 176 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, -96, 24, 925, { .fields = { 88, 72 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 16 } }, -112, -48, 825, { .fields = { 96, 240 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 16 } }, -112, -32, 825, { .fields = { 96, 16 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 16 } }, -112, -16, 825, { .fields = { 112, 240 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 24 } }, -112, 0, 825, { .fields = { 104, 96 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, -96, 8, 941, { .fields = { 88, 104 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, -96, 16, 929, { .fields = { 88, 168 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 24 } }, -120, 24, 787, { .fields = { 112, 144 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 24, 24 } }, 24, -16, 0, { .fields = { 104, 216 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, 8, 24, 0, { .fields = { 88, 112 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 16 } }, 40, 24, 0, { .fields = { 112, 120 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 24 } }, 8, 32, 0, { .fields = { 120, 192 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 16 } }, 8, 8, 864, { .fields = { 104, 200 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 24, 16 } }, 24, 8, 890, { .fields = { 88, 0 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 16 } }, 24, 24, 949, { .fields = { 104, 168 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 8, 24 } }, 16, 32, 944, { .fields = { 112, 24 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 24, 16 } }, 24, 40, 919, { .fields = { 96, 184 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, 8, -16, 786, { .fields = { 88, 96 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, 8, -8, 857, { .fields = { 88, 88 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
    { SHELTER_B2_LABORATORY_VIEW15_TEXTURE_PAGE, SHELTER_B2_LABORATORY_VIEW15_CLUT, { .fields = { 16, 8 } }, 8, 0, 864, { .fields = { 88, 80 } }, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, SHELTER_B2_LABORATORY_VIEW15_NEUTRAL_MODULATION, 0 },
};

/// Number of sources in view 15's first authored range.
enum { SHELTER_B2_LABORATORY_VIEW15_FIRST_RANGE_SPRITE_COUNT = 25 };

/// View 15's image-strip selector and two included ranges covering all 37 sprites.
///
/// Ranges select 0..24 and 25..36; neither is hidden or skips cached processing.
/// The first range retains serialized bytes {1, 0} whose role is unproven.
/// Ends with `SPRITE_BATCH_END`; borrowed through the public view descriptor.
static SpriteBatch _gShelterB2LaboratoryView15SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B2_LABORATORY_VIEW15_FIRST_RANGE_SPRITE_COUNT, 0, 0, { 1, 0 } },
    { SHELTER_B2_LABORATORY_VIEW15_FIRST_RANGE_SPRITE_COUNT, ARRAY_SIZE(_gShelterB2LaboratoryView15SpriteSources) - SHELTER_B2_LABORATORY_VIEW15_FIRST_RANGE_SPRITE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 15's (60, 3, 257, 235)-pixel clip, restored to full screen at depth 745.
///
/// Coordinates are relative to the draw-buffer origin, and the depth is an
/// unscaled OT sorting value. One clip followed by `SPRITE_DRAW_AREA_END`;
/// affects depth-sorted scene drawing as well as the borrowed sprite ranges.
static SpriteDrawArea _gShelterB2LaboratoryView15SpriteDrawAreas[2] = {
    { { 60, 3, 257, 235 }, 745 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteView gShelterB2LaboratorySpriteViews[SHELTER_B2_LABORATORY_VIEW_COUNT] = {
    { { .empty = _gShelterB2LaboratoryView1SpriteBatches }, _gShelterB2LaboratoryView1SpriteBatches, NULL },
    { { .elements = _gShelterB2LaboratoryView2SpriteSources }, _gShelterB2LaboratoryView2SpriteBatches, NULL },
    { { .elements = _gShelterB2LaboratoryView3SpriteSources }, _gShelterB2LaboratoryView3SpriteBatches, NULL },
    { { .elements = _gShelterB2LaboratoryView4SpriteSources }, _gShelterB2LaboratoryView4SpriteBatches, NULL },
    { { .elements = _gShelterB2LaboratoryView5SpriteSources }, _gShelterB2LaboratoryView5SpriteBatches, NULL },
    { { .empty = _gShelterB2LaboratoryView6SpriteBatches }, _gShelterB2LaboratoryView6SpriteBatches, NULL },
    { { .elements = _gShelterB2LaboratoryView7SpriteSources }, _gShelterB2LaboratoryView7SpriteBatches, NULL },
    { { .elements = _gShelterB2LaboratoryView8SpriteSources }, _gShelterB2LaboratoryView8SpriteBatches, NULL },
    { { .elements = _gShelterB2LaboratoryView9SpriteSources }, _gShelterB2LaboratoryView9SpriteBatches, NULL },
    { { .empty = _gShelterB2LaboratoryView10SpriteBatches }, _gShelterB2LaboratoryView10SpriteBatches, NULL },
    { { .empty = _gShelterB2LaboratoryView11SpriteBatches }, _gShelterB2LaboratoryView11SpriteBatches, NULL },
    { { .empty = _gShelterB2LaboratoryView12SpriteBatches }, _gShelterB2LaboratoryView12SpriteBatches, NULL },
    { { .empty = _gShelterB2LaboratoryView13SpriteBatches }, _gShelterB2LaboratoryView13SpriteBatches, NULL },
    { { .empty = _gShelterB2LaboratoryView14SpriteBatches }, _gShelterB2LaboratoryView14SpriteBatches, NULL },
    { { .elements = _gShelterB2LaboratoryView15SpriteSources }, _gShelterB2LaboratoryView15SpriteBatches, _gShelterB2LaboratoryView15SpriteDrawAreas },
};

/// Ten authored point lights: Q12 RGB intensity and inner/outer radii in world units.
///
/// All contribute in every logical view. Coordinate caches, parents and
/// attenuation remain writable while the room lighting collection is bound.
static WorldCoordPointLight _gShelterB2LaboratoryPointLights[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5547, -1204, 1082 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2071, 2222, 2201 }, { 0, 0 } }, 4000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6419, -1756, 3004 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 890, 1978, 1175 }, { 0, 0 } }, 1261, 1721 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2237, -2188, 1564 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1010, 2438, 694 }, { 0, 0 } }, 500, 1600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2783, -1669, 5243 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3331, 3396, 3526 }, { 0, 0 } }, 1640, 2940 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9198, -3405, -364 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1826, 2022, 2062 }, { 0, 0 } }, 2739, 7039 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2D99, -2950, -1960 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1848, 2009, 2285 }, { 0, 0 } }, 1201, 2200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 457, -3405, -852 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1569, 1642, 1699 }, { 0, 0 } }, 1979, 4319 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3290, -989, -1161 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1474, 1940, 1859 }, { 0, 0 } }, 700, 1199 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2B0A, -2989, -198 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 562, 1720, 1805 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2B0A, -3109, 2300 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 562, 1280, 1485 }, { 0, 0 } }, 500, 1500 },
};

WorldCoordRoomLights gShelterB2LaboratoryRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB2LaboratoryPointLights),
    .pointLights           = _gShelterB2LaboratoryPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB2LaboratoryViewBoundaryTriggers[14] = {
    { NULL, NULL, NULL, { 5680, -1616, 1944, 0 }, { { 28, -3232, -2247, 0 }, { -47, -3232, 2228, 0 }, { 28, 3232, -2247, 0 }, { -47, 3232, 2228, 0 } }, { 4098, 0, 68, 0 }, { 0, 0, ONE, 0 }, 3924, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5824, -1568, 2000, 0 }, { { -66, -3216, 2306, 0 }, { 23, -3216, -2349, 0 }, { -66, 3216, 2306, 0 }, { 23, 3216, -2349, 0 } }, { -4103, 0, -79, 0 }, { 0, 0, ONE, 0 }, 3965, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1760, -1601, 3552, 0 }, { { -2912, -3216, -176, 0 }, { 2912, -3216, 176, 0 }, { -2912, 3216, -176, 0 }, { 2912, 3216, 176, 0 } }, { 246, 0, -4090, 0 }, { 0, 0, ONE, 0 }, 4314, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1824, -1568, 3393, 0 }, { { 2976, -3216, 208, 0 }, { -2976, -3216, -208, 0 }, { 2976, 3216, 208, 0 }, { -2976, 3216, -208, 0 } }, { -288, 0, 4102, 0 }, { 0, 0, ONE, 0 }, 4374, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6526, -1504, -2436, 0 }, { { 89, -3216, 1462, 0 }, { -89, -3216, -1462, 0 }, { 89, 3216, 1462, 0 }, { -89, 3216, -1462, 0 } }, { -4107, 0, 249, 0 }, { 0, 0, ONE, 0 }, 3528, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6432, -1600, -2386, 0 }, { { -55, -3216, -1476, 0 }, { 56, -3216, 1477, 0 }, { -55, 3216, -1476, 0 }, { 56, 3216, 1477, 0 } }, { 4095, 0, -155, 0 }, { 0, 0, ONE, 0 }, 3537, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4720, -1568, -80, 0 }, { { -1059, -3216, -27, 0 }, { 1015, -3216, -27, 0 }, { -1059, 3216, -27, 0 }, { 1015, 3216, -27, 0 } }, { 0, 0, -4107, 0 }, { 0, 0, ONE, 0 }, 3376, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4786, -1729, -209, 0 }, { { 1239, -3216, -12, 0 }, { -1257, -3216, -11, 0 }, { 1239, 3216, -12, 0 }, { -1257, 3216, -11, 0 } }, { 1, 0, 4098, 0 }, { 0, 0, ONE, 0 }, 3444, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A20, -1600, -416, 0 }, { { -1268, -3216, -168, 0 }, { 1240, -3216, 147, 0 }, { -1268, 3216, -168, 0 }, { 1240, 3216, 147, 0 } }, { 510, 0, -4068, 0 }, { 0, 0, ONE, 0 }, 3453, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A51, -1536, -544, 0 }, { { 1495, -3216, 197, 0 }, { -1521, -3216, -215, 0 }, { 1495, 3216, 197, 0 }, { -1521, 3216, -215, 0 } }, { -555, 0, 4061, 0 }, { 0, 0, ONE, 0 }, 3556, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2930, -1600, -3297, 0 }, { { 0, -3232, -832, 0 }, { 0, -3232, 832, 0 }, { 0, 3232, -832, 0 }, { 0, 3232, 832, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3328, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2981, -1472, -3312, 0 }, { { 0, -3232, 912, 0 }, { 0, -3232, -912, 0 }, { 0, 3232, 912, 0 }, { 0, 3232, -912, 0 } }, { -4104, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3357, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4431, -1536, -2401, 0 }, { { -685, -3216, 1727, 0 }, { 685, -3216, -1727, 0 }, { -685, 3216, 1727, 0 }, { 685, 3216, -1727, 0 } }, { -3821, 0, -1516, 0 }, { 0, 0, ONE, 0 }, 3709, 0, 15, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4320, -1504, -2400, 0 }, { { 685, -3216, -1727, 0 }, { -685, -3216, 1727, 0 }, { 685, 3216, -1727, 0 }, { -685, 3216, 1727, 0 } }, { 3819, 0, 1515, 0 }, { 0, 0, ONE, 0 }, 3709, 0, 5, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room command and directed-action IDs carried by the console's trigger quads.
enum {
    SHELTER_B2_LABORATORY_TRIGGER_CONSOLE_SCENE_COMMAND = 4,
    SHELTER_B2_LABORATORY_TRIGGER_CONSOLE_ACTION        = 1,
};

WorldCollisionTrigger gShelterB2LaboratoryActionTriggers[19] = {
    { NULL, NULL, NULL, { 0, -48, 1920, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { ONE, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, (1 << 4) | 3, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6384, -48, 2976, 0 }, { { 560, 0, -544, 0 }, { 560, 0, 544, 0 }, { -560, 0, -544, 0 }, { -560, 0, 544, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_OPERATING_ROOM, (2 << 4) | 3, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3489, -64, -3680, 0 }, { { -1056, 0, -224, 0 }, { 960, 0, -224, 0 }, { -1056, 0, 1024, 0 }, { 960, 0, 1024, 0 } }, { 0, ONE, 0, 0 }, { ONE, 0, 0, 0 }, 1470, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B2_LABORATORY_TRIGGER_CONSOLE_SCENE_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3296, -64, -816, 0 }, { { -1120, 0, -1808, 0 }, { 1120, 0, -1808, 0 }, { -1120, 0, 176, 0 }, { 1120, 0, 176, 0 } }, { 0, 4119, 0, 0 }, { ONE, 0, 0, 0 }, 2126, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B2_LABORATORY_TRIGGER_CONSOLE_ACTION, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 432, -64, 4112, 0 }, { { -656, 0, -1104, 0 }, { 432, 0, -1104, 0 }, { -432, 0, 1104, 0 }, { 656, 0, 1104, 0 } }, { 0, 4115, 0, 0 }, { ONE, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, 23, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2288, -64, 4800, 0 }, { { -2544, 0, -464, 0 }, { 2544, 0, -464, 0 }, { -2544, 0, 464, 0 }, { 2544, 0, 464, 0 } }, { 0, 4095, 0, 0 }, { 200, 0, -4091, 0 }, 2585, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8464, -64, 2336, 0 }, { { -1344, 0, -336, 0 }, { 1344, 0, -336, 0 }, { -1344, 0, 336, 0 }, { 1344, 0, 336, 0 } }, { 0, 4105, 0, 0 }, { -201, 0, -4091, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7024, -64, 320, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { -201, 0, 4090, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2848, -64, 320, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { -402, 0, 4076, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7392, -64, -1568, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -ONE, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x29A0, -1491, 3072, 0 }, { { -736, 0, -336, 0 }, { 736, 0, -336, 0 }, { -736, 0, 336, 0 }, { 736, 0, 336, 0 } }, { 0, 4102, 0, 0 }, { -201, 0, -4091, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B20, -1491, 2592, 0 }, { { -416, 0, -464, 0 }, { 416, 0, -464, 0 }, { -416, 0, 464, 0 }, { 416, 0, 464, 0 } }, { 0, ONE, 0, 0 }, { -4091, 0, 201, 0 }, 620, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B40, -1491, 1152, 0 }, { { -416, 0, -464, 0 }, { 416, 0, -464, 0 }, { -416, 0, 464, 0 }, { 416, 0, 464, 0 } }, { 0, ONE, 0, 0 }, { -4091, 0, 201, 0 }, 620, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B20, -1504, -224, 0 }, { { -416, 0, -464, 0 }, { 416, 0, -464, 0 }, { -416, 0, 464, 0 }, { 416, 0, 464, 0 } }, { 0, ONE, 0, 0 }, { -4091, 0, 201, 0 }, 620, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3120, -1491, -3136, 0 }, { { -416, 0, -1072, 0 }, { 416, 0, -1072, 0 }, { -416, 0, 1072, 0 }, { 416, 0, 1072, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3423, -64, 4543, 0 }, { { -308, 0, 1294, 0 }, { -1474, 0, -102, 0 }, { 1475, 0, 103, 0 }, { 309, 0, -1293, 0 } }, { 0, 4099, 0, 0 }, { 4091, 0, -202, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_CAP, 27, 1, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1216, -64, 896, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { 798, 0, 4017, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4800, -64, 2784, 0 }, { { -1008, 0, -112, 0 }, { 1008, 0, -560, 0 }, { -1008, 0, 560, 0 }, { 1008, 0, 112, 0 } }, { 0, 4101, 0, 0 }, { -798, 0, -4017, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3248, -64, -1952, 0 }, { { -768, 0, -672, 0 }, { 768, 0, -672, 0 }, { -768, 0, 672, 0 }, { 768, 0, 672, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -ONE, 0 }, 1019, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B2_LABORATORY_TRIGGER_CONSOLE_ACTION, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Placement key and global-library file components for actor_143000.
enum {
    SHELTER_B2_LABORATORY_TERMINAL_ENTRY_ID        = 101,
    SHELTER_B2_LABORATORY_TERMINAL_FILE_NUMBER     = 430,
    SHELTER_B2_LABORATORY_TERMINAL_CLUT_ROW_OFFSET = 2,
};

/// Terminal package loading recipe for the laboratory's default placement layout.
///
/// Entry 101 loads packed global-library file 143000. Descriptor 0 creates a
/// bodyless task that immediately dies; the interactive terminal session uses
/// a separate descriptor. One resource followed by `AREA_PLACEMENT_END`;
/// loading and spawning borrow this room's table and the loaded actor recipes.
static AreaResource _gShelterB2LaboratoryTerminalResources[2] = {
    { SHELTER_B2_LABORATORY_TERMINAL_ENTRY_ID, SHELTER_B2_LABORATORY_TERMINAL_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor143000TerminalTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Default laboratory layout's terminal package anchor, followed by END.
///
/// Selects resource entry 101 with zero actor mode/variant/row and base file.
/// Texture page stays unchanged; its CLUT moves down two VRAM rows. Its
/// bodyless spawned task immediately dies; keep the table loaded through
/// resource loading and that task's release.
static AreaPlacement _gShelterB2LaboratoryTerminalPlacements[2] = {
    {
        .entryId           = SHELTER_B2_LABORATORY_TERMINAL_ENTRY_ID,
        .variant           = 0,
        .mode              = 0,
        .x                 = 0,
        .y                 = 0,
        .z                 = 0,
        .yaw               = 0,
        .fileIdLow         = 0,
        .texturePageOffset = 0,
        .clutRowOffset     = SHELTER_B2_LABORATORY_TERMINAL_CLUT_ROW_OFFSET,
        .rowIndex          = 0,
    },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB2LaboratoryAreaVariants[11] = {
    { NULL, NULL },
    { _gShelterB2LaboratoryTerminalPlacements, _gShelterB2LaboratoryTerminalResources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordRoomAmbientEntry gShelterB2LaboratoryViewAmbientTable[SHELTER_B2_LABORATORY_VIEW_COUNT + 1] = {
    { .viewCount = ARRAY_SIZE(gShelterB2LaboratoryViewAmbientTable) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 952, 944, 999, 953 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Paired cue-2 bases in the loaded type-1 sound bank; cue 1 uses the next entry.
enum {
    SHELTER_B2_LABORATORY_CLASS_1_FOOTSTEP_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x09,
    SHELTER_B2_LABORATORY_CLASS_1_FOOTSTEP_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0B,
    SHELTER_B2_LABORATORY_CLASS_2_FOOTSTEP_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x61,
    SHELTER_B2_LABORATORY_CLASS_2_FOOTSTEP_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x63,
};

/// Surface class 1's walk/run cue bases; stair climbing selects the walking pair.
static WorldCollisionFootstepSounds _gShelterB2LaboratoryClass1FootstepSounds = {
    .walk         = SHELTER_B2_LABORATORY_CLASS_1_FOOTSTEP_WALK_CUE_BASE,
    .run          = SHELTER_B2_LABORATORY_CLASS_1_FOOTSTEP_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B2_LABORATORY_CLASS_1_FOOTSTEP_WALK_CUE_BASE,
};

/// Surface class 2's walk/run cue bases; stair climbing selects the walking pair.
static WorldCollisionFootstepSounds _gShelterB2LaboratoryClass2FootstepSounds = {
    .walk         = SHELTER_B2_LABORATORY_CLASS_2_FOOTSTEP_WALK_CUE_BASE,
    .run          = SHELTER_B2_LABORATORY_CLASS_2_FOOTSTEP_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B2_LABORATORY_CLASS_2_FOOTSTEP_WALK_CUE_BASE,
};

/// Blocking, impact-enabled surface policy without footsteps, shared by classes 0 and 3..7.
static WorldCollisionSurfaceProperties _gShelterB2LaboratorySilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1's blocking, impact-enabled policy with its walking and running cues.
static WorldCollisionSurfaceProperties _gShelterB2LaboratoryClass1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB2LaboratoryClass1FootstepSounds,
};

/// Surface class 2's blocking, impact-enabled policy with its walking and running cues.
static WorldCollisionSurfaceProperties _gShelterB2LaboratoryClass2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB2LaboratoryClass2FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB2LaboratorySurfacePropertiesByClass[SHELTER_B2_LABORATORY_SURFACE_CLASS_COUNT] = {
    &_gShelterB2LaboratorySilentSurfaceProperties,
    &_gShelterB2LaboratoryClass1SurfaceProperties,
    &_gShelterB2LaboratoryClass2SurfaceProperties,
    &_gShelterB2LaboratorySilentSurfaceProperties,
    &_gShelterB2LaboratorySilentSurfaceProperties,
    &_gShelterB2LaboratorySilentSurfaceProperties,
    &_gShelterB2LaboratorySilentSurfaceProperties,
    &_gShelterB2LaboratorySilentSurfaceProperties,
};

/// Authored saved layouts and map-marker policy after terminal quiz completion.
enum {
    SHELTER_B2_LABORATORY_QUIZ_MINE_LAYOUT       = 3,
    SHELTER_B2_LABORATORY_QUIZ_WATER_TANK_LAYOUT = 12,
    SHELTER_B2_LABORATORY_QUIZ_POD_TUNNEL_LAYOUT = 2,
    SHELTER_B2_LABORATORY_AREA_UPDATE_MARK       = AREA_APPLY_MODE_ALWAYS | 1,
    SHELTER_B2_LABORATORY_AREA_UPDATE_UNMARK     = AREA_APPLY_MODE_ALWAYS,
};

AreaApplyRec gShelterB2LaboratoryQuizCompletionAreaUpdates[5] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_SECRET_PASSAGE, SHELTER_B2_LABORATORY_QUIZ_MINE_LAYOUT, SHELTER_B2_LABORATORY_AREA_UPDATE_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, SHELTER_B2_LABORATORY_QUIZ_MINE_LAYOUT, SHELTER_B2_LABORATORY_AREA_UPDATE_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, SHELTER_B2_LABORATORY_QUIZ_MINE_LAYOUT, SHELTER_B2_LABORATORY_AREA_UPDATE_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, SHELTER_B2_LABORATORY_QUIZ_WATER_TANK_LAYOUT, SHELTER_B2_LABORATORY_AREA_UPDATE_UNMARK },
    { AREA_APPLY_END, 0, 0, 0 },
};

AreaApplyRec gShelterB2LaboratoryPodTunnelAreaUpdates[2] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, SHELTER_B2_LABORATORY_QUIZ_POD_TUNNEL_LAYOUT, SHELTER_B2_LABORATORY_AREA_UPDATE_UNMARK },
    { AREA_APPLY_END, 0, 0, 0 },
};

RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask = { NULL, { 0 } };

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0, 0, 0, 0, 0, 0 };

u8 gRoomEventActive = 0;

/// Values of the ambience start/stop latch; storage remains a signed word.
enum {
    SHELTER_B2_LABORATORY_AMBIENCE_DISABLED = 0,
    SHELTER_B2_LABORATORY_AMBIENCE_ENABLED  = 1,
};

/// Latch requesting the laboratory ambience loop (0 stopped, 1 enabled).
///
/// Set before spawning so repeated starts are inert, including after spawn
/// failure. Console command 4 clears it; the loop observes that only in its
/// view-watching state, then queues its stop and releases its own task.
static s32 _gShelterB2LaboratoryAmbienceEnabled = SHELTER_B2_LABORATORY_AMBIENCE_DISABLED;

/// Persistent console cutscene request, borrowed by the room's scene runner.
///
/// Command 4 fills `rec` for view 13, selecting either full or completed
/// playback. Keep it unchanged until the scene ends. The eight retained bytes
/// after the record are zero and have no established access or role.
static RoomCutsceneRecStorage _gShelterB2LaboratoryConsoleCutsceneRec = { { 0 }, { 0 } };

/// Composed ambience origin at room point (3100, -1500, -3200).
///
/// Each ambience tick binds the current view, invalidates the transform cache
/// and composes it before reading spatial pan/depth. Initial rotation and Euler
/// storage remain zero; the sound uses only the composed origin. Borrows its
/// view parent while the room's ambience task runs.
static GfxCoord _gShelterB2LaboratoryAmbienceCoord = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL };

RoomEventReq gRoomEventReq = { 0, 0, 0, 0, 0, 0 };

/// Cyan glow pulse selection (0 slow, any nonzero 16-bit value fast).
///
/// The glow task resets it on its initial tick. Ambience start selects fast
/// pulsing; console-scene completion selects slow. The event setter preserves
/// all 16 bits without normalization. Capsule geometry and colours are unaffected.
static u16 _gShelterB2LaboratoryFastGlowPulse;

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\xFA\xA9"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

void shelterB2LaboratoryTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

#include "../../shared/room/room_cutscene_task.inc.c"

#include "../../shared/room/room_event.inc.c"

/// Sets the four sound scripts accompanying the laboratory console scene.
///
/// Borrows a writable scene record for this call; changes only its sound IDs.
/// The laboratory sound bank must remain loaded through the runner's playback.
static inline void _shelterB2LaboratorySetConsoleSceneSounds(RoomCutsceneRec* scene)
{
    enum {
        SHELTER_B2_LABORATORY_SCENE_START_SOUND = 5,
        SHELTER_B2_LABORATORY_SCENE_END_SOUND   = 8,
        SHELTER_B2_LABORATORY_SCENE_SOUND       = 6,
        SHELTER_B2_LABORATORY_AFTER_SCENE_SOUND = 7,
    };

    scene->startSound      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_SCENE_START_SOUND);
    scene->endSound        = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_SCENE_END_SOUND);
    scene->sceneSound      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_SCENE_SOUND);
    scene->afterSceneSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_AFTER_SCENE_SOUND);
}

/// Starts the laboratory console's scene and commits its completed progress.
///
/// Handles `ROOM_MESSAGE_COMMAND` with first payload 4; other commands do
/// nothing, and the receiver and second payload are unused. Stops ambience.
/// Progress 2 advances to 3, unlocks the pod-tunnel door and updates the map,
/// route flags and objective before requesting the skipped scene and its
/// follow-up task. Other progress values request the full scene instead.
/// The cutscene runner borrows the singleton record; do not replace it or unload
/// its room, CAP and sound resources while playback runs. Spawn failure leaves
/// committed flags intact. Returns zero regardless of admission.
static s32 _shelterB2LaboratoryCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        SHELTER_B2_LABORATORY_COMMAND_CONSOLE_SCENE     = 4,
        SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE    = 2,
        SHELTER_B2_LABORATORY_CONSOLE_PROGRESS_SETTLED  = 3,
        SHELTER_B2_LABORATORY_SCENE_VIEW                = 13,
        SHELTER_B2_LABORATORY_SKIP_CONSOLE_SCENE        = 1,
        SHELTER_B2_LABORATORY_PLAY_CONSOLE_SCENE        = 0,
        SHELTER_B2_LABORATORY_CAP_COMPLETED_SCENE_SLOT  = 4,
        SHELTER_B2_LABORATORY_CAP_FULL_SCENE_SLOT       = 1,
        SHELTER_B2_LABORATORY_CAP_COMPLETED_SCENE_FILE  = 3,
        SHELTER_B2_LABORATORY_CAP_FULL_SCENE_FILE       = 2,
        SHELTER_B2_LABORATORY_CAP_COMPLETED_TEXTURE_X   = 384,
        SHELTER_B2_LABORATORY_CAP_COMPLETED_TEXTURE_Y   = 256,
        SHELTER_B2_LABORATORY_CAP_DEFAULT_TEXTURE_PAGE  = 0,
        SHELTER_B2_LABORATORY_CAP_AFTER_COMPLETED_SCENE = 1,
        SHELTER_B2_LABORATORY_CAP_AFTER_FULL_SCENE      = 8,
        SHELTER_B2_LABORATORY_OBJECTIVE_ROUTE_SET       = 0x28,
        SHELTER_B2_LABORATORY_OBJECTIVE_ROUTE_CLEAR     = 0x29,
        SHELTER_B2_LABORATORY_MAP_MARK_CLEAR            = 0,
        SHELTER_B2_LABORATORY_PROGRESS_FLAG_SET         = 1,
    };

    if (commandId == SHELTER_B2_LABORATORY_COMMAND_CONSOLE_SCENE) {
        _gShelterB2LaboratoryAmbienceEnabled = SHELTER_B2_LABORATORY_AMBIENCE_DISABLED;
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) == SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE) {
            // Commit progression before either deferred task can run.
            _shelterB2LaboratorySetFastGlowPulse(SHELTER_B2_LABORATORY_GLOW_PULSE_SLOW);
            gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS, SHELTER_B2_LABORATORY_CONSOLE_PROGRESS_SETTLED);
            gameFlagSetNibble(GAME_FLAG_B1_POD_TUNNEL_R47_DOOR_UNLOCKED, SHELTER_B2_LABORATORY_PROGRESS_FLAG_SET);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_B2_LABORATORY, SHELTER_B2_LABORATORY_MAP_MARK_CLEAR);
            gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_017, SHELTER_B2_LABORATORY_PROGRESS_FLAG_SET);
            gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_018, SHELTER_B2_LABORATORY_PROGRESS_FLAG_SET);
            if (gameFlagGetNibble(GAME_FLAG_083) != 0) {
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, SHELTER_B2_LABORATORY_OBJECTIVE_ROUTE_SET);
            } else {
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, SHELTER_B2_LABORATORY_OBJECTIVE_ROUTE_CLEAR);
            }
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.view      = SHELTER_B2_LABORATORY_SCENE_VIEW;
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.capSlot   = SHELTER_B2_LABORATORY_CAP_COMPLETED_SCENE_SLOT;
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.capFile   = SHELTER_B2_LABORATORY_CAP_COMPLETED_SCENE_FILE;
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.skipScene = SHELTER_B2_LABORATORY_SKIP_CONSOLE_SCENE;
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.capTPageX = SHELTER_B2_LABORATORY_CAP_COMPLETED_TEXTURE_X;
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.capTPageY = SHELTER_B2_LABORATORY_CAP_COMPLETED_TEXTURE_Y;
            _shelterB2LaboratorySetConsoleSceneSounds(&_gShelterB2LaboratoryConsoleCutsceneRec.rec);
            taskSpawnFromTable(_gRoomCutsceneTaskDescs, ROOM_CUTSCENE_TASK_SCENE, SHELTER_B2_LABORATORY_CAP_AFTER_COMPLETED_SCENE, &_gShelterB2LaboratoryConsoleCutsceneRec.rec);
            taskSpawnFromTable(_gShelterB2LaboratoryTaskDescriptors, SHELTER_B2_LABORATORY_TASK_FINISH_CONSOLE_SCENE, 0, 0);
        } else {
            // A zero texture X selects the default page; the runner ignores retained Y.
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.view      = SHELTER_B2_LABORATORY_SCENE_VIEW;
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.capSlot   = SHELTER_B2_LABORATORY_CAP_FULL_SCENE_SLOT;
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.capFile   = SHELTER_B2_LABORATORY_CAP_FULL_SCENE_FILE;
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.skipScene = SHELTER_B2_LABORATORY_PLAY_CONSOLE_SCENE;
            _gShelterB2LaboratoryConsoleCutsceneRec.rec.capTPageX = SHELTER_B2_LABORATORY_CAP_DEFAULT_TEXTURE_PAGE;
            _shelterB2LaboratorySetConsoleSceneSounds(&_gShelterB2LaboratoryConsoleCutsceneRec.rec);
            taskSpawnFromTable(_gRoomCutsceneTaskDescs, ROOM_CUTSCENE_TASK_SCENE, SHELTER_B2_LABORATORY_CAP_AFTER_FULL_SCENE, &_gShelterB2LaboratoryConsoleCutsceneRec.rec);
        }
    }
    return 0;
}

/// Laboratory room-message task states: initialize, idle and release (indices 0..2).
///
/// Contains room initialization and idle callbacks followed by the scheduler's
/// teardown callback. Dispatch copies the aggregate and indexes without a
/// bounds check; the task's state must be 0..2.
static const TaskFuncTable3 _gShelterB2LaboratoryRoomStateHandlers = {
    {
        _shelterB2LaboratoryInitRoomTask,
        _shelterB2LaboratoryIdleMessageTask,
        taskKill,
    },
};

/// Refreshes the laboratory ambience point's local-to-view transform.
///
/// Places the retained origin at room coordinates (3100, -1500, -3200), attaches
/// it to the current view and invalidates its cache before composing. The
/// caller reads that cache for spatial pan and attenuation; the view and its
/// ancestors must be live. Leaves the node's local rotation and Euler state
/// intact and borrows the view parent until the next refresh.
static inline void _shelterB2LaboratoryComposeAmbienceOrigin(void)
{
    _gShelterB2LaboratoryAmbienceCoord.coord.t[0]   = 3100;
    _gShelterB2LaboratoryAmbienceCoord.coord.t[1]   = -1500;
    _gShelterB2LaboratoryAmbienceCoord.coord.t[2]   = -3200;
    _gShelterB2LaboratoryAmbienceCoord.parent       = &gGfxViewCoord;
    _gShelterB2LaboratoryAmbienceCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&_gShelterB2LaboratoryAmbienceCoord);
}

/// Runs the laboratory's looping ambience and retunes it after view changes.
///
/// Start at state 0 with the room bank loaded and the room's ambience-enable
/// latch set. Begins at the spatial pan/depth of room point (3100, -1500, -3200).
/// When the saved and active views diverge, waits three intervening callback
/// ticks, then replaces attenuation from the active view's percentage table.
/// The active view must be 0..15. Stop is checked only in the watching state;
/// clearing the latch queues a release-preserving stop and destroys the task.
/// The body, work and spawn arguments are unused. Bank resources must outlive
/// queued events and the running sound; queue-admission failures are ignored.
static void _shelterB2LaboratoryAmbienceTask(Task* task)
{
    enum {
        SHELTER_B2_LABORATORY_AMBIENCE_START           = 0,
        SHELTER_B2_LABORATORY_AMBIENCE_WATCH_VIEW      = 1,
        SHELTER_B2_LABORATORY_AMBIENCE_DELAY_FIRST     = 2,
        SHELTER_B2_LABORATORY_AMBIENCE_DELAY_SECOND    = 3,
        SHELTER_B2_LABORATORY_AMBIENCE_DELAY_THIRD     = 4,
        SHELTER_B2_LABORATORY_AMBIENCE_RETUNE          = 5,
        SHELTER_B2_LABORATORY_AMBIENCE_SOUND_ENTRY     = 0x0E,
        SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION = 127,
        SHELTER_B2_LABORATORY_AMBIENCE_PERCENT_SCALE   = 100,
    };
    s8  panOffset;
    s8  initialAttenuation;
    s32 viewAttenuation;

    // Recompose every tick so both phases use the current camera's spatial pan.
    _shelterB2LaboratoryComposeAmbienceOrigin();
    panOffset          = worldCoordGetOriginAudioPan(&_gShelterB2LaboratoryAmbienceCoord);
    initialAttenuation = worldCoordGetOriginAudioDepth(&_gShelterB2LaboratoryAmbienceCoord);
    switch (task->state) {
        case SHELTER_B2_LABORATORY_AMBIENCE_START:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_AMBIENCE_SOUND_ENTRY), panOffset, initialAttenuation);
            task->state++;
            break;
        case SHELTER_B2_LABORATORY_AMBIENCE_WATCH_VIEW:
            if (_gShelterB2LaboratoryAmbienceEnabled == SHELTER_B2_LABORATORY_AMBIENCE_DISABLED) {
                sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_AMBIENCE_SOUND_ENTRY), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(task);
                return;
            }
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view != gGameSession->location.view) {
                task->state++;
            }
            break;
        case SHELTER_B2_LABORATORY_AMBIENCE_DELAY_FIRST:
        case SHELTER_B2_LABORATORY_AMBIENCE_DELAY_SECOND:
        case SHELTER_B2_LABORATORY_AMBIENCE_DELAY_THIRD:
            task->state++;
            break;
        case SHELTER_B2_LABORATORY_AMBIENCE_RETUNE:
            // The table is a gain percentage; the sound API takes attenuation.
            viewAttenuation = SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION - _gShelterB2LaboratoryAmbienceGainPercentByView[gGameSession->location.view] * SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION / SHELTER_B2_LABORATORY_AMBIENCE_PERCENT_SCALE;
            if (viewAttenuation >= SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION + 1) {
                viewAttenuation = SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION;
            }
            sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_AMBIENCE_SOUND_ENTRY), panOffset, (s8)viewAttenuation);
            task->state = SHELTER_B2_LABORATORY_AMBIENCE_WATCH_VIEW;
            break;
    }
}

#include "../../shared/room/room_cutscene_sound_task.inc.c"

/// Refuses key-item use in the laboratory without consuming the item.
///
/// Handles message 0x13F1. All arguments are ignored; zero selects the
/// item menu's refusal notice and leaves the room and inventory unchanged.
static s32 _shelterB2LaboratoryRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a laboratory departure and gates its corridor-door event.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows a complete request and writable
/// eight-byte reply; they may alias and neither pointer is retained. Copies
/// the record and resolves the Shelter room variant. Progress 2 returns 2,
/// playing CAP command 5 only on execution, for any destination. Otherwise
/// the corridor door returns the gate's 0 refused, 1 bypassed or 2 eligible
/// result, and other destinations return 1. Queries suppress playback and
/// flag writes; door queries still clear the event-start latch. Keep the room,
/// map overlay and event resources loaded through any deferred transition.
static s32 _shelterB2LaboratoryResolveRoomTransition(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B2_LABORATORY_TRANSITION_ACCEPTED    = 1,
        SHELTER_B2_LABORATORY_TRANSITION_SCENE       = 2,
        SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE = 2,
        SHELTER_B2_LABORATORY_CAP_PROGRESS_BLOCK     = 5,
        SHELTER_B2_LABORATORY_CAP_CORRIDOR_DOOR      = 1,
        SHELTER_B2_LABORATORY_NO_COLLECTION_REQUIRED = 0,
        SHELTER_B2_LABORATORY_DOOR_FIRST_SOUND       = 20,
        SHELTER_B2_LABORATORY_DOOR_SECOND_SOUND      = 3,
    };

    RoomEventReq doorEvent;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) == SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommandWithTransition(SHELTER_B2_LABORATORY_CAP_PROGRESS_BLOCK);
        }
        return SHELTER_B2_LABORATORY_TRANSITION_SCENE;
    }
    if (request->areaId != GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
        return SHELTER_B2_LABORATORY_TRANSITION_ACCEPTED;
    }
    doorEvent.capCmd        = SHELTER_B2_LABORATORY_CAP_CORRIDOR_DOOR;
    doorEvent.missingCapCmd = SHELTER_B2_LABORATORY_CAP_CORRIDOR_DOOR;
    doorEvent.firstSnd      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_DOOR_FIRST_SOUND);
    doorEvent.secondSnd     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_DOOR_SECOND_SOUND);
    doorEvent.flagId        = GAME_FLAG_B2_LABORATORY_DOOR_UNLOCKED;
    doorEvent.collectedBit  = SHELTER_B2_LABORATORY_NO_COLLECTION_REQUIRED;
    return _roomEventGate(&doorEvent, reply);
}

/// Handles the directed action on the laboratory console.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a readable request through this
/// dispatch; only action 1 is handled. Its first use plays the introductory
/// CAP command and commits the first-use flag. Later uses request the keypad
/// task while progress is below 2, otherwise replay CAP command 6. The
/// receiver and second payload are unused. Keep the room and actor_143000
/// resources loaded while their tasks run. Always returns zero.
static s32 _shelterB2LaboratoryConsoleActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum {
        SHELTER_B2_LABORATORY_ACTION_CONSOLE             = 1,
        SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE     = 2,
        SHELTER_B2_LABORATORY_CAP_CONSOLE_INTRO          = 0x1E,
        SHELTER_B2_LABORATORY_CAP_CONSOLE_REVISIT        = 6,
        SHELTER_B2_LABORATORY_CONSOLE_FIRST_USE_RECORDED = 1,
    };

    if (request->actionId == SHELTER_B2_LABORATORY_ACTION_CONSOLE) {
        if (gameFlagGetNibble(GAME_FLAG_LABORATORY_CONSOLE_FIRST_USE) != 0) {
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) < SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE) {
                taskSpawnFromTable(_gShelterB2LaboratoryTaskDescriptors, SHELTER_B2_LABORATORY_TASK_CONSOLE_KEYPAD, 0, 0);
            } else {
                capRunCommandWithTransition(SHELTER_B2_LABORATORY_CAP_CONSOLE_REVISIT);
            }
        } else {
            capRunCommandWithTransition(SHELTER_B2_LABORATORY_CAP_CONSOLE_INTRO);
            gameFlagSetNibble(GAME_FLAG_LABORATORY_CONSOLE_FIRST_USE, SHELTER_B2_LABORATORY_CONSOLE_FIRST_USE_RECORDED);
        }
    }
    return 0;
}

/// Maps laboratory sound cue 99 to room-bank sound script 23.
///
/// Receives `ROOM_MESSAGE_SOUND` with an integer cue in the first payload;
/// other arguments and cues are ignored. Always returns zero, including when
/// the sound queue rejects the start. Keep the room sound bank loaded through
/// playback; uses its base pan and depth rather than a spatial origin.
static s32 _shelterB2LaboratoryCueSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueId, s32 unusedSecondArg)
{
    enum {
        SHELTER_B2_LABORATORY_SOUND_CUE_99       = 99,
        SHELTER_B2_LABORATORY_CUE_99_SOUND_ENTRY = 0x17
    };
    if (cueId == SHELTER_B2_LABORATORY_SOUND_CUE_99) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_CUE_99_SOUND_ENTRY), 0, 0);
    }
    return 0;
}

/// Runs the console keypad and commits its accepted-code result.
///
/// Start in state 0 with actor_143000 loaded. Retains the spawned keypad handle
/// in the room's singleton slot, then polls its requested exit on later ticks.
/// A nonzero result sets laboratory progress to 1; zero resumes player control.
/// Clears the handle and kills this task after collecting the result. Body and
/// payloads are unused. The spawn is unchecked: the keypad must have spawned,
/// and this singleton task must not overlap another keypad waiter.
static void _shelterB2LaboratoryConsoleKeypadTask(Task* task)
{
    enum {
        SHELTER_B2_LABORATORY_KEYPAD_START          = 0,
        SHELTER_B2_LABORATORY_KEYPAD_WAIT           = 1,
        SHELTER_B2_LABORATORY_CONSOLE_CODE_ACCEPTED = 1,
    };

    s32 codeAccepted;

    switch (task->state) {
        case SHELTER_B2_LABORATORY_KEYPAD_START:
            _gShelterB2LaboratoryKeypadTask = taskSpawnFromTable(&gActor143000KeypadTaskDesc, 0, 0, 0);
            task->state                    += 1;
            return;
        case SHELTER_B2_LABORATORY_KEYPAD_WAIT:
            // Polling dispatches the keypad exit; stop using its handle after success.
            if (taskPollKill(_gShelterB2LaboratoryKeypadTask, &codeAccepted) != 0) {
                _gShelterB2LaboratoryKeypadTask = NULL;
                if (codeAccepted != 0) {
                    gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS, SHELTER_B2_LABORATORY_CONSOLE_CODE_ACCEPTED);
                } else {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                }
                taskKill(task);
            }
            return;
    }
}

/// Selects post-console music and dialogue after the scripted scene releases control.
///
/// Start in state 0. Waits for active view 13, then writes sceneEvent 14 and
/// requests ordinary stage music with no fade. On a later tick with eventState
/// zero, chooses CAP command 0x24 or 0x23 from the route flag and kills itself.
/// Body and payloads are unused. Keep the room's CAP data and the stage's map
/// music table loaded; music and CAP request admission are not checked.
static void _shelterB2LaboratoryFinishConsoleSceneTask(Task* task)
{
    enum {
        SHELTER_B2_LABORATORY_FINISH_WAIT_VIEW       = 0,
        SHELTER_B2_LABORATORY_FINISH_REQUEST_MUSIC   = 1,
        SHELTER_B2_LABORATORY_FINISH_WAIT_SCENE      = 2,
        SHELTER_B2_LABORATORY_FINISH_SCENE_VIEW      = 13,
        SHELTER_B2_LABORATORY_FINISH_MUSIC_SCENE     = 14,
        SHELTER_B2_LABORATORY_CAP_FINISH_ROUTE_SET   = 0x24,
        SHELTER_B2_LABORATORY_CAP_FINISH_ROUTE_CLEAR = 0x23,
    };

    switch (task->state) {
        case SHELTER_B2_LABORATORY_FINISH_WAIT_VIEW:
            if (gGameSession->location.view == SHELTER_B2_LABORATORY_FINISH_SCENE_VIEW) {
                task->state = SHELTER_B2_LABORATORY_FINISH_REQUEST_MUSIC;
            }
            return;
        case SHELTER_B2_LABORATORY_FINISH_REQUEST_MUSIC:
            // Select music after the scripted view arrives, then await scene release.
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = SHELTER_B2_LABORATORY_FINISH_MUSIC_SCENE;
            gStageMusicParams.fadeOutTicks                      = 0;
            gStageMusicParams.field_2                           = 0;
            taskSpawnFromTable(&gStageMusicTaskDesc, 0, STAGE_MUSIC_REQUEST_ORDINARY, 0);
            task->state++;
            return;
        case SHELTER_B2_LABORATORY_FINISH_WAIT_SCENE:
            if (gGameSession->eventState == 0) {
                capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_083) != 0 ? SHELTER_B2_LABORATORY_CAP_FINISH_ROUTE_SET : SHELTER_B2_LABORATORY_CAP_FINISH_ROUTE_CLEAR);
                taskKill(task);
            }
            return;
    }
}

/// Registers the laboratory's room-message receiver and advances to idle.
///
/// Runs in state 0, borrowing this overlay's message table and publishing the
/// live task in `GAME_TASK_SLOT_ROOM`. Keep both loaded while messages can
/// arrive; registration neither retains the task nor clears the slot at exit.
static void _shelterB2LaboratoryInitRoomTask(Task* task)
{
    task->msgTable = _gShelterB2LaboratoryRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the laboratory's room-message task idle between messages.
///
/// Ignores the receiver and leaves its state unchanged; message dispatch is
/// independent of this per-frame callback.
static void _shelterB2LaboratoryIdleMessageTask(Task* task)
{
    // Preserve the callback's stack reservation.
    char unusedStack[0x10];
}

void shelterB2LaboratoryTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelterB2LaboratoryRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

void shelterB2LaboratoryStartAmbience(void)
{
    if (_gShelterB2LaboratoryAmbienceEnabled == SHELTER_B2_LABORATORY_AMBIENCE_DISABLED) {
        _gShelterB2LaboratoryAmbienceEnabled = SHELTER_B2_LABORATORY_AMBIENCE_ENABLED;
        _shelterB2LaboratorySetFastGlowPulse(SHELTER_B2_LABORATORY_GLOW_PULSE_FAST);
        taskSpawnFromTable(_gShelterB2LaboratoryTaskDescriptors, SHELTER_B2_LABORATORY_TASK_AMBIENCE, 0, 0);
    }
}

void shelterB2LaboratoryGlowTask(Task* task)
{
    enum {
        SHELTER_B2_LABORATORY_GLOW_INITIALIZE                = 0,
        SHELTER_B2_LABORATORY_GLOW_DRAW                      = 1,
        SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE           = 0x60,
        SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE           = 0x180,
        SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE        = 0x80,
        SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE      = 0x180,
        SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE = 0x200,
        SHELTER_B2_LABORATORY_GLOW_DIM_GREY                  = 0x222,
        SHELTER_B2_LABORATORY_GLOW_GREY                      = 0x333,
        SHELTER_B2_LABORATORY_GLOW_BRIGHT_GREY               = 0x444,
        SHELTER_B2_LABORATORY_GLOW_GREEN                     = 0x241,
        SHELTER_B2_LABORATORY_GLOW_BLUE                      = 0x124,
    };

    // Start with slow cyan pulsing; room events select the faster rate.
    if (task->state == SHELTER_B2_LABORATORY_GLOW_INITIALIZE) {
        _gShelterB2LaboratoryFastGlowPulse = SHELTER_B2_LABORATORY_GLOW_PULSE_SLOW;
        task->state                        = SHELTER_B2_LABORATORY_GLOW_DRAW;
    }

    // Capsule colours are packed RGB nibbles; each entry supplies both endpoints.
    // The cyan centre is drawn as a disc in view 13 and as diamonds elsewhere.
    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[0], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[2], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[3], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[4], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[5], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[13], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[15], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[16], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[17], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 3:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[0], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BRIGHT_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[1], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BRIGHT_GREY);
            break;
        case 4:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[12], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BRIGHT_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[14], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[18], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[19], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[20], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 5:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[0], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[2], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[3], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[4], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[5], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[6], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[7], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[13], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[15], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[16], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[17], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[21], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            if (_gShelterB2LaboratoryFastGlowPulse != 0) {
                _shelterB2LaboratoryDrawGlowDiamond(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _shelterB2LaboratoryDrawGlowDiamond(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
        case 6:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[8], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[9], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[10], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[11], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[12], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[18], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[19], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[20], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 7:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[12], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[18], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[19], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[20], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 8:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[8], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[9], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[18], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[19], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[20], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 9:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[21], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 10:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[1], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[2], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[3], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[14], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[21], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            if (_gShelterB2LaboratoryFastGlowPulse != 0) {
                _shelterB2LaboratoryDrawGlowDiamond(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _shelterB2LaboratoryDrawGlowDiamond(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
        case 12:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[6], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[7], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[15], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[16], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[17], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[21], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            if (_gShelterB2LaboratoryFastGlowPulse != 0) {
                _shelterB2LaboratoryDrawGlowDiamond(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _shelterB2LaboratoryDrawGlowDiamond(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
        case 13:
            if (_gShelterB2LaboratoryFastGlowPulse != 0) {
                _glowDrawPulsingDisc(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _glowDrawPulsingDisc(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
        case 15:
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[13], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[15], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[16], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[17], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(_gShelterB2LaboratoryGlowGeometry.capsuleEndpoints[21], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            if (_gShelterB2LaboratoryFastGlowPulse != 0) {
                _shelterB2LaboratoryDrawGlowDiamond(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _shelterB2LaboratoryDrawGlowDiamond(&_gShelterB2LaboratoryGlowGeometry.cyanCentre, SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

/// Initializes one Gouraud half of the laboratory's cyan glow diamond.
///
/// Requires a word-aligned, writable `POLY_G4` supplied by the caller.
/// Sets the opaque untextured command and packet length. Vertex 2 is the
/// diamond centre, with zero red and the low byte of `cyanIntensity` in
/// green and blue; rim vertices 0, 1 and 3 are black. The drawer supplies
/// screen coordinates, DMA linkage and additive semitransparency.
/// Packet storage remains caller-owned.
static inline void _shelterB2LaboratoryInitGlowDiamondHalf(POLY_G4* diamondHalf, s32 cyanIntensity)
{
    setPolyG4(diamondHalf);
    setRGB0(diamondHalf, 0, 0, 0);
    setRGB1(diamondHalf, 0, 0, 0);
    setRGB2(diamondHalf, 0, cyanIntensity, cyanIntensity);
    setRGB3(diamondHalf, 0, 0, 0);
}

/// Initializes one Gouraud diagonal through the laboratory's cyan glow.
///
/// Requires a word-aligned, writable `LINE_G3` supplied by the caller.
/// Sets the opaque command, packet length and polyline terminator, and
/// clears the high byte of the final vertex colour word. Vertex 1 is the
/// centre, with zero red and the low byte of `cyanIntensity` in green and
/// blue; end vertices 0 and 2 are black. The drawer supplies screen
/// coordinates, DMA linkage and additive semitransparency.
/// Packet storage remains caller-owned.
static inline void _shelterB2LaboratoryInitGlowDiagonal(LINE_G3* diagonal, s32 cyanIntensity)
{
    setLineG3(diagonal);
    setRGB0(diagonal, 0, 0, 0);
    setRGB1(diagonal, 0, cyanIntensity, cyanIntensity);
    setRGB2(diagonal, 0, 0, 0);
}

/// Draws a pulsing cyan diamond and two diagonals around a world point.
///
/// Borrows `worldPoint` during the call. The signed low halfword of `pulseRate`
/// is in 4096 angle units per animation frame; green and blue intensity is
/// `rsin(animFrame * pulseRate) / 34 + 120`. The signed low halfword of
/// `radiusScale` gives a pixel half-extent of `radiusScale * 32 / depth`,
/// where depth is camera Z / 4 and must be nonzero. Negative GTE flags reject
/// the point. The second diagonal extends twice as far as the diamond.
///
/// Requires composed view matrices, 24 free scratch-stack bytes, the frame's
/// 1024-entry depth table and space for four additive packets plus blend
/// commands. Releases the scratch block before returning; queued packets
/// borrow the frame's primitive arena until GPU completion.
static void _shelterB2LaboratoryDrawGlowDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale)
{
    _ShelterB2LaboratoryGlowDiamondScratch* block;
    POLY_G4*                                prim;
    LINE_G3*                                line;
    s32                                     pulseSine;
    s32                                     intensity;
    s32                                     screenRadius;
    s32                                     partIndex;
    s32                                     xRadiusMultiple;
    s32                                     yRadiusMultiple;
    s32                                     verticalSide;
    u16                                     screenX;
    u16                                     screenY;

    block = SCRATCH_STACK_RESERVE_BLOCK(_ShelterB2LaboratoryGlowDiamondScratch);

    // Project the centre before deriving screen radius and sorting depth.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulseSine     = rsin(gDisplayState.animFrame * (s16)pulseRate);
        screenRadius  = ((s16)radiusScale * GLOW_DIAMOND_RADIUS_SCALE) / block->otz;
        partIndex     = 0;
        intensity     = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        block->radius = screenRadius;
        // Two Gouraud halves fill the diamond.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _shelterB2LaboratoryInitGlowDiamondHalf(prim, intensity);
            prim->x0     = block->sx - (u16)block->radius;
            screenX      = block->sx;
            prim->x2     = screenX;
            prim->x1     = screenX;
            prim->x3     = block->sx + (u16)block->radius;
            screenY      = block->sy;
            prim->y3     = screenY;
            prim->y2     = screenY;
            prim->y0     = screenY;
            verticalSide = partIndex * 2;
            prim->y1     = (block->sy - (u16)block->radius) + (block->radius * verticalSide);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
            partIndex++;
        } while (partIndex < 2);

        // Overlay the centre-lit diagonals at the same sorting depth.
        partIndex = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            _shelterB2LaboratoryInitGlowDiagonal(line, intensity);
            xRadiusMultiple = partIndex * 3 - 1;
            yRadiusMultiple = partIndex + 1;
            line->x0        = block->sx + (block->radius * xRadiusMultiple);
            line->y0        = block->sy - (block->radius * yRadiusMultiple);
            line->x1        = block->sx;
            line->y1        = block->sy;
            line->x2        = block->sx - (block->radius * xRadiusMultiple);
            line->y2        = block->sy + (block->radius * yRadiusMultiple);
            addPrim((&gGpuCurrentOt[((u32)block->otz << gDisplayState.otDepthShift) >> 4 & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)]),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->otz);
            partIndex = yRadiusMultiple;
        } while (partIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB2LaboratoryGlowDiamondScratch);
}

#include "../../shared/effects/glow_draw_pulsing_disc.inc.c"

/// Selects slow (zero) or fast (nonzero) pulsing for the laboratory's cyan glow.
///
/// Stores all 16 argument bits without normalization. The glow task resets the
/// selection when it starts; callers toggle it as room events start and finish.
static void _shelterB2LaboratorySetFastGlowPulse(s16 enabled)
{
    _gShelterB2LaboratoryFastGlowPulse = enabled;
}
