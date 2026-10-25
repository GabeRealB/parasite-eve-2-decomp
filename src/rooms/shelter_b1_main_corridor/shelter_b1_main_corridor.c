#include "rooms/shelter_b1_main_corridor.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_00300.h"
#include "actors/actor_01100.h"
#include "actors/actor_02100.h"
#include "actors/actor_05600.h"
#include "actors/actor_05700.h"
#include "actors/actor_400600.h"
#include "actors/actor_402200.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
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
#include "main/event_flags.h"
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

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

// The flag symbol carries seven unproven bytes after the flag.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
// The request symbol carries twelve unproven bytes after the request.
#define ROOM_EVENT_REQ gRoomEventReq.request
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"

/// The event the gate last accepted: the message that triggered it, whose
/// `msgId`, `field_2` and `field_3` name the area, warp and room the event
/// task finally loads, and the request whose CAP command and sounds it runs.
extern RoomEventReqStorage gRoomEventReq;
/// Set once the gate has latched an event and spawned its task.
extern RoomEventStartStorage gRoomEventActive;
/// Spawn descriptor of the event task, `roomEventTask`.

/// The event the message handler last started: the message that triggered it,
/// the flag saying one was latched and its task spawned, and the event record.
/// `_gShelterB1MainCorridorStagedEventTaskDesc` spawns that event's task,
/// `roomEventStagedTask`.
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;
/// Spawn argument the event task hands to task 0x31.
extern RoomFadeStorage gRoomEventFade;

/// Per-colour shift amounts the flash effect's brightness is scaled down by,
/// one row per colour its spawner can pick.

/// The two points the beam runs between, relative to its parent coordinate;
/// the second is also declared on its own.

static s32  _shelterB1MainCorridorResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static void _shelterB1MainCorridorInitRoomTask(Task* task);
static s32  _shelterB1MainCorridorRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _shelterB1MainCorridorIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _shelterB1MainCorridorIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _shelterB1MainCorridorHandleSoundMessage(Task* task, s32 messageId, s32 soundId, s32 unusedArg);

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// Spawn recipe for the corridor's deferred CAP, sound and room-reload controller.
///
/// No coordinate body or descriptor data; execution priority is 32. The controller
/// borrows the singleton transition and event snapshots until the room reloads.
static TaskDesc _gShelterB1MainCorridorStagedEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

/// Room-message routes for transitions, key items, direction actions, commands and sounds.
///
/// The receiver borrows this six-record list through teardown. Only transitions
/// and sound cues perform work; `TASK_MESSAGE_TABLE_END` ends the list. Payload
/// types follow each message's contract rather than one shared pointer type.
static TaskMessageEntry _gShelterB1MainCorridorRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1MainCorridorResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1MainCorridorRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1MainCorridorIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1MainCorridorIgnoreCommand },
    { ROOM_MESSAGE_SOUND, _shelterB1MainCorridorHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Four white-beam endpoint pairs in signed world-coordinate units.
///
/// Mapped views 2 and 3 draw the first two pairs on the negative-X side. The
/// remaining two pairs mirror them at positive X and have no drawing consumer.
static SVECTOR _gShelterB1MainCorridorEntryBeamPoints[8] = {
    { -667, -1910, -0x4A3D, 0 },
    { -667, -1910, -0x45ED, 0 },
    { -667, -1910, -0x4309, 0 },
    { -667, -1910, -0x3EB4, 0 },
    { 668, -1910, -0x4A3D, 0 },
    { 668, -1910, -0x45ED, 0 },
    { 668, -1910, -0x4309, 0 },
    { 668, -1910, -0x3EB4, 0 },
};

/// World-space beam endpoints and flare centres for mapped corridor views 4..10.
///
/// Points 0..7 form four green beam pairs, and 8..9 form a red beam pair.
/// Points 10..16 and 17..23 are the negative- and positive-X flare rows.
/// View 4 draws row entries 0..1; view 7 draws entries 5..6. Coordinates are
/// signed game units; the fourth halfword is unused by the drawers.
static SVECTOR _gShelterB1MainCorridorViewLightPoints[24] = {
    { -3246, -2265, -0x2988, 0 },
    { -3246, -2265, -9370, 0 },
    { -3246, -2265, -2620, 0 },
    { -3246, -2265, -1381, 0 },
    { 3260, -2265, -0x2988, 0 },
    { 3260, -2265, -9370, 0 },
    { 3260, -2265, -2620, 0 },
    { 3260, -2265, -1381, 0 },
    { -365, -2226, -180, 0 },
    { 365, -2226, -180, 0 },
    { -1293, -4768, -0x34A6, 0 },
    { -1293, -4768, -0x2CE0, 0 },
    { -1293, -4768, -9483, 0 },
    { -1293, -4768, -7483, 0 },
    { -1293, -4768, -5483, 0 },
    { -1293, -4768, -3483, 0 },
    { -1293, -4768, -1483, 0 },
    { 1293, -4768, -0x34A6, 0 },
    { 1293, -4768, -0x2CE0, 0 },
    { 1293, -4768, -9483, 0 },
    { 1293, -4768, -7483, 0 },
    { 1293, -4768, -5483, 0 },
    { 1293, -4768, -3483, 0 },
    { 1293, -4768, -1483, 0 },
};

/// Initializes this overlay's three halo tint rows and retained trailing halfword.
///
/// Rows 0..2 give RGB right shifts of brightness (0 full, 1 half, 2 quarter).
/// Requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` = `RoomFxHaloStorage` and an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND`. The trailing u16 is preserved
/// verbatim; its role is unproven and it is not a fourth tint row.
/// Bind around `room_visual_effects_halo_data.inc.c`, then undefine.
/// Expands to a constant braced initializer, with no arguments or side effects.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER { \
    .entries = {                                       \
        { .rShift = 0, .gShift = 1, .bShift = 2 },     \
        { .rShift = 2, .gShift = 1, .bShift = 0 },     \
        { .rShift = 0, .gShift = 2, .bShift = 1 },     \
    },                                                 \
    .alignmentFill = 0x70C2,                           \
}
/// Selects the halo tint storage record, including its retained trailing halfword.
///
/// Expands to the `RoomFxHaloStorage` type token; requires an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` and a
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER` for `entries` and `alignmentFill`.
/// Only the three RGB-shift rows are read, at indices 0..2; the halfword's
/// role is unproven. Include `room_visual_effects.h` first, bind around
/// `room_visual_effects_halo_data.inc.c`, then undefine.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE RoomFxHaloStorage
/// Declares one halo storage record, retaining the halfword after its tint rows.
///
/// This empty suffix requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` to be
/// `RoomFxHaloStorage`. Bind around `room_visual_effects_halo_data.inc.c`.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND
#include "../../shared/room/room_visual_effects_halo_data.inc.c"

static void _glowDrawBeam(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades.entries;
}
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

u8* gShelterB1MainCorridorRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB1MainCorridorRoomViewCounts[1] = { ARRAY_SIZE(gShelterB1MainCorridorViewCameras) };

enum {
    SHELTER_B1_MAIN_CORRIDOR_WARP_ARRIVAL_SCRIPT    = 2,
    SHELTER_B1_MAIN_CORRIDOR_WARP_DEPARTURE_SCRIPT  = 1,
    SHELTER_B1_MAIN_CORRIDOR_WARP_BLOCKED_SCRIPT    = 13,
    SHELTER_B1_MAIN_CORRIDOR_WARP_ELEVATOR_HALL     = 1,
    SHELTER_B1_MAIN_CORRIDOR_WARP_CONTROL_ACCESS    = 2,
    SHELTER_B1_MAIN_CORRIDOR_WARP_ARMORY            = 3,
    SHELTER_B1_MAIN_CORRIDOR_WARP_TRANSFER_TUNNEL   = 4,
    SHELTER_B1_MAIN_CORRIDOR_WARP_SLEEPING_QUARTERS = 5,
    SHELTER_B1_MAIN_CORRIDOR_WARP_STERILIZATION     = 6,
};

DirectionWarpEntry gShelterB1MainCorridorWarpEntries[6] = {
    { { { .word = 0 }, 0, 0, -0x4A38 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_ARRIVAL_SCRIPT), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_DEPARTURE_SCRIPT), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1CD },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -3467, 0, -0x2745 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_ARRIVAL_SCRIPT), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_DEPARTURE_SCRIPT), DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 3510, 0, -0x2729 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_ARRIVAL_SCRIPT), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_DEPARTURE_SCRIPT), DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -3500, 0, -1940 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -3500, 0, -1940 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_ARRIVAL_SCRIPT), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_DEPARTURE_SCRIPT), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_BLOCKED_SCRIPT), 10, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_R47_1C6 },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 3500, 0, -1940 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_ARRIVAL_SCRIPT), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_DEPARTURE_SCRIPT), DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 0 }, 10, 0, -390 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_ARRIVAL_SCRIPT), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_MAIN_CORRIDOR_WARP_DEPARTURE_SCRIPT), DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

enum {
    SHELTER_B1_MAIN_CORRIDOR_GRID_CELL_COUNT_X = 3,
    SHELTER_B1_MAIN_CORRIDOR_GRID_CELL_COUNT_Z = 6,
    SHELTER_B1_MAIN_CORRIDOR_GRID_CELL_SIZE    = 4000,
};

/// Collision-face normals in room axes, with ONE (4096) for unit length.
static SVECTOR _gShelterB1MainCorridorCollisionGridNormals[34] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_normals.inc"
};

/// Collision vertices in signed room-coordinate units, indexed by the face pool.
static SVECTOR _gShelterB1MainCorridorCollisionGridVerts[172] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_verts.inc"
};

/// Indexed room collision faces using blocking, pass-through and floor classes 0..2.
static WorldCollisionGridFace _gShelterB1MainCorridorCollisionGridFaces[82] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_faces.inc"
};

/// Face-index storage for eighteen lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB1MainCorridorCollisionGridCells[392] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelterB1MainCorridorCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..391,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_B1_MAIN_CORRIDOR_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1MainCorridorCollisionGridCells[(halfwordOffset)])
/// Cell lists in X-major order, indexed as x * cellCountZ + z.
static s16* _gShelterB1MainCorridorCollisionGridTable[SHELTER_B1_MAIN_CORRIDOR_GRID_CELL_COUNT_X * SHELTER_B1_MAIN_CORRIDOR_GRID_CELL_COUNT_Z] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_table.inc"
};
#undef SHELTER_B1_MAIN_CORRIDOR_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB1MainCorridorCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB1MainCorridorCollisionGridNormals,
    .vertices    = _gShelterB1MainCorridorCollisionGridVerts,
    .faces       = _gShelterB1MainCorridorCollisionGridFaces,
    .cellFaceIds = _gShelterB1MainCorridorCollisionGridTable,
    .xBias       = 5398,
    .zBias       = 19715,
    .cellCountX  = SHELTER_B1_MAIN_CORRIDOR_GRID_CELL_COUNT_X,
    .cellCountZ  = SHELTER_B1_MAIN_CORRIDOR_GRID_CELL_COUNT_Z,
    .cellSize    = SHELTER_B1_MAIN_CORRIDOR_GRID_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gShelterB1MainCorridorCollisionGridFaces),
};

ViewCamera gShelterB1MainCorridorViewCameras[10] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5EE9, 8000 } }, 230 },
    { { { { -4024, 0, 762 }, { 150, 4015, 794 }, { -747, 808, -3945 } }, { -721, 1939, 0x2F2F } }, 275 },
    { { { { -3981, 0, 962 }, { 213, 3993, 882 }, { -938, 908, -3882 } }, { -784, 2080, 8975 } }, 275 },
    { { { { -4071, 0, 448 }, { 31, 4085, 288 }, { -447, 290, -4061 } }, { -1306, 2162, 2495 } }, 269 },
    { { { { -2233, 0, -3433 }, { -683, 4013, 444 }, { 3364, 815, -2188 } }, { -62, 1858, 8634 } }, 269 },
    { { { { -2474, 0, 3264 }, { 649, 4014, 492 }, { -3199, 814, -2424 } }, { -362, 1983, 8329 } }, 269 },
    { { { { 3975, 0, 986 }, { 54, 4089, -219 }, { -984, 225, 3969 } }, { -665, 2036, 0x2EF4 } }, 269 },
    { { { { 4034, 0, 705 }, { 123, 4032, -707 }, { -694, 718, 3972 } }, { -564, 1895, 6271 } }, 257 },
    { { { { 1839, 0, -3659 }, { -944, 3957, -474 }, { 3535, 1056, 1777 } }, { 945, 1694, 2927 } }, 282 },
    { { { { 2080, 0, 3528 }, { 535, 4048, -316 }, { -3487, 622, 2056 } }, { -392, 1529, 3409 } }, 282 },
};

/// Encoded room-sprite texture page and palette, with neutral GPU modulation.
enum {
    SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE  = getTPage(1, 0, 960, 0),
    SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT          = getClut(0, 255),
    SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR = 128,
};

/// Empty sprite list for mapped corridor view 1, retaining the decoded backdrop.
///
/// No source elements; the view's retained source-slot address aliases this list.
static SpriteBatch _gShelterB1MainCorridorView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped corridor view 2, retaining the decoded backdrop.
///
/// No source elements; the view's retained source-slot address aliases this list.
static SpriteBatch _gShelterB1MainCorridorView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped corridor view 3, retaining the decoded backdrop.
///
/// No source elements; the view's retained source-slot address aliases this list.
static SpriteBatch _gShelterB1MainCorridorView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles for mapped corridor view 4, in pixels relative to the view centre.
///
/// Batches select all 56 elements. Depths are unscaled OT sorting values;
/// texture geometry is cached in both frame buffers while this overlay stays live.
static SpriteSource _gShelterB1MainCorridorView4SpriteSources[56] = {
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 48, 56, 1642, { .fields = { 104, 152 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, 64, 1482, { .fields = { 104, 168 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 56, 1484, { .fields = { 120, 96 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 32, 1375, { .fields = { 120, 120 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -120, 32, 1446, { .fields = { 112, 24 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 136, 24, 1691, { .fields = { 120, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 136, 48, 1665, { .fields = { 120, 72 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, 144, 48, 1625, { .fields = { 112, 48 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, -24, 1509, { .fields = { 88, 24 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, -16, 1510, { .fields = { 88, 40 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, -8, 1520, { .fields = { 88, 48 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 0, 1426, { .fields = { 112, 120 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 16, 1432, { .fields = { 112, 136 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 24, 1443, { .fields = { 88, 88 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 16, 1440, { .fields = { 96, 120 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 8, 1437, { .fields = { 96, 80 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 0, 1434, { .fields = { 88, 16 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 32, 1446, { .fields = { 96, 224 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 40, 1449, { .fields = { 96, 144 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 32, 1438, { .fields = { 120, 192 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 48, 1431, { .fields = { 96, 232 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 56, 1401, { .fields = { 96, 136 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 48, 1444, { .fields = { 120, 208 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 72, 1373, { .fields = { 96, 72 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 64, 1372, { .fields = { 96, 128 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 64, 1450, { .fields = { 120, 176 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -112, 56, 1456, { .fields = { 120, 144 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -120, 56, 1451, { .fields = { 120, 160 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -120, 72, 1458, { .fields = { 88, 176 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, -24, 1690, { .fields = { 88, 168 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, 0, 1681, { .fields = { 88, 192 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, 8, 1666, { .fields = { 88, 184 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -40, 64, 1578, { .fields = { 80, 160 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -40, 72, 1596, { .fields = { 80, 104 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 64, 1562, { .fields = { 80, 96 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 72, 1597, { .fields = { 80, 152 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 32, 8 } }, -96, 64, 1546, { .fields = { 72, 112 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 56, 1644, { .fields = { 96, 208 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 64, 1668, { .fields = { 96, 200 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, 64, 56, 1655, { .fields = { 96, 216 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, 64, 64, 1651, { .fields = { 96, 248 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -120, 0, 1479, { .fields = { 104, 240 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -120, 8, 1467, { .fields = { 104, 16 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -120, 16, 1452, { .fields = { 104, 184 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -120, 24, 1442, { .fields = { 104, 192 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, -16, 1685, { .fields = { 112, 224 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 16, 1638, { .fields = { 104, 88 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 32, 1631, { .fields = { 104, 104 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, -24, 1427, { .fields = { 96, 64 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, -16, 1428, { .fields = { 96, 56 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, -8, 1431, { .fields = { 96, 8 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -96, 72, 1574, { .fields = { 96, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -80, 72, 1584, { .fields = { 96, 32 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 128, 56, 1674, { .fields = { 112, 72 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 120, 56, 1668, { .fields = { 112, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 112, 56, 1646, { .fields = { 120, 240 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Backdrop-preserving batch list followed by the full sprite range for mapped view 4.
///
/// The zero-count first record retains decoded image strips; SPRITE_BATCH_END ends the list.
static SpriteBatch _gShelterB1MainCorridorView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1MainCorridorView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-limited clip for mapped view 4, followed by SPRITE_DRAW_AREA_END.
///
/// The rectangle uses draw-buffer pixels and clips scene primitives as well as
/// sprites; the restore depth queues the full-screen clip in the ordering table.
static SpriteDrawArea _gShelterB1MainCorridorView4SpriteDrawAreas[2] = {
    { { 33, 0, 285, 239 }, 1373 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Sprite rectangles for mapped corridor view 5, in pixels relative to the view centre.
///
/// Batches select all 13 elements. Depths are unscaled OT sorting values;
/// texture geometry is cached in both frame buffers while this overlay stays live.
static SpriteSource _gShelterB1MainCorridorView5SpriteSources[13] = {
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 96, 694, { .fields = { 120, 144 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -128, 96, 712, { .fields = { 120, 120 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -136, 96, 712, { .fields = { 120, 168 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -144, 96, 685, { .fields = { 120, 192 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -160, 96, 675, { .fields = { 112, 248 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -136, 72, 854, { .fields = { 112, 24 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, 72, 682, { .fields = { 104, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, 48, 679, { .fields = { 104, 48 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, 32, 650, { .fields = { 96, 120 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 8, 647, { .fields = { 112, 96 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, -16, 650, { .fields = { 112, 72 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, -32, 651, { .fields = { 112, 216 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -160, -48, 679, { .fields = { 120, 232 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Backdrop-preserving batch list followed by the full sprite range for mapped view 5.
///
/// The zero-count first record retains decoded image strips; SPRITE_BATCH_END ends the list.
static SpriteBatch _gShelterB1MainCorridorView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1MainCorridorView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles for mapped corridor view 6, in pixels relative to the view centre.
///
/// Batches select all 17 elements. Depths are unscaled OT sorting values;
/// texture geometry is cached in both frame buffers while this overlay stays live.
static SpriteSource _gShelterB1MainCorridorView6SpriteSources[17] = {
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 136, 104, 0xFFF0, { .fields = { 120, 104 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 104, 104, 732, { .fields = { 120, 232 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 112, 104, 730, { .fields = { 120, 184 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 112, 88, 759, { .fields = { 120, 216 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 120, 104, 746, { .fields = { 120, 200 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, 80, 802, { .fields = { 120, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 128, 104, 779, { .fields = { 112, 16 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 128, 88, 773, { .fields = { 104, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 128, 64, 971, { .fields = { 120, 24 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 136, 72, 754, { .fields = { 120, 168 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, 72, 728, { .fields = { 112, 248 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 56, 769, { .fields = { 104, 88 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 40, 773, { .fields = { 104, 72 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 24, 750, { .fields = { 104, 152 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 8, 752, { .fields = { 104, 136 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, -8, 747, { .fields = { 112, 120 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, 144, -32, 748, { .fields = { 112, 48 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Backdrop-preserving batch list followed by the full sprite range for mapped view 6.
///
/// The zero-count first record retains decoded image strips; SPRITE_BATCH_END ends the list.
static SpriteBatch _gShelterB1MainCorridorView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1MainCorridorView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles for mapped corridor view 7, in pixels relative to the view centre.
///
/// Batches select all 24 elements. Depths are unscaled OT sorting values;
/// texture geometry is cached in both frame buffers while this overlay stays live.
static SpriteSource _gShelterB1MainCorridorView7SpriteSources[24] = {
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 64, 48, 1953, { .fields = { 104, 88 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 64, 40, 2152, { .fields = { 104, 80 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 80, 40, 2102, { .fields = { 112, 224 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 96, 40, 2073, { .fields = { 112, 176 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 40, 2045, { .fields = { 112, 192 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 128, 40, 2024, { .fields = { 120, 240 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 136, 40, 1995, { .fields = { 112, 64 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 40, 1928, { .fields = { 104, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 24, 1922, { .fields = { 104, 48 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 8, 1955, { .fields = { 112, 160 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 152, -16, 1961, { .fields = { 120, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 8, 48, 0xFFF0, { .fields = { 104, 96 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 8, 40, 2257, { .fields = { 104, 144 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -8, 32, 2293, { .fields = { 120, 64 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 0, 40, 2267, { .fields = { 120, 48 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -24, 32, 2308, { .fields = { 112, 128 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -32, 32, 2335, { .fields = { 120, 144 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, 32, 2369, { .fields = { 120, 96 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -48, 24, 2362, { .fields = { 104, 104 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -64, 32, 2311, { .fields = { 104, 112 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 24, 2315, { .fields = { 104, 16 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, 8, 2351, { .fields = { 120, 80 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -64, 8, 2324, { .fields = { 112, 208 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 24 } }, -64, -16, 2289, { .fields = { 104, 24 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Backdrop-preserving batch list followed by the full sprite range for mapped view 7.
///
/// The zero-count first record retains decoded image strips; SPRITE_BATCH_END ends the list.
static SpriteBatch _gShelterB1MainCorridorView7SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1MainCorridorView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-limited clip for mapped view 7, followed by SPRITE_DRAW_AREA_END.
///
/// The rectangle uses draw-buffer pixels and clips scene primitives as well as
/// sprites; the restore depth queues the full-screen clip in the ordering table.
static SpriteDrawArea _gShelterB1MainCorridorView7SpriteDrawAreas[2] = {
    { { 105, 0, 215, 239 }, 2289 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Sprite rectangles for mapped corridor view 8, in pixels relative to the view centre.
///
/// Batches select all 19 elements. Depths are unscaled OT sorting values;
/// texture geometry is cached in both frame buffers while this overlay stays live.
static SpriteSource _gShelterB1MainCorridorView8SpriteSources[19] = {
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, 88, 0, { .fields = { 88, 56 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 56, 112, 0, { .fields = { 96, 48 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 56, 96, 752, { .fields = { 104, 232 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, 56, 88, 802, { .fields = { 96, 72 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, 72, 88, 783, { .fields = { 112, 208 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, 88, 774, { .fields = { 112, 184 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 104, 88, 767, { .fields = { 112, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 120, 88, 758, { .fields = { 112, 32 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 24 } }, 136, 96, 750, { .fields = { 104, 88 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -72, 88, 838, { .fields = { 88, 64 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -64, 72, 881, { .fields = { 96, 32 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, 72, 881, { .fields = { 104, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -88, 72, 874, { .fields = { 112, 112 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -104, 72, 886, { .fields = { 112, 160 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -120, 72, 897, { .fields = { 112, 136 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -136, 72, 909, { .fields = { 112, 64 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 32, 8 } }, -160, 64, 984, { .fields = { 88, 248 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -144, 72, 919, { .fields = { 120, 232 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, 72, 925, { .fields = { 96, 16 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Backdrop-preserving batch list followed by the full sprite range for mapped view 8.
///
/// The zero-count first record retains decoded image strips; SPRITE_BATCH_END ends the list.
static SpriteBatch _gShelterB1MainCorridorView8SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1MainCorridorView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles for mapped corridor view 9, in pixels relative to the view centre.
///
/// Batches select all 26 elements. Depths are unscaled OT sorting values;
/// texture geometry is cached in both frame buffers while this overlay stays live.
static SpriteSource _gShelterB1MainCorridorView9SpriteSources[26] = {
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -144, 88, 0, { .fields = { 80, 208 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, 40, 731, { .fields = { 96, 80 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -120, 24, 744, { .fields = { 88, 32 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -128, 8, 734, { .fields = { 104, 224 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -128, -8, 731, { .fields = { 104, 240 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -136, -24, 729, { .fields = { 88, 112 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 32, 8 } }, -144, -32, 725, { .fields = { 80, 48 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -144, -48, 721, { .fields = { 80, 16 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -144, -56, 717, { .fields = { 80, 216 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -152, -72, 714, { .fields = { 88, 128 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 32, 8 } }, -160, -80, 726, { .fields = { 72, 232 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, -96, 723, { .fields = { 88, 192 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -160, -104, 717, { .fields = { 80, 224 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, -120, 712, { .fields = { 112, 112 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -160, -72, 805, { .fields = { 120, 80 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, -56, 780, { .fields = { 112, 128 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, -40, 843, { .fields = { 112, 192 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, -24, 850, { .fields = { 104, 208 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, -8, 858, { .fields = { 96, 176 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, 8, 745, { .fields = { 96, 144 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 40, 16 } }, -160, 24, 881, { .fields = { 88, 160 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 48, 16 } }, -160, 40, 889, { .fields = { 80, 96 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 56, 923, { .fields = { 112, 32 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 32 } }, -144, 56, 906, { .fields = { 104, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 32, 16 } }, -120, 56, 743, { .fields = { 72, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 40, 24 } }, -120, 72, 843, { .fields = { 88, 56 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Backdrop-preserving batch list followed by the full sprite range for mapped view 9.
///
/// The zero-count first record retains decoded image strips; SPRITE_BATCH_END ends the list.
static SpriteBatch _gShelterB1MainCorridorView9SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1MainCorridorView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite rectangles for mapped corridor view 10, in pixels relative to the view centre.
///
/// Batches select all 20 elements. Depths are unscaled OT sorting values;
/// texture geometry is cached in both frame buffers while this overlay stays live.
static SpriteSource _gShelterB1MainCorridorView10SpriteSources[20] = {
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 32, 8 } }, -128, 112, 610, { .fields = { 88, 200 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 32, 8 } }, -128, 104, 627, { .fields = { 88, 208 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, 96, 663, { .fields = { 96, 192 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, 88, 777, { .fields = { 120, 184 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, 88, 672, { .fields = { 96, 184 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -128, 80, 718, { .fields = { 104, 176 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 8 } }, -128, 72, 771, { .fields = { 112, 248 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, 72, 780, { .fields = { 120, 232 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 56, 937, { .fields = { 120, 216 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, 56, 736, { .fields = { 120, 200 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -144, 56, 677, { .fields = { 120, 160 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, 48, 735, { .fields = { 104, 32 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -144, 40, 735, { .fields = { 104, 160 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -144, 16, 743, { .fields = { 112, 112 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 24, 739, { .fields = { 112, 136 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 0, 697, { .fields = { 112, 88 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 32 } }, -144, -16, 708, { .fields = { 120, 32 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, -16, 701, { .fields = { 104, 48 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, -40, 685, { .fields = { 104, 64 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_MAIN_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B1_MAIN_CORRIDOR_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, -72, 704, { .fields = { 104, 0 } }, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B1_MAIN_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 10's complete sprite range, retaining the decoded background.
///
/// The empty first record selects background strips, the second includes every
/// source, and SPRITE_BATCH_END ends the list; all three records are retained.
static SpriteBatch _gShelterB1MainCorridorView10SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1MainCorridorView10SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB1MainCorridorSpriteViews[10] = {
    { { .empty = _gShelterB1MainCorridorView1SpriteBatches }, _gShelterB1MainCorridorView1SpriteBatches, NULL },
    { { .empty = _gShelterB1MainCorridorView2SpriteBatches }, _gShelterB1MainCorridorView2SpriteBatches, NULL },
    { { .empty = _gShelterB1MainCorridorView3SpriteBatches }, _gShelterB1MainCorridorView3SpriteBatches, NULL },
    { { .elements = _gShelterB1MainCorridorView4SpriteSources }, _gShelterB1MainCorridorView4SpriteBatches, _gShelterB1MainCorridorView4SpriteDrawAreas },
    { { .elements = _gShelterB1MainCorridorView5SpriteSources }, _gShelterB1MainCorridorView5SpriteBatches, NULL },
    { { .elements = _gShelterB1MainCorridorView6SpriteSources }, _gShelterB1MainCorridorView6SpriteBatches, NULL },
    { { .elements = _gShelterB1MainCorridorView7SpriteSources }, _gShelterB1MainCorridorView7SpriteBatches, _gShelterB1MainCorridorView7SpriteDrawAreas },
    { { .elements = _gShelterB1MainCorridorView8SpriteSources }, _gShelterB1MainCorridorView8SpriteBatches, NULL },
    { { .elements = _gShelterB1MainCorridorView9SpriteSources }, _gShelterB1MainCorridorView9SpriteBatches, NULL },
    { { .elements = _gShelterB1MainCorridorView10SpriteSources }, _gShelterB1MainCorridorView10SpriteBatches, NULL },
};

/// Six world-space point lights accepted in every view, with mutable composition and attenuation.
///
/// Positions and inner/outer falloff radii use world units; RGB has 12 fractional bits.
static WorldCoordPointLight _gShelterB1MainCorridorPointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2129, -1643, -681 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 88, -1643, -6981 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2129, -1643, -0x2CF5 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -73, -1489, -0x40FD } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2197, 2212, 2233 }, { 0, 0 } }, 2944, 8822 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2366, -1643, -0x2CF5 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2366, -1643, -681 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2638, 2638, 2688 }, { 0, 0 } }, 2000, 6000 },
};

WorldCoordRoomLights gShelterB1MainCorridorRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB1MainCorridorPointLights),
    .pointLights           = _gShelterB1MainCorridorPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB1MainCorridorViewBoundaries[16] = {
    { NULL, NULL, NULL, { 64, -1568, -0x2EE0, 0 }, { { -2316, -3712, 337, 0 }, { 2305, -3712, -348, 0 }, { -2316, 3712, 337, 0 }, { 2305, 3712, -348, 0 } }, { -603, 0, -4063, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -32, -1696, -0x2F5D, 0 }, { { 2562, -3712, -382, 0 }, { -2565, -3712, 379, 0 }, { 2562, 3712, -382, 0 }, { -2565, 3712, 379, 0 } }, { 601, 0, 4055, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 128, -1568, -7200, 0 }, { { 2592, -3712, 128, 0 }, { -2592, -3712, -128, 0 }, { 2592, 3712, 128, 0 }, { -2592, 3712, -128, 0 } }, { -203, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 112, -1664, -7056, 0 }, { { -2640, -3712, -80, 0 }, { 2640, -3712, 80, 0 }, { -2640, 3712, -80, 0 }, { 2640, 3712, 80, 0 } }, { 124, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 4550, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1664, -3456, 0 }, { { -2640, -3712, -80, 0 }, { 2640, -3712, 80, 0 }, { -2640, 3712, -80, 0 }, { 2640, 3712, 80, 0 } }, { 124, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 4550, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1536, -3616, 0 }, { { 2592, -3712, 128, 0 }, { -2592, -3712, -128, 0 }, { 2592, 3712, 128, 0 }, { -2592, 3712, -128, 0 } }, { -203, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2432, -1632, -9952, 0 }, { { -432, -3712, -2106, 0 }, { 424, -3712, 2099, 0 }, { -432, 3712, -2106, 0 }, { 424, 3712, 2099, 0 } }, { 4018, 0, -819, 0 }, { 0, 0, 4096, 0 }, 4283, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2576, -1632, -9936, 0 }, { { 317, -3712, 1745, 0 }, { -329, -3712, -1755, 0 }, { 317, 3712, 1745, 0 }, { -329, 3712, -1755, 0 } }, { -4037, 0, 744, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2208, -1568, -0x2740, 0 }, { { -544, -3712, 1691, 0 }, { 536, -3712, -1700, 0 }, { -544, 3712, 1691, 0 }, { 536, 3712, -1700, 0 } }, { -3911, 0, -1246, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2337, -1600, -0x27A1, 0 }, { { 466, -3712, -1686, 0 }, { -475, -3712, 1674, 0 }, { 466, 3712, -1686, 0 }, { -475, 3712, 1674, 0 } }, { 3950, 0, 1106, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1568, -1600, -1776, 0 }, { { 48, -3712, -1472, 0 }, { -48, -3712, 1472, 0 }, { 48, 3712, -1472, 0 }, { -48, 3712, 1472, 0 } }, { 4107, 0, 133, 0 }, { 0, 0, 4096, 0 }, 3990, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1664, -1568, -1824, 0 }, { { -58, -3712, 1401, 0 }, { 44, -3712, -1413, 0 }, { -58, 3712, 1401, 0 }, { 44, 3712, -1413, 0 } }, { -4101, 0, -149, 0 }, { 0, 0, 4096, 0 }, 3965, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2144, -1536, -1952, 0 }, { { 206, -3712, 1372, 0 }, { -244, -3712, -1407, 0 }, { 206, 3712, 1372, 0 }, { -244, 3712, -1407, 0 } }, { -4050, 0, 655, 0 }, { 0, 0, 4096, 0 }, 3965, 0, 10, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2272, -1536, -1920, 0 }, { { -195, -3712, -1487, 0 }, { 156, -3712, 1434, 0 }, { -195, 3712, -1487, 0 }, { 156, 3712, 1434, 0 } }, { 4075, 0, -491, 0 }, { 0, 0, 4096, 0 }, 3990, 0, 8, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1696, -0x3C61, 0 }, { { -2293, -3712, 454, 0 }, { 2289, -3712, -457, 0 }, { -2293, 3712, 454, 0 }, { 2289, 3712, -457, 0 } }, { -802, 0, -4028, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1696, -0x3D21, 0 }, { { 2538, -3712, -511, 0 }, { -2546, -3712, 502, 0 }, { 2538, 3712, -511, 0 }, { -2546, 3712, 502, 0 } }, { 801, 0, 4021, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger gShelterB1MainCorridorWarpTriggers[6] = {
    { NULL, NULL, NULL, { 0, -48, -0x4990, 0 }, { { -1024, 0, -560, 0 }, { 1024, 0, -560, 0 }, { -1024, 0, 560, 0 }, { 1024, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, (SHELTER_B1_MAIN_CORRIDOR_WARP_ELEVATOR_HALL << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3488, -48, -9984, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, (SHELTER_B1_MAIN_CORRIDOR_WARP_CONTROL_ACCESS << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3456, -48, -9952, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ARMORY, (SHELTER_B1_MAIN_CORRIDOR_WARP_ARMORY << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3456, -48, -2016, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, (SHELTER_B1_MAIN_CORRIDOR_WARP_TRANSFER_TUNNEL << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3456, -48, -2048, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, (SHELTER_B1_MAIN_CORRIDOR_WARP_SLEEPING_QUARTERS << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -48, 0, 0 }, { { -1024, 0, -560, 0 }, { 1024, 0, -560, 0 }, { -1024, 0, 560, 0 }, { 1024, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -4096, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, (SHELTER_B1_MAIN_CORRIDOR_WARP_STERILIZATION << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Watcher and actor-201100 resources for the default corridor layout.
///
/// Entry IDs match this variant's placements; task index 0 selects each recipe.
/// AREA_PLACEMENT_END terminates the list. Keep the room and selected actor
/// overlays loaded while loading or spawning from these borrowed records.
static AreaResource _gShelterB1MainCorridorVariant1Resources[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { 11, 11, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor201100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Zebra Stalker resource for corridor variant 2.
///
/// Entry IDs match this variant's placements; task index 0 selects each recipe.
/// AREA_PLACEMENT_END terminates the list. Keep the room and selected actor
/// overlays loaded while loading or spawning from these borrowed records.
static AreaResource _gShelterB1MainCorridorVariant2Resources[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor400600ZebraStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor 100300 resource for corridor variant 3.
///
/// Entry IDs match this variant's placements; task index 0 selects each recipe.
/// AREA_PLACEMENT_END terminates the list. Keep the room and selected actor
/// overlays loaded while loading or spawning from these borrowed records.
static AreaResource _gShelterB1MainCorridorVariant3Resources[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor100300Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Knight GOLEM resource for corridor variant 7.
///
/// Entry IDs match this variant's placements; task index 0 selects each recipe.
/// AREA_PLACEMENT_END terminates the list. Keep the room and selected actor
/// overlays loaded while loading or spawning from these borrowed records.
static AreaResource _gShelterB1MainCorridorVariant7Resources[2] = {
    { 22, 22, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor402200GolemTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Pawn and Rook GOLEM resources for corridor variant 11.
///
/// Entry IDs match this variant's placements; task index 0 selects each recipe.
/// AREA_PLACEMENT_END terminates the list. Keep the room and selected actor
/// overlays loaded while loading or spawning from these borrowed records.
static AreaResource _gShelterB1MainCorridorVariant11Resources[3] = {
    { 56, 56, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor105600PawnGolemTasks },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor05700RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Five placements for the default corridor layout: three Watchers and two actor-201100 instances.
///
/// AREA_PLACEMENT_END terminates the list. Positions are signed world units,
/// yaw uses 4096 units per turn, and variant/mode/row select actor-specific
/// behavior. Watcher variants 34 and 4 select patrol range 3/0 and weapon 4;
/// mode supplies placement roll (2048 or 0), and sight rows are 2 or 3.
/// The two actor-201100 placements use high-half spawn behavior 1. Spawned
/// actors borrow these records while the room stays loaded.
static AreaPlacement _gShelterB1MainCorridorVariant1Placements[6] = {
    { 21, 34, 2048, -3600, -1750, -7800, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0, 2, 2 },
    { 21, 4, 0, 1100, -2100, -0x3C8C, 0, 0, 0, 2, 3 },
    { 21, 4, 0, -1100, -2100, -0x3C8C, 0, 0, 0, 2, 3 },
    { 11, 1, 0, 0, 0, -2000, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 2, 4, 0 },
    { 11, 1, 0, 0, 0, -0x2710, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Three Zebra Stalker placements for corridor variant 2.
///
/// AREA_PLACEMENT_END terminates the list. Positions are signed world units,
/// yaw uses 4096 units per turn. Mode 1 retains entrance kind 0 (plain), with
/// tuning row 0. Spawned actors borrow these records while the room stays loaded.
static AreaPlacement _gShelterB1MainCorridorVariant2Placements[4] = {
    { 6, 0, 1, 0, 0, -4000, 0, 0, 0, 2, 0 },
    { 6, 0, 1, 0, 0, -8500, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { 6, 0, 1, 0, 0, -0x38A4, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two actor 100300 placements for corridor variant 3.
///
/// AREA_PLACEMENT_END terminates the list. Positions are signed world units,
/// yaw uses 4096 units per turn. Modes 0 and 1 select separate room patrol
/// routes, and variant 0 permits lock-on. Spawned actors borrow these records
/// while the room stays loaded.
static AreaPlacement _gShelterB1MainCorridorVariant3Placements[3] = {
    { 3, 0, 0, 0, 0, -3000, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { 3, 0, 1, 0, 0, -0x2710, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// One Knight GOLEM placement for corridor variant 7.
///
/// AREA_PLACEMENT_END terminates the list. Positions are signed world units,
/// yaw uses 4096 units per turn, and variant/mode/row select actor-specific
/// behavior. Spawned actors borrow these records while the room stays loaded.
static AreaPlacement _gShelterB1MainCorridorVariant7Placements[2] = {
    { 22, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// One Pawn and one Rook GOLEM placement for corridor variant 11.
///
/// AREA_PLACEMENT_END terminates the list. Positions are signed world units,
/// yaw uses 4096 units per turn. Mode 1 enables patrols; variants 6 and 4
/// set leg lengths of 6000 and 4000 units. Spawned actors borrow these records
/// while the room stays loaded.
static AreaPlacement _gShelterB1MainCorridorVariant11Placements[3] = {
    { 56, 6, 1, 0, 0, -2000, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { 57, 4, 1, -2000, 0, -0x2710, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB1MainCorridorAreaVariants[22] = {
    [1]  = { _gShelterB1MainCorridorVariant1Placements, _gShelterB1MainCorridorVariant1Resources },
    [2]  = { _gShelterB1MainCorridorVariant2Placements, _gShelterB1MainCorridorVariant2Resources },
    [3]  = { _gShelterB1MainCorridorVariant3Placements, _gShelterB1MainCorridorVariant3Resources },
    [7]  = { _gShelterB1MainCorridorVariant7Placements, _gShelterB1MainCorridorVariant7Resources },
    [11] = { _gShelterB1MainCorridorVariant11Placements, _gShelterB1MainCorridorVariant11Resources },
};

enum {
    SHELTER_B1_MAIN_CORRIDOR_FOOTSTEP_WALK_SCRIPT = 0x10000059,
    SHELTER_B1_MAIN_CORRIDOR_FOOTSTEP_RUN_SCRIPT  = 0x1000005B,
};

/// Floor cue-2 script bases in the loaded sound bank; scripted jumps use the walk pair.
static WorldCollisionFootstepSounds _gShelterB1MainCorridorFloorFootstepSounds = {
    .walk         = SHELTER_B1_MAIN_CORRIDOR_FOOTSTEP_WALK_SCRIPT,
    .run          = SHELTER_B1_MAIN_CORRIDOR_FOOTSTEP_RUN_SCRIPT,
    .scriptedJump = SHELTER_B1_MAIN_CORRIDOR_FOOTSTEP_WALK_SCRIPT,
};

/// Blocking, impact-enabled properties without footsteps, shared by classes 0 and 3..7.
static WorldCollisionSurfaceProperties _gShelterB1MainCorridorBlockingSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 1 passes probes and ignores weapon impacts while retaining contact pushback.
static WorldCollisionSurfaceProperties _gShelterB1MainCorridorPassThroughSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_PASS_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 2 floor properties: blocking, impact-enabled, with paired footstep scripts.
static WorldCollisionSurfaceProperties _gShelterB1MainCorridorFloorSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gShelterB1MainCorridorFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB1MainCorridorSurfacePropertiesByClass[8] = {
    &_gShelterB1MainCorridorBlockingSurfaceProperties,
    &_gShelterB1MainCorridorPassThroughSurfaceProperties,
    &_gShelterB1MainCorridorFloorSurfaceProperties,
    &_gShelterB1MainCorridorBlockingSurfaceProperties,
    &_gShelterB1MainCorridorBlockingSurfaceProperties,
    &_gShelterB1MainCorridorBlockingSurfaceProperties,
    &_gShelterB1MainCorridorBlockingSurfaceProperties,
    &_gShelterB1MainCorridorBlockingSurfaceProperties,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

RoomEventStartStorage gRoomEventActive = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Whether the latest staged-departure check attempted to spawn its controller.
///
/// Each check clears `eventStarted` (0 no, 1 attempted), including queries and
/// already-seen events. Spawn failure also leaves it set; it is not a task-
/// lifetime flag. The trailing initialized bytes have no recovered access,
/// and their role and grouping are unproven.
static RoomEventActiveBytes _gShelterB1MainCorridorStagedEventStart = { 0, { 7, 132, 0 } };

RoomEventReqStorage gRoomEventReq;

RoomLatchedEvent gRoomEventLatched;

static void _shelterB1MainCorridorMessageTaskIdle(Task* task);

#include "../../shared/room/room_event.inc.c"

#include "../../shared/room/room_event_staged_task.inc.c"

/// Tests and, in execute mode, latches a room-transition event.
///
/// Returns 1 when the nonzero event flag is already set, otherwise 2, including
/// queries. Every call clears the latest-start marker. Only `ROOM_EVENT_EXECUTE`
/// copies the complete eight-byte transition and twelve-byte event, sets a
/// nonzero flag to 1 and spawns the staged controller. Flag 0 stays eligible.
/// Both inputs are borrowed during this call; their copies and CAP/sound
/// resources must remain in the loaded room until the controller finishes.
static inline s32 _shelterB1MainCorridorStartEvent(const RoomEventMsg* transition, const RoomLatchedEvent* event)
{
    enum {
        ROOM_EVENT_NOT_STARTED  = 0,
        ROOM_EVENT_STARTED      = 1,
        ROOM_EVENT_FLAG_SEEN    = 1,
        ROOM_EVENT_ALREADY_SEEN = 1,
        ROOM_EVENT_ELIGIBLE     = 2
    };

    _gShelterB1MainCorridorStagedEventStart.eventStarted = ROOM_EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (transition->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *transition;
            gRoomEventLatched             = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_SEEN);
            }
            taskSpawnFromTable(&_gShelterB1MainCorridorStagedEventTaskDesc, 0, 0, 0);
            _gShelterB1MainCorridorStagedEventStart.eventStarted = ROOM_EVENT_STARTED;
        }
        return ROOM_EVENT_ELIGIBLE;
    }
    return ROOM_EVENT_ALREADY_SEEN;
}

/// Resolves corridor departures and selects their one-shot scenes or locked-door captions.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with complete borrowed request/reply
/// records, which may alias. Copies the request before resolving the reply's
/// room. Returns 1 for direct travel, 2 for a staged event (including eligible
/// queries), or 0 for the late-story elevator-hall caption. The locked transfer
/// door also returns 2, but starts only a refusal caption. Queries suppress
/// execution side effects. Deferred events copy the reply; their singleton
/// snapshots, this room's CAP/sound resources and the map overlay must remain
/// available. Do not start a second departure while its event task is live.
static s32 _shelterB1MainCorridorResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    /// Prepares a one-shot departure with the corridor's transit sound and no fade.
    ///
    /// eventRecord is a writable RoomLatchedEvent lvalue, evaluated repeatedly;
    /// transition is a borrowed RoomEventMsg*, evaluated once. Pass stable storage.
    /// Command and flag are evaluated once, stored as s32 and s16 respectively.
    /// Sets fade to zero and returns the event gate's result from the caller;
    /// the gate snapshots the records on execution and respects queryOnly.
#define SHELTER_B1_MAIN_CORRIDOR_START_DEPARTURE_SCENE(eventRecord, transition, capCommand, sceneFlag)       \
    {                                                                                                        \
        (eventRecord).capCmd   = (capCommand);                                                               \
        (eventRecord).stageSnd = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 1); \
        (eventRecord).flagId   = (sceneFlag);                                                                \
        (eventRecord).fade     = 0;                                                                          \
        return _shelterB1MainCorridorStartEvent((transition), &(eventRecord));                               \
    }

    enum { TRANSITION_REFUSED                 = 0,
           TRANSITION_DIRECT                  = 1,
           TRANSITION_HANDLED                 = 2,
           TRANSITION_REFUSAL_FLAG            = 2,
           CAP_COMMAND_LOCKED_DOOR            = 1,
           CAP_COMMAND_UNLOCK_ELEVATOR_HALL   = 2,
           CAP_COMMAND_DEPART_ARMORY          = 3,
           CAP_COMMAND_DEPART_QUARTERS        = 4,
           CAP_COMMAND_LATE_ELEVATOR_HALL     = 5,
           CAP_COMMAND_DEPART_STERILIZATION   = 6,
           CAP_COMMAND_DEPART_CONTROL_TUNNEL  = 7,
           CAP_COMMAND_DEPART_TRANSFER_TUNNEL = 8,
           ELEVATOR_HALL_BLOCKED_CHAPTER      = 6 };
    RoomEventReq     gateRequest;
    RoomLatchedEvent event;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B1_ARMORY) {
        SHELTER_B1_MAIN_CORRIDOR_START_DEPARTURE_SCENE(event, reply, CAP_COMMAND_DEPART_ARMORY, GAME_FLAG_B1_CORRIDOR_TO_ARMORY_SCENE);
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS) {
        SHELTER_B1_MAIN_CORRIDOR_START_DEPARTURE_SCENE(event, reply, CAP_COMMAND_DEPART_QUARTERS, GAME_FLAG_B1_CORRIDOR_TO_QUARTERS_SCENE);
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_STERILIZATION_ROOM) {
        SHELTER_B1_MAIN_CORRIDOR_START_DEPARTURE_SCENE(event, reply, CAP_COMMAND_DEPART_STERILIZATION, GAME_FLAG_B1_CORRIDOR_TO_STERILIZATION_SCENE);
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_ELEVATOR_HALL) {
        if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) >= ELEVATOR_HALL_BLOCKED_CHAPTER) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(CAP_COMMAND_LATE_ELEVATOR_HALL);
            }
            return TRANSITION_REFUSED;
        }
        gateRequest.capCmd        = CAP_COMMAND_UNLOCK_ELEVATOR_HALL;
        gateRequest.missingCapCmd = CAP_COMMAND_LOCKED_DOOR;
        gateRequest.firstSnd      = 0;
        gateRequest.secondSnd     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 1);
        gateRequest.flagId        = GAME_FLAG_B1_CORRIDOR_ELEVATOR_HALL_UNLOCKED;
        gateRequest.collectedBit  = 0;
        return _roomEventGate(&gateRequest, reply);
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL) {
        if (gameFlagGetNibble(GAME_FLAG_B1_TRANSFER_TUNNEL_DOOR_UNLOCKED) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, TRANSITION_REFUSAL_FLAG);
                capRunCommandWithTransition(CAP_COMMAND_LOCKED_DOOR);
            }
            return TRANSITION_HANDLED;
        }
        SHELTER_B1_MAIN_CORRIDOR_START_DEPARTURE_SCENE(event, reply, CAP_COMMAND_DEPART_TRANSFER_TUNNEL, GAME_FLAG_B1_CORRIDOR_TO_TRANSFER_SCENE);
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL) {
        SHELTER_B1_MAIN_CORRIDOR_START_DEPARTURE_SCENE(event, reply, CAP_COMMAND_DEPART_CONTROL_TUNNEL, GAME_FLAG_B1_CORRIDOR_TO_CONTROL_TUNNEL_SCENE);
    }
    return TRANSITION_DIRECT;
}
#undef SHELTER_B1_MAIN_CORRIDOR_START_DEPARTURE_SCENE

/// Refuses every key-item-use request, returning zero to the inventory.
///
/// The collected item ID and the zero second payload word are ignored.
static s32 _shelterB1MainCorridorRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores room commands and their payload words, always returning zero.
static s32 _shelterB1MainCorridorIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores the borrowed direction-action request and returns zero.
///
/// The request is neither read nor retained; its second payload word is zero.
static s32 _shelterB1MainCorridorIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Queues room-bank scripts 13 and 14 for the corresponding sound-message IDs.
///
/// Other sound IDs are ignored. All requests return zero even when sound
/// admission fails; the second payload word is unused.
static s32 _shelterB1MainCorridorHandleSoundMessage(Task* task, s32 messageId, s32 soundId, s32 unusedArg)
{
    enum { ROOM_SOUND_SCRIPT_13 = 13,
           ROOM_SOUND_SCRIPT_14 = 14 };

    if (soundId != ROOM_SOUND_SCRIPT_13) {
        if (soundId == ROOM_SOUND_SCRIPT_14) {
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, ROOM_SOUND_SCRIPT_14), 0, 0);
        }
    } else {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, ROOM_SOUND_SCRIPT_13), 0, 0);
    }
    return 0;
}

/// Registers the corridor's room-message receiver and advances it to idle.
///
/// Borrows the message table and publishes the live task in `GAME_TASK_SLOT_ROOM`.
static void _shelterB1MainCorridorInitRoomTask(Task* task)
{
    task->msgTable = _gShelterB1MainCorridorRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the room task available for messages without per-frame work or state changes.
static void _shelterB1MainCorridorMessageTaskIdle(Task* task)
{
}

/// Room-receiver states indexed by `Task::state`: 0 register, 1 idle, 2 release.
///
/// The dispatcher copies all three callbacks before selecting a state; requires
/// state 0..2 and the room overlay to remain loaded through dispatch.
static const TaskFuncTable3 _gShelterB1MainCorridorRoomTaskStates = {
    { _shelterB1MainCorridorInitRoomTask, _shelterB1MainCorridorMessageTaskIdle, taskKill },
};

void shelterB1MainCorridorRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelterB1MainCorridorRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Draws two additive flickering beams with the same radius scale, screen angle and tint.
///
/// `beamEndpoints` supplies four word-aligned world-space points, borrowed
/// only for this call: points 0..1 form the first beam and 2..3 the second.
/// Each end's pixel radius is `radiusScale * 64 / depth`, with depth camera Z / 4.
/// Each beam is omitted if its second end's depth is below 17; its first end's
/// depth is clamped to 16. `screenAngle` uses 4096 units per turn, zero down.
/// `colorFactors` packs a signed red multiplier in bits 8..15 and one-bit green
/// and blue multipliers in bits 4 and 0; all other bits are ignored. These
/// multiply an intensity of 32 or 40 on alternating frames; colour bytes wrap.
/// Requires current view matrices, an initialized scratch stack with room for
/// one `GlowPointPairScratch`, and a current ordering table and packet arena.
/// Each visible beam queues six Gouraud quads and their additive blend commands;
/// the arena stays live until GPU completion.
static inline void _shelterB1MainCorridorDrawBeamPair(const SVECTOR beamEndpoints[4], s16 radiusScale, s16 screenAngle, u16 colorFactors)
{
    _glowDrawBeam(&beamEndpoints[0], radiusScale, screenAngle, colorFactors);
    _glowDrawBeam(&beamEndpoints[2], radiusScale, screenAngle, colorFactors);
}

void shelterB1MainCorridorDrawViewLightsTask(Task* task)
{
    // Beam scales multiply 64/depth; flare scales multiply 39/depth, with depth camera Z/4.
    enum { LIGHTS_INITIALIZE,
           LIGHTS_DRAW,
           BEAM_RADIUS_SCALE    = 0x200,
           FLARE_RADIUS_SCALE   = 0x300,
           FLARE_TEXTURE_COLUMN = 1,
           BEAM_WHITE           = 0x111,
           BEAM_GREEN           = 0x010,
           BEAM_RED             = 0x100,
           VIEW_INDEX_MASK      = 0xFF };

    // Publish the effect entries before any room effect spawns use them.
    if (task->state == LIGHTS_INITIALIZE) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B1_MAIN_CORRIDOR_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B1_MAIN_CORRIDOR_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B1_MAIN_CORRIDOR_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B1_MAIN_CORRIDOR_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B1_MAIN_CORRIDOR_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_MAIN_CORRIDOR_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_MAIN_CORRIDOR_SPARK_BURST;
        task->state               = LIGHTS_DRAW;
    }

    // Each view selects fixed world-space endpoint pairs and flare centres.
    // Views 7 and 8 start at point 2; all offsets remain within the common table.
    switch (viewGetMappedIndex() & VIEW_INDEX_MASK) {
        case 2:
        case 3: {
            const SVECTOR* lightPoints;
            lightPoints = _gShelterB1MainCorridorEntryBeamPoints;
            _shelterB1MainCorridorDrawBeamPair(lightPoints, BEAM_RADIUS_SCALE, GLOW_HALF_TURN, BEAM_WHITE);
            break;
        }
        case 4: {
            const SVECTOR* lightPoints;
            lightPoints = _gShelterB1MainCorridorViewLightPoints;
            _glowDrawBeam(&lightPoints[0], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, BEAM_GREEN);
            _glowDrawBeam(&lightPoints[4], BEAM_RADIUS_SCALE, 0, BEAM_GREEN);
            glowDrawFlareClipped(&lightPoints[10], FLARE_TEXTURE_COLUMN, FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&lightPoints[11], FLARE_TEXTURE_COLUMN, FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&lightPoints[17], FLARE_TEXTURE_COLUMN, FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&lightPoints[18], FLARE_TEXTURE_COLUMN, FLARE_RADIUS_SCALE);
            break;
        }
        case 5:
            _glowDrawBeam(&_gShelterB1MainCorridorViewLightPoints[4], BEAM_RADIUS_SCALE, 0, BEAM_GREEN);
            break;
        case 6:
            _glowDrawBeam(_gShelterB1MainCorridorViewLightPoints, BEAM_RADIUS_SCALE, GLOW_HALF_TURN, BEAM_GREEN);
            break;
        case 7: {
            const SVECTOR* lightPoints;
            lightPoints = &_gShelterB1MainCorridorViewLightPoints[2];
            _glowDrawBeam(&lightPoints[0], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, BEAM_GREEN);
            _glowDrawBeam(&lightPoints[4], BEAM_RADIUS_SCALE, 0, BEAM_GREEN);
            _glowDrawBeam(&lightPoints[6], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, BEAM_RED);
            glowDrawFlareClipped(&lightPoints[13], FLARE_TEXTURE_COLUMN, FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&lightPoints[14], FLARE_TEXTURE_COLUMN, FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&lightPoints[20], FLARE_TEXTURE_COLUMN, FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&lightPoints[21], FLARE_TEXTURE_COLUMN, FLARE_RADIUS_SCALE);
            break;
        }
        case 8: {
            const SVECTOR* lightPoints;
            lightPoints = &_gShelterB1MainCorridorViewLightPoints[2];
            _glowDrawBeam(&lightPoints[0], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, BEAM_GREEN);
            _glowDrawBeam(&lightPoints[6], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, BEAM_RED);
            break;
        }
        case 9:
            _glowDrawBeam(&_gShelterB1MainCorridorViewLightPoints[6], BEAM_RADIUS_SCALE, 0, BEAM_GREEN);
            break;
        case 10:
            _glowDrawBeam(&_gShelterB1MainCorridorViewLightPoints[2], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, BEAM_GREEN);
            break;
    }
}

#include "../../shared/effects/glow_draw_beam.inc.c"

#include "../../shared/effects/glow_draw_flare_clipped.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void shelterB1MainCorridorRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void shelterB1MainCorridorRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB1MainCorridorRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void shelterB1MainCorridorRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB1MainCorridorRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB1MainCorridorRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB1MainCorridorRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
