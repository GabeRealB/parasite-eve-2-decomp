#include "rooms/shelter_b4_upper_sewer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00400.h"
#include "actors/actor_00300.h"
#include "actors/actor_01100.h"
#include "actors/actor_02300.h"
#include "actors/actor_02400.h"
#include "actors/actor_07000.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/effects/glow_draw.h"
#include "../../shared/effects/water_effects.h"

static void _waterDrawSpin(const GfxCoord* coord, s16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTile(const GfxCoord* coord, s16 frameIndex, s16 radiusScale);

/// A rectangular water patch in world coordinates whose entry chooses its own strip axis.
///
/// The same rectangle as `RoomWaterSurface`, with height supplied by its
/// drawer, but with a 16-bit segment count followed by a flag selecting which
/// extent that count divides, using integer division; the other extent spans
/// the strip. A list ends at an entry with
/// `segmentCount == WATER_SURFACE_LIST_END`, whose other fields are not read.
/// Every entry before it needs a nonzero `segmentCount`, and a positive one to
/// draw anything.
typedef struct {
    s16 x;              // Starting X in world units
    s16 z;              // Starting Z in world units
    s16 width;          // Extent along +X in world units
    s16 depth;          // Extent along +Z in world units
    s16 segmentCount;   // Quads in the strip (>0 drawable entry, -1 list end)
    s16 segmentsAlongZ; // Divided extent (0 width, quads side by side along X; nonzero depth, along Z)
} _ShelterB4UpperSewerWaterSurface;
STATIC_ASSERT_SIZEOF(_ShelterB4UpperSewerWaterSurface, 0xC);

static void _shelterB4UpperSewerInitializeRoom(Task* task);
static void _shelterB4UpperSewerIdleRoom(Task* task);
static void _shelterB4UpperSewerDrawWaterSurfaces(Task* task, const _ShelterB4UpperSewerWaterSurface* surface, s16 waterY, u8 blueIntensity);
static void _shelterB4UpperSewerInitializeWater(Task* task);
static void _shelterB4UpperSewerSetView13SpriteHidden(s32 hidden);
#include "../../shared/room/room_visual_effects.h"

/// Room lifecycle callbacks indexed by task state (0 setup, 1 idle, 2 teardown).
///
/// Copied by value for dispatch; all three slots are live, with no terminator.
static const TaskFuncTable3 _gShelterB4UpperSewerRoomStates = {
    { _shelterB4UpperSewerInitializeRoom, _shelterB4UpperSewerIdleRoom, taskKill }
};

static void _shelterB4UpperSewerWaterTask(Task* task);

static s32  _shelterB4UpperSewerRejectKeyItemMessage(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
static s32  _shelterB4UpperSewerResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB4UpperSewerHandleWaterHoleCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _shelterB4UpperSewerIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _shelterB4UpperSewerHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg);
static void _shelterB4UpperSewerOpenWaterHoleTask(Task* task);
static void _shelterB4UpperSewerExitTransitTask(Task* task);
static void _shelterB4UpperSewerRestoreSavedView(void);

/// Synchronous room messages for transitions, key items, actions, commands and sound.
///
/// Installed on the room task; the final entry is `TASK_MESSAGE_TABLE_END`.
/// Transition records and action requests are borrowed only for dispatch.
static TaskMessageEntry _gShelterB4UpperSewerRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB4UpperSewerResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB4UpperSewerRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB4UpperSewerIgnoreRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _shelterB4UpperSewerHandleWaterHoleCommand },
    { ROOM_MESSAGE_SOUND, _shelterB4UpperSewerHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless priority-32 tasks for the water-hole scene (0) and exit transit (1).
///
/// Indexed directly without a terminator. Exit transit takes its confirmation
/// CAP command in spawn argument 1; the opening scene ignores both arguments.
static TaskDesc _gShelterB4UpperSewerInteractionTasks[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB4UpperSewerOpenWaterHoleTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB4UpperSewerExitTransitTask, { .value = 0 } },
};

enum {
    SHELTER_B4_UPPER_SEWER_WATER_HOLE_EVENT_CAP_COMMAND    = 2,
    SHELTER_B4_UPPER_SEWER_WATER_HOLE_RESTORE_DELAY_FRAMES = 3,
};

/// Water-hole opening script, ending at `EVENT_SCRIPT_OPCODE_END`.
///
/// Starts CAP 2, waits for its two cues around room sound 5, restores the saved
/// view and shows both actors, then releases scripted control after a
/// three-update countdown.
/// The interpreter borrows this stream and its callback until completion.
static EvsCommand _gShelterB4UpperSewerWaterHoleScript[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = SHELTER_B4_UPPER_SEWER_WATER_HOLE_EVENT_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 5) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB4UpperSewerRestoreSavedView }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B4_UPPER_SEWER_WATER_HOLE_RESTORE_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s16 gShelterB4UpperSewerWaterY = -1600;

/// Single bodyless priority-96 water task recipe, indexed at zero without a terminator.
static TaskDesc _gShelterB4UpperSewerWaterTasks[1] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_ACTOR } }, _shelterB4UpperSewerWaterTask, { .value = 0 } },
};

enum {
    SHELTER_B4_UPPER_SEWER_WATER_STRIP_ALONG_X = 0,
    SHELTER_B4_UPPER_SEWER_WATER_STRIP_ALONG_Z = 1,
};

/// Five water patches and their shared `WATER_SURFACE_LIST_END` record.
///
/// View 12 starts at entry 1; every other view starts at entry 0. World-space
/// rectangles draw 16, 32, 32, 32 and 32 quads at the supplied water height.
static _ShelterB4UpperSewerWaterSurface _gShelterB4UpperSewerWaterSurfaces[6] = {
    { -9700, 5600, 1720, 1800, 16, SHELTER_B4_UPPER_SEWER_WATER_STRIP_ALONG_X },
    { -8000, 5000, 7120, 2900, 32, SHELTER_B4_UPPER_SEWER_WATER_STRIP_ALONG_X },
    { -900, 3100, 2800, 4800, 32, SHELTER_B4_UPPER_SEWER_WATER_STRIP_ALONG_Z },
    { -900, -5900, 1800, 9000, 32, SHELTER_B4_UPPER_SEWER_WATER_STRIP_ALONG_Z },
    { 900, -5900, 3000, 3800, 32, SHELTER_B4_UPPER_SEWER_WATER_STRIP_ALONG_Z },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END, 0 },
};

/// Pool of consecutive world-point pairs used by upper-sewer glow drawing.
///
/// XYZ are signed world coordinates. Mapped views borrow overlapping ranges
/// of this pool during drawing; each selected pair includes its next point.
/// Zero-based pair 12 is shared by views 4, 8, 9 and 13; pair 9 by views 11 and 12.
static SVECTOR _gShelterB4UpperSewerGlowPoints[] = {
    { -7580, -5500, 9840, 0 },
    { -6420, -5500, 9840, 0 },
    { -3580, -5500, 9840, 0 },
    { -2420, -5500, 9840, 0 },
    { 420, -5500, 9840, 0 },
    { 1580, -5500, 9840, 0 },
    { 3600, -5500, 7080, 0 },
    { 3600, -5500, 5920, 0 },
    { 3600, -5500, 3080, 0 },
    { 3600, -5500, 1920, 0 },
    { 3600, -5500, -920, 0 },
    { 3600, -5500, -2080, 0 },
    { 2080, -3480, -5900, 0 },
    { 910, -3480, -5900, 0 },
    { -600, -3480, -80, 0 },
    { -600, -3480, 1080, 0 },
    { -2420, -3480, 5400, 0 },
    { -3580, -3480, 5400, 0 },
    { -6420, -3480, 5400, 0 },
    { -7580, -3480, 5400, 0 },
    { -8730, -4410, 5060, 0 },
    { -9280, -4410, 5060, 0 },
    { 3930, -2500, 8490, 0 },
    { 3930, -2500, 7790, 0 },
    { 3950, -3700, -210, 0 },
    { 3950, -3700, -790, 0 },
};

/// Initializes this overlay's three halo tint rows.
///
/// Rows 0..2 give RGB right shifts of brightness (0 full, 1 half, 2 quarter).
/// Requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` = `RoomFxShade` and
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` =
/// `[ROOM_VISUAL_EFFECTS_HALO_SHADE_COUNT]`; supplies exactly three elements.
/// Bind around `room_visual_effects_halo_data.inc.c`, then undefine.
/// Expands to a constant braced initializer, with no arguments or side effects.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER { \
    { .rShift = 0, .gShift = 1, .bShift = 2 },         \
    { .rShift = 2, .gShift = 1, .bShift = 0 },         \
    { .rShift = 0, .gShift = 2, .bShift = 1 },         \
}
/// Selects the element type of this overlay's three-row halo tint array.
///
/// Expands to the `RoomFxShade` type token; requires
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` =
/// `[ROOM_VISUAL_EFFECTS_HALO_SHADE_COUNT]` and a
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER` containing three RGB-shift rows.
/// Read indices are 0..2. Include `room_visual_effects.h` first, bind around
/// `room_visual_effects_halo_data.inc.c`, then undefine.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE RoomFxShade
/// Declares the three-row halo tint array.
///
/// Supplies an array bound in `RoomFxShade` elements; requires
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` to be `RoomFxShade`. Bind around
/// `room_visual_effects_halo_data.inc.c`.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND [ROOM_VISUAL_EFFECTS_HALO_SHADE_COUNT]
#include "../../shared/room/room_visual_effects_halo_data.inc.c"
#include "../../shared/room/room_visual_effects_trail_data.inc.c"
#include "../../shared/room/room_visual_effects_disc_data.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades;
}
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND

u8* gShelterB4UpperSewerViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB4UpperSewerViewCounts[1] = { ARRAY_SIZE(gShelterB4UpperSewerViewCameras) };

DirectionWarpEntry gShelterB4UpperSewerWarpEntries[4] = {
    { { { .word = 1024 }, -9200, 0, 6080 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -9200, 0, 6850 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 2), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 1), DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 3344, 0, -3054 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 3350, 0, -3930 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 3), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_FADE_DEPARTURE, GAME_FLAG_MAP_MARK_RESERVOIR_1BE },
    { { { .word = 3072 }, 3360, -2000, 9000 }, { 0, 0, 0, 0 }, { { .word = 2816 }, 3360, -2000, 8300 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 3), DIRECTION_WARP_SOUND_NONE, 10, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -8960, -2000, 5600 }, { 0, 0, 0, 0 }, { { .word = 512 }, -9600, -2000, 6200 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 7), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 6), DIRECTION_WARP_SOUND_NONE, 12, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Eight-point Bog Diver patrol ring selected by spawn ring indices 0, 2 and 3.
///
/// X/Z use world units; Y is depth relative to the water height sampled at spawn.
/// Indices wrap at eight with no terminator; divers borrow the loaded room's ring.
static SVECTOR _gShelterB4UpperSewerDiverPatrolRing0[8] = {
    { 200, 1000, -3000, 0 },
    { 200, 1000, 1000, 0 },
    { 200, 1000, 5000, 0 },
    { -1700, 1000, 6800, 0 },
    { -3800, 1000, 6800, 0 },
    { 200, 1000, 3900, 0 },
    { 0, 1000, -800, 0 },
    { 400, 1000, -3200, 0 },
};

/// Eight-point Bog Diver patrol ring selected by spawn ring index 1.
///
/// X/Z use world units; Y is depth relative to the water height sampled at spawn.
/// Indices wrap at eight with no terminator; divers borrow the loaded room's ring.
static SVECTOR _gShelterB4UpperSewerDiverPatrolRing1[8] = {
    { 160, 500, 6300, 0 },
    { -3680, 500, 6300, 0 },
    { -6500, 500, 6300, 0 },
    { 0, 500, 6300, 0 },
    { 0, 500, 2500, 0 },
    { 0, 500, -2800, 0 },
    { 0, 500, -800, 0 },
    { 0, 500, 1200, 0 },
};

SVECTOR* gShelterB4UpperSewerDiverPatrolRings[4] = {
    _gShelterB4UpperSewerDiverPatrolRing0,
    _gShelterB4UpperSewerDiverPatrolRing1,
    _gShelterB4UpperSewerDiverPatrolRing0,
    _gShelterB4UpperSewerDiverPatrolRing0,
};

enum {
    SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_FREE = 0,
    SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_END  = -1,
};

SVECTOR gShelterB4UpperSewerDiverSurfaceSpots[9] = {
    { 0, 0, 0, SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_FREE },
    { 0, 1000, -2300, SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_FREE },
    { 0, 1000, 700, SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_FREE },
    { 200, 1000, 4000, SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_FREE },
    { 500, 1000, 6600, SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_FREE },
    { -2000, 1000, 6600, SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_FREE },
    { -4500, 1000, 6600, SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_FREE },
    { -6600, 1000, 6600, SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_FREE },
    { 0, 0, 0, SHELTER_B4_UPPER_SEWER_DIVER_SURFACE_SPOT_END },
};

/// Room-space collision normals, with ONE (4096) representing unit length.
static SVECTOR _gShelterB4UpperSewerCollisionGridNormals[] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_normals.inc"
};

/// Collision vertex pool in signed world-coordinate units.
static SVECTOR _gShelterB4UpperSewerCollisionGridVerts[] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_verts.inc"
};

/// Indexed collision quads carrying normals and room-local surface classes.
static WorldCollisionGridFace _gShelterB4UpperSewerCollisionGridFaces[] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_faces.inc"
};

/// Packed cell face-index lists, each ending at WORLD_COLLISION_GRID_CELL_END.
///
/// The cell table selects list starts; stored words after a list's end are
/// retained asset data and are outside that list.
static s16 _gShelterB4UpperSewerCollisionGridCells[] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_cells.inc"
};

enum {
    SHELTER_B4_UPPER_SEWER_COLLISION_CELL_COUNT_X = 6,
    SHELTER_B4_UPPER_SEWER_COLLISION_CELL_COUNT_Z = 5
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB4UpperSewerCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..291
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B4_UPPER_SEWER_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB4UpperSewerCollisionGridCells[(halfwordOffset)])
/// X-major cell table, indexed by x * cellCountZ + z.
static s16* _gShelterB4UpperSewerCollisionGridTable[] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_table.inc"
};
#undef SHELTER_B4_UPPER_SEWER_CELL_FACE_LIST_AT_OFFSET

STATIC_ASSERT_SIZEOF(_gShelterB4UpperSewerCollisionGridTable,
                     sizeof(s16*) * SHELTER_B4_UPPER_SEWER_COLLISION_CELL_COUNT_X * SHELTER_B4_UPPER_SEWER_COLLISION_CELL_COUNT_Z);

WorldCollisionGrid gShelterB4UpperSewerCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB4UpperSewerCollisionGridNormals,
    .vertices    = _gShelterB4UpperSewerCollisionGridVerts,
    .faces       = _gShelterB4UpperSewerCollisionGridFaces,
    .cellFaceIds = _gShelterB4UpperSewerCollisionGridTable,
    .xBias       = 10010,
    .zBias       = 6000,
    .cellCountX  = SHELTER_B4_UPPER_SEWER_COLLISION_CELL_COUNT_X,
    .cellCountZ  = SHELTER_B4_UPPER_SEWER_COLLISION_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB4UpperSewerCollisionGridFaces),
};

ViewCamera gShelterB4UpperSewerViewCameras[14] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x7530, -1010 } }, 230 },
    { { { { -4063, 0, -514 }, { -449, 1990, 3551 }, { 249, 3580, -1974 } }, { -1380, 5260, 1530 } }, 257 },
    { { { { -4012, 0, -820 }, { -133, 4041, 653 }, { 809, 666, -3959 } }, { 130, 1900, -3960 } }, 257 },
    { { { { -3991, 0, -921 }, { -208, 3989, 902 }, { 897, 925, -3887 } }, { 360, 2120, -7690 } }, 257 },
    { { { { -247, 0, 4088 }, { 3783, 1552, 228 }, { -1550, 3790, -93 } }, { -2160, 5120, -6800 } }, 257 },
    { { { { -655, 0, 4043 }, { 1258, 3892, 203 }, { -3842, 1274, -622 } }, { -1210, 1950, -7340 } }, 257 },
    { { { { -509, 0, 4064 }, { 1212, 3909, 151 }, { -3879, 1221, -486 } }, { 5360, 1950, -6825 } }, 257 },
    { { { { -3832, 0, -1445 }, { -47, 4093, 126 }, { 1445, 135, -3830 } }, { -180, 3740, -6530 } }, 257 },
    { { { { -3906, 0, -1230 }, { -185, 4049, 589 }, { 1216, 617, -3862 } }, { -900, 3500, -9640 } }, 257 },
    { { { { 751, 0, -4026 }, { -274, 4086, -51 }, { 4017, 278, 749 } }, { 6060, 3500, -7800 } }, 257 },
    { { { { 683, 0, 4038 }, { 309, 4083, -52 }, { -4026, 313, 681 } }, { 1210, 3500, -7740 } }, 257 },
    { { { { -3964, 0, 1030 }, { 165, 4042, 636 }, { -1016, 657, -3912 } }, { 8020, 3500, -9160 } }, 257 },
    { { { { -1086, 0, -3949 }, { -519, 4060, 142 }, { 3914, 538, -1077 } }, { -3130, 3460, 250 } }, 380 },
    { { { { -1049, 0, -3959 }, { 841, 4002, -223 }, { 3868, -870, -1025 } }, { -0x2E41, 3890, 333 } }, 230 },
};

/// Empty batch list for mapped view 1, retaining the decoded background.
///
/// The zero-count first record precedes SPRITE_BATCH_END. Its retained address
/// in the source slot supplies no source sprites and is never dereferenced as one.
static SpriteBatch _gShelterB4UpperSewerView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture format and colour/code defaults for the foreground sprite sources.
enum {
    SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT     = 1,
    SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR    = 128,
    SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED = 0
};

/// Foreground sprite rectangles for mapped view 2.
///
/// Positions are pixels relative to the view centre; dimensions are pixels and
/// depths are unscaled sorting values. Texture pages and CLUT encode VRAM addresses.
/// The room owns the mutable sources borrowed by its view's batch list.
static SpriteSource _gShelterB4UpperSewerView2SpriteSources[] = {
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -160, 72, 750, { .fields = { 104, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 40 } }, -136, 80, 750, { .fields = { 64, 48 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 32 } }, -72, 88, 750, { .fields = { 72, 88 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, -16, 96, 750, { .fields = { 72, 120 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
};

/// Foreground batch list for mapped view 2, retaining the decoded background.
///
/// The zero-count first record selects image strips; the next draws the complete
/// source array. SPRITE_BATCH_END terminates the list, independently of counts.
static SpriteBatch _gShelterB4UpperSewerView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4UpperSewerView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 3.
///
/// Positions are pixels relative to the view centre; dimensions are pixels and
/// depths are unscaled sorting values. Texture pages and CLUT encode VRAM addresses.
/// The room owns the mutable sources borrowed by its view's batch list.
static SpriteSource _gShelterB4UpperSewerView3SpriteSources[] = {
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -8, -48, 1250, { .fields = { 112, 184 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -24, -56, 1250, { .fields = { 112, 136 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -24, 56, 1094, { .fields = { 120, 208 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 128 } }, -104, -56, 1250, { .fields = { 120, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 136 } }, -160, -64, 1250, { .fields = { 72, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
};

/// Foreground batch list for mapped view 3, retaining the decoded background.
///
/// The zero-count first record selects image strips; the next draws the complete
/// source array. SPRITE_BATCH_END terminates the list, independently of counts.
static SpriteBatch _gShelterB4UpperSewerView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4UpperSewerView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 4.
///
/// Positions are pixels relative to the view centre; dimensions are pixels and
/// depths are unscaled sorting values. Texture pages and CLUT encode VRAM addresses.
/// The room owns the mutable sources borrowed by its view's batch list.
static SpriteSource _gShelterB4UpperSewerView4SpriteSources[] = {
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 16, -56, 2200, { .fields = { 96, 136 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 8, -56, 2125, { .fields = { 96, 88 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 8, -8, 2125, { .fields = { 96, 112 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 80 } }, -16, -56, 2125, { .fields = { 104, 88 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 88 } }, -56, -64, 2125, { .fields = { 88, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 104, 80 } }, -160, -56, 2125, { .fields = { 24, 168 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
};

/// Foreground batch list for mapped view 4, retaining the decoded background.
///
/// The zero-count first record selects image strips; the next draws the complete
/// source array. SPRITE_BATCH_END terminates the list, independently of counts.
static SpriteBatch _gShelterB4UpperSewerView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4UpperSewerView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 5.
///
/// Positions are pixels relative to the view centre; dimensions are pixels and
/// depths are unscaled sorting values. Texture pages and CLUT encode VRAM addresses.
/// The room owns the mutable sources borrowed by its view's batch list.
static SpriteSource _gShelterB4UpperSewerView5SpriteSources[] = {
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 40 } }, -160, 80, 726, { .fields = { 64, 184 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, -96, 80, 726, { .fields = { 8, 184 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 32 } }, -40, 88, 722, { .fields = { 16, 224 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 32 } }, 8, 88, 722, { .fields = { 8, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, 56, 88, 722, { .fields = { 16, 32 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 24 } }, 96, 96, 718, { .fields = { 120, 64 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 32 } }, 96, 64, 746, { .fields = { 64, 224 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 48 } }, 88, 16, 791, { .fields = { 56, 48 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 48 } }, 88, -32, 852, { .fields = { 56, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 80, 48 } }, 80, -80, 925, { .fields = { 48, 96 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 80, 40 } }, 80, -120, 1000, { .fields = { 48, 144 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
};

/// Foreground batch list for mapped view 5, retaining the decoded background.
///
/// The zero-count first record selects image strips; the next draws the complete
/// source array. SPRITE_BATCH_END terminates the list, independently of counts.
static SpriteBatch _gShelterB4UpperSewerView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4UpperSewerView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty batch list for mapped view 6, retaining the decoded background.
///
/// The zero-count first record precedes SPRITE_BATCH_END. Its retained address
/// in the source slot supplies no source sprites and is never dereferenced as one.
static SpriteBatch _gShelterB4UpperSewerView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty batch list for mapped view 7, retaining the decoded background.
///
/// The zero-count first record precedes SPRITE_BATCH_END. Its retained address
/// in the source slot supplies no source sprites and is never dereferenced as one.
static SpriteBatch _gShelterB4UpperSewerView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 8.
///
/// Eight-texel-high strips carry individual depths to follow the foreground.
/// Positions are pixels relative to the view centre; dimensions are pixels and
/// depths are unscaled sorting values. Texture pages and CLUT encode VRAM addresses.
/// The room owns the mutable sources borrowed by its view's batch list.
static SpriteSource _gShelterB4UpperSewerView8SpriteSources[] = {
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -16, 32, 2555, { .fields = { 120, 224 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 40, 2250, { .fields = { 120, 232 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 48, 2150, { .fields = { 120, 240 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 56, 2000, { .fields = { 120, 248 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -40, 40, 2555, { .fields = { 80, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 8, 40, 2450, { .fields = { 72, 8 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -56, 48, 2128, { .fields = { 72, 16 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 0, 48, 2175, { .fields = { 64, 24 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 8 } }, -80, 56, 1900, { .fields = { 56, 32 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 8 } }, -8, 56, 2044, { .fields = { 56, 40 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -96, 64, 1825, { .fields = { 80, 48 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -48, 64, 1825, { .fields = { 72, 56 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 8, 64, 1825, { .fields = { 72, 64 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -112, 72, 1750, { .fields = { 72, 72 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -56, 72, 1750, { .fields = { 64, 80 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 8, 72, 1750, { .fields = { 64, 88 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -136, 80, 1700, { .fields = { 88, 96 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -96, 80, 1700, { .fields = { 80, 104 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -48, 80, 1700, { .fields = { 72, 112 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 8, 80, 1700, { .fields = { 64, 120 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -152, 88, 1625, { .fields = { 72, 128 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -96, 88, 1625, { .fields = { 72, 136 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -40, 88, 1625, { .fields = { 72, 144 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, 16, 88, 1391, { .fields = { 88, 152 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 8, 96, 1275, { .fields = { 80, 160 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 8, 104, 1225, { .fields = { 80, 168 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -16, 112, 1200, { .fields = { 64, 176 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -56, 104, 1225, { .fields = { 64, 184 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -56, 96, 1275, { .fields = { 64, 192 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -80, 112, 892, { .fields = { 64, 200 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, 104, 1225, { .fields = { 104, 208 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, 96, 1275, { .fields = { 104, 216 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
};

/// Foreground batch list for mapped view 8, retaining the decoded background.
///
/// The zero-count first record selects image strips; the next draws the complete
/// source array. SPRITE_BATCH_END terminates the list, independently of counts.
static SpriteBatch _gShelterB4UpperSewerView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4UpperSewerView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 9.
///
/// Eight-texel-high strips carry individual depths to follow the foreground.
/// Positions are pixels relative to the view centre; dimensions are pixels and
/// depths are unscaled sorting values. Texture pages and CLUT encode VRAM addresses.
/// The room owns the mutable sources borrowed by its view's batch list.
static SpriteSource _gShelterB4UpperSewerView9SpriteSources[] = {
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -8, -8, 2450, { .fields = { 88, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 32, -8, 2450, { .fields = { 80, 8 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -24, 0, 2375, { .fields = { 80, 16 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 0, 2375, { .fields = { 72, 24 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -40, 8, 2250, { .fields = { 72, 32 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 16, 8, 2250, { .fields = { 64, 40 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 16, 16, 2000, { .fields = { 64, 48 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 16, 24, 1692, { .fields = { 64, 56 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
};

/// Foreground batch list for mapped view 9, retaining the decoded background.
///
/// The zero-count header selects image strips; the following range draws the
/// complete source array. `SPRITE_BATCH_END` terminates the list.
static SpriteBatch _gShelterB4UpperSewerView9SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4UpperSewerView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped view 10, retaining the decoded background.
///
/// The zero-count header precedes `SPRITE_BATCH_END`. The source slot retains
/// this list's address without supplying or dereferencing source sprites.
static SpriteBatch _gShelterB4UpperSewerView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 11.
///
/// Positions and dimensions are pixels relative to the view centre; depths
/// are unscaled sorting values. Texture pages and CLUTs encode VRAM addresses.
/// The room owns the mutable sources borrowed by this view's batches.
static SpriteSource _gShelterB4UpperSewerView11SpriteSources[3] = {
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 96, 8 } }, -136, 24, 2000, { .fields = { 32, 16 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 104, 8 } }, -144, 32, 1750, { .fields = { 24, 8 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 104, 8 } }, -144, 40, 1625, { .fields = { 24, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
};

/// Foreground batch list for mapped view 11, retaining the decoded background.
///
/// The zero-count header selects image strips; the following range draws the
/// complete source array. `SPRITE_BATCH_END` terminates the list.
static SpriteBatch _gShelterB4UpperSewerView11SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4UpperSewerView11SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 12.
///
/// Positions and dimensions are pixels relative to the view centre; depths
/// are unscaled sorting values. Texture pages and CLUTs encode VRAM addresses.
/// The room owns the mutable sources borrowed by this view's batches.
static SpriteSource _gShelterB4UpperSewerView12SpriteSources[26] = {
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -32, 48, 1175, { .fields = { 80, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 16, 48, 1175, { .fields = { 72, 8 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -72, 56, 1125, { .fields = { 72, 16 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -16, 56, 1125, { .fields = { 80, 24 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 32, 56, 1125, { .fields = { 80, 32 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -72, 64, 1075, { .fields = { 72, 40 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -16, 64, 1075, { .fields = { 72, 48 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 40, 64, 1075, { .fields = { 72, 56 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -72, 72, 1025, { .fields = { 72, 64 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -16, 72, 1025, { .fields = { 64, 72 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 48, 72, 1025, { .fields = { 72, 80 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -72, 80, 975, { .fields = { 64, 88 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -8, 80, 975, { .fields = { 64, 96 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 56, 80, 975, { .fields = { 72, 104 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -72, 88, 925, { .fields = { 64, 112 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -8, 88, 925, { .fields = { 64, 120 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 8 } }, 56, 88, 925, { .fields = { 56, 128 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 8 } }, -72, 96, 875, { .fields = { 56, 136 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 0, 96, 875, { .fields = { 64, 144 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 8 } }, 64, 96, 875, { .fields = { 56, 152 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 88, 8 } }, -64, 104, 825, { .fields = { 40, 160 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 24, 104, 825, { .fields = { 64, 168 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 88, 104, 825, { .fields = { 64, 176 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 80, 8 } }, -64, 112, 775, { .fields = { 48, 184 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 80, 8 } }, 16, 112, 775, { .fields = { 48, 192 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 96, 112, 775, { .fields = { 64, 200 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
};

/// Foreground batch list for mapped view 12, retaining the decoded background.
///
/// The zero-count header selects image strips; the following range draws the
/// complete source array. `SPRITE_BATCH_END` terminates the list.
static SpriteBatch _gShelterB4UpperSewerView12SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4UpperSewerView12SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 13.
///
/// Positions and dimensions are pixels relative to the view centre; depths
/// are unscaled sorting values. Texture pages and CLUTs encode VRAM addresses.
/// The room owns the mutable sources borrowed by this view's batches.
static SpriteSource _gShelterB4UpperSewerView13SpriteSources[1] = {
    { getTPage(SHELTER_B4_UPPER_SEWER_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 112 } }, 0, -56, 180, { .fields = { 64, 0 } }, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_UPPER_SEWER_SPRITE_OPAQUE_MODULATED },
};

/// Foreground batch list for mapped view 13, retaining the decoded background.
///
/// The zero-count header selects image strips; the following range draws the
/// complete source array. `SPRITE_BATCH_END` terminates the list.
/// The water-hole scene changes the foreground batch's `hidden` byte.
static SpriteBatch _gShelterB4UpperSewerView13SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4UpperSewerView13SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped view 14, retaining the decoded background.
///
/// The zero-count header precedes `SPRITE_BATCH_END`. The source slot retains
/// this list's address without supplying or dereferencing source sprites.
static SpriteBatch _gShelterB4UpperSewerView14SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB4UpperSewerSpriteViews[14] = {
    { { .empty = _gShelterB4UpperSewerView1SpriteBatches }, _gShelterB4UpperSewerView1SpriteBatches, NULL },
    { { .elements = _gShelterB4UpperSewerView2SpriteSources }, _gShelterB4UpperSewerView2SpriteBatches, NULL },
    { { .elements = _gShelterB4UpperSewerView3SpriteSources }, _gShelterB4UpperSewerView3SpriteBatches, NULL },
    { { .elements = _gShelterB4UpperSewerView4SpriteSources }, _gShelterB4UpperSewerView4SpriteBatches, NULL },
    { { .elements = _gShelterB4UpperSewerView5SpriteSources }, _gShelterB4UpperSewerView5SpriteBatches, NULL },
    { { .empty = _gShelterB4UpperSewerView6SpriteBatches }, _gShelterB4UpperSewerView6SpriteBatches, NULL },
    { { .empty = _gShelterB4UpperSewerView7SpriteBatches }, _gShelterB4UpperSewerView7SpriteBatches, NULL },
    { { .elements = _gShelterB4UpperSewerView8SpriteSources }, _gShelterB4UpperSewerView8SpriteBatches, NULL },
    { { .elements = _gShelterB4UpperSewerView9SpriteSources }, _gShelterB4UpperSewerView9SpriteBatches, NULL },
    { { .empty = _gShelterB4UpperSewerView10SpriteBatches }, _gShelterB4UpperSewerView10SpriteBatches, NULL },
    { { .elements = _gShelterB4UpperSewerView11SpriteSources }, _gShelterB4UpperSewerView11SpriteBatches, NULL },
    { { .elements = _gShelterB4UpperSewerView12SpriteSources }, _gShelterB4UpperSewerView12SpriteBatches, NULL },
    { { .elements = _gShelterB4UpperSewerView13SpriteSources }, _gShelterB4UpperSewerView13SpriteBatches, NULL },
    { { .empty = _gShelterB4UpperSewerView14SpriteBatches }, _gShelterB4UpperSewerView14SpriteBatches, NULL },
};

/// Point-light placements and falloff for the upper sewer's model lighting.
///
/// All 19 lights admit every room view. Positions and falloff radii use world
/// units; RGB intensities use 12 fractional bits. View parenting, composition
/// caches and attenuation are mutable while this room overlay remains loaded.
static WorldCoordPointLight _gShelterB4UpperSewerPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -3000, 500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3161, 4401 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -3000, 3995 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3241, 5061 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -7000, 0x2C24 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2710, -7000, 0x2C24 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -7000, 0x4650 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2710, -7000, 0x4650 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3500, -4000, 0x2C24 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1228, 1228, 1228 }, { 0, 0 } }, 6000, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1500, -3000, -5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3621, 6283 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7000, -3000, 6000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3825, -3703, -500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, ONE, ONE }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3805, -2489, 8145 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9000, -4570, 5430 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, ONE, 3276 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3000, -3000, 6000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3364, 5642 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3000, -5500, 9000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -5500, 9000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -5500, 6505 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -5500, 2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -5500, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7000, -5500, 9000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
};

WorldCoordRoomLights gShelterB4UpperSewerRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB4UpperSewerPointLights),
    .pointLights           = _gShelterB4UpperSewerPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB4UpperSewerViewBoundaryTriggers[18] = {
    { NULL, NULL, NULL, { -1938, -1921, -3234, 0 }, { { -3109, -4304, -1110, 0 }, { 2989, -4304, 1073, 0 }, { -3109, 4304, -1110, 0 }, { 2989, 4304, 1073, 0 } }, { 1382, 0, -3863, 0 }, { 0, 0, ONE, 0 }, 5369, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1698, -1920, -3234, 0 }, { { 2834, -4304, 1035, 0 }, { -2923, -4304, -1085, 0 }, { 2834, 4304, 1035, 0 }, { -2923, 4304, -1085, 0 } }, { -1418, 0, 3848, 0 }, { 0, 0, ONE, 0 }, 5271, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -129, -2017, 4319, 0 }, { { 2168, -4304, 303, 0 }, { -2173, -4304, -308, 0 }, { 2168, 4304, 303, 0 }, { -2173, 4304, -308, 0 } }, { -575, 0, 4078, 0 }, { 0, 0, ONE, 0 }, 4830, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 159, -2016, 4639, 0 }, { { -1890, -4304, -267, 0 }, { 1883, -4304, 260, 0 }, { -1890, 4304, -267, 0 }, { 1883, 4304, 260, 0 } }, { 566, 0, -4060, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1196, -2400, 6326, 0 }, { { -23, -4304, -1879, 0 }, { 7, -4304, 1860, 0 }, { -23, 4304, -1879, 0 }, { 7, 4304, 1860, 0 } }, { 4099, 0, -34, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1156, -2401, 6284, 0 }, { { 6, -4304, 1882, 0 }, { -40, -4304, -1915, 0 }, { 6, 4304, 1882, 0 }, { -40, 4304, -1915, 0 } }, { -4103, 0, 49, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7553, -2369, 6143, 0 }, { { 169, -4304, 1881, 0 }, { -168, -4304, -1880, 0 }, { 169, 4304, 1881, 0 }, { -168, 4304, -1880, 0 } }, { -4082, 0, 365, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7841, -2368, 6175, 0 }, { { -199, -4304, -1861, 0 }, { 199, -4304, 1861, 0 }, { -199, 4304, -1861, 0 }, { 199, 4304, 1861, 0 } }, { 4073, 0, -436, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4160, -2177, 3264, 0 }, { { 2163, -4304, 302, 0 }, { -2177, -4304, -310, 0 }, { 2163, 4304, 302, 0 }, { -2177, 4304, -310, 0 } }, { -576, 0, 4077, 0 }, { 0, 0, ONE, 0 }, 4830, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4175, -2209, 3376, 0 }, { { -2206, -4304, -326, 0 }, { 2200, -4304, 319, 0 }, { -2206, 4304, -326, 0 }, { 2200, 4304, 319, 0 } }, { 594, 0, -4065, 0 }, { 0, 0, ONE, 0 }, 4830, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3983, -2368, 6863, 0 }, { { 2098, -4304, 7, 0 }, { -2100, -4304, -8, 0 }, { 2098, 4304, 7, 0 }, { -2100, 4304, -8, 0 } }, { -15, 0, 4101, 0 }, { 0, 0, ONE, 0 }, 4775, 0, 10, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3776, -2369, 6912, 0 }, { { -1876, -4304, 12, 0 }, { 1868, -4304, -20, 0 }, { -1876, 4304, 12, 0 }, { 1868, 4304, -20, 0 } }, { -36, 0, -4098, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 9, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3968, -2369, 9760, 0 }, { { 74, -4304, -1872, 0 }, { -79, -4304, 1868, 0 }, { 74, 4304, -1872, 0 }, { -79, 4304, 1868, 0 } }, { 4092, 0, 167, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 10, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3777, -2336, 9791, 0 }, { { -19, -4304, 1886, 0 }, { 13, -4304, -1891, 0 }, { -19, 4304, 1886, 0 }, { 13, 4304, -1891, 0 } }, { -4099, 0, -36, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 11, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1095, -64, 543, 0 }, { { 2166, -4304, 328, 0 }, { -2168, -4304, -330, 0 }, { 2166, 4304, 328, 0 }, { -2168, 4304, -330, 0 } }, { -619, 0, 4072, 0 }, { 0, 0, ONE, 0 }, 4830, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1077, 0, 734, 0 }, { { -2173, -4304, -334, 0 }, { 2163, -4304, 324, 0 }, { -2173, 4304, -334, 0 }, { 2163, 4304, 324, 0 } }, { 614, 0, -4050, 0 }, { 0, 0, ONE, 0 }, 4830, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9793, -6208, 6912, 0 }, { { -1855, -4304, 352, 0 }, { 1855, -4304, -352, 0 }, { -1855, 4304, 352, 0 }, { 1855, 4304, -352, 0 } }, { -765, 0, -4027, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 12, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9825, -6240, 6816, 0 }, { { 1855, -4304, -352, 0 }, { -1855, -4304, 352, 0 }, { 1855, 4304, -352, 0 }, { -1855, 4304, 352, 0 } }, { 763, 0, 4025, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 11, 12, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger gShelterB4UpperSewerActionTriggers[8] = {
    { NULL, NULL, NULL, { 3744, -2096, 9024, 0 }, { { -448, 0, -736, 0 }, { 448, 0, -736, 0 }, { -448, 0, 736, 0 }, { 448, 0, 736, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 861, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_WATER_SUPPLY, (3 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3712, -48, -3008, 0 }, { { -448, 0, -464, 0 }, { 448, 0, -464, 0 }, { -448, 0, 464, 0 }, { 448, 0, 464, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 643, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_RESERVOIR, (2 << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9408, -48, 6496, 0 }, { { -448, 0, -1008, 0 }, { 448, 0, -1008, 0 }, { -448, 0, 1008, 0 }, { 448, 0, 1008, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_LOWER_SEWER, (1 << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9024, -2080, 5632, 0 }, { { 1008, 0, -448, 0 }, { 1008, 0, 448, 0 }, { -1008, 0, -448, 0 }, { -1008, 0, 448, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_LOWER_SEWER, (4 << 4) | 4, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3344, -2080, -384, 0 }, { { -528, 0, -1008, 0 }, { 528, 0, -1008, 0 }, { -528, 0, 1008, 0 }, { 528, 0, 1008, 0 } }, { 0, 4119, 0, 0 }, { -4096, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 208, -64, -5663, 0 }, { { -944, 0, -1008, 0 }, { 944, 0, -1008, 0 }, { -944, 0, 1008, 0 }, { 944, 0, 1008, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1378, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9568, -2080, 6400, 0 }, { { -448, 0, -656, 0 }, { 448, 0, -656, 0 }, { -448, 0, 656, 0 }, { 448, 0, 656, 0 } }, { 0, 4106, 0, 0 }, { 4096, 0, 0, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1568, -64, 3696, 0 }, { { -448, 0, -864, 0 }, { 448, 0, -864, 0 }, { -448, 0, 864, 0 }, { 448, 0, 864, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 972, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Resource-entry keys joining the upper sewer's placement and actor recipes.
enum {
    SHELTER_B4_UPPER_SEWER_RESOURCE_STINGER        = 3,
    SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER      = 4,
    SHELTER_B4_UPPER_SEWER_RESOURCE_ROOK_GOLEM     = 23,
    SHELTER_B4_UPPER_SEWER_RESOURCE_AMOEBA         = 24,
    SHELTER_B4_UPPER_SEWER_RESOURCE_LARGE_MOSSBACK = 49,
    SHELTER_B4_UPPER_SEWER_RESOURCE_SLOUCH         = 70,
    SHELTER_B4_UPPER_SEWER_RESOURCE_SUCKLERCEPH    = 71,
};

/// Slouch and Sucklerceph resource recipes for upper-sewer area variant 1.
///
/// Entry IDs join this variant's placement and resource lists. The list ends
/// at `AREA_PLACEMENT_END`; room storage and borrowed actor task descriptors
/// must remain loaded through loading, spawning and actor teardown.
/// ID 71 requests absent file 107100 without starting a disk read. Its
/// Sucklerceph recipe is supplied by actor_107000, loaded through ID 70.
static AreaResource _gShelterB4UpperSewerVariant1Resources[3] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_SLOUCH, 70, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor107000SlouchTaskDescs },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_SUCKLERCEPH, 71, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor107000SucklercephTaskDescs },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Bog Diver resource recipes for upper-sewer area variant 2.
///
/// Entry IDs join this variant's placement and resource lists. The list ends
/// at `AREA_PLACEMENT_END`; room storage and borrowed actor task descriptors
/// must remain loaded through loading, spawning and actor teardown.
static AreaResource _gShelterB4UpperSewerVariant2Resources[2] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Amoeba and slot-2 Bog Diver resource recipes for upper-sewer area variant 3.
///
/// Entry IDs join this variant's placement and resource lists. The list ends
/// at `AREA_PLACEMENT_END`; room storage and borrowed actor task descriptors
/// must remain loaded through loading, spawning and actor teardown.
static AreaResource _gShelterB4UpperSewerVariant3Resources[3] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_AMOEBA, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102400Tasks },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 4, AREA_RESOURCE_FILE_GROUP_BASE_20, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor200400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 Bog Diver and large Mossback resource recipes for upper-sewer area variant 4.
///
/// Entry IDs join this variant's placement and resource lists. The list ends
/// at `AREA_PLACEMENT_END`; room storage and borrowed actor task descriptors
/// must remain loaded through loading, spawning and actor teardown.
static AreaResource _gShelterB4UpperSewerVariant4Resources[3] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_LARGE_MOSSBACK, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor01100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Rook GOLEM resource recipes for upper-sewer area variant 7.
///
/// Entry IDs join this variant's placement and resource lists. The list ends
/// at `AREA_PLACEMENT_END`; room storage and borrowed actor task descriptors
/// must remain loaded through loading, spawning and actor teardown.
static AreaResource _gShelterB4UpperSewerVariant7Resources[2] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_ROOK_GOLEM, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102300RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Rook GOLEM resource recipes for upper-sewer area variant 8.
///
/// Entry IDs join this variant's placement and resource lists. The list ends
/// at `AREA_PLACEMENT_END`; room storage and borrowed actor task descriptors
/// must remain loaded through loading, spawning and actor teardown.
static AreaResource _gShelterB4UpperSewerVariant8Resources[2] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_ROOK_GOLEM, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102300RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Stinger and slot-2 Bog Diver resource recipes for upper-sewer area variant 11.
///
/// Entry IDs join this variant's placement and resource lists. The list ends
/// at `AREA_PLACEMENT_END`; room storage and borrowed actor task descriptors
/// must remain loaded through loading, spawning and actor teardown.
static AreaResource _gShelterB4UpperSewerVariant11Resources[3] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_STINGER, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor100300Tasks },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 4, AREA_RESOURCE_FILE_GROUP_BASE_20, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor200400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Placement modes and Diver shot-speed rows used by the upper sewer.
///
/// Diver mode bit 4 selects waypoint ring 1; its low nibble selects swimming.
/// Shot-speed rows select local-forward distances in world units per tick.
enum {
    SHELTER_B4_UPPER_SEWER_SUCKLERCEPH_TEXTURE_VARIANT = 1,
    SHELTER_B4_UPPER_SEWER_AMOEBA_VARIANT_1            = 1,
    SHELTER_B4_UPPER_SEWER_DIVER_SWIM                  = 1,
    SHELTER_B4_UPPER_SEWER_DIVER_WAYPOINT_RING_1       = 1 << 4,
    SHELTER_B4_UPPER_SEWER_DIVER_SHOT_SPEED_210        = 2,
    SHELTER_B4_UPPER_SEWER_DIVER_SHOT_SPEED_230        = 4,
    SHELTER_B4_UPPER_SEWER_DIVER_SHOT_SPEED_270        = 6,
    SHELTER_B4_UPPER_SEWER_ROOK_PATROL                 = 1,
};

/// Two Slouch and two Sucklerceph placements for upper-sewer area variant 1.
///
/// Ends at `AREA_PLACEMENT_END`. Positions use world units and yaw uses
/// 4096 units per turn; texture offsets count VRAM pages and CLUT rows.
/// Actors may retain these room-owned records until teardown.
/// The Sucklerceph mode adds one texture page and one CLUT row.
static AreaPlacement _gShelterB4UpperSewerVariant1Placements[5] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_SLOUCH, 0, 0, -1600, 0, 6900, 3200, 0, 0, 2, 0 },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_SLOUCH, 0, 0, 300, 0, 500, 4000, 0, 0, 2, 0 },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_SUCKLERCEPH, 0, SHELTER_B4_UPPER_SEWER_SUCKLERCEPH_TEXTURE_VARIANT, 1350, 0, 4000, 3300, 0, 0, 2, 0 },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_SUCKLERCEPH, 0, SHELTER_B4_UPPER_SEWER_SUCKLERCEPH_TEXTURE_VARIANT, 1350, 0, 7300, 2550, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// One Bog Diver placement for upper-sewer area variant 2.
///
/// Ends at `AREA_PLACEMENT_END`. Positions use world units and yaw uses
/// 4096 units per turn; texture offsets count VRAM pages and CLUT rows.
/// Actors may retain these room-owned records until teardown.
/// The Diver swims waypoint ring 0 and selects shot-speed row 4 (230 units/tick).
static AreaPlacement _gShelterB4UpperSewerVariant2Placements[2] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 0, SHELTER_B4_UPPER_SEWER_DIVER_SWIM, 200, 1000, -3400, 0, 0, 2, 4, SHELTER_B4_UPPER_SEWER_DIVER_SHOT_SPEED_230 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two Amoeba and two Bog Diver placements for upper-sewer area variant 3.
///
/// Ends at `AREA_PLACEMENT_END`. Positions use world units and yaw uses
/// 4096 units per turn; texture offsets count VRAM pages and CLUT rows.
/// Actors may retain these room-owned records until teardown.
/// Divers swim waypoint rings 0 and 1 with shot-speed row 2 (210 units/tick).
static AreaPlacement _gShelterB4UpperSewerVariant3Placements[5] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_AMOEBA, 0, SHELTER_B4_UPPER_SEWER_AMOEBA_VARIANT_1, -4350, -2000, 9300, 2650, 0, 0, 2, 0 },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_AMOEBA, 0, SHELTER_B4_UPPER_SEWER_AMOEBA_VARIANT_1, 600, -2000, 8350, 3400, 0, 0, 2, 0 },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 0, SHELTER_B4_UPPER_SEWER_DIVER_SWIM, 200, 1000, -3400, 0, 0, 2, 4, SHELTER_B4_UPPER_SEWER_DIVER_SHOT_SPEED_210 },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 0, SHELTER_B4_UPPER_SEWER_DIVER_SWIM | SHELTER_B4_UPPER_SEWER_DIVER_WAYPOINT_RING_1, 160, 1000, 6300, 3072, 0, 2, 4, SHELTER_B4_UPPER_SEWER_DIVER_SHOT_SPEED_210 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// One large Mossback and two Bog Diver placements for upper-sewer area variant 4.
///
/// Ends at `AREA_PLACEMENT_END`. Positions use world units and yaw uses
/// 4096 units per turn; texture offsets count VRAM pages and CLUT rows.
/// Actors may retain these room-owned records until teardown.
/// ID 49 selects the large Mossback; Divers swim rings 0 and 1 at shot-speed row 2.
static AreaPlacement _gShelterB4UpperSewerVariant4Placements[4] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_LARGE_MOSSBACK, 1, 0, -3400, -2000, 9200, 2250, 0, 0, 2, 0 },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 0, SHELTER_B4_UPPER_SEWER_DIVER_SWIM, 200, 1000, -3400, 0, 0, 2, 4, SHELTER_B4_UPPER_SEWER_DIVER_SHOT_SPEED_210 },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 0, SHELTER_B4_UPPER_SEWER_DIVER_SWIM | SHELTER_B4_UPPER_SEWER_DIVER_WAYPOINT_RING_1, 160, 1000, 6300, 3072, 0, 2, 4, SHELTER_B4_UPPER_SEWER_DIVER_SHOT_SPEED_210 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// One Rook GOLEM placement for upper-sewer area variant 7.
///
/// Ends at `AREA_PLACEMENT_END`. Positions use world units and yaw uses
/// 4096 units per turn; texture offsets count VRAM pages and CLUT rows.
/// Actors may retain these room-owned records until teardown.
/// The Rook patrol mode uses variant 3 for a 3000-world-unit patrol leg.
static AreaPlacement _gShelterB4UpperSewerVariant7Placements[2] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_ROOK_GOLEM, 3, SHELTER_B4_UPPER_SEWER_ROOK_PATROL, -3000, 0, 6700, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// One Rook GOLEM placement for upper-sewer area variant 8.
///
/// Ends at `AREA_PLACEMENT_END`. Positions use world units and yaw uses
/// 4096 units per turn; texture offsets count VRAM pages and CLUT rows.
/// Actors may retain these room-owned records until teardown.
/// The Rook patrol mode uses variant 5 for a 5000-world-unit patrol leg.
static AreaPlacement _gShelterB4UpperSewerVariant8Placements[2] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_ROOK_GOLEM, 5, SHELTER_B4_UPPER_SEWER_ROOK_PATROL, -3000, -2000, 9000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// One Stinger and one Bog Diver placement for upper-sewer area variant 11.
///
/// Ends at `AREA_PLACEMENT_END`. Positions use world units and yaw uses
/// 4096 units per turn; texture offsets count VRAM pages and CLUT rows.
/// Actors may retain these room-owned records until teardown.
/// The Stinger selects patrol route 0; the Diver swims ring 0 with speed row 6 (270).
static AreaPlacement _gShelterB4UpperSewerVariant11Placements[3] = {
    { SHELTER_B4_UPPER_SEWER_RESOURCE_STINGER, 0, 0, 1000, -2000, 9000, 3072, 0, 0, 2, 0 },
    { SHELTER_B4_UPPER_SEWER_RESOURCE_BOG_DIVER, 0, SHELTER_B4_UPPER_SEWER_DIVER_SWIM, 200, 1000, -3400, 0, 0, 2, 4, SHELTER_B4_UPPER_SEWER_DIVER_SHOT_SPEED_270 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB4UpperSewerAreaVariants[12] = {
    [1]  = { _gShelterB4UpperSewerVariant1Placements, _gShelterB4UpperSewerVariant1Resources },
    [2]  = { _gShelterB4UpperSewerVariant2Placements, _gShelterB4UpperSewerVariant2Resources },
    [3]  = { _gShelterB4UpperSewerVariant3Placements, _gShelterB4UpperSewerVariant3Resources },
    [4]  = { _gShelterB4UpperSewerVariant4Placements, _gShelterB4UpperSewerVariant4Resources },
    [7]  = { _gShelterB4UpperSewerVariant7Placements, _gShelterB4UpperSewerVariant7Resources },
    [8]  = { _gShelterB4UpperSewerVariant8Placements, _gShelterB4UpperSewerVariant8Resources },
    [11] = { _gShelterB4UpperSewerVariant11Placements, _gShelterB4UpperSewerVariant11Resources },
};

WorldCollisionOccluder gShelterB4UpperSewerOccluders[3] = {
    { NULL, NULL, { -4928, -3088, 208, 0 }, { { -3680, 4656, -4624, 0 }, { 3680, 4656, 4624, 0 }, { -3680, -4656, -4624, 0 }, { 3680, -4656, 4624, 0 } }, { -3206, 0, 2551, 0 }, 7507, 1, 0 },
    { NULL, NULL, { 4464, -176, 2784, 0 }, { { -3216, 1920, -4624, 0 }, { 3216, 1920, 4624, 0 }, { -3216, -1920, -4624, 0 }, { 3216, -1920, 4624, 0 } }, { -3368, 0, 2342, 0 }, 5948, 1, 0 },
    { NULL, NULL, { -9041, -1824, 6399, 0 }, { { -1056, 8, -992, 0 }, { -1056, -7, 993, 0 }, { 1056, 8, -992, 0 }, { 1056, -7, 993, 0 } }, { 0, -4096, -32, 0 }, 1448, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Cue-2 bases in the loaded type-1 sound bank; cue 1 uses the following entry.
enum {
    SHELTER_B4_UPPER_SEWER_CLASS5_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
    SHELTER_B4_UPPER_SEWER_CLASS5_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x43,
    SHELTER_B4_UPPER_SEWER_CLASS2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    SHELTER_B4_UPPER_SEWER_CLASS2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
    SHELTER_B4_UPPER_SEWER_CLASS3_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    SHELTER_B4_UPPER_SEWER_CLASS3_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
};

/// Class-5 footstep cue pairs; scripted jumps reuse the walking pair.
///
/// These type-1 requests borrow the loaded sound bank. Cue 1 adds one to each
/// base; companion playback adds 100. The room owns the record until unloading.
static WorldCollisionFootstepSounds _gShelterB4UpperSewerClass5FootstepSounds = {
    .walk         = SHELTER_B4_UPPER_SEWER_CLASS5_WALK_CUE_BASE,
    .run          = SHELTER_B4_UPPER_SEWER_CLASS5_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_UPPER_SEWER_CLASS5_WALK_CUE_BASE,
};

/// Class-2 footstep cue pairs; scripted jumps reuse the walking pair.
///
/// These type-1 requests borrow the loaded sound bank. Cue 1 adds one to each
/// base; companion playback adds 100. The room owns the record until unloading.
static WorldCollisionFootstepSounds _gShelterB4UpperSewerClass2FootstepSounds = {
    .walk         = SHELTER_B4_UPPER_SEWER_CLASS2_WALK_CUE_BASE,
    .run          = SHELTER_B4_UPPER_SEWER_CLASS2_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_UPPER_SEWER_CLASS2_WALK_CUE_BASE,
};

/// Class-3 footstep cue pairs; scripted jumps reuse the walking pair.
///
/// These type-1 requests borrow the loaded sound bank. Cue 1 adds one to each
/// base; companion playback adds 100. The room owns the record until unloading.
static WorldCollisionFootstepSounds _gShelterB4UpperSewerClass3FootstepSounds = {
    .walk         = SHELTER_B4_UPPER_SEWER_CLASS3_WALK_CUE_BASE,
    .run          = SHELTER_B4_UPPER_SEWER_CLASS3_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_UPPER_SEWER_CLASS3_WALK_CUE_BASE,
};

/// Silent blocking policy shared by surface classes 0, 4, 6 and 7.
///
/// Allows weapon impacts and pushback. The first byte's role is unproven.
static WorldCollisionSurfaceProperties _gShelterB4UpperSewerBlockingSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's silent policy: pass probes, ignore weapon impacts and apply pushback.
///
/// The first byte's role is unproven.
static WorldCollisionSurfaceProperties _gShelterB4UpperSewerProbePassThroughSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 2's blocking policy, with footstep cues, weapon impacts and pushback.
///
/// The first byte's role is unproven.
static WorldCollisionSurfaceProperties _gShelterB4UpperSewerClass2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4UpperSewerClass2FootstepSounds,
};

/// Class 3's blocking policy, with footstep cues, weapon impacts and pushback.
///
/// The first byte's role is unproven.
static WorldCollisionSurfaceProperties _gShelterB4UpperSewerClass3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4UpperSewerClass3FootstepSounds,
};

/// Class 5's blocking policy, with footstep cues, weapon impacts and pushback.
///
/// The first byte's role is unproven.
static WorldCollisionSurfaceProperties _gShelterB4UpperSewerClass5SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4UpperSewerClass5FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB4UpperSewerSurfacePropertiesByClass[8] = {
    &_gShelterB4UpperSewerBlockingSilentSurface,
    &_gShelterB4UpperSewerProbePassThroughSurface,
    &_gShelterB4UpperSewerClass2SurfaceProperties,
    &_gShelterB4UpperSewerClass3SurfaceProperties,
    &_gShelterB4UpperSewerBlockingSilentSurface,
    &_gShelterB4UpperSewerClass5SurfaceProperties,
    &_gShelterB4UpperSewerBlockingSilentSurface,
    &_gShelterB4UpperSewerBlockingSilentSurface,
};

/// Persistent subtractive screen-fade storage for an accepted exit transit.
///
/// The fade task borrows `fade` for its 16-frame ramp before room reload.
/// The trailing word stays zero and has no established consumer.
static RoomFadeStorage _gShelterB4UpperSewerExitFade = { 0 };

/// Resolved destination selectors held while exit confirmation and transit run.
///
/// Only `loc.roomState`, `loc.area` and `loc.warp` are staged and committed;
/// the remaining five bytes stay zero and are not consumed. A new exit replaces
/// the selectors. Retains the eight-byte allocation and its two-byte alignment.
static GameLocationKey _gShelterB4UpperSewerExitDestination __attribute__((aligned(2))) = { 0 };

/// Interrupted view selector restored after the water-hole scene.
///
/// Scene entry replaces `view`; cancellation and script completion restore it.
/// The three following nonzero image bytes have no recovered access or role.
static struct {
    u8 view;         // Saved 1-based room view before the scene selects view 13
    u8 unknown_1[3]; // Retained image bytes; role and grouping unproven
} _gShelterB4UpperSewerSavedViewStorage = { 0, { 162, 51, 44 } };

/// Next free byte in the current frame's borrowed water GPU packet arena.
///
/// Reset each draw to the unused actor buffer's display half (0xC000 bytes).
/// Each emitted quad advances through a 24-byte `POLY_F4` and a 12-byte
/// `DR_MODE`; at most 144 pairs use 5184 bytes. These mixed packet views need
/// word-aligned storage and remain borrowed until that frame's GPU work ends.
static u8* _gShelterB4UpperSewerWaterPacketCursor;

static void _shelterB4UpperSewerUpdateWater(Task* task);

/// Runs the water-hole opening choice and its route-unlocking event script.
///
/// Starts in state 0 after both actors have been hidden and held and view 13
/// selected. CAP key 12 cancels, restoring the saved view, actors and HUD.
/// Another key starts the room's event script, opens the Shelter route and
/// clears the water map marker. Completion waits for the script to clear
/// `GameSession::eventState`. Requires the room's CAP and event resources live.
static void _shelterB4UpperSewerOpenWaterHoleTask(Task* task)
{
    enum {
        SHELTER_B4_UPPER_SEWER_WATER_HOLE_SHOW_SPRITE,
        SHELTER_B4_UPPER_SEWER_WATER_HOLE_START_CAP,
        SHELTER_B4_UPPER_SEWER_WATER_HOLE_WAIT_CAP,
        SHELTER_B4_UPPER_SEWER_WATER_HOLE_CHOOSE,
        SHELTER_B4_UPPER_SEWER_WATER_HOLE_WAIT_EVENT,
        SHELTER_B4_UPPER_SEWER_WATER_HOLE_CAP_COMMAND    = 1,
        SHELTER_B4_UPPER_SEWER_WATER_HOLE_CANCEL_KEY     = 12,
        SHELTER_B4_UPPER_SEWER_CAP_KEEP_ACTORS_PAUSED    = 1,
        SHELTER_B4_UPPER_SEWER_CAP_CHOICE_ROW_STRIDE     = 5,
        SHELTER_B4_UPPER_SEWER_INTERACTION_DELAY_UPDATES = 10,
    };

    switch (task->state) {
        case SHELTER_B4_UPPER_SEWER_WATER_HOLE_SHOW_SPRITE:
            _shelterB4UpperSewerSetView13SpriteHidden(0);
            gGameSession->eventState = 1;
            task->state++;
            break;
        case SHELTER_B4_UPPER_SEWER_WATER_HOLE_START_CAP:
            gGameSession->hideHud = 1;
            capRunCommand(SHELTER_B4_UPPER_SEWER_WATER_HOLE_CAP_COMMAND, CAP_PLAYBACK_IN_PLACE);
            gCapPreserveActorControl = SHELTER_B4_UPPER_SEWER_CAP_KEEP_ACTORS_PAUSED;
            gCapChoiceRowStride      = SHELTER_B4_UPPER_SEWER_CAP_CHOICE_ROW_STRIDE;
            task->state++;
            break;
        case SHELTER_B4_UPPER_SEWER_WATER_HOLE_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B4_UPPER_SEWER_WATER_HOLE_CHOOSE:
            if (capGetVariantKey() == SHELTER_B4_UPPER_SEWER_WATER_HOLE_CANCEL_KEY) {
                taskKill(task);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = _gShelterB4UpperSewerSavedViewStorage.view;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
                companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
                gGameSession->eventState          = 0;
                gGameSession->hideHud             = 0;
                gSceneCombatState.actorControl    = SCENE_COMBAT_ACTORS_RUNNING;
                gDirectionInteractionRearmUpdates = SHELTER_B4_UPPER_SEWER_INTERACTION_DELAY_UPDATES;
                break;
            }
            // The event script restores the view and releases the held actors.
            evsStartScript(_gShelterB4UpperSewerWaterHoleScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_WATER, 0);
            task->state++;
            break;
        case SHELTER_B4_UPPER_SEWER_WATER_HOLE_WAIT_EVENT:
            if (gGameSession->eventState == 0) {
                gDirectionInteractionRearmUpdates = SHELTER_B4_UPPER_SEWER_INTERACTION_DELAY_UPDATES;
                gSceneCombatState.actorControl    = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
            }
            break;
    }
}

/// Runs an exit CAP choice, a 30-tick fade, transit sound and saved-destination reload.
///
/// Starts in state 0; `spawnArg1.value` selects the loaded CAP command (the
/// room resolver supplies 7 for the reservoir or 8 for water supply). Key 10
/// commits the exit; another key resumes the player and actors. The room-owned
/// fade record and stored destination must remain live until reload. Pauses
/// actors throughout the accepted exit and requests battle escape before fading.
static void _shelterB4UpperSewerExitTransitTask(Task* task)
{
    enum {
        SHELTER_B4_UPPER_SEWER_EXIT_START_CAP,
        SHELTER_B4_UPPER_SEWER_EXIT_WAIT_CAP,
        SHELTER_B4_UPPER_SEWER_EXIT_CHOOSE,
        SHELTER_B4_UPPER_SEWER_EXIT_WAIT_FADE,
        SHELTER_B4_UPPER_SEWER_EXIT_WAIT_SOUND,
        SHELTER_B4_UPPER_SEWER_EXIT_RELOAD,
        SHELTER_B4_UPPER_SEWER_EXIT_ACCEPT_KEY           = 10,
        SHELTER_B4_UPPER_SEWER_EXIT_FADE_TICKS           = 30,
        SHELTER_B4_UPPER_SEWER_INTERACTION_DELAY_UPDATES = 10,
        SHELTER_B4_UPPER_SEWER_SCREEN_FADE_TASK_BANK     = 1,
        SHELTER_B4_UPPER_SEWER_SCREEN_FADE_TASK_TYPE     = 0x31,
        SHELTER_B4_UPPER_SEWER_DEFAULT_SPRITE_VARIANT    = 1,
    };

    switch (task->state) {
        case SHELTER_B4_UPPER_SEWER_EXIT_START_CAP:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case SHELTER_B4_UPPER_SEWER_EXIT_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B4_UPPER_SEWER_EXIT_CHOOSE:
            if (capGetVariantKey() != SHELTER_B4_UPPER_SEWER_EXIT_ACCEPT_KEY) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl    = SCENE_COMBAT_ACTORS_RUNNING;
                gDirectionInteractionRearmUpdates = SHELTER_B4_UPPER_SEWER_INTERACTION_DELAY_UPDATES;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            // Freeze the battle result before fading the outgoing room.
            sceneQueueBattleEscapeResult();
            _gShelterB4UpperSewerExitFade.fade.blend      = SCREEN_FADE_SUBTRACT;
            _gShelterB4UpperSewerExitFade.fade.phase      = SCREEN_FADE_RUNNING;
            _gShelterB4UpperSewerExitFade.fade.rampFrames = SHELTER_B4_UPPER_SEWER_EXIT_FADE_TICKS;
            taskSpawn(SHELTER_B4_UPPER_SEWER_SCREEN_FADE_TASK_BANK, SHELTER_B4_UPPER_SEWER_SCREEN_FADE_TASK_TYPE, 0, &_gShelterB4UpperSewerExitFade.fade);
            task->killCountdown = SHELTER_B4_UPPER_SEWER_EXIT_FADE_TICKS;
            task->state++;
            break;
        case SHELTER_B4_UPPER_SEWER_EXIT_WAIT_FADE:
            if (--task->killCountdown == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B4_UPPER_SEWER_EXIT_TRANSIT, 0, 0);
                task->state++;
            }
            break;
        case SHELTER_B4_UPPER_SEWER_EXIT_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_SHELTER_B4_UPPER_SEWER_EXIT_TRANSIT) == 0) {
                task->state++;
            }
            break;
        case SHELTER_B4_UPPER_SEWER_EXIT_RELOAD:
            // Publish the staged destination after the fade and transit sound finish.
            gDisplayState.spriteVariant                                 = SHELTER_B4_UPPER_SEWER_DEFAULT_SPRITE_VARIANT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = _gShelterB4UpperSewerExitDestination.area;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = _gShelterB4UpperSewerExitDestination.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = _gShelterB4UpperSewerExitDestination.roomState;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
            taskKill(task);
            break;
    }
}

/// Refuses key-item use in this room without consuming the item.
///
/// All arguments are ignored. The zero result selects the item menu's refusal notice.
static s32 _shelterB4UpperSewerRejectKeyItemMessage(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Saves a resolved exit selector and starts the room's confirmation/transit task.
///
/// Borrows the reply for this call. Area and room narrow to their unsigned
/// saved-location bytes; warp stays unsigned.
/// The loaded room owns that selector until transit consumes it; another start
/// overwrites it. `capCommand` selects the exit confirmation script.
static inline void _shelterB4UpperSewerStageExit(const RoomEventMsg* reply, s32 capCommand)
{
    enum { SHELTER_B4_UPPER_SEWER_EXIT_TASK_INDEX = 1 };

    _gShelterB4UpperSewerExitDestination.area      = (u8)reply->areaId;
    _gShelterB4UpperSewerExitDestination.warp      = reply->warp;
    _gShelterB4UpperSewerExitDestination.roomState = reply->roomState;
    taskSpawnFromTable(_gShelterB4UpperSewerInteractionTasks, SHELTER_B4_UPPER_SEWER_EXIT_TASK_INDEX, capCommand, 0);
}

/// Resolves departures and handles the reservoir and water-supply exits locally.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`, borrowing complete eight-byte records
/// which may alias. Returns 0 for these two exits and 1 for ordinary passage.
/// Queries only resolve the reply; execute requests save its destination and
/// start confirmation CAP 7 or 8. The receiver and message ID are unused.
static s32 _shelterB4UpperSewerResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B4_UPPER_SEWER_RESERVOIR_EXIT_CAP_COMMAND    = 7,
           SHELTER_B4_UPPER_SEWER_WATER_SUPPLY_EXIT_CAP_COMMAND = 8,
           SHELTER_B4_UPPER_SEWER_TRANSITION_HANDLED            = 0,
           SHELTER_B4_UPPER_SEWER_TRANSITION_ALLOWED            = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B4_RESERVOIR) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            _shelterB4UpperSewerStageExit(reply, SHELTER_B4_UPPER_SEWER_RESERVOIR_EXIT_CAP_COMMAND);
        }
        return SHELTER_B4_UPPER_SEWER_TRANSITION_HANDLED;
    }
    if (request->areaId == GAME_AREA_SHELTER_B4_WATER_SUPPLY) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            _shelterB4UpperSewerStageExit(reply, SHELTER_B4_UPPER_SEWER_WATER_SUPPLY_EXIT_CAP_COMMAND);
        }
        return SHELTER_B4_UPPER_SEWER_TRANSITION_HANDLED;
    }
    return SHELTER_B4_UPPER_SEWER_TRANSITION_ALLOWED;
}

/// Starts the unopened water-hole route scene or replays its open-route CAP.
///
/// Handles `ROOM_MESSAGE_COMMAND` command 1. An unopened route hides and holds
/// player, companion and scene actors, saves the live view byte, selects view
/// 13 and starts the opening task. An open route plays CAP 6 instead. Other
/// commands have no effect. All calls return 0; the receiver and other words
/// are unused. The room owns the saved view until the scene restores it.
static s32 _shelterB4UpperSewerHandleWaterHoleCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum { SHELTER_B4_UPPER_SEWER_WATER_HOLE_COMMAND          = 1,
           SHELTER_B4_UPPER_SEWER_WATER_HOLE_SCENE_VIEW       = 13,
           SHELTER_B4_UPPER_SEWER_WATER_HOLE_OPEN_TASK_INDEX  = 0,
           SHELTER_B4_UPPER_SEWER_WATER_HOLE_OPEN_CAP_COMMAND = 6,
           SHELTER_B4_UPPER_SEWER_COMMAND_REPLY               = 0 };
    u8 savedView;

    if (commandId == SHELTER_B4_UPPER_SEWER_WATER_HOLE_COMMAND) {
        if (gameFlagGetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN) == 0) {
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
            // Preserve the interrupted camera view before the opening scene takes it.
            savedView                                              = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = SHELTER_B4_UPPER_SEWER_WATER_HOLE_SCENE_VIEW;
            _gShelterB4UpperSewerSavedViewStorage.view             = savedView;
            taskSpawnFromTable(_gShelterB4UpperSewerInteractionTasks, SHELTER_B4_UPPER_SEWER_WATER_HOLE_OPEN_TASK_INDEX, 0, 0);
        } else {
            capRunCommandWithTransition(SHELTER_B4_UPPER_SEWER_WATER_HOLE_OPEN_CAP_COMMAND);
        }
    }
    return SHELTER_B4_UPPER_SEWER_COMMAND_REPLY;
}

/// Ignores the room action supplied by a direction trigger and returns zero.
///
/// All arguments are unused; the borrowed request is neither read nor retained.
static s32 _shelterB4UpperSewerIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Hides view 13's switchable sprite and starts the room sound for cue 4.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cue IDs have no effect. Requires this
/// room's loaded sprite directory and sound bank. The receiver, message ID and
/// second payload are unused, and every cue returns zero.
static s32 _shelterB4UpperSewerHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg)
{
    enum {
        SHELTER_B4_UPPER_SEWER_SOUND_CUE_HIDE_SPRITE = 4,
        SHELTER_B4_UPPER_SEWER_HIDE_SPRITE_SOUND     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 4)
    };

    if (cueId == SHELTER_B4_UPPER_SEWER_SOUND_CUE_HIDE_SPRITE) {
        _shelterB4UpperSewerSetView13SpriteHidden(1);
        sndEvtRequestScriptStart(SHELTER_B4_UPPER_SEWER_HIDE_SPRITE_SOUND, 0, 0);
    }
    return 0;
}

/// Restores the live save's view after the water-hole access scene's temporary view.
///
/// Requires the outgoing view captured before that scene starts. This event-script
/// callback changes only the saved view selector; it does not load a camera.
static void _shelterB4UpperSewerRestoreSavedView(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = _gShelterB4UpperSewerSavedViewStorage.view;
}

/// Publishes the room task and restores water rendering after the reservoir event.
///
/// Runs in state 0 and advances to idle state 1. A completed reservoir event
/// seeds water Y at -1800 world units and starts the room's water task; that task
/// publishes the seed before clamping later drawing ticks to -1600..0. Otherwise
/// stores Y 0 without starting water rendering. The room's message table and
/// water resources must stay loaded for their tasks' lifetimes.
static void _shelterB4UpperSewerInitializeRoom(Task* task)
{
    enum { WATER_INITIAL_Y = -1800,
           WATER_ABSENT_Y  = 0 };

    task->msgTable = _gShelterB4UpperSewerRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
        gShelterB4UpperSewerWaterY = WATER_INITIAL_Y;
        taskSpawnFromTable(_gShelterB4UpperSewerWaterTasks, 0, 0, 0);
    } else {
        gShelterB4UpperSewerWaterY = WATER_ABSENT_Y;
    }
    task->state++;
}

/// Keeps the initialized room task available for messages without per-frame work.
///
/// This is room state 1; the task is unused and its state is left intact.
static void _shelterB4UpperSewerIdleRoom(Task* task)
{
}

void shelterB4UpperSewerRoomTask(Task* task)
{
    TaskFuncTable3 handlers = _gShelterB4UpperSewerRoomStates;

    handlers.funcs[task->state](task);
}

/// Clamps and publishes the water height, then draws the current view's flat water.
///
/// Height is in world units, limited to -1600..0. Blue intensity is
/// (-height * 16) / 225. The current display half of an unused actor-load
/// buffer supplies packet storage; view 12 omits the first water rectangle.
static void _shelterB4UpperSewerUpdateWater(Task* task)
{
    enum {
        SHELTER_B4_UPPER_SEWER_WATER_PACKET_HALF_BYTES     = 0xC000,
        SHELTER_B4_UPPER_SEWER_WATER_MIN_Y                 = -1600,
        SHELTER_B4_UPPER_SEWER_WATER_MAX_Y                 = 0,
        SHELTER_B4_UPPER_SEWER_WATER_VIEW_SKIP_FIRST_PATCH = 12
    };
    s16 clampedWaterY;
    u8  blueIntensity;
    s32 waterY;

    // Borrow the unused actor-load buffer, with one packet area per display buffer.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        _gShelterB4UpperSewerWaterPacketCursor = gFsActorBuffer2Base + gDisplayState.otBuffer * SHELTER_B4_UPPER_SEWER_WATER_PACKET_HALF_BYTES;
    } else {
        _gShelterB4UpperSewerWaterPacketCursor = gFsActorBuffer1Base + gDisplayState.otBuffer * SHELTER_B4_UPPER_SEWER_WATER_PACKET_HALF_BYTES;
    }
    if (gShelterB4UpperSewerWaterY < SHELTER_B4_UPPER_SEWER_WATER_MIN_Y) {
        gShelterB4UpperSewerWaterY = SHELTER_B4_UPPER_SEWER_WATER_MIN_Y;
    } else if (gShelterB4UpperSewerWaterY > SHELTER_B4_UPPER_SEWER_WATER_MAX_Y) {
        gShelterB4UpperSewerWaterY = SHELTER_B4_UPPER_SEWER_WATER_MAX_Y;
    }
    clampedWaterY        = gShelterB4UpperSewerWaterY;
    gGameSession->waterY = clampedWaterY;
    waterY               = clampedWaterY;
    blueIntensity        = (-waterY * 16) / 225;
    if (gGameSession->location.view != SHELTER_B4_UPPER_SEWER_WATER_VIEW_SKIP_FIRST_PATCH) {
        _shelterB4UpperSewerDrawWaterSurfaces(task, _gShelterB4UpperSewerWaterSurfaces, waterY, blueIntensity);
    } else {
        _shelterB4UpperSewerDrawWaterSurfaces(task, &_gShelterB4UpperSewerWaterSurfaces[1], waterY, blueIntensity);
    }
}

/// Draws a terminated list of flat water rectangles at a shared world-unit height.
///
/// Borrows the list through `WATER_SURFACE_LIST_END`; every drawable entry
/// needs a positive segment count. Each valid projected strip quad has RGB
/// (0, blueIntensity / 4, blueIntensity), subtractive blending and an ordering
/// depth bias of 96. The byte cursor must have aligned storage for one POLY_F4
/// and one DR_MODE per drawn quad (36 bytes, sending only its first command).
/// Scratch storage is released before returning. `task` is unused, retained in the matched call interface.
static void _shelterB4UpperSewerDrawWaterSurfaces(Task* task, const _ShelterB4UpperSewerWaterSurface* surface, s16 waterY, u8 blueIntensity)
{
    enum {
        SHELTER_B4_UPPER_SEWER_WATER_DEPTH_BIAS      = 96,
        SHELTER_B4_UPPER_SEWER_WATER_QUAD_CODE       = 0x2A,
        SHELTER_B4_UPPER_SEWER_WATER_DRAW_MODE_WORDS = 1,
        // GPU draw-mode opcode, subtractive blend and retained texture page 10.
        SHELTER_B4_UPPER_SEWER_WATER_DRAW_MODE = 0xE1000000 | (GPU_BLEND_SUBTRACT << 5) | 10
    };

    /// Queues one flat water quad followed by its subtractive draw-mode command.
    ///
    /// Captures screenXY0..3, orderingDepth, blueIntensity, waterQuad and drawMode.
    /// Uses the aligned byte cursor for 36 bytes of frame storage, retaining the
    /// DR_MODE's unused second slot while sending one word. Call as a statement
    /// after a valid projection inside braces; expands to a statement list that
    /// updates the cursor and links both packets, with no additional control flow.
    /// RGB is (0, blueIntensity / 4, blueIntensity) and both packets use the
    /// projected depth + 96. Head insertion makes the mode execute before the
    /// quad. Packed XY words contain signed pixel halfwords; no vector overlay
    /// is implied by those word stores. Packets stay live through GPU consumption.
#define SHELTER_B4_UPPER_SEWER_EMIT_WATER_QUAD()                                                        \
    waterQuad                              = (POLY_F4*)_gShelterB4UpperSewerWaterPacketCursor;          \
    _gShelterB4UpperSewerWaterPacketCursor = (u8*)(waterQuad + 1);                                      \
    setlen(waterQuad, sizeof(*waterQuad) / sizeof(u32) - 1);                                            \
    setcode(waterQuad, SHELTER_B4_UPPER_SEWER_WATER_QUAD_CODE);                                         \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 0) = screenXY0;                                                    \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 1) = screenXY1;                                                    \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 2) = screenXY2;                                                    \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 3) = screenXY3;                                                    \
    waterQuad->r0                       = 0;                                                            \
    waterQuad->g0                       = blueIntensity >> 2;                                           \
    waterQuad->b0                       = blueIntensity;                                                \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(orderingDepth + SHELTER_B4_UPPER_SEWER_WATER_DEPTH_BIAS), \
            waterQuad);                                                                                 \
    drawMode                               = (DR_MODE*)_gShelterB4UpperSewerWaterPacketCursor;          \
    _gShelterB4UpperSewerWaterPacketCursor = (u8*)(drawMode + 1);                                       \
    setlen(drawMode, SHELTER_B4_UPPER_SEWER_WATER_DRAW_MODE_WORDS);                                     \
    drawMode->code[0] = SHELTER_B4_UPPER_SEWER_WATER_DRAW_MODE;                                         \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(orderingDepth + SHELTER_B4_UPPER_SEWER_WATER_DEPTH_BIAS), \
            drawMode);

    SVECTOR           corner0, corner1, corner2, corner3;
    long              screenXY0, screenXY1, screenXY2, screenXY3;
    long              depthCue, projectionFlags;
    WaterQuadScratch* scratch;
    WaterQuadScratch* scratchEnd;
    POLY_F4*          waterQuad;
    DR_MODE*          drawMode;
    s32               orderingDepth;
    s32               segmentIndex;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    scratch->y = waterY;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        if (surface->segmentsAlongZ == 0) {
            scratch->dx = surface->width / surface->segmentCount;
            scratch->dz = surface->depth;
            scratch->x  = surface->x;
            scratch->z  = surface->z;
            for (segmentIndex = 0; segmentIndex < surface->segmentCount; segmentIndex++) {
                corner0.vx       = scratch->x + scratch->dx * segmentIndex;
                corner0.vy       = scratch->y;
                corner0.vz       = scratch->z;
                corner1.vx       = scratch->x + scratch->dx * (segmentIndex + 1);
                corner1.vy       = scratch->y;
                corner1.vz       = scratch->z;
                scratch->yOffset = 0;
                corner2.vx       = scratch->x + scratch->dx * segmentIndex;
                corner2.vy       = scratch->y + scratch->yOffset;
                corner2.vz       = scratch->z + scratch->dz;
                scratch->yOffset = 0;
                corner3.vx       = scratch->x + scratch->dx * (segmentIndex + 1);
                corner3.vy       = scratch->y + scratch->yOffset;
                corner3.vz       = scratch->z + scratch->dz;
                orderingDepth    = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &depthCue, &projectionFlags);
                if (projectionFlags >= 0) {
                    SHELTER_B4_UPPER_SEWER_EMIT_WATER_QUAD();
                }
            }
        } else {
            scratch->dx = surface->width;
            scratch->dz = surface->depth / surface->segmentCount;
            scratch->x  = surface->x;
            scratch->z  = surface->z;
            for (segmentIndex = 0; segmentIndex < surface->segmentCount; segmentIndex++) {
                corner0.vx       = scratch->x;
                corner0.vy       = scratch->y;
                corner0.vz       = scratch->z + scratch->dz * segmentIndex;
                corner1.vx       = scratch->x;
                corner1.vy       = scratch->y;
                corner1.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
                scratch->yOffset = 0;
                corner2.vx       = scratch->x + scratch->dx;
                corner2.vy       = scratch->y + scratch->yOffset;
                corner2.vz       = scratch->z + scratch->dz * segmentIndex;
                scratch->yOffset = 0;
                corner3.vx       = scratch->x + scratch->dx;
                corner3.vy       = scratch->y + scratch->yOffset;
                corner3.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
                orderingDepth    = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &depthCue, &projectionFlags);
                if (projectionFlags >= 0) {
                    SHELTER_B4_UPPER_SEWER_EMIT_WATER_QUAD();
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
#undef SHELTER_B4_UPPER_SEWER_EMIT_WATER_QUAD
}

/// Draws the room's water and publishes its height for water-dependent actors.
///
/// Start in state 0 with this room loaded: initialization clears the selected
/// actor-buffer marker and advances to state 1. Drawing ticks borrow that buffer
/// for packets and clamp height to -1600..0 world units. The first tick publishes
/// the configured height before clamping. Only states 0 and 1 are valid; this
/// bodyless task ignores spawn arguments and does not retire itself.
static void _shelterB4UpperSewerWaterTask(Task* task)
{
    TaskFunc states[] = { _shelterB4UpperSewerInitializeWater, _shelterB4UpperSewerUpdateWater };

    states[task->state](task);
    gGameSession->waterY = gShelterB4UpperSewerWaterY;
}

/// Clears the selected actor-buffer session marker before water rendering begins.
///
/// No companion selects actor buffer 2; a companion selects buffer 1.
/// The marker's nonzero meaning is unproven. Advances the water task to drawing.
static void _shelterB4UpperSewerInitializeWater(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    task->state++;
}

/// Sets whether the current area's view-13 sprite batch is hidden.
///
/// Only the low byte of `hidden` is tested (0 visible, nonzero hidden).
/// Requires the loaded sprite directory for this room and its three-entry
/// view-13 batch list; batch 1 covers the view's single source sprite.
static void _shelterB4UpperSewerSetView13SpriteHidden(s32 hidden)
{
    enum {
        SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_VIEW  = 13,
        SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_BATCH = 1
    };
    const GameLocationKey* location = &gGameSession->location;
    SpriteBatch*           batches;

    batches = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1][SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_VIEW - 1].batches;
    if ((hidden & 0xFF) == 0) {
        batches[SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_BATCH].hidden = 0;
    } else {
        batches[SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_BATCH].hidden = 1;
    }
}

void shelterB4UpperSewerDrawGlowsTask(Task* task)
{
    enum {
        SHELTER_B4_UPPER_SEWER_GLOW_TASK_INITIALIZE = 0,
        SHELTER_B4_UPPER_SEWER_GLOW_TASK_ACTIVE     = 1,
        SHELTER_B4_UPPER_SEWER_RESERVOIR_COMPLETE   = 1,
        SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE    = 0x200,
        // RGB nibbles; the capsule drawer multiplies each channel by 16.
        SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY = 0x444,
        SHELTER_B4_UPPER_SEWER_GLOW_DIM_GRAY    = 0x222,
        SHELTER_B4_UPPER_SEWER_GLOW_BLUE        = 0x124,
        SHELTER_B4_UPPER_SEWER_GLOW_GREEN       = 0x343
    };

    // Install the package's effect callbacks before drawing the visible glow pairs.
    if (task->state == SHELTER_B4_UPPER_SEWER_GLOW_TASK_INITIALIZE) {
        if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == SHELTER_B4_UPPER_SEWER_RESERVOIR_COMPLETE) {
            gRoomEffectWaterRippleId = EFFECT_SHELTER_B4_UPPER_SEWER_WATER_RIPPLE;
            gRoomEffectWaterSprayId  = EFFECT_SHELTER_B4_UPPER_SEWER_WATER_SPRAY;
        }
        gRoomEffectMoteId         = EFFECT_SHELTER_B4_UPPER_SEWER_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B4_UPPER_SEWER_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B4_UPPER_SEWER_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B4_UPPER_SEWER_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B4_UPPER_SEWER_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B4_UPPER_SEWER_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B4_UPPER_SEWER_SPARK_BURST;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B4_UPPER_SEWER_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B4_UPPER_SEWER_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B4_UPPER_SEWER_ORANGE_BURST_2;
        task->state               = SHELTER_B4_UPPER_SEWER_GLOW_TASK_ACTIVE;
    }

    switch (viewGetMappedIndex() & 0xFF) {
        case 3:
            _glowDrawCapsule(&_gShelterB4UpperSewerGlowPoints[12], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            break;
        case 4:
            _glowDrawCapsule(&_gShelterB4UpperSewerGlowPoints[12], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_DIM_GRAY);
            _glowDrawCapsule(&_gShelterB4UpperSewerGlowPoints[24], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BLUE);
            break;
        case 8: {
            const SVECTOR* glowPoints = &_gShelterB4UpperSewerGlowPoints[8];
            _glowDrawCapsule(&glowPoints[0], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[2], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[4], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_DIM_GRAY);
            _glowDrawCapsule(&glowPoints[6], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[16], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BLUE);
            break;
        }
        case 9: {
            const SVECTOR* glowPoints = &_gShelterB4UpperSewerGlowPoints[8];
            _glowDrawCapsule(&glowPoints[0], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[2], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[4], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_DIM_GRAY);
            _glowDrawCapsule(&glowPoints[6], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[16], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BLUE);
            break;
        }
        case 10: {
            const SVECTOR* glowPoints = &_gShelterB4UpperSewerGlowPoints[4];
            _glowDrawCapsule(&glowPoints[0], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[2], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[18], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_GREEN);
            break;
        }
        case 11: {
            const SVECTOR* glowPoints = _gShelterB4UpperSewerGlowPoints;
            _glowDrawCapsule(&glowPoints[0], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[6], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[18], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[20], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_GREEN);
            break;
        }
        case 12:
            _glowDrawCapsule(&_gShelterB4UpperSewerGlowPoints[18], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            break;
        case 13:
            _glowDrawCapsule(&_gShelterB4UpperSewerGlowPoints[24], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BLUE);
            break;
    }
}

#include "../../shared/effects/water_ripple_task.inc.c"

void shelterB4UpperSewerWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task.inc.c"

void shelterB4UpperSewerWaterDriftTask(Task* task)
{
    _waterDriftTask(task);
}

#include "../../shared/effects/water_spin.inc.c"

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void shelterB4UpperSewerRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void shelterB4UpperSewerRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB4UpperSewerRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void shelterB4UpperSewerRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB4UpperSewerRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB4UpperSewerRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB4UpperSewerRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

    void shelterB4UpperSewerRoomVisualEffectsGlowDiscTask(Task * task)
    {
        _roomVisualEffectsGlowDiscTask(task);
    }

    void shelterB4UpperSewerRoomVisualEffectsFlyingSparkTask(Task * task)
    {
        _roomVisualEffectsFlyingSparkTask(task);
    }

#include "../../shared/room/room_visual_effects_burst.inc.c"

    void shelterB4UpperSewerRoomVisualEffectsFlyingOrangeBurstTask(Task * task)
    {
        _roomVisualEffectsFlyingOrangeBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
