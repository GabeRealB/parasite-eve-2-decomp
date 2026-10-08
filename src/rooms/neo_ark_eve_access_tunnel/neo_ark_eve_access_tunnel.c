#include "rooms/neo_ark_eve_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "neo_ark_eve_access_tunnel_private.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"

static s32 _roomVariantResolveShelter(RoomEventMsg* request, RoomEventMsg* reply);

/// Scene id byte; the tunnel stamps 0x18 when it hands the save location off.

/// Set when the tunnel's save is written to the memory card.

/// Staging save location the room commits when the tunnel's save is taken:
/// `field_2` / `field_4` / `field_1` hold what `neoArkEveAccessTunnelElevatorDepartureTask`
/// later copies into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area` / `warp` / `room`.
extern RoomEventMsg D_neo_ark_eve_access_tunnel_801807A0;

/// The staged event descriptor, read by the task spawned above.
extern RoomDeparture gRoomDeparture;

static void _neoArkEveAccessTunnelInitializeRoom(Task* task);
static void _neoArkEveAccessTunnelUpdateRoom(Task* unusedTask);

enum {
    NEO_ARK_EVE_ACCESS_TUNNEL_INTACT_PART_VARIANT_LIMIT = 4,
    NEO_ARK_EVE_ACCESS_TUNNEL_MASKED_BACKDROP_VARIANT   = 11,
};

enum {
    NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_START,
    NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_WAIT_CAP,
    NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_CHECK_REPLY,
    NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_WAIT,
    NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_COMMIT,
    NEO_ARK_EVE_ACCESS_TUNNEL_CAP_DECLINED              = 12,
    NEO_ARK_EVE_ACCESS_TUNNEL_INTERACTION_REARM_UPDATES = 10,
    NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_TASK              = 0,
    NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_TASK             = 1,
    NEO_ARK_EVE_ACCESS_TUNNEL_CAP_COMPLETION_TASK       = 2,
};

extern AreaResource D_neo_ark_eve_access_tunnel_8018067C[2];
extern AreaResource D_neo_ark_eve_access_tunnel_80180694[3];

extern SpriteSource D_neo_ark_eve_access_tunnel_8017F18C[69];

SpriteSource D_neo_ark_eve_access_tunnel_8017F18C[69] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 16, 0, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 40, 761, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 64, 837, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 80, 772, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 16, 766, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 16, 755, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 16, 745, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, 32, 747, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 753, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 758, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 80, 758, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, 56, 760, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 88, 668, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 88, 663, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 72, 648, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 56, 648, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 40, 648, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 24, 648, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 8, 648, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -8, 649, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -24, 649, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -40, 649, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -56, 649, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -72, 649, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 104, 0, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -136, 88, 606, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -136, 104, 599, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 112, 597, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 112, 568, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 72, 561, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, 40, 602, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, -8, 604, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 24, 562, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -24, 527, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, -56, 579, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -72, 535, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -96, 607, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -120, 566, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -136, -120, 617, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -120, 697, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, -96, 650, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 64, 40, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 88, 599, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 64, 600, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 40, 600, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 24, 601, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 24, 601, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 24, 601, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 24, 621, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 40, 598, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 56, 597, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 72, 597, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 88, 596, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 104, 596, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 80, 683, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 56, 683, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 24, 684, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 24, 716, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 40, 715, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 56, 715, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 72, 714, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 40, 789, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 56, 788, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 72, 787, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 24, 714, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 32, 789, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -102, 96, 593, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -118, 104, 572, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -150, 112, 566, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_eve_access_tunnel_8017F6F0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 2, 0 } },
    { 12, 29, 0, 0, { 3, 0 } },
    { 41, 25, 0, 0, { 1, 0 } },
    { 66, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_eve_access_tunnel_8017F720[48] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 40, 540, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 48, 544, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 56, 545, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 72, 548, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 88, 551, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 104, 545, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 40, 545, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 40, 552, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 40, 546, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 48, 555, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 40, 513, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 40, 505, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 40, 0, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 48, 480, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 56, 0, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 56, 486, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 64, 489, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 72, 489, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 80, 490, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 88, 491, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 104, 493, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 56, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 56, 557, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 72, 561, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 88, 563, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 568, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 56, 0, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, 80, 260, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -112, 80, 238, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 235, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 56, 0, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -112, 88, 1250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 80, 271, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 72, 357, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 64, 389, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 56, 423, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 56, 435, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 56, 439, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 56, 448, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 80, 454, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 72, 449, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, 80, 452, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 104, 243, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -88, 104, 0, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, -96, -64, 3567, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, -32, -64, 3653, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, -32, 0, 3694, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, -96, 0, 3603, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_eve_access_tunnel_8017FAE0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 3, 0 } },
    { 27, 3, 0, 0, { 0, 0 } },
    { 30, 14, 0, 0, { 2, 0 } },
    { 44, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_eve_access_tunnel_8017FB10[34] = {
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -40, -120, 1180, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -40, -40, 1180, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 8, 1187, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -16, 1182, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -32, 1153, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -56, 1157, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -80, 1121, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, -120, 1000, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -8, -120, 0, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, -72, 1166, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -8, -40, 0, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -8, 0, 1317, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 40, 453, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 40, 461, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 40, 454, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 40, 427, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 40, 0, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 40, 405, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 56, 393, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 72, 0, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 72, 400, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 88, 402, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 104, 406, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -104, 64, 0, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 56, 475, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 80, 479, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 104, 485, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -80, 56, 0, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 56, 461, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 64, 467, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 80, 470, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 88, 472, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 104, 479, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 72 } }, -8, -56, 2500, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_eve_access_tunnel_8017FDB8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 1, 0 } },
    { 12, 21, 0, 0, { 2, 0 } },
    { 33, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_eve_access_tunnel_8017FDE0[32] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, -8, 678, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, -8, 809, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 725, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -96, -120, 725, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, -120, 725, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, -120, 645, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -80, 0, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, -80, 725, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -80, 725, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, -40, 725, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, 8, 725, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, -48, 725, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -96, -64, 725, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -96, -8, 725, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -96, 56, 725, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 104, 0, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 88, 0, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 96, 604, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 88, 619, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 80, 0, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 80, 623, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 48, 765, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, 32, 729, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -72, 32, 725, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -8, 725, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -48, 725, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -88, 725, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -88, 680, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, -48, 670, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, 0, 730, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, -16, 718, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 96 } }, -24, -80, 1354, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_eve_access_tunnel_80180060[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 1, 0 } },
    { 31, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_eve_access_tunnel_80180080[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_eve_access_tunnel_80180090[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_eve_access_tunnel_801800A0[7] = {
    { { .empty = D_neo_ark_eve_access_tunnel_8017F17C }, D_neo_ark_eve_access_tunnel_8017F17C, NULL },
    { { .elements = D_neo_ark_eve_access_tunnel_8017F18C }, D_neo_ark_eve_access_tunnel_8017F6F0, NULL },
    { { .elements = D_neo_ark_eve_access_tunnel_8017F720 }, D_neo_ark_eve_access_tunnel_8017FAE0, NULL },
    { { .elements = D_neo_ark_eve_access_tunnel_8017FB10 }, D_neo_ark_eve_access_tunnel_8017FDB8, NULL },
    { { .elements = D_neo_ark_eve_access_tunnel_8017FDE0 }, D_neo_ark_eve_access_tunnel_80180060, NULL },
    { { .empty = D_neo_ark_eve_access_tunnel_80180080 }, D_neo_ark_eve_access_tunnel_80180080, NULL },
    { { .empty = D_neo_ark_eve_access_tunnel_80180090 }, D_neo_ark_eve_access_tunnel_80180090, NULL },
};

WorldCoordPointLight D_neo_ark_eve_access_tunnel_801800F4[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -246, -254, 933 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1806, 1806, 1855 }, { 0, 0 } }, 600, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1061, -254, 1859 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1806, 1806, 1855 }, { 0, 0 } }, 600, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1241, -2216, 6212 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1241, -2216, 3555 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3268, -2216, 973 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 6000 },
};

WorldCoordRoomLights D_neo_ark_eve_access_tunnel_801802D4[1] = {
    { 0, NULL, ARRAY_SIZE(D_neo_ark_eve_access_tunnel_801800F4), D_neo_ark_eve_access_tunnel_801800F4, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_eve_access_tunnel_801802EC[6] = {
    { NULL, NULL, NULL, { -1168, -1504, 4192, 0 }, { { 2032, -1904, 0, 0 }, { -2032, -1904, 0, 0 }, { 2032, 1904, 0, 0 }, { -2032, 1904, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1200, -1505, 4304, 0 }, { { -2000, -1904, 16, 0 }, { 2000, -1904, -16, 0 }, { -2000, 1904, 16, 0 }, { 2000, 1904, -16, 0 } }, { -34, 0, -4095, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1312, -1344, 2081, 0 }, { { 2032, -1904, 0, 0 }, { -2032, -1904, 0, 0 }, { 2032, 1904, 0, 0 }, { -2032, 1904, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1280, -1281, 2176, 0 }, { { -2000, -1904, 16, 0 }, { 2000, -1904, -16, 0 }, { -2000, 1904, 16, 0 }, { 2000, 1904, -16, 0 } }, { -34, 0, -4095, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2016, -1376, 960, 0 }, { { -304, -1904, 2160, 0 }, { 304, -1904, -2160, 0 }, { -304, 1904, 2160, 0 }, { 304, 1904, -2160, 0 } }, { -4057, 0, -572, 0 }, { 0, 0, 4096, 0 }, 2884, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2128, -1376, 1041, 0 }, { { 256, -1904, -1888, 0 }, { -256, -1904, 1888, 0 }, { 256, 1904, -1888, 0 }, { -256, 1904, 1888, 0 } }, { 4063, 0, 551, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_eve_access_tunnel_801804B4[6] = {
    { NULL, NULL, NULL, { -1600, -48, 4960, 0 }, { { -352, 0, -736, 0 }, { 352, 0, -736, 0 }, { -352, 0, 736, 0 }, { 352, 0, 736, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1040, -48, 736, 0 }, { { -848, 0, -608, 0 }, { 848, 0, -608, 0 }, { -848, 0, 608, 0 }, { 848, 0, 608, 0 } }, { 0, 4094, 0, 0 }, { 0, 0, 4096, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1696, -64, 3840, 0 }, { { -352, 0, -544, 0 }, { 352, 0, -544, 0 }, { -352, 0, 544, 0 }, { 352, 0, 544, 0 } }, { 0, 4103, 0, 0 }, { 3405, 0, -2276, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4064, -64, 960, 0 }, { { -352, 0, -736, 0 }, { 352, 0, -736, 0 }, { -352, 0, 736, 0 }, { 352, 0, 736, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1280, -64, 7024, 0 }, { { -608, 0, -336, 0 }, { 608, 0, -336, 0 }, { -608, 0, 336, 0 }, { 608, 0, 336, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 692, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1024, -64, 736, 0 }, { { -848, 0, -608, 0 }, { 848, 0, -608, 0 }, { -848, 0, 608, 0 }, { 848, 0, 608, 0 } }, { 0, 4094, 0, 0 }, { -4096, 0, 0, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_eve_access_tunnel_8018067C[2] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_eve_access_tunnel_80180694[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 52, 52, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205200_8014CA60 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_eve_access_tunnel_801806B8[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B030, D_neo_ark_eve_access_tunnel_8018067C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017B080, D_neo_ark_eve_access_tunnel_80180694 },
    { NULL, NULL },
};

WorldCollisionOccluder D_neo_ark_eve_access_tunnel_80180720[1] = {
    { NULL, NULL, { -3712, -1376, 3440, 0 }, { { -1664, 2368, 1296, 0 }, { 1664, 2368, -1296, 0 }, { -1664, -2368, 1296, 0 }, { 1664, -2368, -1296, 0 } }, { 2527, 0, 3244, 0 }, 3166, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_eve_access_tunnel_8018075C = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_neo_ark_eve_access_tunnel_80180768[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_eve_access_tunnel_80180770[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_eve_access_tunnel_8018075C },
};

WorldCollisionSurfaceProperties D_neo_ark_eve_access_tunnel_80180778[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_eve_access_tunnel_8018075C },
};

WorldCollisionSurfaceProperties* D_neo_ark_eve_access_tunnel_80180780[8] = {
    D_neo_ark_eve_access_tunnel_80180768,
    D_neo_ark_eve_access_tunnel_80180770,
    D_neo_ark_eve_access_tunnel_80180778,
    D_neo_ark_eve_access_tunnel_80180768,
    D_neo_ark_eve_access_tunnel_80180768,
    D_neo_ark_eve_access_tunnel_80180768,
    D_neo_ark_eve_access_tunnel_80180768,
    D_neo_ark_eve_access_tunnel_80180768,
};

RoomEventMsg D_neo_ark_eve_access_tunnel_801807A0;

RoomDeparture gRoomDeparture;

#include "../../shared/room_variants_shelter.inc.c"
#undef ROOM_VARIANT_RESOLVE_SHELTER

#include "../../shared/room_event_departure_task.inc.c"

/// The room task's three states: install the message table, adjust the views
/// each frame, and end the task. `neoArkEveAccessTunnelRoomTask` runs
/// them through a stack copy.
static const TaskFuncTable3 D_neo_ark_eve_access_tunnel_8017D688 = {
    _neoArkEveAccessTunnelInitializeRoom,
    _neoArkEveAccessTunnelUpdateRoom,
    taskKill,
};

/// Resolves a departure's destination selectors in place for an executed transition.
///
/// Only area, warp and room are passed through the borrowed stage resolver.
/// The resolver must accept one request as both input and output.
static __inline__ void _roomVariantResolveDeparture(RoomDeparture* departure, RoomVariantResolver resolve)
{
    RoomEventMsg request;

    request.areaId    = departure->area;
    request.warp      = departure->warp;
    request.room      = departure->room;
    request.queryOnly = ROOM_EVENT_EXECUTE;
    resolve(&request, &request);
    departure->area = request.areaId;
    departure->warp = request.warp;
    departure->room = request.room;
}

void neoArkEveAccessTunnelShelterDepartureTask(Task* task)
{
    enum {
        NEO_ARK_EVE_ACCESS_TUNNEL_CAP_SHELTER_DEPARTURE = 3,
        NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_DEFAULT_ROOM  = 1,
        NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_ARRIVAL       = 2,
        NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_FACING        = 0x800, // Half a turn, 4096 units per turn.
    };

    switch (task->state) {
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_START:
            capRunCommandWithTransition(NEO_ARK_EVE_ACCESS_TUNNEL_CAP_SHELTER_DEPARTURE);
            task->state++;
            return;
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_WAIT_CAP:
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            return;
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_CHECK_REPLY:
            D_80114D08 = NEO_ARK_EVE_ACCESS_TUNNEL_INTERACTION_REARM_UPDATES;
            if (capGetVariantKey() == NEO_ARK_EVE_ACCESS_TUNNEL_CAP_DECLINED) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                return;
            }
            task->state++;
            return;
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_WAIT:
            task->state++;
            return;
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_COMMIT: {
            RoomDeparture       departure;
            RoomVariantResolver resolve = _roomVariantResolveShelter;

            departure.stage    = GAME_STAGE_MINE_SHELTER;
            departure.area     = (u8)task->spawnArg1.value;
            departure.room     = NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_DEFAULT_ROOM;
            departure.warp     = NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_ARRIVAL;
            departure.sndEvent = 0;
            departure.facing   = NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_FACING;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            // Commit the resolved selectors before the departure task borrows them.
            _roomVariantResolveDeparture(&departure, resolve);
            gRoomDeparture = departure;
            taskSpawnFromTable(&D_neo_ark_eve_access_tunnel_8017EA88, 0, 0, 0);
            taskKill(task);
            break;
        }
    }
}

void neoArkEveAccessTunnelElevatorDepartureTask(Task* task)
{
    enum { NEO_ARK_EVE_ACCESS_TUNNEL_CAP_ELEVATOR_DEPARTURE = 2 };

    switch (task->state) {
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_START:
            capRunCommandWithTransition(NEO_ARK_EVE_ACCESS_TUNNEL_CAP_ELEVATOR_DEPARTURE);
            task->state++;
            return;
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_WAIT_CAP:
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            return;
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_CHECK_REPLY:
            D_80114D08 = NEO_ARK_EVE_ACCESS_TUNNEL_INTERACTION_REARM_UPDATES;
            if (capGetVariantKey() == NEO_ARK_EVE_ACCESS_TUNNEL_CAP_DECLINED) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                return;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            task->state++;
            sndEvtRequestScriptStart(SOUND_NEO_ARK_EVE_TUNNEL_TO_ELEVATOR, 0, 0);
            return;
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_WAIT:
            if (sndScriptHasActiveId(SOUND_NEO_ARK_EVE_TUNNEL_TO_ELEVATOR) != 0) {
                return;
            }
            task->state++;
            return;
        case NEO_ARK_EVE_ACCESS_TUNNEL_DEPARTURE_COMMIT:
            // Commit only area, warp and room; the remaining saved location bytes stay live.
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_neo_ark_eve_access_tunnel_801807A0.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_neo_ark_eve_access_tunnel_801807A0.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_neo_ark_eve_access_tunnel_801807A0.areaId)[1];
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            break;
    }
}

s32 neoArkEveAccessTunnelRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 neoArkEveAccessTunnelResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        NEO_ARK_EVE_ACCESS_TUNNEL_CAP_LOCKED_ELEVATOR  = 1,
        NEO_ARK_EVE_ACCESS_TUNNEL_MAP_FLAG_VISIBLE     = 2,
        NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_SCENE_EVENT = 24,
    };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    switch (request->areaId) {
        case GAME_AREA_NEO_ARK_EVE_ELEVATOR:
            switch (gameFlagGetNibble(GAME_FLAG_NEO_ARK_EVE_ELEVATOR_UNLOCKED)) {
                case 0:
                    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                        gameFlagSetNibbleIfPresent(request->flagId, NEO_ARK_EVE_ACCESS_TUNNEL_MAP_FLAG_VISIBLE);
                        capRunCommandWithTransition(NEO_ARK_EVE_ACCESS_TUNNEL_CAP_LOCKED_ELEVATOR);
                    }
                    break;
                default:
                    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                        // Preserve the three resolved selectors for the deferred reload.
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent    = NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_SCENE_EVENT;
                        D_neo_ark_eve_access_tunnel_801807A0.warp              = (u8)reply->areaId;
                        D_neo_ark_eve_access_tunnel_801807A0.field_4           = reply->warp;
                        ((u8*)&D_neo_ark_eve_access_tunnel_801807A0.areaId)[1] = reply->room;
                        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                        taskSpawnFromTable(D_neo_ark_eve_access_tunnel_8017EAC4, NEO_ARK_EVE_ACCESS_TUNNEL_ELEVATOR_TASK, 0, 0);
                    }
                    break;
            }
            return ROOM_VARIANT_TRANSITION_REFUSED;
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

s32 neoArkEveAccessTunnelHandleCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        NEO_ARK_EVE_ACCESS_TUNNEL_COMMAND_PART_0  = 6,
        NEO_ARK_EVE_ACCESS_TUNNEL_COMMAND_PART_1  = 7,
        NEO_ARK_EVE_ACCESS_TUNNEL_CAP_PART_0_DOWN = 8,
        NEO_ARK_EVE_ACCESS_TUNNEL_CAP_PART_1_DOWN = 9,
    };

    if (gGameSession->location.loc.variant == NEO_ARK_EVE_ACCESS_TUNNEL_MASKED_BACKDROP_VARIANT) {
        switch (commandId) {
            case NEO_ARK_EVE_ACCESS_TUNNEL_COMMAND_PART_0:
                if (gameFlagGetNibble(GAME_FLAG_EVE_ACCESS_TUNNEL_PART_0_DOWN) == 0) {
                    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                        capRunCommandWithTransition(NEO_ARK_EVE_ACCESS_TUNNEL_COMMAND_PART_0);
                    }
                } else {
                    capRunCommandWithTransition(NEO_ARK_EVE_ACCESS_TUNNEL_CAP_PART_0_DOWN);
                }
                break;
            case NEO_ARK_EVE_ACCESS_TUNNEL_COMMAND_PART_1:
                if (gameFlagGetNibble(GAME_FLAG_EVE_ACCESS_TUNNEL_PART_1_DOWN) == 0) {
                    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                        capRunCommandWithTransition(NEO_ARK_EVE_ACCESS_TUNNEL_COMMAND_PART_1);
                    }
                } else {
                    capRunCommandWithTransition(NEO_ARK_EVE_ACCESS_TUNNEL_CAP_PART_1_DOWN);
                }
                break;
        }
    }
    return 0;
}

s32 neoArkEveAccessTunnelHandleAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        NEO_ARK_EVE_ACCESS_TUNNEL_ACTION_SHELTER_DEPARTURE = 10,
        NEO_ARK_EVE_ACCESS_TUNNEL_CAP_DEPARTURE_BLOCKED    = 5,
        NEO_ARK_EVE_ACCESS_TUNNEL_BLOCKED_DEPARTURE_FLAG   = 0x1AF,
    };
    if (request->actionId == NEO_ARK_EVE_ACCESS_TUNNEL_ACTION_SHELTER_DEPARTURE) {
        if (gameFlagGetNibble(GAME_FLAG_0F8) != 0) {
            capRunCommandWithTransition(NEO_ARK_EVE_ACCESS_TUNNEL_CAP_DEPARTURE_BLOCKED);
            taskSpawnFromTable(D_neo_ark_eve_access_tunnel_8017EAC4, NEO_ARK_EVE_ACCESS_TUNNEL_CAP_COMPLETION_TASK, NEO_ARK_EVE_ACCESS_TUNNEL_BLOCKED_DEPARTURE_FLAG, 0);
        } else {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            // Widen the action byte to the task argument's ABI word.
            taskSpawnFromTable(D_neo_ark_eve_access_tunnel_8017EAC4, NEO_ARK_EVE_ACCESS_TUNNEL_SHELTER_TASK, (s32)request->argument, 0);
        }
        return 0;
    }
    return 0;
}

s32 neoArkEveAccessTunnelSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum {
        NEO_ARK_EVE_ACCESS_TUNNEL_SOUND_CUE_1  = 1,
        NEO_ARK_EVE_ACCESS_TUNNEL_SCRIPT_CUE_1 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL, 1),
    };

    if (cueKey == NEO_ARK_EVE_ACCESS_TUNNEL_SOUND_CUE_1) {
        sndEvtRequestScriptStart(NEO_ARK_EVE_ACCESS_TUNNEL_SCRIPT_CUE_1, 0, 0);
    }
    return 0;
}

void neoArkEveAccessTunnelRecordCapCompletionTask(Task* task)
{
    enum {
        NEO_ARK_EVE_ACCESS_TUNNEL_CAP_DECLINED_KEY = 12,
        NEO_ARK_EVE_ACCESS_TUNNEL_FLAG_COMPLETED   = 2,
    };

    if (capIsBusy() == 0) {
        if (capGetVariantKey() != NEO_ARK_EVE_ACCESS_TUNNEL_CAP_DECLINED_KEY) {
            gameFlagSetNibble(task->spawnArg1.value, NEO_ARK_EVE_ACCESS_TUNNEL_FLAG_COMPLETED);
        }
        taskKill(task);
    }
}

/// Sets the GPU mask bit on every pixel of the resident RGB16 backdrop.
///
/// Requires a completed image in the borrowed 320x240 workspace; no decoding
/// or capture may overwrite it during the pass. Pixel order is immaterial.
static __inline__ void _neoArkEveAccessTunnelMaskBackdrop(void)
{
    enum { NEO_ARK_EVE_ACCESS_TUNNEL_BACKDROP_PIXELS = (s32)(sizeof(*Fs_ImgBuffers) / sizeof(u16)) };
    u16* pixel      = (u16*)Fs_ImgBuffers;
    s32  pixelIndex = 0;

    do {
        *pixel      = (u16)(*pixel | FILE_SYSTEM_IMAGE_PIXEL_MASK);
        pixelIndex += 1;
        pixel      += 1;
    } while (pixelIndex <= NEO_ARK_EVE_ACCESS_TUNNEL_BACKDROP_PIXELS - 1);
}

/// Installs the tunnel's room-message receiver and advances to its update state.
///
/// In variant 11, masks the completed backdrop and selects ending-music
/// suppression. The session slot borrows the task; teardown does not clear it.
static void _neoArkEveAccessTunnelInitializeRoom(Task* task)
{
    task->msgTable = D_neo_ark_eve_access_tunnel_8017EA94;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == NEO_ARK_EVE_ACCESS_TUNNEL_MASKED_BACKDROP_VARIANT) {
        // Preserve the mask on the loaded backdrop before subsequent view decodes.
        _neoArkEveAccessTunnelMaskBackdrop();
        gGameSession->flowFlags = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC;
    }
    task->state = task->state + 1;
}

/// Keeps both tunnel parts intact in variants 0..3 and masks view decodes in variant 11.
///
/// Re-arms the one-image RGB16 MDEC mode each tick because decoding consumes
/// and resets it. The room task remains in state 1 to receive messages.
static void _neoArkEveAccessTunnelUpdateRoom(Task* unusedTask)
{
    CdCmdQueue* queue = &gCdCmdQueue;

    if (gGameSession->location.loc.variant < (u32)NEO_ARK_EVE_ACCESS_TUNNEL_INTACT_PART_VARIANT_LIMIT) {
        neoArkEveAccessTunnelSetPartDestroyedSprites(0, NEO_ARK_EVE_ACCESS_TUNNEL_PART_INTACT);
        neoArkEveAccessTunnelSetPartDestroyedSprites(1, NEO_ARK_EVE_ACCESS_TUNNEL_PART_INTACT);
    }
    if (gGameSession->location.loc.variant == NEO_ARK_EVE_ACCESS_TUNNEL_MASKED_BACKDROP_VARIANT) {
        queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    }
}

void neoArkEveAccessTunnelRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_neo_ark_eve_access_tunnel_8017D688;
    handlers.funcs[task->state](task);
}
