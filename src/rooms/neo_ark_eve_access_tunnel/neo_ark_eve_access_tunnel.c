#include "rooms/neo_ark_eve_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "neo_ark_eve_access_tunnel_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
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

/// Scene id byte; the tunnel stamps 0x18 when it hands the save location off.

/// Set when the tunnel's save is written to the memory card.

/// Staging save location the room commits when the tunnel's save is taken:
/// `field_2` / `field_4` / `field_1` hold what `func_neo_ark_eve_access_tunnel_8017DB18`
/// later copies into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area` / `warp` / `room`.
extern RoomEventMsg D_neo_ark_eve_access_tunnel_801807A0;

/// The staged event descriptor, read by the task spawned above.
extern RoomDeparture gRoomDeparture;

static void func_neo_ark_eve_access_tunnel_8017DF24(Task* arg0);
static void func_neo_ark_eve_access_tunnel_8017DFC0(Task* task);

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

#include "../../shared/room_event_departure_task.inc.c"

/// The room task's three states: install the message table, adjust the views
/// each frame, and end the task. `func_neo_ark_eve_access_tunnel_8017E038` runs
/// them through a stack copy.
static const TaskFuncTable3 D_neo_ark_eve_access_tunnel_8017D688 = {
    func_neo_ark_eve_access_tunnel_8017DF24,
    func_neo_ark_eve_access_tunnel_8017DFC0,
    taskKill,
};

/// Tunnel departure sequence, advanced one step per call: step 0 raises CAP
/// command 3, step 1 waits for the CAP system to go idle, step 2 arms the CAP
/// countdown at 0xA and waits for the event key it answers with - 0xC kills the
/// sequence and messages the player weapon - and step 3 falls through to the
/// shared advance. Step 4 stages `gRoomDeparture` (stage 4, area from the
/// task's `spawnArg1`, room 1, warp 2, facing 0x800 and no sound), runs area,
/// warp and room through the room's resolver, and spawns the tunnel's outgoing
/// task, whose callback is `roomDepartureTask`.
void func_neo_ark_eve_access_tunnel_8017D980(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(3);
            task->state++;
            return;
        case 1:
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            return;
        case 2:
            D_80114D08 = 0xA;
            if (capGetVariantKey() == 0xC) {
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            task->state++;
            return;
        case 3:
            task->state++;
            return;
        case 4: {
            RoomDeparture       work;
            RoomEventMsg        msg;
            RoomDeparture*      wp;
            RoomVariantResolver resolve = roomVariantResolveShelter;

            work.stage    = GAME_STAGE_MINE_SHELTER;
            work.area     = (u8)task->spawnArg1.value;
            work.room     = 1;
            work.warp     = 2;
            work.sndEvent = 0;
            work.facing   = 0x800;
            Gp_MsgPlayerWeapon(0);
            // The destination room depends on game progress: run the departure's
            // selectors through the stage's resolver, request and reply in one record.
            wp            = &work;
            msg.areaId    = wp->area;
            msg.warp      = wp->warp;
            msg.room      = wp->room;
            msg.queryOnly = ROOM_EVENT_EXECUTE;
            resolve(&msg, &msg);
            wp->area       = msg.areaId;
            wp->warp       = msg.warp;
            wp->room       = msg.room;
            gRoomDeparture = work;
            taskSpawnFromTable(&D_neo_ark_eve_access_tunnel_8017EA88, 0, 0, 0);
            taskKill(task);
            break;
        }
    }
}

/// Tunnel save sequence, advanced one step per call: step 0 raises CAP command
/// 2, step 1 waits for the CAP system to go idle, step 2 latches the save flag
/// into CAP and waits for the event key it answers with, step 3 waits for the
/// queued sound to finish, and step 4 commits the staged save location to
/// `gMcSaveData` and spawns the outgoing task.
void func_neo_ark_eve_access_tunnel_8017DB18(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(2);
            task->state++;
            return;
        case 1:
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            return;
        case 2:
            D_80114D08 = 0xA;
            if (capGetVariantKey() == 0xC) {
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            Gp_MsgPlayerWeapon(0);
            task->state++;
            sndEvtRequestScriptStart(SOUND_NEO_ARK_EVE_TUNNEL_TO_ELEVATOR, 0, 0);
            return;
        case 3:
            if (SndVoice_HasActiveId(SOUND_NEO_ARK_EVE_TUNNEL_TO_ELEVATOR) != 0) {
                return;
            }
            task->state++;
            return;
        case 4:
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_neo_ark_eve_access_tunnel_801807A0.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_neo_ark_eve_access_tunnel_801807A0.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_neo_ark_eve_access_tunnel_801807A0.areaId)[1];
            taskSpawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Message handler for id 0x13F1 in the room's message table: accepts the
/// message and does nothing.
s32 func_neo_ark_eve_access_tunnel_8017DC64(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Tunnel message handler. Message 9 either raises the CAP command that opens
/// the tunnel (nibble 0xB9 still clear) or, once that nibble is set, latches the
/// save location the outgoing message carries and starts the cutscene that
/// leads to the EVE encounter. The two `switch`es are load-bearing: the
/// equivalent `if` / `else` chain makes reorg fill the second queryOnly branch's
/// delay slot from the return block instead of the fall-through.
s32 func_neo_ark_eve_access_tunnel_8017DC6C(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    func_map_neo_ark_80179B14(src, dst);
    switch (src->areaId) {
        case GAME_AREA_NEO_ARK_EVE_ELEVATOR:
            switch (gameFlagGetNibble(GAME_FLAG_NEO_ARK_EVE_ELEVATOR_UNLOCKED)) {
                case 0:
                    if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                        Gp_SetNibbleIf(src->flagId, 2);
                        Gp_RunCapCmd1(1);
                    }
                    break;
                default:
                    if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent    = 0x18;
                        D_neo_ark_eve_access_tunnel_801807A0.warp              = (u8)dst->areaId;
                        D_neo_ark_eve_access_tunnel_801807A0.field_4           = dst->warp;
                        ((u8*)&D_neo_ark_eve_access_tunnel_801807A0.areaId)[1] = dst->room;
                        Gp_MsgPlayerWeapon(0);
                        taskSpawnFromTable(D_neo_ark_eve_access_tunnel_8017EAC4, 1, 0, 0);
                    }
                    break;
            }
            return 0;
    }
    return 1;
}

s32 func_neo_ark_eve_access_tunnel_8017DD70(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (gGameSession->location.loc.variant == 0xB) {
        switch (arg2) {
            case 6:
                if (gameFlagGetNibble(GAME_FLAG_EVE_ACCESS_TUNNEL_PART_0_DOWN) == 0) {
                    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                        Gp_RunCapCmd1(6);
                    }
                } else {
                    Gp_RunCapCmd1(8);
                }
                break;
            case 7:
                if (gameFlagGetNibble(GAME_FLAG_EVE_ACCESS_TUNNEL_PART_1_DOWN) == 0) {
                    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                        Gp_RunCapCmd1(7);
                    }
                } else {
                    Gp_RunCapCmd1(9);
                }
                break;
        }
    }
    return 0;
}

s32 func_neo_ark_eve_access_tunnel_8017DE1C(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 0xA) {
        if (gameFlagGetNibble(GAME_FLAG_0F8) != 0) {
            Gp_RunCapCmd1(5);
            taskSpawnFromTable(D_neo_ark_eve_access_tunnel_8017EAC4, 2, 0x1AF, 0);
        } else {
            Gp_MsgPlayerWeapon(0);
            // Widen the action byte to the task argument's ABI word.
            taskSpawnFromTable(D_neo_ark_eve_access_tunnel_8017EAC4, 0, (s32)request->argument, 0);
        }
        return 0;
    }
    return 0;
}

s32 func_neo_ark_eve_access_tunnel_8017DE9C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        sndEvtRequestScriptStart(0x55080000 | 1, 0, 0);
    }
    return 0;
}

/// Third entry of the room's task table: waits for the CAP command to finish,
/// then, unless it ended on event key 0xC, sets the game-flag nibble named by
/// the task's spawn argument to 2, and ends the task.
void func_neo_ark_eve_access_tunnel_8017DED0(Task* arg0)
{
    if (capIsBusy() == 0) {
        if (capGetVariantKey() != 0xC) {
            gameFlagSetNibble(arg0->spawnArg1.value, 2);
        }
        taskKill(arg0);
    }
}

/// State 0 of the tunnel's message task: park the room's message table in
/// `Task::msgTable` and publish the task in pointer slot 7, as every room-entry
/// task does. Then, once the session has reached state 0xB, set bit 15 of every
/// 16-bit half of the 0x25800-byte image buffer and latch `GameSession::flowFlags`
/// bit 0 - the flag that suppresses the bank-load spawn when the task ends.
static void func_neo_ark_eve_access_tunnel_8017DF24(Task* arg0)
{
    arg0->msgTable = D_neo_ark_eve_access_tunnel_8017EA94;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 0xB) {
        u16* ptr = (u16*)Fs_ImgBuffers;
        s32  i   = 0;

        do {
            *ptr = (u16)(*ptr | FILE_SYSTEM_IMAGE_PIXEL_MASK);
            i   += 1;
            ptr += 1;
        } while (i <= FILE_SYSTEM_IMAGE_STRIP_COUNT * FILE_SYSTEM_IMAGE_STRIP_WORDS * 2 - 1);
        gGameSession->flowFlags = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// State 1 of the tunnel's message task, run every frame: while the session is
/// below state 4 it sets both runs of view flags, and at state 0xB it sets the
/// CD command queue's `field_22A` to 2.
static void func_neo_ark_eve_access_tunnel_8017DFC0(Task* task)
{
    CdCmdQueue* queue = &gCdCmdQueue;

    if (gGameSession->location.loc.variant < 4U) {
        neoArkEveAccessTunnelSetPartDestroyedSprites(0, NEO_ARK_EVE_ACCESS_TUNNEL_PART_INTACT);
        neoArkEveAccessTunnelSetPartDestroyedSprites(1, NEO_ARK_EVE_ACCESS_TUNNEL_PART_INTACT);
    }
    if (gGameSession->location.loc.variant == 0xB) {
        queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    }
}

/// Runs the task's current state through a stack copy of the room's
/// three-entry state table.
void func_neo_ark_eve_access_tunnel_8017E038(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_eve_access_tunnel_8017D688;
    sp.funcs[task->state](task);
}
