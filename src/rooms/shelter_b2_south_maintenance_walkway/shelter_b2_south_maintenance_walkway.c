#include "rooms/shelter_b2_south_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_04400.h"
#include "actors/task_tables.h"

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

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
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
#include "main/sound_ids.h"

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

/// The walkway's message table, installed as the room task's `msgTable`.

/// Spawn payload of the task 0x31 the handler's event task may start.
extern RoomFadeStorage gRoomEventFade;

/// The request the event gate latched for its event task.
extern RoomEventReqStorage gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern RoomEventStartStorage gRoomEventActive;

/// The message and the event the walkway's handler latched for its event task,
/// and the flag saying its last call did so.
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static s32 _shelterB2SouthMaintenanceWalkwayResolveRoomEventMessage(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

enum { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, ROOM_EVENT_TASK_PRIORITY } }, roomEventTask, { .value = 0 } };

/// Bodyless priority-32 recipe for this room's staged departure task.
///
/// The gate reads index 0 synchronously with zero metadata and unused payloads.
/// The deferred task borrows the latched event and destination through CAP,
/// optional sound/fade and reload; retain them and the loaded room throughout.
static TaskDesc _gShelterB2SouthMaintenanceWalkwayStagedEventTaskDesc = { { { TASK_BODY_NONE, ROOM_EVENT_TASK_PRIORITY } }, roomEventStagedTask, { .value = 0 } };

/// Room receiver routes for departures, key-item refusal and ignored actions/commands.
///
/// Four live IDs followed by END/NULL; other IDs return zero. Payloads are
/// message-specific borrowed words consumed synchronously. The receiver borrows
/// this table and its callback code for the loaded room's lifetime.
static TaskMessageEntry _gShelterB2SouthMaintenanceWalkwayRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB2SouthMaintenanceWalkwayResolveRoomEventMessage },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM, _shelterB2SouthMaintenanceWalkwayIgnoreKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2SouthMaintenanceWalkwayIgnoreAction },
    { ROOM_MESSAGE_COMMAND, _shelterB2SouthMaintenanceWalkwayIgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-space endpoints of ten grey capsule glows and the red-disc centre.
///
/// Entries 0..19 are consecutive endpoint pairs, shared by mapped views 2..5;
/// entry 20 is the disc drawn in view 4. Coordinates use signed game units.
static SVECTOR _gShelterB2SouthMaintenanceWalkwayGlowAnchors[21] = {
    { 894, -196, 3102, 0 },
    { 894, -196, 2264, 0 },
    { 894, -196, 404, 0 },
    { 894, -196, -385, 0 },
    { 894, -196, -2032, 0 },
    { 894, -196, -2645, 0 },
    { 3102, -196, 3102, 0 },
    { 3102, -196, 2264, 0 },
    { 3102, -196, 404, 0 },
    { 3102, -196, -385, 0 },
    { 3102, -196, -2032, 0 },
    { 3102, -196, -2645, 0 },
    { 650, -196, -2895, 0 },
    { -31, -196, -2895, 0 },
    { 600, -196, -5113, 0 },
    { -45, -196, -5113, 0 },
    { -1556, -196, -2895, 0 },
    { -2498, -196, -2895, 0 },
    { -1556, -196, -5113, 0 },
    { -2498, -196, -5113, 0 },
    { 763, -1283, -2117, 0 },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

#include "../../shared/room/room_visual_effects_disc_data.inc.c"

u8* gShelterB2SouthMaintenanceWalkwayRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB2SouthMaintenanceWalkwayRoomViewCounts[1] = { 5 };

/// Optional map-marker nibble keys published by these arrival endpoints.
enum {
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WARP_1_MAP_FLAG = 457,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WARP_2_MAP_FLAG = 456,
};

DirectionWarpEntry gShelterB2SouthMaintenanceWalkwayWarpEntries[2] = {
    {
        .player         = { { .word = 1024 }, -2412, 0, -3946 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 1024 }, -2412, 0, -3946 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WARP_1_MAP_FLAG,
    },
    {
        .player         = { { .word = 2048 }, 2048, 0, 4600 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 2048 }, 2048, 0, 4600 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WARP_2_MAP_FLAG,
    },
};

/// Room-space unit normals for the collision grid's quad faces, with scale ONE.
static SVECTOR _gShelterB2SouthMaintenanceWalkwayCollisionGridNormals[14] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_normals.inc"
};

/// Room-space vertices indexed by the collision grid's faces, in signed game units.
static SVECTOR _gShelterB2SouthMaintenanceWalkwayCollisionGridVerts[38] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_verts.inc"
};

/// Eighteen collision quads referencing the grid's vertex/normal pools and classes 0 or 1.
static WorldCollisionGridFace _gShelterB2SouthMaintenanceWalkwayCollisionGridFaces[18] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_faces.inc"
};

/// Serialized signed face-index runs for the six cells, each ending with -1.
///
/// Retains all 82 halfwords from the asset, including bytes between live runs.
static s16 _gShelterB2SouthMaintenanceWalkwayCollisionGridCells[82] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB2SouthMaintenanceWalkwayCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..81
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB2SouthMaintenanceWalkwayCollisionGridCells[(halfwordOffset)])
/// X-major cell pointers, indexed as x * 3 + z for x in 0..1 and z in 0..2.
static s16* _gShelterB2SouthMaintenanceWalkwayCollisionGridTable[6] = {
#include "assets/shelter_b2_south_maintenance_walkway_collision_05428_table.inc"
};
#undef SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB2SouthMaintenanceWalkwayCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB2SouthMaintenanceWalkwayCollisionGridNormals,
    .vertices    = _gShelterB2SouthMaintenanceWalkwayCollisionGridVerts,
    .faces       = _gShelterB2SouthMaintenanceWalkwayCollisionGridFaces,
    .cellFaceIds = _gShelterB2SouthMaintenanceWalkwayCollisionGridTable,
    .xBias       = 2872,
    .zBias       = 5300,
    .cellCountX  = 2,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB2SouthMaintenanceWalkwayCollisionGridFaces),
};

ViewCamera gShelterB2SouthMaintenanceWalkwayViewCameras[5] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x35D2, 0 } }, 235 },
    { { { { 813, 0, 4014 }, { 1626, 3744, -329 }, { -3670, 1659, 743 } }, { -2032, 2295, 4776 } }, 235 },
    { { { { 950, 0, -3984 }, { -1563, 3767, -372 }, { 3664, 1607, 874 } }, { 2128, 2217, 4776 } }, 246 },
    { { { { 3928, 0, 1158 }, { 443, 3784, -1503 }, { -1070, 1567, 3629 } }, { -2962, 2210, 4830 } }, 235 },
    { { { { 3846, 0, 1407 }, { 529, 3795, -1446 }, { -1304, 1539, 3564 } }, { -2962, 2210, 182 } }, 235 },
};

/// View 1's empty sprite list, retaining decoded background-image strips.
static SpriteBatch _gShelterB2SouthMaintenanceWalkwayView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite list, retaining decoded background-image strips.
static SpriteBatch _gShelterB2SouthMaintenanceWalkwayView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Neutral RGB modulation for the view's colour-modulated source sprites.
enum { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR = 128 };

/// View 3's 64 screen-space texture rectangles and their scene-sorting depths.
///
/// Uses the 8-bit texture page at VRAM (960, 0) and palette row 255 at X 0.
/// The view's sole populated batch covers this complete array; it has no sentinel.
static SpriteSource _gShelterB2SouthMaintenanceWalkwayView3SpriteSources[64] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -72, 72, 0, { .fields = { 80, 120 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -80, 80, 0, { .fields = { 88, 176 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -88, 88, 0, { .fields = { 72, 80 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -64, 64, 0, { .fields = { 32, 192 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -56, 48, 0, { .fields = { 72, 120 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -72, -80, 0, { .fields = { 88, 152 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -72, -56, 0, { .fields = { 112, 160 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -72, -16, 0, { .fields = { 112, 120 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -64, -80, 0, { .fields = { 96, 152 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -64, -48, 0, { .fields = { 104, 120 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -64, -8, 0, { .fields = { 96, 184 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -56, -120, 0, { .fields = { 120, 208 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -56, -80, 0, { .fields = { 104, 160 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -56, -40, 0, { .fields = { 88, 32 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -56, -8, 0, { .fields = { 104, 200 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -80, -16, 0, { .fields = { 112, 200 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -80, -80, 0, { .fields = { 88, 224 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -80, -56, 0, { .fields = { 120, 168 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -88, -80, 0, { .fields = { 88, 64 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -88, -56, 0, { .fields = { 120, 120 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -88, -8, 0, { .fields = { 88, 88 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -96, -64, 0, { .fields = { 88, 0 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -96, -32, 0, { .fields = { 96, 216 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -104, -48, 0, { .fields = { 80, 0 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -56, 32, 1019, { .fields = { 72, 64 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -64, 24, 974, { .fields = { 72, 16 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -72, 24, 977, { .fields = { 112, 240 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -72, 40, 968, { .fields = { 88, 248 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, 48, 1010, { .fields = { 24, 32 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, 56, 992, { .fields = { 32, 168 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -120, 48, 937, { .fields = { 48, 224 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -160, 48, 766, { .fields = { 48, 240 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 64, 750, { .fields = { 8, 184 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -120, 64, 966, { .fields = { 0, 152 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -120, 72, 928, { .fields = { 8, 144 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 72, 750, { .fields = { 40, 216 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 80, 750, { .fields = { 16, 160 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -120, 80, 839, { .fields = { 8, 176 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -128, 88, 850, { .fields = { 40, 48 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, 88, 750, { .fields = { 48, 32 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -120, 40, 934, { .fields = { 0, 224 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 40, 750, { .fields = { 8, 40 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -80, 24, 944, { .fields = { 104, 240 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -88, 16, 914, { .fields = { 80, 48 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -96, 0, 896, { .fields = { 80, 24 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -128, 24, 966, { .fields = { 32, 64 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, 24, 750, { .fields = { 40, 80 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, -160, -40, 750, { .fields = { 96, 0 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -160, -96, 750, { .fields = { 96, 64 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -160, -120, 750, { .fields = { 56, 152 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, -128, -120, 631, { .fields = { 48, 176 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -88, -120, 794, { .fields = { 56, 96 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -88, -96, 749, { .fields = { 40, 200 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -128, -96, 805, { .fields = { 16, 96 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -128, -80, 737, { .fields = { 40, 0 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -128, -64, 753, { .fields = { 24, 16 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -112, -48, 805, { .fields = { 80, 72 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -112, -24, 842, { .fields = { 56, 120 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -104, -8, 863, { .fields = { 72, 200 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -112, 8, 836, { .fields = { 40, 128 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -128, 8, 769, { .fields = { 40, 112 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -128, -8, 788, { .fields = { 56, 136 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -128, -24, 765, { .fields = { 56, 16 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -128, -48, 747, { .fields = { 80, 200 } }, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 3's background-preserving header, full sprite range and end marker.
static SpriteBatch _gShelterB2SouthMaintenanceWalkwayView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB2SouthMaintenanceWalkwayView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's empty sprite list, retaining decoded background-image strips.
static SpriteBatch _gShelterB2SouthMaintenanceWalkwayView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's empty sprite list, retaining decoded background-image strips.
static SpriteBatch _gShelterB2SouthMaintenanceWalkwayView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB2SouthMaintenanceWalkwaySpriteViews[5] = {
    { { .empty = _gShelterB2SouthMaintenanceWalkwayView1SpriteBatches }, _gShelterB2SouthMaintenanceWalkwayView1SpriteBatches, NULL },
    { { .empty = _gShelterB2SouthMaintenanceWalkwayView2SpriteBatches }, _gShelterB2SouthMaintenanceWalkwayView2SpriteBatches, NULL },
    { { .elements = _gShelterB2SouthMaintenanceWalkwayView3SpriteSources }, _gShelterB2SouthMaintenanceWalkwayView3SpriteBatches, NULL },
    { { .empty = _gShelterB2SouthMaintenanceWalkwayView4SpriteBatches }, _gShelterB2SouthMaintenanceWalkwayView4SpriteBatches, NULL },
    { { .empty = _gShelterB2SouthMaintenanceWalkwayView5SpriteBatches }, _gShelterB2SouthMaintenanceWalkwayView5SpriteBatches, NULL },
};

/// Six all-view room point lights, with world positions and distance falloff.
///
/// The lighting runtime binds their parents, composes transforms and overwrites
/// attenuation; their storage remains writable for the loaded room's lifetime.
static WorldCoordPointLight _gShelterB2SouthMaintenanceWalkwayPointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2022, -223, -3929 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2348, 2757, 2798 }, { 0, 0 } }, 1852, 2632 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 367, -223, -4032 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2570, 3039, 3058 }, { 0, 0 } }, 1540, 3660 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2187, -303, -4148 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1365, 1697, 1877 }, { 0, 0 } }, 1821, 2131 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1972, -223, 2426 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2007, 2638, 2897 }, { 0, 0 } }, 1742, 2801 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1986, -223, -1184 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2509, 3039, 3079 }, { 0, 0 } }, 2121, 3602 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2025, -223, -2356 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1847, 2297, 2416 }, { 0, 0 } }, 1500, 1800 },
};

WorldCoordRoomLights gShelterB2SouthMaintenanceWalkwayRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB2SouthMaintenanceWalkwayPointLights),
    .pointLights           = _gShelterB2SouthMaintenanceWalkwayPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB2SouthMaintenanceWalkwayViewBoundaryTriggers[6] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2000, -1361, 1664, 0 }, .vertices = { { -1321, -1936, -474, 0 }, { 1306, -1936, 456, 0 }, { -1321, 1937, -474, 0 }, { 1306, 1937, 456, 0 } }, .normal = { 1372, 0, -3878, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2374, .control = 0, .parameter0 = 4, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2000, -1408, 1537, 0 }, .vertices = { { 1224, -1936, 369, 0 }, { -1229, -1936, -377, 0 }, { 1224, 1937, 369, 0 }, { -1229, 1937, -377, 0 } }, .normal = { -1198, 0, 3932, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2318, .control = 0, .parameter0 = 5, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2224, -1408, -2769, 0 }, .vertices = { { 1360, -1936, 208, 0 }, { -1360, -1936, -208, 0 }, { 1360, 1937, 208, 0 }, { -1360, 1937, -208, 0 } }, .normal = { -622, 0, 4052, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2374, .control = 0, .parameter0 = 4, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2176, -1281, -2657, 0 }, .vertices = { { -1392, -1936, -224, 0 }, { 1392, -1936, 224, 0 }, { -1392, 1937, -224, 0 }, { 1392, 1937, 224, 0 } }, .normal = { 651, 0, -4054, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2387, .control = 0, .parameter0 = 3, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 224, -1440, -4128, 0 }, .vertices = { { 11, -1936, 1210, 0 }, { -21, -1936, -1221, 0 }, { 11, 1937, 1210, 0 }, { -21, 1937, -1221, 0 } }, .normal = { -4112, 0, 53, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2275, .control = 0, .parameter0 = 2, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 96, -1408, -4160, 0 }, .vertices = { { -20, -1936, -1316, 0 }, { 12, -1936, 1308, 0 }, { -20, 1937, -1316, 0 }, { 12, 1937, 1308, 0 } }, .normal = { 4115, 0, -52, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2332, .control = 0, .parameter0 = 3, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// Action payloads: packed departure/arrival slots and room-local selectors.
enum {
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_TRIGGER_WARP_TO_B2_ELEVATOR_HALL  = (1 << 4) | 3,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_TRIGGER_WARP_TO_B2_OPERATING_ROOM = (2 << 4) | 1,
};

WorldCollisionTrigger gShelterB2SouthMaintenanceWalkwayActionTriggers[2] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { -2528, -48, -3968, 0 }, .vertices = { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, .normal = { 0, 4102, 0, 0 }, .facingNormal = { 4096, 0, 0, 0 }, .radius = 1159, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_SHELTER_B2_ELEVATOR_HALL, .parameter1 = SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_TRIGGER_WARP_TO_B2_ELEVATOR_HALL, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 1952, -48, 4448, 0 }, .vertices = { { 1024, 0, -544, 0 }, { 1024, 0, 544, 0 }, { -1024, 0, -544, 0 }, { -1024, 0, 544, 0 } }, .normal = { 0, 4102, 0, 0 }, .facingNormal = { 0, 0, -4096, 0 }, .radius = 1159, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_SHELTER_B2_OPERATING_ROOM, .parameter1 = SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_TRIGGER_WARP_TO_B2_OPERATING_ROOM, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// Placement/resource keys used by the walkway's encounter variants.
///
/// Each key selects a recipe in that variant's resource list; file numbers
/// remain independent catalog selections even where they equal the key.
enum {
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER            = 21,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_ROOK_GOLEM         = 23,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA             = 24,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_MAD_CHASER         = 44,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_BRUTE_MOSSBACK     = 49,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_MAGGOT_CATERPILLAR = 55,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_CREEPING_STRANGER  = 72,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_SKULL_STALKER      = 73
};

/// Actor-specific placement options selected by the walkway's encounter records.
///
/// Watcher variants select weapon rows with no patrol-distance tens digit.
/// Amoeba, Caterpillar, Skull Stalker and Rook options occupy the mode field.
enum {
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_BLUE_BEAM           = 2,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_YELLOW_BEAM         = 3,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AMOEBA_STANDARD             = 0,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AMOEBA_STRONG               = 1,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_CATERPILLAR_AMBUSH_FOLLOWER = 3,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SKULL_STALKER_ALTERNATE     = 1,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ROOK_GOLEM_PATROL           = 1
};

/// Resource recipes for variant 1's Watchers and Amoebas, ending with `AREA_PLACEMENT_END`.
static AreaResource _gShelterB2SouthMaintenanceWalkwayVariant1Resources[3] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 24, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Resource recipes for variant 2's Brute Mossbacks and Creeping Stranger.
static AreaResource _gShelterB2SouthMaintenanceWalkwayVariant2Resources[3] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_BRUTE_MOSSBACK, 49, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor101100Tasks },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_CREEPING_STRANGER, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor207200CreepingStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Resource recipes for variant 3's Mad Chaser, Creeping Strangers and Skull Stalker.
static AreaResource _gShelterB2SouthMaintenanceWalkwayVariant3Resources[4] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_MAD_CHASER, 44, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_04400_MAD_CHASER_TASK_ORDINARY, { 0, 0 }, gActor04400MadChaserTaskDescs },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_CREEPING_STRANGER, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor207200CreepingStrangerTaskDesc },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_SKULL_STALKER, 73, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor207200SkullStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Resource recipes for variant 11's Amoeba and Maggot Caterpillars.
static AreaResource _gShelterB2SouthMaintenanceWalkwayVariant11Resources[3] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102400Tasks },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_MAGGOT_CATERPILLAR, 55, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor205500MaggotCaterpillarTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Resource recipes for variant 21's Watchers and Beam Sword Rook GOLEM.
static AreaResource _gShelterB2SouthMaintenanceWalkwayVariant21Resources[3] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_ROOK_GOLEM, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202300RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Four Watcher and four Amoeba placements for encounter variant 1.
///
/// Positions use world game units and yaw uses 4096 units per turn.
/// The final record is `AREA_PLACEMENT_END`; actors borrow live records while spawned.
static AreaPlacement _gShelterB2SouthMaintenanceWalkwayVariant1Placements[9] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_YELLOW_BEAM, 0, 950, -2000, 2500, 1024, 0, 0, 2, 1 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_YELLOW_BEAM, 0, 950, -2000, -500, 1024, 0, 0, 2, 7 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_BLUE_BEAM, 0, 1400, -2000, 5000, 2048, 0, 0, 2, 5 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_BLUE_BEAM, 0, 2600, -2000, 5000, 2048, 0, 0, 2, 5 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AMOEBA_STANDARD, 2000, 0, -4000, 3700, 0, 2, 4, 0 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AMOEBA_STANDARD, 2400, 0, -2900, 3200, 0, 2, 4, 0 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AMOEBA_STANDARD, 2400, 0, -850, 3600, 0, 2, 4, 0 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AMOEBA_STANDARD, 1450, 0, -2500, 1700, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// One Creeping Stranger and two Brute Mossback placements for encounter variant 2.
///
/// Positions use world game units, yaw uses 4096 units per turn, and the last
/// record ends the list. Actors borrow the records while spawned.
static AreaPlacement _gShelterB2SouthMaintenanceWalkwayVariant2Placements[4] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_CREEPING_STRANGER, 0, 0, 2150, 0, 2050, 3700, 0, 0, 2, 0 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_BRUTE_MOSSBACK, 0, 0, 1600, 0, -3600, 3500, 0, 2, 4, 0 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_BRUTE_MOSSBACK, 0, 0, 1650, 0, 0, 1000, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Mad Chaser, two Creeping Stranger and Skull Stalker placements for variant 3.
///
/// Positions use world game units, yaw uses 4096 units per turn, and the last
/// record ends the list. Actors borrow the records while spawned.
static AreaPlacement _gShelterB2SouthMaintenanceWalkwayVariant3Placements[5] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_MAD_CHASER, 0, 0, 2050, 0, 720, 3584, 0, 0, 2, 0 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_CREEPING_STRANGER, 0, 0, 2000, 0, -1400, 2048, 0, 2, 4, 0 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_CREEPING_STRANGER, 0, 0, 850, 0, -4200, 3072, 0, 2, 4, 0 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_SKULL_STALKER, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SKULL_STALKER_ALTERNATE, 1500, 0, -230, 1000, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// One Amoeba and four Maggot Caterpillar placements for encounter variant 11.
///
/// Positions use world game units, yaw uses 4096 units per turn, and the last
/// record ends the list. Actors borrow the records while spawned.
static AreaPlacement _gShelterB2SouthMaintenanceWalkwayVariant11Placements[6] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AMOEBA_STRONG, 2150, 0, 1750, 2000, 0, 0, 2, 0 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_MAGGOT_CATERPILLAR, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_CATERPILLAR_AMBUSH_FOLLOWER, 2550, -2700, 550, 3360, 0, 2, 4, 5 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_MAGGOT_CATERPILLAR, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_CATERPILLAR_AMBUSH_FOLLOWER, 2050, -2700, -1050, 3900, 0, 2, 4, 3 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_MAGGOT_CATERPILLAR, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_CATERPILLAR_AMBUSH_FOLLOWER, 1600, -2700, -2200, 200, 0, 2, 4, 4 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_MAGGOT_CATERPILLAR, 0, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_CATERPILLAR_AMBUSH_FOLLOWER, 1000, -2700, -4100, 200, 0, 2, 4, 5 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Four Watchers and one Beam Sword Rook GOLEM placement for encounter variant 21.
///
/// Positions use world game units, yaw uses 4096 units per turn, and the last
/// record ends the list. Actors borrow the records while spawned.
static AreaPlacement _gShelterB2SouthMaintenanceWalkwayVariant21Placements[6] = {
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_YELLOW_BEAM, 0, 800, -2200, 2500, 1024, 0, 0, 2, 1 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_YELLOW_BEAM, 0, 800, -2200, 0, 1024, 0, 0, 2, 5 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_BLUE_BEAM, 0, 1400, -2000, 5000, 2048, 0, 0, 2, 5 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_WATCHER_BLUE_BEAM, 0, 2600, -2000, 5000, 2048, 0, 0, 2, 5 },
    { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ENTRY_ROOK_GOLEM, 7, SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ROOK_GOLEM_PATROL, 2000, 0, -3000, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB2SouthMaintenanceWalkwayAreaVariants[SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = _gShelterB2SouthMaintenanceWalkwayVariant1Placements, .resources = _gShelterB2SouthMaintenanceWalkwayVariant1Resources },
    [2]                    = { .placements = _gShelterB2SouthMaintenanceWalkwayVariant2Placements, .resources = _gShelterB2SouthMaintenanceWalkwayVariant2Resources },
    [3]                    = { .placements = _gShelterB2SouthMaintenanceWalkwayVariant3Placements, .resources = _gShelterB2SouthMaintenanceWalkwayVariant3Resources },
    [11]                   = { .placements = _gShelterB2SouthMaintenanceWalkwayVariant11Placements, .resources = _gShelterB2SouthMaintenanceWalkwayVariant11Resources },
    [21]                   = { .placements = _gShelterB2SouthMaintenanceWalkwayVariant21Placements, .resources = _gShelterB2SouthMaintenanceWalkwayVariant21Resources },
};

WorldCollisionOccluder gShelterB2SouthMaintenanceWalkwaySightOccluders[1] = {
    { .next = NULL, .prevLink = NULL, .origin = { -1616, -1280, -544, 0 }, .vertices = { { -2384, 2304, 2240, 0 }, { 2384, 2304, -2240, 0 }, { -2384, -2304, 2240, 0 }, { 2384, -2304, -2240, 0 } }, .normal = { 2809, 0, 2989, 0 }, .radius = 3998, .flags = 1 | WORLD_COLLISION_OCCLUDER_LAST, .unknown_3B = 0 },
};

enum {
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FOOTSTEP_WALK_CUE_2 = 0x5D,
    SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FOOTSTEP_RUN_CUE_2  = 0x5F
};

/// Surface class 1's paired walking/running cues in the loaded type-1 sound bank.
///
/// Scripted jumps use the walking pair. Cue 1 adds one to the stored cue-2 base;
/// companion playback adds 100 after selecting the pair.
static WorldCollisionFootstepSounds _gShelterB2SouthMaintenanceWalkwaySurfaceClass1FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FOOTSTEP_WALK_CUE_2,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FOOTSTEP_RUN_CUE_2,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FOOTSTEP_WALK_CUE_2,
};

/// Blocking, impact-enabled, pushback surface without footstep cues, for classes 0 and 2..7.
static WorldCollisionSurfaceProperties _gShelterB2SouthMaintenanceWalkwayDefaultSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 1's blocking, impact-enabled, pushback surface with walking/running footstep cues.
static WorldCollisionSurfaceProperties _gShelterB2SouthMaintenanceWalkwaySurfaceClass1Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gShelterB2SouthMaintenanceWalkwaySurfaceClass1FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB2SouthMaintenanceWalkwaySurfacePropertiesByClass[8] = {
    &_gShelterB2SouthMaintenanceWalkwayDefaultSurfaceProperties,
    &_gShelterB2SouthMaintenanceWalkwaySurfaceClass1Properties,
    &_gShelterB2SouthMaintenanceWalkwayDefaultSurfaceProperties,
    &_gShelterB2SouthMaintenanceWalkwayDefaultSurfaceProperties,
    &_gShelterB2SouthMaintenanceWalkwayDefaultSurfaceProperties,
    &_gShelterB2SouthMaintenanceWalkwayDefaultSurfaceProperties,
    &_gShelterB2SouthMaintenanceWalkwayDefaultSurfaceProperties,
    &_gShelterB2SouthMaintenanceWalkwayDefaultSurfaceProperties,
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

/// Four-byte storage for the latest staged-event gate's start indication.
///
/// Every call clears eventStarted, including queries; execution raises it
/// after the unchecked spawn. The task does not update it. The three nonzero
/// trailing bytes have no recovered access or established role.
static RoomEventActiveBytes _gShelterB2SouthMaintenanceWalkwayStagedEventStart = {
    .eventStarted = false,
    .unknown      = { 7, 123, 189 },
};

RoomEventReqStorage gRoomEventReq;

RoomLatchedEvent gRoomEventLatched;

#include "../../shared/room/room_event.inc.c"

/// Latches a first-use departure event, or tests its eligibility for a query.
///
/// Borrows complete eight-byte `destination` and twelve-byte `event` records
/// for this call. A zero flag ID stays eligible; other IDs must name valid
/// game-flag nibbles. Returns 1 for an already latched flag, allowing ordinary
/// departure, or 2 for an eligible staged departure. Execution copies both
/// records into room-owned storage, sets a nonzero flag's nibble to 1 and
/// attempts to spawn the staged task. Every call clears the event-started
/// byte; eligible execution raises it even if spawning fails. The room
/// overlay must remain loaded until the staged task finishes.
static inline s32 _shelterB2SouthMaintenanceWalkwayStartEvent(const RoomEventMsg* destination, const RoomLatchedEvent* event)
{
    enum { EVENT_NO_FLAG            = 0,
           EVENT_FLAG_LATCHED       = 1,
           EVENT_ORDINARY_DEPARTURE = 1,
           EVENT_STAGED_DEPARTURE   = 2 };

    _gShelterB2SouthMaintenanceWalkwayStagedEventStart.eventStarted = false;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == EVENT_NO_FLAG) {
        if (destination->queryOnly == ROOM_EVENT_EXECUTE) {
            // Own the caller's records before the staged task can consume them.
            ROOM_EVENT_STAGED_DESTINATION = *destination;
            gRoomEventLatched             = *event;
            if (event->flagId != EVENT_NO_FLAG) {
                gameFlagSetNibble(event->flagId, EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&_gShelterB2SouthMaintenanceWalkwayStagedEventTaskDesc, 0, 0, 0);
            _gShelterB2SouthMaintenanceWalkwayStagedEventStart.eventStarted = true;
        }
        return EVENT_STAGED_DEPARTURE;
    }
    return EVENT_ORDINARY_DEPARTURE;
}

#include "../../shared/room/room_event_staged_task.inc.c"

/// Resolves operating-room and elevator-hall departures through their door events.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with complete eight-byte request/reply
/// records, which may alias. Copies the request and resolves the reply's room
/// before testing its destination area. The operating-room gate requires no
/// collected item; the elevator scene latches once. Returns 1 for direct
/// departure or 2 for an eligible deferred event. Queries preserve the copied
/// room and suppress event effects, but clear the selected gate's latest-start
/// indication. Executing events copy the reply into room-owned state; neither
/// pointer is retained. Keep this overlay, map and CAP/sound resources loaded
/// until the deferred event ends. Receiver and message ID are unused.
static s32 _shelterB2SouthMaintenanceWalkwayResolveRoomEventMessage(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DEPARTURE_DIRECT           = 1,
           NO_COLLECTION_REQUIRED     = 0,
           EVENT_NO_FADE              = 0,
           CAP_OPERATING_DOOR         = 1,
           CAP_OPERATING_MISSING_ITEM = 1,
           CAP_ELEVATOR_DEPARTURE     = 2,
           SOUND_OPERATING_FIRST      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 5),
           SOUND_DEPARTURE            = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 1) };

    RoomEventReq     gateRequest;
    RoomLatchedEvent event;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B2_OPERATING_ROOM) {
        gateRequest.capCmd        = CAP_OPERATING_DOOR;
        gateRequest.missingCapCmd = CAP_OPERATING_MISSING_ITEM;
        gateRequest.firstSnd      = SOUND_OPERATING_FIRST;
        gateRequest.secondSnd     = SOUND_DEPARTURE;
        gateRequest.flagId        = GAME_FLAG_OPERATING_ROOM_SOUTH_DOOR_UNLOCKED;
        gateRequest.collectedBit  = NO_COLLECTION_REQUIRED;
        return _roomEventGate(&gateRequest, reply);
    }
    if (request->areaId != GAME_AREA_SHELTER_B2_ELEVATOR_HALL) {
        return DEPARTURE_DIRECT;
    }
    event.capCmd   = CAP_ELEVATOR_DEPARTURE;
    event.stageSnd = SOUND_DEPARTURE;
    event.flagId   = GAME_FLAG_B2_SOUTH_WALKWAY_TO_ELEVATOR_SCENE;
    event.fade     = EVENT_NO_FADE;
    return _shelterB2SouthMaintenanceWalkwayStartEvent(reply, &event);
}

/// Refuses every key-item use request, returning zero without consuming the item.
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores every room command and returns zero; neither payload word is read.
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores every trigger action and returns zero without reading the borrowed request.
static s32 _shelterB2SouthMaintenanceWalkwayIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Installs the room's message handlers and publishes its task for room requests.
///
/// Runs at state 0 and advances to idle state 1. The task and this overlay's
/// message table must remain live while `GAME_TASK_SLOT_ROOM` can receive messages.
static void _shelterB2SouthMaintenanceWalkwayInitializeRoomTask(Task* task)
{
    task->msgTable = _gShelterB2SouthMaintenanceWalkwayRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the room task available for messages without per-frame work.
static void _shelterB2SouthMaintenanceWalkwayIdleRoomTask(Task* task)
{
}

/// Room-controller callbacks indexed by state: 0 initialize, 1 idle, 2 release.
///
/// Dispatch copies all three pointers; state must be 0..2. There is no
/// terminator or bounds check; keep the room's callback code loaded.
static const TaskFuncTable3 _gShelterB2SouthMaintenanceWalkwayRoomTaskStates = {
    { _shelterB2SouthMaintenanceWalkwayInitializeRoomTask, _shelterB2SouthMaintenanceWalkwayIdleRoomTask, taskKill },
};

void shelterB2SouthMaintenanceWalkwayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelterB2SouthMaintenanceWalkwayRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Installs this room's packed bank-6 task IDs for the six shared combat effects.
///
/// Call after room-effect controller initialization and before spawning these
/// effects. The selectors persist until cleared or replaced; this overlay must
/// remain loaded while the selected effect tasks run.
static inline void _shelterB2SouthMaintenanceWalkwaySelectEffectTasks(void)
{
    gRoomEffectFlashId        = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_SPARK_BURST;
    gRoomEffectGlowDiscId     = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_GLOW_DISC;
    gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_FLYING_SPARK;
    gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_ORANGE_BURST_2;
}

void shelterB2SouthMaintenanceWalkwayDrawGlowsTask(Task* task)
{
    enum { GLOW_INITIALIZE,
           GLOW_DRAW,
           GLOW_RADIUS_SCALE = 0x200,
           GLOW_QUARTER_TURN = 0x400,
           GLOW_HALF_TURN    = 0x800,
           GLOW_DISC_ANCHOR  = 20 };

    u8 viewIndex;

    if (task->state == GLOW_INITIALIZE) {
        _shelterB2SouthMaintenanceWalkwaySelectEffectTasks();
        task->state = GLOW_DRAW;
    }

    // Each view selects visible pairs of world-space anchors; view 4 adds the disc.
    viewIndex = viewGetMappedIndex();
    switch (viewIndex) {
        case 2:
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[12], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[14], GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[16], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[18], GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        case 3:
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[10], GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[12], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[14], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            break;
        case 4:
            glowDrawRedDisc(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[GLOW_DISC_ANCHOR], GLOW_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[2], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[4], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[6], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[8], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[10], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[12], GLOW_RADIUS_SCALE, 0);
            break;
        case 5:
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB2SouthMaintenanceWalkwayGlowAnchors[6], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            break;
    }
}

#include "../../shared/effects/glow_draw_cone.inc.c"

#include "../../shared/effects/glow_draw_red_disc.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"

#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

    void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask(Task * task)
    {
        _roomVisualEffectsGlowDiscTask(task);
    }

    void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask(Task * task)
    {
        _roomVisualEffectsFlyingSparkTask(task);
    }

#include "../../shared/room/room_visual_effects_burst.inc.c"

    void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask(Task * task)
    {
        _roomVisualEffectsFlyingOrangeBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
