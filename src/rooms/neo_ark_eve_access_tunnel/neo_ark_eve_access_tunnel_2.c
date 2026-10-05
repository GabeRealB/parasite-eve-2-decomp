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
    { 5105, func_neo_ark_eve_access_tunnel_8017DC64 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_eve_access_tunnel_8017DE1C },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_eve_access_tunnel_8017DD70 },
    { ROOM_MESSAGE_SOUND, func_neo_ark_eve_access_tunnel_8017DE9C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_neo_ark_eve_access_tunnel_8017EAC4[3] = {
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_eve_access_tunnel_8017D980, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_eve_access_tunnel_8017DB18, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_eve_access_tunnel_8017DED0, { .value = 0 } },
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

/// Hides or shows sprite commands of the area's views through their
/// `SpriteBatch::hidden`: `arg0` 0 drives command 4 of view 2, `arg0` 1 command
/// 3 of view 3 and command 2 of view 4. `arg1` 0 hides them and 1 shows them;
/// any other value changes nothing.
void func_neo_ark_eve_access_tunnel_8017E090(s32 arg0, s32 arg1)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    SpriteView*      rec  = Gp_SprtTables[sess->stage - 1]->areaViews[sess->area - 1];
    SpriteBatch*     batches;
    s32              run = arg0 & 0xFF;
    s32              flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            batches           = rec[2].batches;
            batches[4].hidden = 1;
            return;
        }
        if (flag == 1) {
            batches           = rec[2].batches;
            batches[4].hidden = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            batches           = rec[3].batches;
            batches[3].hidden = run;
            batches           = rec[4].batches;
            batches[2].hidden = run;
            return;
        }
        if (flag == run) {
            batches           = rec[3].batches;
            batches[3].hidden = 0;
            batches           = rec[4].batches;
            batches[2].hidden = 0;
        }
    }
}

/// Draws whichever emitters the current view shows: two adjacent positions per
/// drawn wedge, stepping through the view's run. View 4 chains into view 5's
/// emitters (`D_..._8017EB08` then `D_..._8017EB28`); every other view stops at
/// its own.
void func_neo_ark_eve_access_tunnel_8017E15C(Task* unused)
{
    u8 view;

    view = viewGetMappedIndex();
    switch (view) {
        case 2:
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB48[0], 0x180, 0x444);
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB48[2], 0x180, 0x444);
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB48[4], 0x180, 0x444);
            break;
        case 3:
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EAE8[0], 0x180, 0x444);
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EAE8[2], 0x180, 0x444);
            break;
        case 4:
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB08[0], 0x180, 0x444);
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB08[2], 0x180, 0x444);
            /* fallthrough */
        case 5:
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB28[0], 0x180, 0x444);
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB28[2], 0x180, 0x444);
            break;
        case 6:
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB48[0], 0x180, 0x444);
            _glowDrawCapsule(&D_neo_ark_eve_access_tunnel_8017EB48[2], 0x180, 0x444);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"
