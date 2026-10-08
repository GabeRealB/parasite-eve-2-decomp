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
#include "main/task_types.h"

#include "overlay.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"

/// Live emitters for the tunnel's views, in the shared data blob at the end of
/// the overlay. `D_..._8017EB48` doubles as case 2's three-entry run and case 6's
/// two-entry one, so both views share one base pointer.
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EAE8[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB08[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB28[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB48[];

extern WorldCollisionGrid D_neo_ark_eve_access_tunnel_8017F05C[1];

TaskDesc D_neo_ark_eve_access_tunnel_8017EA88 = { { { TASK_BODY_NONE, 32 } }, roomDepartureTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_eve_access_tunnel_8017EA94[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_eve_access_tunnel_8017DC6C },
    { ROOM_MESSAGE_USE_KEY_ITEM, neoArkEveAccessTunnelRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_eve_access_tunnel_8017DE1C },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_eve_access_tunnel_8017DD70 },
    { ROOM_MESSAGE_SOUND, neoArkEveAccessTunnelSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_neo_ark_eve_access_tunnel_8017EAC4[3] = {
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_eve_access_tunnel_8017D980, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_eve_access_tunnel_8017DB18, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, neoArkEveAccessTunnelRecordCapCompletionTask, { .value = 0 } },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017EAE8[4] = {
    { -3797, -2535, 1155, 0 },
    { -2796, -2535, 1155, 0 },
    { -3797, -2535, 928, 0 },
    { -2796, -2535, 928, 0 },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017EB08[4] = {
    { -1355, -2535, 2897, 0 },
    { -1355, -2535, 3897, 0 },
    { -1123, -2535, 2897, 0 },
    { -1123, -2535, 3897, 0 },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017EB28[4] = {
    { -1355, -2535, 5762, 0 },
    { -1355, -2535, 6762, 0 },
    { -1123, -2535, 5762, 0 },
    { -1123, -2535, 6762, 0 },
};

SVECTOR D_neo_ark_eve_access_tunnel_8017EB48[6] = {
    { -1874, -39, 1161, 0 },
    { -1874, -39, 667, 0 },
    { -267, -39, 1161, 0 },
    { -267, -39, 667, 0 },
    { -835, -39, 1836, 0 },
    { -1335, -39, 1836, 0 },
};

WorldCollisionRoomResources D_neo_ark_eve_access_tunnel_8017EB78[1] = {
    { D_neo_ark_eve_access_tunnel_8017F05C, D_neo_ark_eve_access_tunnel_801802EC, D_neo_ark_eve_access_tunnel_801804B4, D_neo_ark_eve_access_tunnel_80180720 },
};

WorldCoordRoomLighting D_neo_ark_eve_access_tunnel_8017EB88[1] = {
    { D_neo_ark_eve_access_tunnel_801802D4, NULL },
};

u8* D_neo_ark_eve_access_tunnel_8017EB90[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_eve_access_tunnel_8017EB94[1] = { 7 };

DirectionWarpEntry D_neo_ark_eve_access_tunnel_8017EB98[2] = {
    { { { .word = 0 }, -800, 0, 850 }, { 0, 0, 0, 0 }, { { .word = 0 }, -800, 0, 850 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 431 },
    { { { .word = 1024 }, -1167, 0, 4892 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -1167, 0, 4892 }, { 0, 0, 0, 0 }, 0x55080004, 0x55080003, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POWER_PLANT_2 },
};

static SVECTOR _gNeoArkEveAccessTunnelCollision01A9CNormals[19] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_normals.inc"
};

static SVECTOR _gNeoArkEveAccessTunnelCollision01A9CVerts[55] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_verts.inc"
};

static WorldCollisionGridFace _gNeoArkEveAccessTunnelCollision01A9CFaces[28] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_faces.inc"
};

static s16 _gNeoArkEveAccessTunnelCollision01A9CCells[82] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkEveAccessTunnelCollision01A9CCells[i])
static s16* _gNeoArkEveAccessTunnelCollision01A9CTable[4] = {
#include "assets/neo_ark_eve_access_tunnel_collision_01A9C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_eve_access_tunnel_8017F05C[1] = {
    { NULL, _gNeoArkEveAccessTunnelCollision01A9CNormals, _gNeoArkEveAccessTunnelCollision01A9CVerts, _gNeoArkEveAccessTunnelCollision01A9CFaces, _gNeoArkEveAccessTunnelCollision01A9CTable, 4452, 146, 2, 2, 4000, 28 },
};

ViewCamera D_neo_ark_eve_access_tunnel_8017F080[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 2144, 0x363E, -3758 } }, 333 },
    { { { { 1138, 0, -3934 }, { 45, 4095, 13 }, { 3934, -47, 1138 } }, { 4412, 1126, -236 } }, 235 },
    { { { { 1280, 0, 3890 }, { 317, 4082, -104 }, { -3877, 334, 1275 } }, { 98, 1391, -212 } }, 235 },
    { { { { 4057, 0, 557 }, { 80, 4052, -589 }, { -551, 594, 4014 } }, { 914, 1449, -144 } }, 225 },
    { { { { 4058, 0, 556 }, { 116, 4004, -852 }, { -543, 860, 3967 } }, { 919, 1520, -2227 } }, 225 },
    { { { { -3951, 0, -1077 }, { -517, 3593, 1896 }, { 945, 1965, -3467 } }, { 1340, 1600, -1902 } }, 257 },
    { { { { 1700, 0, 3726 }, { 1226, 3867, -559 }, { -3518, 1348, 1605 } }, { 667, 1600, -4048 } }, 257 },
};

SpriteBatch D_neo_ark_eve_access_tunnel_8017F17C[2] = {
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
    const GameLocationKey* location = &gGameSession->location.loc;
    SpriteView*            views    = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    SpriteBatch*           batches;
    s32                    slot = partSlot;
    s32                    destroyedState;

    // Updates both views of part 1; captures views and the shared batches temp.
    // The isHidden argument must be a side-effect-free byte value, used twice.
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
            _neoArkEveAccessTunnelDrawGlowPair(D_neo_ark_eve_access_tunnel_8017EB48, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB48[4], NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            break;
        case 3:
            _neoArkEveAccessTunnelDrawGlowPair(D_neo_ark_eve_access_tunnel_8017EAE8, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            break;
        case 4:
            _neoArkEveAccessTunnelDrawGlowPair(D_neo_ark_eve_access_tunnel_8017EB08, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            // This view also sees the next run of glows.
            /* fallthrough */
        case 5:
            _neoArkEveAccessTunnelDrawGlowPair(D_neo_ark_eve_access_tunnel_8017EB28, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            break;
        case 6:
            _neoArkEveAccessTunnelDrawGlowPair(D_neo_ark_eve_access_tunnel_8017EB48, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_RADIUS_SCALE, NEO_ARK_EVE_ACCESS_TUNNEL_GLOW_COLOR);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"
