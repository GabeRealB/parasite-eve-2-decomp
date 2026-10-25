#include "rooms/dryfield_garage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor_120300.h"
#include "actors/actor_00100.h"
#include "actors/actor_01500.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
#include "../../shared/garage.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The event request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventReq gRoomEventReq;

/// Descriptor of the event task `roomEventTask`.

/// Storage for the task of the scene that turns the player back from the
/// junk-yard door.
///
/// While the water-tank scene has not been seen, a warp request for the junk
/// yard is refused and a scene task of the Gary Douglas actor loaded with
/// the room is spawned in its place; `task` receives that spawn's result. The
/// room only stores it. The four bytes after it are zero in the image and
/// have no recovered access; whether they are a second member or a separate
/// unreferenced variable is unproven.
typedef struct {
    Task* task;       // Scene task last spawned for a refused junk-yard warp; NULL before the first
    u8    unknown[4]; // Role unproven; zero, with no recovered access
} _DryfieldGarageDoorSceneStorage;
STATIC_ASSERT_SIZEOF(_DryfieldGarageDoorSceneStorage, 8);

static s32  _dryfieldGarageRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static void _dryfieldGarageInitRoomTask(Task* task);
static void _dryfieldGarageIdleRoomTask(Task* unusedTask);

static WorldCollisionGrid               _gDryfieldGarageCollisionGrid;
static WorldCollisionTrigger            _gDryfieldGarageViewBoundaryTriggers[14];
static WorldCollisionTrigger            _gDryfieldGarageActionTriggers[11];
static const WorldCoordRoomAmbientEntry _gDryfieldGarageAmbientByView[16];
static WorldCoordRoomLights             _gDryfieldGarageRoomLights;
static s32                              _dryfieldGarageResolveRoomEventMessage(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32                              _dryfieldGarageHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32                              _dryfieldGarageHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void                             _dryfieldGarageJunkYardRefusalTask(Task* task);

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// The room receiver's five message handlers and unsupported-message terminator.
///
/// Installed during entry and borrowed through teardown. The callbacks accept
/// their message-specific integer or pointer payloads through the 32-bit task ABI.
static const TaskMessageEntry _gDryfieldGarageMessageTable[6] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldGarageResolveRoomEventMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldGarageRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _garageSoundMsg },
    { ROOM_MESSAGE_COMMAND, _dryfieldGarageHandleRoomCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldGarageHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless priority-32 wrapper for the refused junk-yard departure scene.
///
/// Only slot 0 is spawnable; the second record ends the descriptor table.
/// The wrapper starts Gary's scene task independently and then releases itself.
static TaskDesc _gDryfieldGarageJunkYardRefusalTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _dryfieldGarageJunkYardRefusalTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Companion placement for entry warp 2, in world units with a half-turn yaw.
///
/// The complete position and Euler angles are borrowed during synchronous placement.
static const ActorTransform _gDryfieldGarageCompanionEntryPlacement SECTION(".data") = {
    { 1680, 0, 6170, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 }
};

WorldCollisionRoomResources gDryfieldGarageRoomCollisionResources[1] = {
    { &_gDryfieldGarageCollisionGrid, _gDryfieldGarageViewBoundaryTriggers, _gDryfieldGarageActionTriggers, NULL },
};

u8* gDryfieldGarageRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldGarageRoomViewCounts[1] = { DRYFIELD_GARAGE_VIEW_COUNT };

WorldCoordRoomLighting gDryfieldGarageRoomLighting[1] = {
    { &_gDryfieldGarageRoomLights, _gDryfieldGarageAmbientByView },
};

/// Map-marker nibbles for the garage's factory and junk-yard endpoints.
enum {
    DRYFIELD_GARAGE_FACTORY_WARP_MAP_FLAG   = 473,
    DRYFIELD_GARAGE_JUNK_YARD_WARP_MAP_FLAG = 472,
};

DirectionWarpEntry gDryfieldGarageWarpEntries[2] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 853, 0, 2741 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 7 / 8 }, 940, 0, 2200 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GARAGE, 4), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GARAGE, 3), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DRYFIELD_GARAGE_FACTORY_WARP_MAP_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 950, 0, 7300 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 1100, 0, 7300 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GARAGE, 2), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GARAGE, 1), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GARAGE, 7), 6, DIRECTION_WARP_FLAG_NONE, DRYFIELD_GARAGE_JUNK_YARD_WARP_MAP_FLAG },
};

SVECTOR gDryfieldGarageCollisionGridNormals[39] = {
#include "assets/dryfield_garage_collision_0108C_normals.inc"
};

SVECTOR gDryfieldGarageCollisionGridVerts[106] = {
#include "assets/dryfield_garage_collision_0108C_verts.inc"
};

WorldCollisionGridFace gDryfieldGarageCollisionGridFaces[54] = {
#include "assets/dryfield_garage_collision_0108C_faces.inc"
};

/// Signed face-index lists for the nine cells, each ending with -1.
static s16 _gDryfieldGarageCollisionGridCells[214] = {
#include "assets/dryfield_garage_collision_0108C_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldGarageCollisionGridCells` (214 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_GARAGE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldGarageCollisionGridCells[(halfwordOffset)])

/// X-major 3-by-3 directory into the grid's face-index lists.
static s16* _gDryfieldGarageCollisionGridTable[9] = {
#include "assets/dryfield_garage_collision_0108C_table.inc"
};
#undef DRYFIELD_GARAGE_CELL_FACE_LIST_AT_OFFSET

/// The garage's room collision mesh and 3-by-3 XZ cell index.
///
/// Cells span 4000 room-coordinate units, starting at X=-150 and Z=0.
/// Activation binds the current view transform. Gary's scene actor replaces the
/// leading three normals, twelve vertices and three faces in the public pools.
/// The descriptor and all pools remain live while the garage overlay is loaded.
static WorldCollisionGrid _gDryfieldGarageCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = gDryfieldGarageCollisionGridNormals,
    .vertices    = gDryfieldGarageCollisionGridVerts,
    .faces       = gDryfieldGarageCollisionGridFaces,
    .cellFaceIds = _gDryfieldGarageCollisionGridTable,
    .xBias       = 150,
    .zBias       = 0,
    .cellCountX  = 3,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(gDryfieldGarageCollisionGridFaces),
};

ViewCamera gDryfieldGarageViewCameras[DRYFIELD_GARAGE_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -5508, 0x33C2, -3974 } }, 257 },
    { { { { 3382, 0, 2309 }, { 756, 3870, -1107 }, { -2182, 1340, 3196 } }, { -3863, 2161, -312 } }, 257 },
    { { { { 1352, 0, 3866 }, { 657, 4036, -230 }, { -3809, 696, 1332 } }, { -0x294D, 1921, -1936 } }, 257 },
    { { { { 4042, 0, 661 }, { 142, 3999, -870 }, { -646, 882, 3947 } }, { -9784, 1851, -1821 } }, 257 },
    { { { { 2349, 0, 3354 }, { 3299, 742, -2310 }, { -607, 4028, 425 } }, { -9916, 5826, -2247 } }, 257 },
    { { { { 3533, 0, -2071 }, { -1751, 2188, -2987 }, { 1106, 3462, 1887 } }, { -997, 4246, -3702 } }, 257 },
    { { { { 1536, 0, 3796 }, { 0, ONE, 0 }, { -3796, 0, 1536 } }, { -4981, 1166, -1302 } }, 348 },
    { { { { -525, 0, -4062 }, { -354, 4080, 45 }, { 4046, 357, -523 } }, { -2448, 1577, -2001 } }, 269 },
    { { { { 3882, 0, -1306 }, { -40, 4093, -121 }, { 1305, 127, 3880 } }, { -3150, 1402, -251 } }, 289 },
    { { { { 2526, 0, -3224 }, { -214, 4086, -167 }, { 3216, 272, 2520 } }, { -1837, 1404, -858 } }, 259 },
    { { { { 2108, 0, 3511 }, { -16, 4095, 9 }, { -3511, -19, 2108 } }, { -4394, 1394, -628 } }, 541 },
    { { { { 1810, 0, 3674 }, { 1435, 3770, -707 }, { -3382, 1600, 1666 } }, { -5513, 2036, -1107 } }, 312 },
    { { { { 4005, 0, -857 }, { 90, 4072, 425 }, { 852, -434, 3982 } }, { -2588, 997, -330 } }, 289 },
    { { { { 2567, 0, -3191 }, { -300, 4077, -241 }, { 3177, 385, 2556 } }, { -2689, 1639, -896 } }, 447 },
    { { { { 1971, 0, 3590 }, { 498, 4056, -274 }, { -3555, 569, 1952 } }, { -6690, 1477, -1070 } }, 257 },
};

/// Room-sprite GPU encodings: 8-bit pages at Y=0 and a palette at (0, 255).
///
/// RGB 128 is neutral modulation; zero code flags select opaque modulation.
enum {
    DRYFIELD_GARAGE_SPRITE_PAGE_960         = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_GARAGE_SPRITE_PAGE_896         = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_GARAGE_SPRITE_CLUT             = getClut(0, 255),
    DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR    = 128,
    DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED = 0,
};

/// View 1's empty sprite list, retaining decoded background strips.
///
/// A zero-count background selector precedes the terminal record. Its address
/// also occupies the unused source slot and is never read as source sprites.
static SpriteBatch _gDryfieldGarageView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 41 depth-sorted texture rectangles from the 8-bit page at X=960.
///
/// XY and sizes are pixels relative to the view origin; UVs are texels and depth
/// is an unscaled ordering value. Uses the palette at (0, 255). Packet allocation
/// snapshots geometry and texture data; keep the room overlay loaded while used.
static SpriteSource _gDryfieldGarageView2SpriteSources[41] = {
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 16, 705, { .fields = { 80, 176 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 56, 965, { .fields = { 120, 232 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 56, 40 } }, 104, -64, 1117, { .fields = { 48, 96 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, -48, 1377, { .fields = { 88, 80 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 56, 32 } }, 104, -24, 1037, { .fields = { 24, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 40 } }, 96, -32, 1109, { .fields = { 104, 184 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 48 } }, 88, -32, 1019, { .fields = { 112, 192 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 48 } }, 80, -32, 921, { .fields = { 112, 144 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 64, 8 } }, 96, 8, 1021, { .fields = { 0, 208 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 24 } }, 136, 16, 673, { .fields = { 40, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 24 } }, 112, 16, 687, { .fields = { 48, 168 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 88, 16, 714, { .fields = { 40, 80 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 104, 517, { .fields = { 112, 240 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, 96, 548, { .fields = { 48, 216 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, 88, 572, { .fields = { 24, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 120, 88, 572, { .fields = { 88, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 24 } }, 120, 96, 540, { .fields = { 48, 224 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 48 } }, 136, 40, 613, { .fields = { 96, 48 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 8 } }, 96, 80, 599, { .fields = { 32, 248 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 32 } }, 96, 88, 567, { .fields = { 48, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 40 } }, 72, 80, 597, { .fields = { 80, 184 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, 72, 630, { .fields = { 96, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 24 } }, 24, 96, 668, { .fields = { 40, 32 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 16, 104, 695, { .fields = { 80, 80 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 32 } }, 24, 64, 667, { .fields = { 72, 224 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 32, 32, 661, { .fields = { 64, 40 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 32, 0, 635, { .fields = { 96, 224 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 48, 56, 677, { .fields = { 40, 192 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 40 } }, 112, 40, 602, { .fields = { 72, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, 32, 639, { .fields = { 88, 144 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 8 } }, 72, 72, 628, { .fields = { 8, 8 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, 24, 676, { .fields = { 104, 96 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 48 } }, 72, -24, 858, { .fields = { 120, 184 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 40 } }, 64, -16, 795, { .fields = { 80, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 64, 24, 716, { .fields = { 72, 168 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 72 } }, 56, -16, 760, { .fields = { 120, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 64 } }, 48, -8, 710, { .fields = { 120, 72 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 48 } }, -144, 40, 800, { .fields = { 120, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 56, 800, { .fields = { 80, 40 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 32, 790, { .fields = { 64, 200 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 8, 756, { .fields = { 64, 72 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
};

/// View 2's 2 sprite ranges, covering all 41 source elements.
///
/// The initial zero-count record retains decoded background strips; the final
/// sentinel ends the list. Visibility flags remain writable; the two serialized
/// bytes at the end of each record retain their unproven interpretation.
static SpriteBatch _gDryfieldGarageView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 37, 0, 0, { 1, 0 } },
    { 37, ARRAY_SIZE(_gDryfieldGarageView2SpriteSources) - 37, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 28 depth-sorted texture rectangles from the 8-bit page at X=960.
///
/// XY and sizes are pixels relative to the view origin; UVs are texels and depth
/// is an unscaled ordering value. Uses the palette at (0, 255). Packet allocation
/// snapshots geometry and texture data; keep the room overlay loaded while used.
static SpriteSource _gDryfieldGarageView3SpriteSources[28] = {
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, 48, 905, { .fields = { 72, 216 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -144, 48, 903, { .fields = { 72, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 32 } }, -160, 64, 900, { .fields = { 88, 224 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -160, 56, 861, { .fields = { 64, 72 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 48 } }, -160, 64, 850, { .fields = { 96, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 40 } }, -144, 64, 871, { .fields = { 96, 184 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 88 } }, 128, 24, 837, { .fields = { 120, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, 24, 827, { .fields = { 64, 248 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, 32, 800, { .fields = { 48, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 80 } }, 136, 40, 800, { .fields = { 120, 168 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 80 } }, 144, 40, 762, { .fields = { 112, 88 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 16, -40, 1975, { .fields = { 88, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 8 } }, 24, -40, 1925, { .fields = { 32, 64 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 24, -32, 1822, { .fields = { 72, 80 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 48, -32, 1786, { .fields = { 80, 184 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 8 } }, 64, -24, 2000, { .fields = { 40, 144 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 8 } }, -24, -24, 1875, { .fields = { 32, 88 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 48, 8 } }, -72, -24, 1850, { .fields = { 24, 80 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 56 } }, -72, -16, 1750, { .fields = { 112, 168 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -80, 0, 1825, { .fields = { 112, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 80, 56 } }, -64, -16, 1650, { .fields = { 40, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 40 } }, 72, -16, 1862, { .fields = { 72, 144 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 56, 40 } }, 16, -16, 1769, { .fields = { 56, 104 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 8 } }, -160, 8, 1600, { .fields = { 64, 96 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, 16, 1562, { .fields = { 56, 184 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, 16, 1575, { .fields = { 72, 232 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 16, 1600, { .fields = { 80, 232 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, 32, 1525, { .fields = { 64, 200 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
};

/// View 3's 5 sprite ranges, covering all 28 source elements.
///
/// The initial zero-count record retains decoded background strips; the final
/// sentinel ends the list. Visibility flags remain writable; the two serialized
/// bytes at the end of each record retain their unproven interpretation.
static SpriteBatch _gDryfieldGarageView3SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 3, 0, 0, { 3, 0 } },
    { 6, 5, 0, 0, { 2, 0 } },
    { 11, 12, 0, 0, { 4, 0 } },
    { 23, ARRAY_SIZE(_gDryfieldGarageView3SpriteSources) - 23, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 11 depth-sorted texture rectangles from the 8-bit page at X=960.
///
/// XY and sizes are pixels relative to the view origin; UVs are texels and depth
/// is an unscaled ordering value. Uses the palette at (0, 255). Packet allocation
/// snapshots geometry and texture data; keep the room overlay loaded while used.
static SpriteSource _gDryfieldGarageView4SpriteSources[11] = {
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 80 } }, -64, 16, 798, { .fields = { 112, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 80 } }, -80, 16, 784, { .fields = { 104, 80 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 96 } }, -104, 0, 854, { .fields = { 120, 96 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 96 } }, -48, 0, 854, { .fields = { 120, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 80 } }, -56, 16, 820, { .fields = { 112, 160 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 80 } }, -96, 16, 824, { .fields = { 104, 160 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 80 } }, -88, 16, 789, { .fields = { 104, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -80, 8, 825, { .fields = { 112, 240 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 48, 8 } }, -96, 0, 897, { .fields = { 80, 248 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -96, 8, 837, { .fields = { 96, 240 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 8, 837, { .fields = { 88, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
};

/// View 4's one sprite range, covering all 11 source elements.
///
/// The initial zero-count record retains decoded background strips; the final
/// sentinel ends the list. Visibility flags remain writable; the two serialized
/// bytes at the end of each record retain their unproven interpretation.
static SpriteBatch _gDryfieldGarageView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldGarageView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 12 depth-sorted texture rectangles from the 8-bit page at X=960.
///
/// XY and sizes are pixels relative to the view origin; UVs are texels and depth
/// is an unscaled ordering value. Uses the palette at (0, 255). Packet allocation
/// snapshots geometry and texture data; keep the room overlay loaded while used.
static SpriteSource _gDryfieldGarageView5SpriteSources[12] = {
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -120, -48, 1405, { .fields = { 112, 160 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -144, -40, 1407, { .fields = { 104, 208 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, -56, 1405, { .fields = { 120, 192 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -144, -56, 1407, { .fields = { 112, 176 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 24 } }, -136, -48, 1392, { .fields = { 96, 32 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 48, -80, 1417, { .fields = { 120, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 56, -80, 1415, { .fields = { 120, 128 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 64, -80, 1416, { .fields = { 112, 144 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, -64, 1416, { .fields = { 104, 216 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 40, -104, 1250, { .fields = { 120, 104 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 32, -80, 1425, { .fields = { 112, 80 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 32 } }, 48, -112, 1175, { .fields = { 96, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
};

/// View 5's 3 sprite ranges, covering all 12 source elements.
///
/// The initial zero-count record retains decoded background strips; the final
/// sentinel ends the list. Visibility flags remain writable; the two serialized
/// bytes at the end of each record retain their unproven interpretation.
static SpriteBatch _gDryfieldGarageView5SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 1, 0, 0, { 2, 0 } },
    { 5, ARRAY_SIZE(_gDryfieldGarageView5SpriteSources) - 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's 13 depth-sorted texture rectangles from 8-bit pages at X=896 and X=960.
///
/// XY and sizes are pixels relative to the view origin; UVs are texels and depth
/// is an unscaled ordering value. Uses the palette at (0, 255). Packet allocation
/// snapshots geometry and texture data; keep the room overlay loaded while used.
static SpriteSource _gDryfieldGarageView6SpriteSources[13] = {
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 48 } }, 80, -64, 1023, { .fields = { 48, 208 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 96 } }, 144, -48, 950, { .fields = { 96, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 88 } }, 120, -40, 975, { .fields = { 72, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 72 } }, 104, -24, 1036, { .fields = { 16, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 64 } }, 80, -16, 1026, { .fields = { 8, 72 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 112 } }, 64, -64, 1150, { .fields = { 112, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_896, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 88, 72 } }, 72, 48, 950, { .fields = { 112, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 96, 136 } }, -32, -88, 1275, { .fields = { 32, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 16 } }, 64, -80, 934, { .fields = { 72, 232 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 40 } }, 104, -80, 1000, { .fields = { 16, 208 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, -40, 1000, { .fields = { 0, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_896, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 64, 32 } }, 64, -112, 1017, { .fields = { 80, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_896, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 72, 24 } }, -8, -112, 1226, { .fields = { 72, 32 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
};

/// View 6's one sprite range, covering all 13 source elements.
///
/// The initial zero-count record retains decoded background strips; the final
/// sentinel ends the list. Visibility flags remain writable; the two serialized
/// bytes at the end of each record retain their unproven interpretation.
static SpriteBatch _gDryfieldGarageView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldGarageView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's 25 depth-sorted texture rectangles from the 8-bit page at X=960.
///
/// XY and sizes are pixels relative to the view origin; UVs are texels and depth
/// is an unscaled ordering value. Uses the palette at (0, 255). Packet allocation
/// snapshots geometry and texture data; keep the room overlay loaded while used.
static SpriteSource _gDryfieldGarageView7SpriteSources[25] = {
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 96 } }, 40, -16, 600, { .fields = { 104, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 48, 8 } }, 112, 112, 399, { .fields = { 32, 96 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, 96, 490, { .fields = { 64, 64 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 56, 8 } }, 40, 80, 550, { .fields = { 16, 40 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, 104, 429, { .fields = { 56, 48 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 32 } }, 96, -16, 653, { .fields = { 64, 160 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 32 } }, 128, -16, 682, { .fields = { 64, 192 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 64 } }, 96, 16, 538, { .fields = { 72, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, 96, 457, { .fields = { 48, 88 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 72, 96, 456, { .fields = { 56, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 8 } }, 88, 96, 463, { .fields = { 32, 8 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 96, 104, 410, { .fields = { 96, 240 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 8 } }, 112, 104, 429, { .fields = { 48, 104 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 72, 104, 464, { .fields = { 96, 64 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 48 } }, 32, 72, 602, { .fields = { 88, 112 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 48 } }, 16, 72, 605, { .fields = { 80, 64 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 40, 88, 529, { .fields = { 72, 112 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 80, 104, 441, { .fields = { 72, 144 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 64 } }, 64, -16, 626, { .fields = { 96, 176 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 32 } }, 64, 48, 570, { .fields = { 64, 224 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 80 } }, 128, 16, 462, { .fields = { 96, 96 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 80, 510, { .fields = { 112, 240 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 96, 80, 550, { .fields = { 56, 32 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 8 } }, 56, 88, 509, { .fields = { 40, 16 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 88, 503, { .fields = { 48, 24 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
};

/// View 7's one sprite range, covering all 25 source elements.
///
/// The initial zero-count record retains decoded background strips; the final
/// sentinel ends the list. Visibility flags remain writable; the two serialized
/// bytes at the end of each record retain their unproven interpretation.
static SpriteBatch _gDryfieldGarageView7SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldGarageView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's empty sprite list, retaining decoded background strips.
///
/// A zero-count background selector precedes the terminal record. Its address
/// also occupies the unused source slot and is never read as source sprites.
static SpriteBatch _gDryfieldGarageView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's empty sprite list, retaining decoded background strips.
///
/// A zero-count background selector precedes the terminal record. Its address
/// also occupies the unused source slot and is never read as source sprites.
static SpriteBatch _gDryfieldGarageView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's empty sprite list, retaining decoded background strips.
///
/// A zero-count background selector precedes the terminal record. Its address
/// also occupies the unused source slot and is never read as source sprites.
static SpriteBatch _gDryfieldGarageView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 11's empty sprite list, retaining decoded background strips.
///
/// A zero-count background selector precedes the terminal record. Its address
/// also occupies the unused source slot and is never read as source sprites.
static SpriteBatch _gDryfieldGarageView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 12's empty sprite list, retaining decoded background strips.
///
/// The zero-count selector and terminal record also occupy the unused source
/// slot; that retained address is never dereferenced as source sprites.
static SpriteBatch _gDryfieldGarageView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 13's empty sprite list, retaining decoded background strips.
///
/// The zero-count selector and terminal record also occupy the unused source
/// slot; that retained address is never dereferenced as source sprites.
static SpriteBatch _gDryfieldGarageView13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 14's empty sprite list, retaining decoded background strips.
///
/// The zero-count selector and terminal record also occupy the unused source
/// slot; that retained address is never dereferenced as source sprites.
static SpriteBatch _gDryfieldGarageView14SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 15's 23 depth-sorted texture rectangles from the 8-bit page at X=960.
///
/// XY and sizes are pixels relative to the view origin; UVs are texels and depth
/// is an unscaled ordering value. Uses the palette at (0, 255). Packet allocation
/// snapshots geometry and texture data; keep the room overlay loaded while used.
static SpriteSource _gDryfieldGarageView15SpriteSources[23] = {
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 8 } }, 120, -48, 1091, { .fields = { 16, 216 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, -48, 1231, { .fields = { 32, 208 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 88, -48, 1326, { .fields = { 88, 160 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 24 } }, 96, -40, 1243, { .fields = { 64, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 24 } }, 120, -40, 1099, { .fields = { 48, 112 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 96 } }, -40, -24, 1000, { .fields = { 104, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, 16, 1026, { .fields = { 88, 112 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 48, 8 } }, -32, -24, 1037, { .fields = { 8, 168 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 16, 104 } }, -32, -16, 970, { .fields = { 112, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 24, 96 } }, -16, 0, 928, { .fields = { 104, 104 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 32, 16 } }, 56, -32, 1276, { .fields = { 56, 160 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 8 } }, 16, -24, 1229, { .fields = { 56, 192 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 80, 56 } }, 8, 0, 807, { .fields = { 24, 0 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 72, 56 } }, 88, 0, 922, { .fields = { 56, 200 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 32 } }, 88, 56, 817, { .fields = { 48, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 80, 24 } }, 8, 56, 975, { .fields = { 8, 88 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 16 } }, -16, -16, 932, { .fields = { 24, 136 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 48, 16 } }, 24, -16, 820, { .fields = { 8, 192 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 40, 16 } }, 120, -16, 966, { .fields = { 16, 152 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 48, 16 } }, 72, -16, 857, { .fields = { 40, 176 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, 64, 725, { .fields = { 96, 160 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 48 } }, -152, 64, 712, { .fields = { 88, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_GARAGE_SPRITE_PAGE_960, DRYFIELD_GARAGE_SPRITE_CLUT, { .fields = { 8, 56 } }, -160, 64, 700, { .fields = { 96, 56 } }, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_NEUTRAL_COLOR, DRYFIELD_GARAGE_SPRITE_OPAQUE_MODULATED },
};

/// View 15's complete 23-sprite list in ranges [0, 20) and [20, 23).
///
/// The initial zero-count selector retains decoded background strips. The final
/// sentinel ends the list; visibility stays writable. The serialized trailing
/// bytes, including the first range's value 1, have no established interpretation.
static SpriteBatch _gDryfieldGarageView15SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 1, 0 } },
    { 20, ARRAY_SIZE(_gDryfieldGarageView15SpriteSources) - 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldGarageSpriteViews[DRYFIELD_GARAGE_VIEW_COUNT] = {
    { { .empty = _gDryfieldGarageView1SpriteBatches }, _gDryfieldGarageView1SpriteBatches, NULL },
    { { .elements = _gDryfieldGarageView2SpriteSources }, _gDryfieldGarageView2SpriteBatches, NULL },
    { { .elements = _gDryfieldGarageView3SpriteSources }, _gDryfieldGarageView3SpriteBatches, NULL },
    { { .elements = _gDryfieldGarageView4SpriteSources }, _gDryfieldGarageView4SpriteBatches, NULL },
    { { .elements = _gDryfieldGarageView5SpriteSources }, _gDryfieldGarageView5SpriteBatches, NULL },
    { { .elements = _gDryfieldGarageView6SpriteSources }, _gDryfieldGarageView6SpriteBatches, NULL },
    { { .elements = _gDryfieldGarageView7SpriteSources }, _gDryfieldGarageView7SpriteBatches, NULL },
    { { .empty = _gDryfieldGarageView8SpriteBatches }, _gDryfieldGarageView8SpriteBatches, NULL },
    { { .empty = _gDryfieldGarageView9SpriteBatches }, _gDryfieldGarageView9SpriteBatches, NULL },
    { { .empty = _gDryfieldGarageView10SpriteBatches }, _gDryfieldGarageView10SpriteBatches, NULL },
    { { .empty = _gDryfieldGarageView11SpriteBatches }, _gDryfieldGarageView11SpriteBatches, NULL },
    { { .empty = _gDryfieldGarageView12SpriteBatches }, _gDryfieldGarageView12SpriteBatches, NULL },
    { { .empty = _gDryfieldGarageView13SpriteBatches }, _gDryfieldGarageView13SpriteBatches, NULL },
    { { .empty = _gDryfieldGarageView14SpriteBatches }, _gDryfieldGarageView14SpriteBatches, NULL },
    { { .elements = _gDryfieldGarageView15SpriteSources }, _gDryfieldGarageView15SpriteBatches, NULL },
};

/// Fourteen directed boundaries between garage views, ending on the last live quad.
///
/// Source/destination view pairs are 15/2, 2/6, 3/4, 3/5, 5/4, 4/3 and 3/15,
/// in both directions. Setup binds the current view, links and enables all records.
/// Positions and radii use room units; normals use 4096 per unit. Storage must
/// stay loaded until the view-boundary list is cleared or unlinked.
static WorldCollisionTrigger _gDryfieldGarageViewBoundaryTriggers[14] = {
    { NULL, NULL, NULL, { 1455, -1648, 1775, 0 }, { { -1960, -2672, -1625, 0 }, { 1960, -2672, 1624, 0 }, { -1960, 2672, -1625, 0 }, { 1960, 2672, 1624, 0 } }, { 2615, 0, -3157, 0 }, { 0, 0, ONE, 0 }, 3683, 0, 15, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1566, -1584, 1614, 0 }, { { 1960, -2608, 1673, 0 }, { -1960, -2608, -1673, 0 }, { 1960, 2608, 1673, 0 }, { -1960, 2608, -1673, 0 } }, { -2662, 0, 3116, 0 }, { 0, 0, ONE, 0 }, 3665, 0, 2, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 991, -1664, 4894, 0 }, { { 2101, -2688, -1181, 0 }, { -2101, -2688, 1181, 0 }, { 2101, 2688, -1181, 0 }, { -2101, 2688, 1181, 0 } }, { 2010, 0, 3577, 0 }, { 0, 0, ONE, 0 }, 3602, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1024, -1600, 5023, 0 }, { { -2101, -2624, 1181, 0 }, { 2101, -2624, -1181, 0 }, { -2101, 2624, 1181, 0 }, { 2101, 2624, -1181, 0 } }, { -2011, 0, -3577, 0 }, { 0, 0, ONE, 0 }, 3556, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7827, -1552, 5066, 0 }, { { -435, -2576, 52, 0 }, { 436, -2576, -51, 0 }, { -435, 2576, 52, 0 }, { 436, 2576, -51, 0 } }, { -484, 0, -4077, 0 }, { 0, 0, ONE, 0 }, 2610, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7817, -1760, 4954, 0 }, { { 491, -2784, -59, 0 }, { -490, -2784, 60, 0 }, { 491, 2784, -59, 0 }, { -490, 2784, 60, 0 } }, { 491, 0, 4066, 0 }, { 0, 0, ONE, 0 }, 2816, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7326, -1536, 2718, 0 }, { { 1076, -2560, 2157, 0 }, { -1076, -2560, -2156, 0 }, { 1076, 2560, 2157, 0 }, { -1076, 2560, -2156, 0 } }, { -3672, 0, 1831, 0 }, { 0, 0, ONE, 0 }, 3510, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7199, -1728, 2782, 0 }, { { -1076, -2752, -2156, 0 }, { 1076, -2752, 2157, 0 }, { -1076, 2752, -2156, 0 }, { 1076, 2752, 2157, 0 } }, { 3664, 0, -1829, 0 }, { 0, 0, ONE, 0 }, 3656, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9680, -1840, 4735, 0 }, { { -1379, -2864, 332, 0 }, { 1379, -2864, -332, 0 }, { -1379, 2864, 332, 0 }, { 1379, 2864, -332, 0 } }, { -960, 0, -3984, 0 }, { 0, 0, ONE, 0 }, 3187, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9664, -1792, 4640, 0 }, { { 1379, -2816, -332, 0 }, { -1379, -2816, 332, 0 }, { 1379, 2816, -332, 0 }, { -1379, 2816, 332, 0 } }, { 958, 0, 3982, 0 }, { 0, 0, ONE, 0 }, 3145, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6315, -1792, 6411, 0 }, { { 1136, -2784, -1556, 0 }, { -1136, -2784, 1556, 0 }, { 1136, 2784, -1556, 0 }, { -1136, 2784, 1556, 0 } }, { 3318, 0, 2422, 0 }, { 0, 0, ONE, 0 }, 3376, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6427, -1760, 6490, 0 }, { { -1047, -2784, 1415, 0 }, { 1047, -2784, -1414, 0 }, { -1047, 2784, 1415, 0 }, { 1047, 2784, -1414, 0 } }, { -3299, 0, -2442, 0 }, { 0, 0, ONE, 0 }, 3288, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4465, -1664, 1695, 0 }, { { 148, -2672, -2050, 0 }, { -163, -2672, 2041, 0 }, { 148, 2672, -2050, 0 }, { -163, 2672, 2041, 0 } }, { 4088, 0, 310, 0 }, { 0, 0, ONE, 0 }, 3367, 0, 3, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4592, -1760, 1648, 0 }, { { -175, -2672, 1978, 0 }, { 162, -2672, -1987, 0 }, { -175, 2672, 1978, 0 }, { 162, 2672, -1987, 0 } }, { -4098, 0, -349, 0 }, { 0, 0, ONE, 0 }, 3328, 0, 15, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Six point lights contributing in every garage view.
///
/// Positions and falloff radii use room-coordinate units; RGB intensity uses
/// twelve fractional bits. The view-bound transforms and attenuation remain
/// writable for composition and lighting queries while this overlay is loaded.
static WorldCoordPointLight _gDryfieldGaragePointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3565, -1012, 1211 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 642, 2848 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7335, -1012, 1897 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 564, 2686 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2845, -1012, 4145 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 562, 1281 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5512, -5017, 2053 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2375, 2457, 2457 }, { 0, 0 } }, 0, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2367, -1012, 6804 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 441, 1201 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6622, -1012, 6746 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 522, 1542 },
};

/// The garage's six point lights, without directional or cone lights.
///
/// Borrows writable light records for coordinate updates and shading queries;
/// neither this collection nor its pointers may outlive the loaded room overlay.
static WorldCoordRoomLights _gDryfieldGarageRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldGaragePointLights),
    .pointLights           = _gDryfieldGaragePointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Eleven garage interactions and exit quads, ending on the last live record.
///
/// Warps use endpoint 2 to junk-yard endpoint 1 and endpoint 1 to factory endpoint 3.
/// Other records request room actions 1/2 or CAP commands 16/17/18/20/21; command
/// 16 routes through the room handler outside battle. Entry disables action 1
/// outside variant 1. Setup binds the view and mutates links, enables and hit latches.
/// Positions and radii use room units; normals use 4096 per unit. Keep loaded until unlinked.
static WorldCollisionTrigger _gDryfieldGarageActionTriggers[11] = {
    { NULL, NULL, NULL, { 3552, -64, 2384, 0 }, { { -1728, 0, -1392, 0 }, { 2048, 0, -1392, 0 }, { -1792, 0, 944, 0 }, { 2016, 0, 944, 0 } }, { 0, 4116, 0, 0 }, { 0, 0, ONE, 0 }, 2468, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 656, -48, 7376, 0 }, { { -976, 0, -256, 0 }, { 976, 0, -256, 0 }, { -976, 0, 256, 0 }, { 976, 0, 256, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -ONE, 0 }, 1007, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_JUNK_YARD, (2 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 608, -48, 2960, 0 }, { { 272, 0, -976, 0 }, { 272, 0, 976, 0 }, { -272, 0, -976, 0 }, { -272, 0, 976, 0 } }, { 0, 4111, 0, 0 }, { ONE, 0, 0, 0 }, 1011, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_FACTORY, (1 << 4) | 3, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2192, -64, 5344, 0 }, { { -512, 0, -2880, 0 }, { 512, 0, -2880, 0 }, { -512, 0, 2880, 0 }, { 512, 0, 2880, 0 } }, { 0, 4104, 0, 0 }, { -ONE, 0, 0, 0 }, 2918, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5024, -64, 5312, 0 }, { { -512, 0, -2880, 0 }, { 512, 0, -2880, 0 }, { -512, 0, 2880, 0 }, { 512, 0, 2880, 0 } }, { 0, 4104, 0, 0 }, { 4091, 0, -201, 0 }, 2918, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9600, -64, 6368, 0 }, { { 688, 0, -1552, 0 }, { 688, 0, 1552, 0 }, { -688, 0, -1552, 0 }, { -688, 0, 1552, 0 } }, { 0, 4103, 0, 0 }, { -ONE, 0, 0, 0 }, 1693, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 16, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9680, -64, 1696, 0 }, { { 1088, 0, -624, 0 }, { 1088, 0, 624, 0 }, { -1088, 0, -624, 0 }, { -1088, 0, 624, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, ONE, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8335, -64, 4895, 0 }, { { 23, 0, -1493, 0 }, { 1494, 0, -22, 0 }, { -1493, 0, 23, 0 }, { -22, 0, 1494, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, ONE, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 976, -64, 1792, 0 }, { { 784, 0, -720, 0 }, { 784, 0, 48, 0 }, { -784, 0, -48, 0 }, { -784, 0, 720, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, ONE, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6752, -64, 1312, 0 }, { { 1088, 0, -464, 0 }, { 1088, 0, 464, 0 }, { -1088, 0, -464, 0 }, { -1088, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, ONE, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3624, -64, 2944, 0 }, { { -1864, 0, -1552, 0 }, { 1912, 0, -1552, 0 }, { -1928, 0, 1008, 0 }, { 1880, 0, 1008, 0 } }, { 0, 4119, 0, 0 }, { 201, 0, -4091, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Placement-entry keys used by this room's resource recipes.
enum {
    DRYFIELD_GARAGE_GARY_RESOURCE_ENTRY = 106,
};

/// Gary Douglas's loading and body-spawn recipe for garage variant 1.
///
/// The map's single placement uses entry 106. Base-group-10 file 203 loads
/// actor_120300; the descriptor index selects Gary's body.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldGarageVariant1Resources[2] = {
    { .entryId = DRYFIELD_GARAGE_GARY_RESOURCE_ENTRY, .fileNumber = 203, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = ACTOR_120300_TASK_BODY, .taskTable = gActor120300TaskDescriptors },
    { .entryId = AREA_PLACEMENT_END },
};

/// Placement keys matching the garage's enemy loading recipes.
enum {
    DRYFIELD_GARAGE_MIND_SUCKLER_RESOURCE_ENTRY  = 15,
    DRYFIELD_GARAGE_DESERT_CHASER_RESOURCE_ENTRY = 75,
};

/// Variant 2's Mind Suckler recipe for five map-owned placements using entry 15.
///
/// Loads base-group-10 file 15 and spawns descriptor 0; the final record terminates
/// loading and lookup. The room, map and actor overlays must stay loaded while used.
static AreaResource _gDryfieldGarageVariant2Resources[2] = {
    { DRYFIELD_GARAGE_MIND_SUCKLER_RESOURCE_ENTRY, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Mind Suckler recipe for three map-owned placements using entry 15.
///
/// Loads base-group-10 file 15 and spawns descriptor 0; the final record terminates
/// loading and lookup. Kept as a distinct table for this layout despite matching
/// variant 2's recipe. The room, map and actor overlays must stay loaded while used.
static AreaResource _gDryfieldGarageVariant3Resources[2] = {
    { DRYFIELD_GARAGE_MIND_SUCKLER_RESOURCE_ENTRY, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 10's Desert Chaser recipe for its one room-owned placement using entry 75.
///
/// Loads base-group-40 file 75 and spawns descriptor 0; the final record terminates
/// loading and lookup. Keep the room and actor overlays loaded while used.
static AreaResource _gDryfieldGarageVariant10Resources[2] = {
    { DRYFIELD_GARAGE_DESERT_CHASER_RESOURCE_ENTRY, 75, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 10's single Desert Chaser placement and table terminator.
///
/// World position (4676, 0, 2038), yaw 3072 of 4096 per turn; texture pages are
/// unchanged and the CLUT row offset is 2. Spawn variant, mode and parameter row
/// are zero. Actors borrow the placement from the loaded garage overlay.
static AreaPlacement _gDryfieldGarageVariant10Placements[2] = {
    { DRYFIELD_GARAGE_DESERT_CHASER_RESOURCE_ENTRY, 0, 0, 4676, 0, 2038, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gDryfieldGarageAreaVariants[13] = {
    [1]  = { .placements = gMapDryfieldGarageVariant1Placements, .resources = _gDryfieldGarageVariant1Resources },
    [2]  = { .placements = gMapDryfieldGarageVariant2Placements, .resources = _gDryfieldGarageVariant2Resources },
    [3]  = { .placements = gMapDryfieldGarageVariant3Placements, .resources = _gDryfieldGarageVariant3Resources },
    [10] = { .placements = _gDryfieldGarageVariant10Placements, .resources = _gDryfieldGarageVariant10Resources },
};

/// View-count header followed by the fifteen views' minimum ambient colours.
///
/// Entries 1..15 use sixteen units per 8-bit colour level. The retained luma
/// values do not participate in ambient clamping. Views above the count use
/// gameplay's default ambient entry.
static const WorldCoordRoomAmbientEntry _gDryfieldGarageAmbientByView[16] SECTION(".data") = {
    { .viewCount = ARRAY_SIZE(_gDryfieldGarageAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 200, 200, 200, 200 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 500, 500, 500, 500 } },
    { .color = { 350, 350, 350, 350 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 560, 560, 560, 560 } },
};

enum {
    DRYFIELD_GARAGE_WALK_FOOTSTEP_CUE2 = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    DRYFIELD_GARAGE_RUN_FOOTSTEP_CUE2  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53,
};

/// Class-1 footstep cue bases in the currently loaded type-1 sound bank.
///
/// Cue 1 uses base + 1; ordinary companion playback adds 100. Scripted jumps
/// share the walking pair. The surface policy borrows this immutable record.
static const WorldCollisionFootstepSounds _gDryfieldGarageSurface1FootstepSounds SECTION(".data") = {
    .walk         = DRYFIELD_GARAGE_WALK_FOOTSTEP_CUE2,
    .run          = DRYFIELD_GARAGE_RUN_FOOTSTEP_CUE2,
    .scriptedJump = DRYFIELD_GARAGE_WALK_FOOTSTEP_CUE2,
};

/// Blocking, impact-enabled, pushback-enabled policy without footsteps for classes 0 and 2..7.
static WorldCollisionSurfaceProperties _gDryfieldGarageSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-1 blocking, impact-enabled, pushback-enabled policy with footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldGarageSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldGarageSurface1FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldGarageSurfacePropertiesByClass[8] = {
    &_gDryfieldGarageSilentSurfaceProperties,
    &_gDryfieldGarageSurface1Properties,
    &_gDryfieldGarageSilentSurfaceProperties,
    &_gDryfieldGarageSilentSurfaceProperties,
    &_gDryfieldGarageSilentSurfaceProperties,
    &_gDryfieldGarageSilentSurfaceProperties,
    &_gDryfieldGarageSilentSurfaceProperties,
    &_gDryfieldGarageSilentSurfaceProperties,
};

/// Low policy bit requesting a saved-area map mark.
enum { DRYFIELD_GARAGE_AREA_UPDATE_MAP_MARK = 1 };

const AreaApplyRec gDryfieldGarageGarySceneAreaUpdates[6] SECTION(".data") = {
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_GARAGE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_1, 3, AREA_APPLY_MODE_ALWAYS | DRYFIELD_GARAGE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_GARAGE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 2, AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_GARAGE_AREA_UPDATE_MAP_MARK },
    { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_GARAGE_AREA_UPDATE_MAP_MARK },
    { AREA_APPLY_END, 0, 0, 0 },
};

/// Last task spawned for the junk-yard refusal scene; the room never polls it.
///
/// The handle is borrowed, starts NULL and may become stale after the scene ends.
/// The four trailing bytes remain zero with no recovered access; their grouping
/// with the handle is unproven. Keep the existing eight-byte storage intact.
static _DryfieldGarageDoorSceneStorage _gDryfieldGarageDoorScene = { 0 };

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 222, 221, 253 } };

RoomEventReq gRoomEventReq;

#include "../../shared/room/room_event.inc.c"

/// The room task's three-state table, run from a stack copy by
/// `dryfieldGarageRoomTask`: the entry state
/// `_dryfieldGarageInitRoomTask`, the idle state
/// `_dryfieldGarageIdleRoomTask`, then `taskKill`.
static const TaskFuncTable3 _gDryfieldGarageRoomTaskStates = {
    { _dryfieldGarageInitRoomTask, _dryfieldGarageIdleRoomTask, taskKill },
};

#include "../../shared/garage_sound_msg.inc.c"

/// Refuses every key-item use request in Dryfield Garage.
///
/// Ignores all arguments and returns 0, selecting the inventory's "No use now"
/// notice without consuming an item or starting a room event.
static s32 _dryfieldGarageRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves factory rooms and gates junk-yard departure on the water-tank scene.
///
/// Borrows a complete eight-byte request and writable reply for
/// `ROOM_EVENT_MESSAGE_RESOLVE`; copies the request before resolving it.
/// Before the water-tank scene, junk-yard requests return 2 and executing requests
/// spawn the refusal scene. Once seen, the first request latches door/companion
/// progress even during a query. Factory execution selects room 1 or 2 from the
/// barrier flag. Other paths return 1; request and reply may alias.
static s32 _dryfieldGarageResolveRoomEventMessage(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        DRYFIELD_GARAGE_EVENT_ALLOW                    = 1,
        DRYFIELD_GARAGE_EVENT_REFUSE_WITH_SCENE        = 2,
        DRYFIELD_GARAGE_COMPANION_AFTER_JUNK_YARD_DOOR = 3,
        DRYFIELD_GARAGE_FACTORY_ROOM_DEFAULT           = 1,
        DRYFIELD_GARAGE_FACTORY_ROOM_BARRIER_CLEARED   = 2,
    };
    // This unused event-request local preserves the binary's stack frame.
    RoomEventReq unusedEventRequest;
    s32          factoryRoom;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_JUNK_YARD) {
        if (gameFlagGetNibble(GAME_FLAG_WATER_TANK_SCENE_SEEN) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                taskSpawnFromTable(_gDryfieldGarageJunkYardRefusalTaskDescriptors, 0, 0, 0);
            }
            return DRYFIELD_GARAGE_EVENT_REFUSE_WITH_SCENE;
        }
        if (gameFlagGetNibble(GAME_FLAG_GARAGE_JUNK_YARD_DOOR_PASSED) == 0) {
            gameFlagSetNibble(GAME_FLAG_GARAGE_JUNK_YARD_DOOR_PASSED, 1);
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, DRYFIELD_GARAGE_COMPANION_AFTER_JUNK_YARD_DOOR);
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_FACTORY) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            factoryRoom = gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED);
            if (factoryRoom == 0) {
                factoryRoom = DRYFIELD_GARAGE_FACTORY_ROOM_DEFAULT;
            } else {
                factoryRoom = DRYFIELD_GARAGE_FACTORY_ROOM_BARRIER_CLEARED;
            }
            reply->roomState = factoryRoom;
        }
    }
    return DRYFIELD_GARAGE_EVENT_ALLOW;
}

/// Selects one of two CAP commands for room command 16.
///
/// The live flag 0xFD selects command 22 when set, otherwise command 16.
/// Its story meaning is unproven. Other room commands do nothing; all return zero.
static s32 _dryfieldGarageHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_GARAGE_COMMAND_16         = 0x10,
        DRYFIELD_GARAGE_CAP_FLAG_0FD_CLEAR = 0x10,
        DRYFIELD_GARAGE_CAP_FLAG_0FD_SET   = 0x16,
    };
    if (command == DRYFIELD_GARAGE_COMMAND_16) {
        capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_0FD) != 0 ? DRYFIELD_GARAGE_CAP_FLAG_0FD_SET : DRYFIELD_GARAGE_CAP_FLAG_0FD_CLEAR);
    }
    return 0;
}

/// Starts CAP command 19 for directed room action 2 outside area variant 1.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows the four-byte request through dispatch;
/// only its unsigned action byte is read. The zero second word is ignored.
/// The result is unspecified and must not be consumed by a sender.
static s32 _dryfieldGarageHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_GARAGE_ACTION_2                  = 2,
        DRYFIELD_GARAGE_ACTION_SUPPRESSED_VARIANT = 1,
        DRYFIELD_GARAGE_CAP_ACTION_2              = 0x13,
    };
    if ((request->actionId == DRYFIELD_GARAGE_ACTION_2) && (gGameSession->location.variant != DRYFIELD_GARAGE_ACTION_SUPPRESSED_VARIANT)) {
        capSpawnEventIfIdle(DRYFIELD_GARAGE_CAP_ACTION_2, CAP_EVENT_NO_FLAGS);
    }
    // The binary leaves the result unspecified; this message's sender ignores it.
}

/// Spawns Gary Douglas's scene for a refused junk-yard departure, then ends.
///
/// Requires the loaded actor_120300 task table. Records the borrowed scene handle
/// for the room without polling it; the scene runs independently of this wrapper.
static void _dryfieldGarageJunkYardRefusalTask(Task* task)
{
    enum {
        DRYFIELD_GARAGE_REFUSAL_SPAWN = 0,
        DRYFIELD_GARAGE_REFUSAL_EXIT  = 1,
    };
    Task* sceneTask;
    s32   currentState;
    switch (task->state) {
        case DRYFIELD_GARAGE_REFUSAL_SPAWN:
            sceneTask                      = taskSpawnFromTable(gActor120300TaskDescriptors, ACTOR_120300_TASK_JUNK_YARD_REFUSAL, 0, 0);
            currentState                   = task->state;
            _gDryfieldGarageDoorScene.task = sceneTask;
            task->state                    = currentState + 1;
            break;
        case DRYFIELD_GARAGE_REFUSAL_EXIT:
            taskKill(task);
            break;
    }
}

/// Selects the garage entry's follow-up dialogue after the preceding story beat.
///
/// Changes story-dialogue index 1 to 2 and clears its follow-up state. Every
/// other index leaves both flags intact. Requires the live game-flag bank;
/// it selects later dialogue without starting CAP playback itself.
static inline void _dryfieldGarageAdvanceEntryDialogue(void)
{
    enum {
        DRYFIELD_GARAGE_PRE_ENTRY_DIALOGUE = 1,
        DRYFIELD_GARAGE_ENTRY_DIALOGUE     = 2,
    };

    if (gameFlagGetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX) == DRYFIELD_GARAGE_PRE_ENTRY_DIALOGUE) {
        gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
        gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, DRYFIELD_GARAGE_ENTRY_DIALOGUE);
    }
}

/// Registers the garage's room receiver and applies its entry placement and progress.
///
/// Requires the live room task in state 0 and the active garage layout. Warp 2
/// places a live companion at the room's entry transform. The first room-action
/// trigger is available only in variant 1. Borrows the message table and
/// advances to idle state 1.
static void _dryfieldGarageInitRoomTask(Task* task)
{
    enum {
        DRYFIELD_GARAGE_COMPANION_PLACEMENT_WARP = 2,
        DRYFIELD_GARAGE_ROOM_ACTION_VARIANT      = 1,
        DRYFIELD_GARAGE_VARIANT_ACTION_TRIGGER   = 0,
    };

    task->msgTable = _gDryfieldGarageMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) && (gGameSession->location.warp == DRYFIELD_GARAGE_COMPANION_PLACEMENT_WARP)) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldGarageCompanionEntryPlacement, 0);
    }
    _dryfieldGarageAdvanceEntryDialogue();
    if (gGameSession->location.variant != DRYFIELD_GARAGE_ROOM_ACTION_VARIANT) {
        _gDryfieldGarageActionTriggers[DRYFIELD_GARAGE_VARIANT_ACTION_TRIGGER].flags &= (u8)~WORLD_COLLISION_TRIGGER_ENABLED;
    }
    task->state += 1;
}

/// Keeps the initialized room task idle between messages.
///
/// Ignores `unusedTask` without advancing its state or releasing it; the room's
/// installed message table remains available to synchronous dispatch.
static void _dryfieldGarageIdleRoomTask(Task* unusedTask)
{
}

void dryfieldGarageRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldGarageRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void dryfieldGarageEffectNoopTaskD6(Task* unusedTask)
{
}
