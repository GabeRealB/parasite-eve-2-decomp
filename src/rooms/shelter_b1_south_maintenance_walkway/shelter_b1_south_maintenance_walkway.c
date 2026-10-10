#include "rooms/shelter_b1_south_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
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

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"

extern s8 D_shelter_b1_south_maintenance_walkway_80183644;

/// Descriptor of the event task the message handler spawns.
extern TaskDesc D_shelter_b1_south_maintenance_walkway_801822FC;

/// The room's message table.
extern TaskMessageEntry D_shelter_b1_south_maintenance_walkway_80182308[];

/// Spawn payload of the task 0x31 the event task may start.
extern RoomFadeStorage  gRoomEventFade;
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static WorldCollisionGrid     _gShelterB1SouthMaintenanceWalkwayCollisionGrid;
extern WorldCollisionOccluder D_shelter_b1_south_maintenance_walkway_80183274[1];
extern WorldCollisionTrigger  D_shelter_b1_south_maintenance_walkway_801830AC[6];
extern WorldCollisionTrigger  D_shelter_b1_south_maintenance_walkway_801832B0[2];
static WorldCoordRoomLights   _gShelterB1SouthMaintenanceWalkwayRoomLights;
static s32                    _shelterB1SouthMaintenanceWalkwayResolveDepartureMessage(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32                    _shelterB1SouthMaintenanceWalkwayRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32                    _shelterB1SouthMaintenanceWalkwayIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32                    _shelterB1SouthMaintenanceWalkwayIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

enum { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskDesc D_shelter_b1_south_maintenance_walkway_801822FC = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_south_maintenance_walkway_80182308[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1SouthMaintenanceWalkwayResolveDepartureMessage },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM, _shelterB1SouthMaintenanceWalkwayRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1SouthMaintenanceWalkwayIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1SouthMaintenanceWalkwayIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-space anchors for the room's view-selected capsule glows and red disc.
///
/// Capsule endpoints use signed game-coordinate units. The drawers borrow
/// these positions without modifying them; storage lasts with the room overlay.
static struct {
    SVECTOR capsuleEndpoints[10][2]; // Two world points per capsule
    SVECTOR redDiscCenter;           // Disc position, visible in views 4 and 5
} _gShelterB1SouthMaintenanceWalkwayGlowGeometry = {
    {
        { { 894, -196, 3102, 0 }, { 894, -196, 2264, 0 } },
        { { 894, -196, 404, 0 }, { 894, -196, -385, 0 } },
        { { 894, -196, -2032, 0 }, { 894, -196, -2645, 0 } },
        { { 3102, -196, 3102, 0 }, { 3102, -196, 2264, 0 } },
        { { 3102, -196, 404, 0 }, { 3102, -196, -385, 0 } },
        { { 3102, -196, -2032, 0 }, { 3102, -196, -2645, 0 } },
        { { 650, -196, -2895, 0 }, { -31, -196, -2895, 0 } },
        { { 600, -196, -5113, 0 }, { -45, -196, -5113, 0 } },
        { { -1556, -196, -2895, 0 }, { -2498, -196, -2895, 0 } },
        { { -1556, -196, -5113, 0 }, { -2498, -196, -5113, 0 } },
    },
    { 763, -1283, -2117, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

#include "../../shared/room_visual_effects_disc_data.inc.c"

WorldCoordRoomLighting gShelterB1SouthMaintenanceWalkwayRoomLighting[1] = {
    { &_gShelterB1SouthMaintenanceWalkwayRoomLights, NULL },
};

WorldCollisionRoomResources D_shelter_b1_south_maintenance_walkway_801823FC[1] = {
    { &_gShelterB1SouthMaintenanceWalkwayCollisionGrid, D_shelter_b1_south_maintenance_walkway_801830AC, D_shelter_b1_south_maintenance_walkway_801832B0, D_shelter_b1_south_maintenance_walkway_80183274 },
};

u8* gShelterB1SouthMaintenanceWalkwayRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_south_maintenance_walkway_80182410[1] = { 5 };

DirectionWarpEntry D_shelter_b1_south_maintenance_walkway_80182414[2] = {
    { { { .word = 1024 }, -2357, 0, -3990 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -2357, 0, -3990 }, { 0, 0, 0, 0 }, 0x540A0002, 0x540A0001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 2048, 0, 4600 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 2048, 0, 4600 }, { 0, 0, 0, 0 }, 0x540A0004, 0x540A0003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1AE },
};

/// Room-local collision normals, with ONE representing unit length.
static SVECTOR _gShelterB1SouthMaintenanceWalkwayCollisionGridNormals[14] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_normals.inc"
};

/// Room-local collision vertices in signed game-coordinate units.
static SVECTOR _gShelterB1SouthMaintenanceWalkwayCollisionGridVerts[38] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_verts.inc"
};

/// Indexed room mesh: class 1 is the floor; the other faces use class 0.
static WorldCollisionGridFace _gShelterB1SouthMaintenanceWalkwayCollisionGridFaces[18] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_faces.inc"
};

/// Six signed face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB1SouthMaintenanceWalkwayCollisionGridCells[82] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1SouthMaintenanceWalkwayCollisionGridCells[i])
/// X-major directory for the grid's two X cells and three Z cells.
static s16* _gShelterB1SouthMaintenanceWalkwayCollisionGridTable[6] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_table.inc"
};
#undef GRID_CELL

/// The room's collision mesh and six-cell XZ index, bound to the view on entry.
///
/// Cell coordinates add (2872, 5300) to room X/Z and divide by 4000 game units.
/// The loaded room owns the descriptor and its writable mesh/index pools.
static WorldCollisionGrid _gShelterB1SouthMaintenanceWalkwayCollisionGrid = {
    NULL,
    _gShelterB1SouthMaintenanceWalkwayCollisionGridNormals,
    _gShelterB1SouthMaintenanceWalkwayCollisionGridVerts,
    _gShelterB1SouthMaintenanceWalkwayCollisionGridFaces,
    _gShelterB1SouthMaintenanceWalkwayCollisionGridTable,
    2872,
    5300,
    2,
    3,
    4000,
    ARRAY_SIZE(_gShelterB1SouthMaintenanceWalkwayCollisionGridFaces),
};

ViewCamera gShelterB1SouthMaintenanceWalkwayViewCameras[5] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x35D2, 0 } }, 235 },
    { { { { 3964, 0, 1030 }, { 222, 3999, -857 }, { -1006, 885, 3870 } }, { -2760, 1734, -200 } }, 235 },
    { { { { 4064, 0, 509 }, { 89, 4032, -715 }, { -501, 721, 4000 } }, { -2760, 1952, 4026 } }, 235 },
    { { { { -3922, 0, 1178 }, { 415, 3832, 1384 }, { -1102, 1445, -3670 } }, { -2962, 1986, -667 } }, 235 },
    { { { { 1594, 0, 3772 }, { 1049, 3934, -443 }, { -3623, 1139, 1531 } }, { -2935, 1592, 4947 } }, 235 },
};

/// View 1 keeps the decoded backdrop and selects no source sprites.
static SpriteBatch _gShelterB1SouthMaintenanceWalkwayView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2 keeps the decoded backdrop and selects no source sprites.
static SpriteBatch _gShelterB1SouthMaintenanceWalkwayView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3 keeps the decoded backdrop and selects no source sprites.
static SpriteBatch _gShelterB1SouthMaintenanceWalkwayView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 66 screen-space sprite rectangles, with neutral RGB modulation.
///
/// The first 57 use CLUT row 255 and the last nine row 256 of the 8-bit page
/// at VRAM (960, 0). Positions/dimensions are pixels; depth is the OT sort value.
static SpriteSource _gShelterB1SouthMaintenanceWalkwayView4SpriteSources[66] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 24, -120, 753, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, 104, 16, 936, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, 104, -8, 920, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, 104, -32, 936, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, 104, -56, 861, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, 104, -80, 800, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, 104, -120, 703, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, 32, -120, 776, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 72, -120, 703, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 32, 8, 1122, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 8, 1050, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 48, 0, 1063, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 56, -104, 1490, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 40, -104, 745, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, -96, 857, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, -40, 925, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 88, -40, 925, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, -32, 935, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 88, -32, 925, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, -24, 951, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 88, -24, 920, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, -48, 915, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 88, -48, 893, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, -56, 903, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 88, -56, 925, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 88, -64, 893, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, -64, 887, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, -72, 939, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, -72, 877, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, -80, 914, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, -80, 768, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, -88, 854, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, -88, 777, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, -96, 837, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, -104, 821, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, -104, 727, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, -96, 631, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, -16, 997, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, -16, 936, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 56, -8, 1014, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, -8, 925, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 56, 0, 1002, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, 0, 925, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 56, 8, 1009, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, 8, 1000, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 88, 8, 1000, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 56, 16, 1037, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, 16, 1000, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 88, 16, 1000, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, 24, 1075, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 48, 24, 1050, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, 24, 939, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, 24, 1050, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, 32, 1050, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 48, 32, 1050, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, 32, 1050, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, 32, 1050, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 32, 40 } }, 128, 32, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 24, 40 } }, 104, 32, 829, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, 88, 32, 863, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, 72, 32, 864, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, 88, 56, 812, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, 72, 56, 812, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, 56, 56, 853, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, 56, 32, 925, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 48, 32, 963, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
};

enum { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VIEW4_FIRST_SPRITE_COUNT = 57 };

/// View 4 keeps the backdrop and draws source ranges 0..56 and 57..65.
///
/// The zero-count leading record selects decoded image strips; the final
/// SPRITE_BATCH_END record ends the list. The two ranges cover every source.
static SpriteBatch _gShelterB1SouthMaintenanceWalkwayView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VIEW4_FIRST_SPRITE_COUNT, 0, 0, { 1, 0 } },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VIEW4_FIRST_SPRITE_COUNT, ARRAY_SIZE(_gShelterB1SouthMaintenanceWalkwayView4SpriteSources) - SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VIEW4_FIRST_SPRITE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5 keeps the decoded backdrop and selects no source sprites.
static SpriteBatch _gShelterB1SouthMaintenanceWalkwayView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB1SouthMaintenanceWalkwaySpriteViews[5] = {
    { { .empty = _gShelterB1SouthMaintenanceWalkwayView1SpriteBatches }, _gShelterB1SouthMaintenanceWalkwayView1SpriteBatches, NULL },
    { { .empty = _gShelterB1SouthMaintenanceWalkwayView2SpriteBatches }, _gShelterB1SouthMaintenanceWalkwayView2SpriteBatches, NULL },
    { { .empty = _gShelterB1SouthMaintenanceWalkwayView3SpriteBatches }, _gShelterB1SouthMaintenanceWalkwayView3SpriteBatches, NULL },
    { { .elements = _gShelterB1SouthMaintenanceWalkwayView4SpriteSources }, _gShelterB1SouthMaintenanceWalkwayView4SpriteBatches, NULL },
    { { .empty = _gShelterB1SouthMaintenanceWalkwayView5SpriteBatches }, _gShelterB1SouthMaintenanceWalkwayView5SpriteBatches, NULL },
};

/// Six point lights contributing in every view, with world-unit falloff radii.
///
/// Lighting setup parents and composes their transforms; queries overwrite
/// attenuation. RGB intensities use 12 fractional bits. Room-owned and writable.
static WorldCoordPointLight _gShelterB1SouthMaintenanceWalkwayPointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1622, -223, -3869 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2555, 2518, 2578 }, { 0, 0 } }, 1550, 2671 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 524, -224, -3732 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2429, 2417, 2416 }, { 0, 0 } }, 1899, 2620 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2187, -303, -4148 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1526, 1617, 1737 }, { 0, 0 } }, 1500, 3411 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1972, -223, 2267 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2568, 2676, 2615 }, { 0, 0 } }, 1961, 3743 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1987, -223, -206 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2549, 2539, 2597 }, { 0, 0 } }, 1701, 2461 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2025, -223, -1515 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2588, 2641, 2677 }, { 0, 0 } }, 1923, 3843 },
};

/// The room's six point lights, with no directional or cone lights.
static WorldCoordRoomLights _gShelterB1SouthMaintenanceWalkwayRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gShelterB1SouthMaintenanceWalkwayPointLights),
    _gShelterB1SouthMaintenanceWalkwayPointLights,
    0,
    NULL,
};

WorldCollisionTrigger D_shelter_b1_south_maintenance_walkway_801830AC[6] = {
    { NULL, NULL, NULL, { 2080, -1584, 2560, 0 }, { { -1380, -1904, -226, 0 }, { 1363, -1904, 207, 0 }, { -1380, 1904, -226, 0 }, { 1363, 1904, 207, 0 } }, { 639, 0, -4061, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2048, -1600, 2401, 0 }, { { 1408, -1904, 152, 0 }, { -1416, -1904, -160, 0 }, { 1408, 1904, 152, 0 }, { -1416, 1904, -160, 0 } }, { -453, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2112, -1504, -1233, 0 }, { { 1717, -1904, 24, 0 }, { -1729, -1904, -41, 0 }, { 1717, 1904, 24, 0 }, { -1729, 1904, -41, 0 } }, { -79, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2176, -1505, -1072, 0 }, { { -1720, -1904, -82, 0 }, { 1700, -1904, 41, 0 }, { -1720, 1904, -82, 0 }, { 1700, 1904, 41, 0 } }, { 146, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1008, -1568, -4241, 0 }, { { 112, -1904, -1309, 0 }, { -114, -1904, 1306, 0 }, { 112, 1904, -1309, 0 }, { -114, 1904, 1306, 0 } }, { 4099, 0, 354, 0 }, { 0, 0, 4096, 0 }, 2304, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1120, -1600, -4176, 0 }, { { -100, -1904, 1323, 0 }, { 98, -1904, -1325, 0 }, { -100, 1904, 1323, 0 }, { 98, 1904, -1325, 0 } }, { -4084, 0, -307, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_shelter_b1_south_maintenance_walkway_80183274[1] = {
    { NULL, NULL, { -1504, -1344, -1424, 0 }, { { -2080, -2368, 1200, 0 }, { 2080, -2368, -1200, 0 }, { -2080, 2368, 1200, 0 }, { 2080, 2368, -1200, 0 } }, { -2053, 0, -3558, 0 }, 3367, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_south_maintenance_walkway_801832B0[2] = {
    { NULL, NULL, NULL, { -2512, -48, -4032, 0 }, { { -432, 0, -1024, 0 }, { 432, 0, -1024, 0 }, { -432, 0, 1024, 0 }, { 432, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1984, -48, 4480, 0 }, { { -1024, 0, 432, 0 }, { -1024, 0, -432, 0 }, { 1024, 0, 432, 0 }, { 1024, 0, -432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room-local resource keys shared by recipes and actor placements.
enum {
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_RAT         = 7,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_MOSSBACK    = 11,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_PAWN_GOLEM  = 20,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_WATCHER     = 21,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA      = 24,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_CATERPILLAR = 26,
};

/// Watcher and Rat loading and spawn recipes for placement variant 1.
///
/// The two live entries load `actor_102100` and `actor_200700`.
/// Each spawns descriptor 0; `AREA_PLACEMENT_END` terminates the table.
static AreaResource _gShelterB1SouthMaintenanceWalkwayVariant1Resources[3] = {
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_RAT, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor200700RatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Mossback and Amoeba loading and spawn recipes for placement variant 2.
///
/// The two live entries load `actor_101100` and `actor_202400`.
/// Each spawns descriptor 0; `AREA_PLACEMENT_END` terminates the table.
static AreaResource _gShelterB1SouthMaintenanceWalkwayVariant2Resources[3] = {
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_MOSSBACK, 11, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor101100Tasks },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 24, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Amoeba and Caterpillar loading and spawn recipes for placement variant 3.
///
/// The two live entries load `actor_102400` and `actor_202600`.
/// Each spawns descriptor 0; `AREA_PLACEMENT_END` terminates the table.
static AreaResource _gShelterB1SouthMaintenanceWalkwayVariant3Resources[3] = {
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102400Tasks },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_CATERPILLAR, 26, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor202600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Watcher and Pawn GOLEM loading and spawn recipes for placement variant 11.
///
/// The two live entries load `actor_102100` and `actor_202000`.
/// Each spawns descriptor 0; `AREA_PLACEMENT_END` terminates the table.
static AreaResource _gShelterB1SouthMaintenanceWalkwayVariant11Resources[3] = {
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_PAWN_GOLEM, 20, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202000PawnGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 1 spawns two Watchers and six Rats.
///
/// 8 live placements precede AREA_PLACEMENT_END. Positions are world units,
/// yaw is 4096 per turn; texture offsets place the two resource groups separately.
static AreaPlacement _gShelterB1SouthMaintenanceWalkwayVariant1Placements[9] = {
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_WATCHER, 0, 0, 2700, -2000, 5000, 2048, 0, 0, 2, 6 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_WATCHER, 0, 0, 1300, -2000, 5000, 2048, 0, 0, 2, 6 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_RAT, 0, 0, 2300, 0, -2500, -200, 0, 2, 4, 4 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_RAT, 0, 0, 1700, 0, -2500, 200, 0, 2, 4, 4 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_RAT, 0, 0, 2300, 0, -2200, 0, 0, 2, 4, 4 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_RAT, 0, 0, 1700, 0, -2200, -500, 0, 2, 4, 4 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_RAT, 0, 0, 2300, 0, -1900, 2600, 0, 2, 4, 4 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_RAT, 0, 0, 1700, 0, -1900, 1600, 0, 2, 4, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 2 spawns two Mossbacks and three Amoebas.
///
/// 5 live placements precede AREA_PLACEMENT_END. Positions are world units,
/// yaw is 4096 per turn; texture offsets place the two resource groups separately.
static AreaPlacement _gShelterB1SouthMaintenanceWalkwayVariant2Placements[6] = {
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_MOSSBACK, 0, 0, 2000, 0, 800, 0, 0, 0, 2, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_MOSSBACK, 0, 0, 2400, 0, -4000, 3072, 0, 0, 2, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 0, 0, 2550, 0, 0, 3600, 0, 2, 4, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 0, 0, 1750, 0, -600, 3072, 0, 2, 4, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 0, 0, 1550, 0, -1600, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

enum { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_AMOEBA_SECOND_PARAMETER_SET = 1 };

/// Variant 3 spawns five Amoebas and three Caterpillars.
///
/// 8 live placements precede AREA_PLACEMENT_END. Positions are world units,
/// yaw is 4096 per turn; texture offsets place the two resource groups separately.
static AreaPlacement _gShelterB1SouthMaintenanceWalkwayVariant3Placements[9] = {
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 0, SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_AMOEBA_SECOND_PARAMETER_SET, 2000, 0, -800, 0, 0, 0, 2, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 0, 0, 1500, 0, 2400, 450, 0, 0, 2, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 0, 0, 2350, 0, 400, 3400, 0, 0, 2, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 0, 0, 900, 0, -4500, 3100, 0, 0, 2, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_AMOEBA, 0, 0, 750, 0, -3500, 2750, 0, 0, 2, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_CATERPILLAR, 0, 0, 2500, 0, -4500, 3400, 0, 2, 4, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_CATERPILLAR, 0, 0, 2450, 0, -3400, 0, 0, 2, 4, 0 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_CATERPILLAR, 0, 0, 1500, 0, -3300, 1600, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

enum { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_WATCHER_GUN_VARIANT = 4 };

/// Variant 11 spawns two Watchers and one Pawn GOLEM.
///
/// 3 live placements precede AREA_PLACEMENT_END. Positions are world units,
/// yaw is 4096 per turn; texture offsets place the two resource groups separately.
static AreaPlacement _gShelterB1SouthMaintenanceWalkwayVariant11Placements[4] = {
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_WATCHER, SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_WATCHER_GUN_VARIANT, 0, 2700, -2000, 5000, 2048, 0, 0, 2, 6 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_WATCHER, SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_WATCHER_GUN_VARIANT, 0, 1300, -2000, 5000, 2048, 0, 0, 2, 6 },
    { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_RESOURCE_PAWN_GOLEM, 0, 0, 2000, 0, 3500, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Saved placement-variant indices; the other table slots are absent.
enum {
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VARIANT_WATCHERS_AND_RATS        = 1,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VARIANT_MOSSBACKS_AND_AMOEBAS    = 2,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VARIANT_AMOEBAS_AND_CATERPILLARS = 3,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VARIANT_WATCHERS_AND_GOLEM       = 11,
};

AreaVariant gShelterB1SouthMaintenanceWalkwayAreaVariants[12] = {
    [SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VARIANT_WATCHERS_AND_RATS]        = { _gShelterB1SouthMaintenanceWalkwayVariant1Placements, _gShelterB1SouthMaintenanceWalkwayVariant1Resources },
    [SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VARIANT_MOSSBACKS_AND_AMOEBAS]    = { _gShelterB1SouthMaintenanceWalkwayVariant2Placements, _gShelterB1SouthMaintenanceWalkwayVariant2Resources },
    [SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VARIANT_AMOEBAS_AND_CATERPILLARS] = { _gShelterB1SouthMaintenanceWalkwayVariant3Placements, _gShelterB1SouthMaintenanceWalkwayVariant3Resources },
    [SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_VARIANT_WATCHERS_AND_GOLEM]       = { _gShelterB1SouthMaintenanceWalkwayVariant11Placements, _gShelterB1SouthMaintenanceWalkwayVariant11Resources },
};

enum {
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SOUND_FLOOR_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5D,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SOUND_FLOOR_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5F,
};

/// Floor footstep cue bases in the currently loaded type-1 sound bank.
///
/// Walk and scripted jump use entry 0x5D; running uses 0x5F. Animation cue 2
/// uses the base, cue 1 adds one. This record belongs to collision surface class 1.
static WorldCollisionFootstepSounds _gShelterB1SouthMaintenanceWalkwayFloorFootstepSounds = {
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SOUND_FLOOR_WALK,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SOUND_FLOOR_RUN,
    SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SOUND_FLOOR_WALK,
};

/// Blocking surfaces with pushback and weapon impacts, but no footstep cues.
///
/// Classes 0 and 2..7 share this record; the stored first byte is uninterpreted.
static WorldCollisionSurfaceProperties _gShelterB1SouthMaintenanceWalkwaySilentSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class-1 floor surfaces with pushback, weapon impacts and paired footstep cues.
///
/// Segment probes are blocked. The stored first byte is uninterpreted.
static WorldCollisionSurfaceProperties _gShelterB1SouthMaintenanceWalkwayFloorSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gShelterB1SouthMaintenanceWalkwayFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB1SouthMaintenanceWalkwaySurfacePropertiesByClass[8] = {
    &_gShelterB1SouthMaintenanceWalkwaySilentSurfaceProperties,
    &_gShelterB1SouthMaintenanceWalkwayFloorSurfaceProperties,
    &_gShelterB1SouthMaintenanceWalkwaySilentSurfaceProperties,
    &_gShelterB1SouthMaintenanceWalkwaySilentSurfaceProperties,
    &_gShelterB1SouthMaintenanceWalkwaySilentSurfaceProperties,
    &_gShelterB1SouthMaintenanceWalkwaySilentSurfaceProperties,
    &_gShelterB1SouthMaintenanceWalkwaySilentSurfaceProperties,
    &_gShelterB1SouthMaintenanceWalkwaySilentSurfaceProperties,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

s8 D_shelter_b1_south_maintenance_walkway_80183644 = 0;

/// Three bytes stored after the flag; nothing references them.
u8 D_shelter_b1_south_maintenance_walkway_80183645 = 26;

u8 D_shelter_b1_south_maintenance_walkway_80183646 = 67;

u8 D_shelter_b1_south_maintenance_walkway_80183647 = 220;

RoomLatchedEvent gRoomEventLatched;

static void _shelterB1SouthMaintenanceWalkwayIdle(Task* task);

/// Binds this room's six actor-effect selectors to its exported effect tasks.
///
/// Installs packed bank/type IDs for the flash, twin trail, spark burst, glow
/// disc, flying spark and orange burst. Call after room-effect controller
/// initialization and before spawning these effects. The IDs persist until
/// the controller resets them or another room replaces them; this overlay
/// must remain loaded while the selected tasks run.
static inline void _shelterB1SouthMaintenanceWalkwayBindEffects(void)
{
    gRoomEffectFlashId        = EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SPARK_BURST;
    gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_GLOW_DISC;
    gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_FLYING_SPARK;
    gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_ORANGE_BURST_2;
}

/// Latches an eligible departure event and starts its staged task on execution.
///
/// Returns 2 for an eligible event, including a query, or 1 for direct departure
/// when its nonzero flag is already set. Every call clears the latest-start byte.
/// Only `message->queryOnly == ROOM_EVENT_EXECUTE` copies the complete eight-byte
/// message and twelve-byte event, sets a nonzero flag to 1 and raises that byte
/// after spawning. Flag IDs must be 0..503, including the always-eligible zero.
/// Inputs are borrowed for this call. The singleton copies and room CAP/sound
/// resources must stay live and unchanged until the staged task ends.
static __inline__ s32 _shelterB1SouthMaintenanceWalkwayStartEvent(
    const RoomEventMsg* message, const RoomLatchedEvent* event)
{
    enum { ROOM_EVENT_FLAG_NONE         = 0,
           ROOM_EVENT_FLAG_CLEAR        = 0,
           ROOM_EVENT_FLAG_LATCHED      = 1,
           ROOM_EVENT_DEPARTURE_DIRECT  = 1,
           ROOM_EVENT_DEPARTURE_HANDLED = 2 };

    D_shelter_b1_south_maintenance_walkway_80183644 = false;
    if (gameFlagGetNibble(event->flagId) == ROOM_EVENT_FLAG_CLEAR || event->flagId == ROOM_EVENT_FLAG_NONE) {
        if (message->queryOnly == ROOM_EVENT_EXECUTE) {
            _gRoomEventStagedMsg = *message;
            gRoomEventLatched    = *event;
            if (event->flagId != ROOM_EVENT_FLAG_NONE) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&D_shelter_b1_south_maintenance_walkway_801822FC, 0, 0, 0);
            D_shelter_b1_south_maintenance_walkway_80183644 = true;
        }
        return ROOM_EVENT_DEPARTURE_HANDLED;
    }
    return ROOM_EVENT_DEPARTURE_DIRECT;
}

#include "../../shared/room_event_staged_task.inc.c"

/// Resolves departures and stages the first elevator-hall and storeroom scenes.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with a complete borrowed request and
/// writable reply, which may alias. Copies all eight bytes before resolving
/// the destination. Elevator-hall/storeroom departures select CAP commands 1/2
/// and separate one-shot flags; neither event fades. Returns 2 for an eligible
/// scene, otherwise 1 for direct departure. Queries preserve selectors and only
/// clear the event gate's latest-start byte. Executing scenes retain copies of
/// both records, so room resources and singleton storage must remain live until
/// completion. The receiver and message ID are unused.
static s32 _shelterB1SouthMaintenanceWalkwayResolveDepartureMessage(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_CAP_TO_ELEVATOR    = 1,
           SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_CAP_TO_STOREROOM   = 2,
           SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SOUND_TO_ELEVATOR  = 1,
           SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SOUND_TO_STOREROOM = 3,
           SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_EVENT_NO_FADE      = 0,
           SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_DEPARTURE_DIRECT   = 1 };
    RoomLatchedEvent event;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B1_ELEVATOR_HALL) {
        event.capCmd   = SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_CAP_TO_ELEVATOR;
        event.stageSnd = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY, SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SOUND_TO_ELEVATOR);
        event.flagId   = GAME_FLAG_B1_SOUTH_WALKWAY_TO_ELEVATOR_SCENE;
        event.fade     = SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_EVENT_NO_FADE;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(reply, &event);
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_STOREROOM) {
        event.capCmd   = SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_CAP_TO_STOREROOM;
        event.stageSnd = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY, SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_SOUND_TO_STOREROOM);
        event.flagId   = GAME_FLAG_B1_SOUTH_WALKWAY_TO_STOREROOM_SCENE;
        event.fade     = SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_EVENT_NO_FADE;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(reply, &event);
    }
    return SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_DEPARTURE_DIRECT;
}

/// Rejects key-item use in this room with a zero result.
///
/// The item ID and all other arguments are neither read nor retained.
static s32 _shelterB1SouthMaintenanceWalkwayRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores room commands from scripts and triggers and returns zero.
///
/// Neither command word nor any other argument is read or retained.
static s32 _shelterB1SouthMaintenanceWalkwayIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room-action requests from direction triggers and returns zero.
///
/// The borrowed request and all other arguments are neither read nor retained.
static s32 _shelterB1SouthMaintenanceWalkwayIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Publishes the initialized room task as the room-message receiver.
///
/// Called in state 0; installs the borrowed room message table, replaces
/// `GAME_TASK_SLOT_ROOM` and advances to idle state 1. The task and this overlay
/// must remain live while messages can reach that slot.
static void _shelterB1SouthMaintenanceWalkwayInitializeRoomTask(Task* task)
{
    task->msgTable = D_shelter_b1_south_maintenance_walkway_80182308;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized room task available for messages without per-frame work.
static void _shelterB1SouthMaintenanceWalkwayIdle(Task* task)
{
}

/// The room task's three states: set-up, idle and exit.
static const TaskFuncTable3 D_shelter_b1_south_maintenance_walkway_8017D5D8 = {
    { _shelterB1SouthMaintenanceWalkwayInitializeRoomTask, _shelterB1SouthMaintenanceWalkwayIdle, taskKill },
};

void shelterB1SouthMaintenanceWalkwayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_b1_south_maintenance_walkway_8017D5D8;
    stateHandlers.funcs[task->state](task);
}

void shelterB1SouthMaintenanceWalkwayDrawGlowsTask(Task* task)
{
    enum { GLOWS_INITIALIZE,
           GLOWS_DRAW,
           GLOW_RADIUS_SCALE = 0x200 };

    if (task->state == GLOWS_INITIALIZE) {
        _shelterB1SouthMaintenanceWalkwayBindEffects();
        task->state = GLOWS_DRAW;
    }

    // Select the visible endpoint pairs by the saved room view; view 1 draws none.
    switch (gGameSession->location.loc.view) {
        case 2:
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[0][0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[3][0], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            break;
        case 3:
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[0][0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[1][0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[3][0], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[4][0], GLOW_RADIUS_SCALE, GLOW_QUARTER_TURN);
            break;
        case 4:
            glowDrawRedDisc(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.redDiscCenter, GLOW_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[2][0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[5][0], GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[7][0], GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            break;
        case 5:
            glowDrawRedDisc(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.redDiscCenter, GLOW_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[2][0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[6][0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[7][0], GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[8][0], GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&_gShelterB1SouthMaintenanceWalkwayGlowGeometry.capsuleEndpoints[9][0], GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
    }
}

#include "../../shared/glow_draw_cone.inc.c"

#include "../../shared/glow_draw_red_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
