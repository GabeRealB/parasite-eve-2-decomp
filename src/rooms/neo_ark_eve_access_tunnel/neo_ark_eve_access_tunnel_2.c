#include "rooms/neo_ark_eve_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_eve_access_tunnel_private.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/task_types.h"

#include "overlay.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_events.h"

static WorldCollisionGrid _gNeoArkEveAccessTunnelCollisionGrid;

enum {
    NEO_ARK_EVE_ACCESS_TUNNEL_GRID_CELLS_X           = 2,
    NEO_ARK_EVE_ACCESS_TUNNEL_GRID_CELLS_Z           = 2,
    NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_MAP_FLAG       = 0x1AF,
    NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_ARRIVAL_VIEW   = 2,
    NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_ARRIVAL_VIEW  = 5,
    NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_ARRIVAL_YAW   = 1024, // Quarter turn, 4096 angle units per turn
    NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_ARRIVAL_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL, 4),
};

TaskDesc gNeoArkEveAccessTunnelDepartureTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventDepartureTask, { .value = 0 } };

TaskMessageEntry gNeoArkEveAccessTunnelRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, neoArkEveAccessTunnelResolveTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, neoArkEveAccessTunnelRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, neoArkEveAccessTunnelHandleAction },
    { ROOM_MESSAGE_COMMAND, neoArkEveAccessTunnelHandleCommand },
    { ROOM_MESSAGE_SOUND, neoArkEveAccessTunnelSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gNeoArkEveAccessTunnelEventTasks[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, neoArkEveAccessTunnelShelterDepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, neoArkEveAccessTunnelElevatorDepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, neoArkEveAccessTunnelRecordCapCompletionTask, { .value = 0 } },
};

/// Two fixed capsule-glow segments visible in mapped view 3.
///
/// Signed world-coordinate endpoints pair [0, 1] and [2, 3]; borrowed during drawing.
static SVECTOR _gNeoArkEveAccessTunnelView3GlowEndpoints[4] = {
    { -3797, -2535, 1155, 0 },
    { -2796, -2535, 1155, 0 },
    { -3797, -2535, 928, 0 },
    { -2796, -2535, 928, 0 },
};

/// Two fixed capsule-glow segments selected by mapped view 4 before view 5's run.
///
/// Signed world-coordinate endpoints pair [0, 1] and [2, 3]; borrowed during drawing.
static SVECTOR _gNeoArkEveAccessTunnelView4GlowEndpoints[4] = {
    { -1355, -2535, 2897, 0 },
    { -1355, -2535, 3897, 0 },
    { -1123, -2535, 2897, 0 },
    { -1123, -2535, 3897, 0 },
};

/// Two fixed capsule-glow segments visible in mapped views 4 and 5.
///
/// Signed world-coordinate endpoints pair [0, 1] and [2, 3]; borrowed during drawing.
static SVECTOR _gNeoArkEveAccessTunnelView5GlowEndpoints[4] = {
    { -1355, -2535, 5762, 0 },
    { -1355, -2535, 6762, 0 },
    { -1123, -2535, 5762, 0 },
    { -1123, -2535, 6762, 0 },
};

/// Three fixed capsule-glow segments for mapped view 2; view 6 sees the first two.
///
/// Signed world-coordinate endpoints pair [0, 1], [2, 3] and [4, 5]. Borrowed
/// during drawing; the shared prefix is the same pair of segments in both views.
static SVECTOR _gNeoArkEveAccessTunnelViews2And6GlowEndpoints[6] = {
    { -1874, -39, 1161, 0 },
    { -1874, -39, 667, 0 },
    { -267, -39, 1161, 0 },
    { -267, -39, 667, 0 },
    { -835, -39, 1836, 0 },
    { -1335, -39, 1836, 0 },
};

WorldCollisionRoomResources gNeoArkEveAccessTunnelCollisionRooms[1] = {
    { &_gNeoArkEveAccessTunnelCollisionGrid, gNeoArkEveAccessTunnelViewBoundaryTriggers, gNeoArkEveAccessTunnelActionTriggers, gNeoArkEveAccessTunnelSightOccluders },
};

WorldCoordRoomLighting gNeoArkEveAccessTunnelRoomLighting[1] = {
    { &gNeoArkEveAccessTunnelRoomLights, NULL },
};

u8* gNeoArkEveAccessTunnelViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gNeoArkEveAccessTunnelViewCounts[1] = { NEO_ARK_EVE_ACCESS_TUNNEL_VIEW_COUNT };

DirectionWarpEntry gNeoArkEveAccessTunnelWarpEntries[NEO_ARK_EVE_ACCESS_TUNNEL_WARP_COUNT] = {
    {
        .player         = { { .word = 0 }, -800, 0, 850 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, -800, 0, 850 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_ARRIVAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_MAP_FLAG,
    },
    {
        .player         = { { .word = NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_ARRIVAL_YAW }, -1167, 0, 4892 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_ARRIVAL_YAW }, -1167, 0, 4892 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_ARRIVAL_SOUND,
        .departureSound = SOUND_NEO_ARK_EVE_TUNNEL_TO_ELEVATOR,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_ARRIVAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_POWER_PLANT_2,
    },
};

/// Room-space unit normals for the tunnel's collision faces, with 4096 per unit.
static SVECTOR _gNeoArkEveAccessTunnelCollisionGridNormals[19] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_normals.inc"
};

/// Collision vertex pool in signed room-coordinate units.
static SVECTOR _gNeoArkEveAccessTunnelCollisionGridVerts[55] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_verts.inc"
};

/// Twenty-seven quads and one triangle, with room surface classes 0, 1 and 2.
static WorldCollisionGridFace _gNeoArkEveAccessTunnelCollisionGridFaces[28] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_faces.inc"
};

/// Four terminated face-index lists and one retained halfword outside those lists.
///
/// The cell table selects starts 0, 28, 46 and 72; each list ends at
/// `WORLD_COLLISION_GRID_CELL_END`. Halfword 71 has no selected use.
static s16 _gNeoArkEveAccessTunnelCollisionGridCells[82] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gNeoArkEveAccessTunnelCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define NEO_ARK_EVE_ACCESS_TUNNEL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkEveAccessTunnelCollisionGridCells[(halfwordOffset)])
/// X-major 2-by-2 cell directory; every entry borrows one terminated face list.
static s16* _gNeoArkEveAccessTunnelCollisionGridTable[NEO_ARK_EVE_ACCESS_TUNNEL_GRID_CELLS_X * NEO_ARK_EVE_ACCESS_TUNNEL_GRID_CELLS_Z] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_table.inc"
};
#undef NEO_ARK_EVE_ACCESS_TUNNEL_CELL_FACE_LIST_AT_OFFSET

/// The tunnel's room collision mesh and 2-by-2 XZ face index.
///
/// Cells span 4000 world units from origin (-4452, -146) on X/Z. The descriptor
/// borrows 19 normals, 55 vertices, 28 faces and four cell lists from this
/// overlay. Room setup binds the mutable view coordinate; keep all pools
/// loaded while active. Surface classes are 0..2.
static WorldCollisionGrid _gNeoArkEveAccessTunnelCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkEveAccessTunnelCollisionGridNormals,
    .vertices    = _gNeoArkEveAccessTunnelCollisionGridVerts,
    .faces       = _gNeoArkEveAccessTunnelCollisionGridFaces,
    .cellFaceIds = _gNeoArkEveAccessTunnelCollisionGridTable,
    .xBias       = 4452,
    .zBias       = 146,
    .cellCountX  = NEO_ARK_EVE_ACCESS_TUNNEL_GRID_CELLS_X,
    .cellCountZ  = NEO_ARK_EVE_ACCESS_TUNNEL_GRID_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkEveAccessTunnelCollisionGridFaces),
};

ViewCamera gNeoArkEveAccessTunnelViewCameras[NEO_ARK_EVE_ACCESS_TUNNEL_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 2144, 0x363E, -3758 } }, 333 },
    { { { { 1138, 0, -3934 }, { 45, 4095, 13 }, { 3934, -47, 1138 } }, { 4412, 1126, -236 } }, 235 },
    { { { { 1280, 0, 3890 }, { 317, 4082, -104 }, { -3877, 334, 1275 } }, { 98, 1391, -212 } }, 235 },
    { { { { 4057, 0, 557 }, { 80, 4052, -589 }, { -551, 594, 4014 } }, { 914, 1449, -144 } }, 225 },
    { { { { 4058, 0, 556 }, { 116, 4004, -852 }, { -543, 860, 3967 } }, { 919, 1520, -2227 } }, 225 },
    { { { { -3951, 0, -1077 }, { -517, 3593, 1896 }, { 945, 1965, -3467 } }, { 1340, 1600, -1902 } }, 257 },
    { { { { 1700, 0, 3726 }, { 1226, 3867, -559 }, { -3518, 1348, 1605 } }, { 667, 1600, -4048 } }, 257 },
};

SpriteBatch gNeoArkEveAccessTunnelView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Draws two additive capsule glows for a tunnel light run.
///
/// `worldPoints` supplies four readable, word-aligned world endpoints, paired
/// as [0, 1] and [2, 3] and borrowed only during the call. Each capsule projects
/// through the current view and rejects negative GTE flags independently.
/// Accepted endpoints must have nonzero depth (camera Z / 4). The signed low
/// halfword of `radiusScale` gives each pixel radius as `radiusScale * 64 / depth`.
/// `packedColor` bits 8..11, 4..7 and 0..3 are RGB nibbles scaled by 16;
/// odd animation frames set bit 3 in each channel. Both capsules share these values.
///
/// Requires the composed view, initialized scratch stack and a current ordering
/// table and packet arena with room for up to twelve Gouraud quads and their
/// additive blend commands. Queued packets remain live until GPU completion.
static inline void _neoArkEveAccessTunnelDrawGlowPair(const SVECTOR worldPoints[4], s32 radiusScale, s32 packedColor)
{
    _glowDrawCapsule(&worldPoints[0], radiusScale, packedColor);
    _glowDrawCapsule(&worldPoints[2], radiusScale, packedColor);
}

void neoArkEveAccessTunnelSetPartDestroyedSprites(u8 partSlot, u8 destroyed)
{
    const GameLocationKey* location = &gGameSession->location;
    SpriteView*            views    = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    SpriteBatch*           batches;
    s32                    slot = partSlot;
    s32                    destroyedState;

    /// Sets the visibility of the scenery exposed by destruction of model part slot 1.
    ///
    /// Captures the current area's `views` and overwrites `batches`, leaving it
    /// at view slot 4. Requires batch 3 in view slot 3 and batch 2 in view slot 4.
    /// `isHidden` is evaluated twice and stored as a byte (true hidden, false
    /// visible); require a stable expression without side effects. Slot 0's
    /// separate scenery is outside this helper. No packets are allocated.
#define NEO_ARK_EVE_ACCESS_TUNNEL_SET_PART_1_SPRITES_HIDDEN(isHidden) \
    do {                                                              \
        batches           = views[3].batches;                         \
        batches[3].hidden = (isHidden);                               \
        batches           = views[4].batches;                         \
        batches[2].hidden = (isHidden);                               \
    } while (0)

    // A destroyed part exposes its scenery in each view that can show it.
    if (slot == 0) {
        destroyedState = destroyed;
        if (destroyedState == NEO_ARK_EVE_ACCESS_TUNNEL_PART_INTACT) {
            batches           = views[2].batches;
            batches[4].hidden = true;
            return;
        }
        if (destroyedState == NEO_ARK_EVE_ACCESS_TUNNEL_PART_DESTROYED) {
            batches           = views[2].batches;
            batches[4].hidden = false;
            return;
        }
    } else if (slot == 1) {
        destroyedState = destroyed;
        if (destroyedState == NEO_ARK_EVE_ACCESS_TUNNEL_PART_INTACT) {
            NEO_ARK_EVE_ACCESS_TUNNEL_SET_PART_1_SPRITES_HIDDEN(true);
            return;
        }
        if (destroyedState == NEO_ARK_EVE_ACCESS_TUNNEL_PART_DESTROYED) {
            NEO_ARK_EVE_ACCESS_TUNNEL_SET_PART_1_SPRITES_HIDDEN(false);
        }
    }
#undef NEO_ARK_EVE_ACCESS_TUNNEL_SET_PART_1_SPRITES_HIDDEN
}

void neoArkEveAccessTunnelDrawViewGlowsTask(Task* unusedTask)
{
    enum {
        NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE = 0x180, // Pixel radius = scale * 64 / (camera Z / 4)
        NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR        = 0x444, // Packed RGB nibbles; channels 64, or 72 on odd frames
    };
    u8 mappedViewIndex;

    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
            _neoArkEveAccessTunnelDrawGlowPair(_gNeoArkEveAccessTunnelViews2And6GlowEndpoints, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            _glowDrawCapsule(&_gNeoArkEveAccessTunnelViews2And6GlowEndpoints[4], NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            break;
        case 3:
            _neoArkEveAccessTunnelDrawGlowPair(_gNeoArkEveAccessTunnelView3GlowEndpoints, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            break;
        case 4:
            _neoArkEveAccessTunnelDrawGlowPair(_gNeoArkEveAccessTunnelView4GlowEndpoints, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            // This view also sees the next run of glows.
            /* fallthrough */
        case 5:
            _neoArkEveAccessTunnelDrawGlowPair(_gNeoArkEveAccessTunnelView5GlowEndpoints, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            break;
        case 6:
            _neoArkEveAccessTunnelDrawGlowPair(_gNeoArkEveAccessTunnelViews2And6GlowEndpoints, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            break;
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"
