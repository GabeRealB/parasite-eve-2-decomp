#include "rooms/neo_ark_savanna_zone.h"

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
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
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

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/room_events.h"

extern TaskDesc         D_neo_ark_savanna_zone_8017F9A0;
extern RoomEventMsg     gRoomEventStagedMsg;
extern s8               D_neo_ark_savanna_zone_80180998;
extern RoomLatchedEvent gRoomEventLatched;

/// Payload handed to the helper task 0x31 the event may start.
extern RoomFadeStorage gRoomEventFade;

/// The room's message table, which the room setup task installs.
extern TaskMessageEntry D_neo_ark_savanna_zone_8017F9AC[];

/// The coordinate trail's two spawn offsets: `[0]` places the object's own
/// frame and `[1]` the second trail's frame. `RoomFx_TrailOffsets[1]`
/// is `[1]` under its own name, which the per-frame path reads directly.

extern WorldCollisionGrid         D_neo_ark_savanna_zone_8017FBD0[1];
extern WorldCollisionOccluder     D_neo_ark_savanna_zone_801808CC[1];
extern WorldCollisionTrigger      D_neo_ark_savanna_zone_801804EC[4];
extern WorldCollisionTrigger      D_neo_ark_savanna_zone_8018061C[5];
extern WorldCoordRoomAmbientEntry D_neo_ark_savanna_zone_80180908[5];
extern WorldCoordRoomLights       D_neo_ark_savanna_zone_801804D4[1];
extern TaskDesc                   Actor00100_D1BA84;
s32                               func_neo_ark_savanna_zone_8017D77C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32                        _neoArkSavannaZoneRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32                        _neoArkSavannaZoneIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32                        _neoArkSavannaZoneIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

/// Requests use of the selected collected key item; zero refuses the request.
enum { NEO_ARK_SAVANNA_ZONE_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskDesc D_neo_ark_savanna_zone_8017F9A0 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_savanna_zone_8017F9AC[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_savanna_zone_8017D77C },
    { NEO_ARK_SAVANNA_ZONE_MESSAGE_USE_KEY_ITEM, _neoArkSavannaZoneRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkSavannaZoneIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkSavannaZoneIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_neo_ark_savanna_zone_8017F9E4[1] = {
    { D_neo_ark_savanna_zone_8017FBD0, D_neo_ark_savanna_zone_801804EC, D_neo_ark_savanna_zone_8018061C, D_neo_ark_savanna_zone_801808CC },
};

WorldCoordRoomLighting D_neo_ark_savanna_zone_8017F9F4[1] = {
    { D_neo_ark_savanna_zone_801804D4, D_neo_ark_savanna_zone_80180908 },
};

u8* D_neo_ark_savanna_zone_8017F9FC[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_savanna_zone_8017FA00[1] = { 4 };

DirectionWarpEntry D_neo_ark_savanna_zone_8017FA04[2] = {
    { { { .word = 1024 }, 510, 0, 1500 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 510, 0, 1500 }, { 0, 0, 0, 0 }, 0x55120002, 0x55120001, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x34BC, 0, 1500 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x34BC, 0, 1500 }, { 0, 0, 0, 0 }, 0x55120004, 0x55120003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkSavannaZoneCollision02610Normals[5] = {
#include "assets/neo_ark_savanna_zone_collision_02610_normals.inc"
};

static SVECTOR _gNeoArkSavannaZoneCollision02610Verts[12] = {
#include "assets/neo_ark_savanna_zone_collision_02610_verts.inc"
};

static WorldCollisionGridFace _gNeoArkSavannaZoneCollision02610Faces[11] = {
#include "assets/neo_ark_savanna_zone_collision_02610_faces.inc"
};

static s16 _gNeoArkSavannaZoneCollision02610Cells[32] = {
#include "assets/neo_ark_savanna_zone_collision_02610_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkSavannaZoneCollision02610Cells[i])
static s16* _gNeoArkSavannaZoneCollision02610Table[4] = {
#include "assets/neo_ark_savanna_zone_collision_02610_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_savanna_zone_8017FBD0[1] = {
    { NULL, _gNeoArkSavannaZoneCollision02610Normals, _gNeoArkSavannaZoneCollision02610Verts, _gNeoArkSavannaZoneCollision02610Faces, _gNeoArkSavannaZoneCollision02610Table, 50, 50, 4, 1, 4000, 11 },
};

ViewCamera D_neo_ark_savanna_zone_8017FBF4[4] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { -7500, 0x77D8, -1950 } }, 329 },
    { { { { -676, 0, -4039 }, { -670, 4039, 112 }, { 3983, 679, -667 } }, { -6330, 1950, -2240 } }, 329 },
    { { { { -365, 0, -4079 }, { -37, 4095, 3 }, { 4079, 38, -365 } }, { -1580, 1430, -2240 } }, 329 },
    { { { { -753, 0, 4026 }, { 207, 4090, 38 }, { -4020, 211, -752 } }, { -9980, 1620, -2260 } }, 329 },
};

SpriteBatch D_neo_ark_savanna_zone_8017FC84[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_savanna_zone_8017FC94[24] = {
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 16, 825, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 16, 825, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -128, -8, 875, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, -8, 875, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -32, 925, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -8, 1237, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -40, 1237, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, -40, 1237, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -96, -24, 1237, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -72, 1237, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, -72, 1237, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -152, -96, 1237, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -128, -104, 1237, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, 8, 1025, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -120, 8, 1025, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, 8, 1025, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -144, -8, 1025, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 0, 1025, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 40, 962, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 48, 962, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 48, 962, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 16, 962, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 16, 962, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 8, 962, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_savanna_zone_8017FE74[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 3, 0 } },
    { 5, 8, 0, 0, { 0, 0 } },
    { 13, 5, 0, 0, { 2, 0 } },
    { 18, 6, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_savanna_zone_8017FEA4[35] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 625, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 625, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 625, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 625, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -40, 625, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, 0, 625, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -40, 625, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -80, -80, 625, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, -120, 625, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, -120, 625, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 40, -120, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 40, -96, 625, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 96, 712, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 56, 712, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 56, 712, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 16, 712, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 16, 712, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -24, 712, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -24, 712, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -64, 712, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -64, 712, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -64, 712, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -104, 712, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -88, 712, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, -120, 712, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -120, -120, 712, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -24, 1062, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -32, 1062, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 8, 1062, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 48, 1062, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -120, 48, 1062, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 8, 1062, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -120, -24, 1062, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -80, 24, 1062, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_savanna_zone_80180160[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 14, 0, 0, { 2, 0 } },
    { 27, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_savanna_zone_80180188[29] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 0, 1312, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 1312, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 1312, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 1312, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 0, 1312, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -120, 1312, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -80, 1312, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -40, 1312, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -40, 1312, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -64, 1312, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -120, 1312, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -96, 1312, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, -80, 1312, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 1700, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -8, 1700, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -48, 1700, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -72, 1700, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, -72, 1700, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -48, 1700, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -8, 1700, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 88, 32, 1700, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -32, 1700, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -64, 1700, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 48, 1050, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 1050, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -8, 1050, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, -24, 1050, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 8, 1050, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, 40, 1050, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_savanna_zone_801803CC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 10, 0, 0, { 2, 0 } },
    { 23, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_savanna_zone_801803F4[4] = {
    { { .empty = D_neo_ark_savanna_zone_8017FC84 }, D_neo_ark_savanna_zone_8017FC84, NULL },
    { { .elements = D_neo_ark_savanna_zone_8017FC94 }, D_neo_ark_savanna_zone_8017FE74, NULL },
    { { .elements = D_neo_ark_savanna_zone_8017FEA4 }, D_neo_ark_savanna_zone_80180160, NULL },
    { { .elements = D_neo_ark_savanna_zone_80180188 }, D_neo_ark_savanna_zone_801803CC, NULL },
};

WorldCoordLight D_neo_ark_savanna_zone_80180424[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 952, -472, 229 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 5837, 5715, 5574 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -834, -528, -362 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4243, 4202, 4138 }, { 0, 0 } },
};

WorldCoordRoomLights D_neo_ark_savanna_zone_801804D4[1] = {
    { ARRAY_SIZE(D_neo_ark_savanna_zone_80180424), D_neo_ark_savanna_zone_80180424, 0, NULL, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_savanna_zone_801804EC[4] = {
    { NULL, NULL, NULL, { 5503, -2305, 1552, 0 }, { { 55, -2608, -3196, 0 }, { -59, -2608, 3190, 0 }, { 55, 2609, -3196, 0 }, { -59, 2609, 3190, 0 } }, { 4098, 0, 73, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5727, -2305, 1744, 0 }, { { -59, -2608, 3336, 0 }, { 53, -2608, -3342, 0 }, { -59, 2608, 3336, 0 }, { 53, 2608, -3342, 0 } }, { -4095, 0, -69, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x277F, -2256, 1727, 0 }, { { -55, -2688, 3339, 0 }, { 56, -2688, -3338, 0 }, { -55, 2688, 3339, 0 }, { 56, 2688, -3338, 0 } }, { -4103, 0, -69, 0 }, { 0, 0, 4096, 0 }, 4283, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9951, -2224, 1695, 0 }, { { 57, -2688, -3193, 0 }, { -56, -2688, 3194, 0 }, { 57, 2688, -3193, 0 }, { -56, 2688, 3194, 0 } }, { 4095, 0, 72, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_savanna_zone_8018061C[5] = {
    { NULL, NULL, NULL, { 736, -48, 1376, 0 }, { { -672, 0, -736, 0 }, { 672, 0, -736, 0 }, { -672, 0, 736, 0 }, { 672, 0, 736, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 995, WORLD_COLLISION_TRIGGER_ACTION_WARP, 21, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x33C0, -48, 1440, 0 }, { { -672, 0, -1136, 0 }, { 672, 0, -1136, 0 }, { -672, 0, 1136, 0 }, { 672, 0, 1136, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 1317, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7776, -64, 272, 0 }, { { -1440, 0, -432, 0 }, { 1440, 0, -432, 0 }, { -1440, 0, 432, 0 }, { 1440, 0, 432, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, 4096, 0 }, 1498, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6992, -64, 256, 0 }, { { -6992, 0, -432, 0 }, { 6992, 0, -432, 0 }, { -6992, 0, 432, 0 }, { 6992, 0, 432, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 7001, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6976, -64, 2720, 0 }, { { -6992, 0, -432, 0 }, { 6992, 0, -432, 0 }, { -6992, 0, 432, 0 }, { 6992, 0, 432, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -4096, 0 }, 7001, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_savanna_zone_80180798[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, Actor02500_D05B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_savanna_zone_801807BC[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_savanna_zone_801807D4[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { 26, 26, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_savanna_zone_801807F8[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, Actor02500_D05B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_savanna_zone_8018081C[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { 56, 56, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205600_801602C0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_savanna_zone_80180840[3] = {
    { 56, 56, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_105600_801482C0 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_savanna_zone_80180864[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017BB70, D_neo_ark_savanna_zone_80180798 },
    { D_map_neo_ark_8017BBF0, D_neo_ark_savanna_zone_801807BC },
    { D_map_neo_ark_8017BC70, D_neo_ark_savanna_zone_801807D4 },
    { D_map_neo_ark_8017BCE0, D_neo_ark_savanna_zone_801807F8 },
    { D_map_neo_ark_8017BD50, D_neo_ark_savanna_zone_8018081C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017BD80, D_neo_ark_savanna_zone_80180840 },
    { NULL, NULL },
};

WorldCollisionOccluder D_neo_ark_savanna_zone_801808CC[1] = {
    { NULL, NULL, { 6928, -3008, 1552, 0 }, { { -7952, 0, -2576, 0 }, { 7952, 0, -2576, 0 }, { -7952, 0, 2576, 0 }, { 7952, 0, 2576, 0 } }, { 0, 4103, 0, 0 }, 8318, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_neo_ark_savanna_zone_80180908[5] = {
    { .viewCount = ARRAY_SIZE(D_neo_ark_savanna_zone_80180908) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 422, 394, 324, 395 } },
    { .color = { 422, 394, 324, 395 } },
    { .color = { 422, 394, 324, 395 } },
};

WorldCollisionFootstepSounds D_neo_ark_savanna_zone_80180930 = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

WorldCollisionFootstepSounds D_neo_ark_savanna_zone_8018093C = {
    0x10000031,
    0x10000033,
    0x10000031,
};

WorldCollisionSurfaceProperties D_neo_ark_savanna_zone_80180948[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_savanna_zone_80180950[1] = { 0 };

WorldCollisionSurfaceProperties D_neo_ark_savanna_zone_80180958[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_savanna_zone_80180930 },
};

WorldCollisionSurfaceProperties D_neo_ark_savanna_zone_80180960[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_savanna_zone_8018093C },
};

WorldCollisionSurfaceProperties* D_neo_ark_savanna_zone_80180968[8] = {
    D_neo_ark_savanna_zone_80180948,
    D_neo_ark_savanna_zone_80180950,
    D_neo_ark_savanna_zone_80180958,
    D_neo_ark_savanna_zone_80180960,
    D_neo_ark_savanna_zone_80180948,
    D_neo_ark_savanna_zone_80180948,
    D_neo_ark_savanna_zone_80180948,
    D_neo_ark_savanna_zone_80180948,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

s8 D_neo_ark_savanna_zone_80180998 = 0;

RoomLatchedEvent gRoomEventLatched = { 0 };

static void _neoArkSavannaZoneInitializeRoom(Task* task);
static void _neoArkSavannaZoneSetupIdleState(Task* task);

#include "../../shared/room_event_staged_task.inc.c"

/// Tests and, in execute mode, latches a room-transition event.
///
/// Returns 1 when the nonzero event flag is already set, otherwise 2, including
/// queries. Every call clears the latest-start marker. Only `ROOM_EVENT_EXECUTE`
/// copies the complete eight-byte transition and twelve-byte event, sets a
/// nonzero flag to 1 and spawns the staged controller. Flag 0 stays eligible.
/// Both inputs are borrowed during this call; their copies and CAP/sound
/// resources must remain in the loaded room until the controller finishes.
static __inline__ s32 _neoArkSavannaZoneStartEvent(const RoomEventMsg* transition, const RoomLatchedEvent* event)
{
    enum {
        ROOM_EVENT_NOT_STARTED  = 0,
        ROOM_EVENT_STARTED      = 1,
        ROOM_EVENT_FLAG_SEEN    = 1,
        ROOM_EVENT_ALREADY_SEEN = 1,
        ROOM_EVENT_ELIGIBLE     = 2
    };

    D_neo_ark_savanna_zone_80180998 = ROOM_EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (transition->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *transition;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_SEEN);
            }
            taskSpawnFromTable(&D_neo_ark_savanna_zone_8017F9A0, 0, 0, 0);
            D_neo_ark_savanna_zone_80180998 = ROOM_EVENT_STARTED;
        }
        return ROOM_EVENT_ELIGIBLE;
    }
    return ROOM_EVENT_ALREADY_SEEN;
}

/// Room handler for the save-location message: copies the incoming record onto
/// the outgoing one and forwards both to `mapNeoArkResolveRoomVariant`. Messages 0x13 and
/// 0x15 build the room's event record - cap command 3 / 2, the stage sound and
/// flag 0x15E / 0x15F - and hand it to `_neoArkSavannaZoneStartEvent`; every
/// other message is not consumed and answers 1.
s32 func_neo_ark_savanna_zone_8017D77C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;
    s32              cmd;
    s32              snd;
    s16              flag;

    *out = *in;
    mapNeoArkResolveRoomVariant(in, out);
    if (in->areaId != GAME_AREA_NEO_ARK_SOUTH_PROMENADE) {
        goto message15;
    }
    snd            = 0x55120003;
    cmd            = 3;
    event.stageSnd = snd;
    flag           = 0x15E;
start_event:
    event.capCmd = cmd;
    event.flagId = flag;
    event.fade   = 0;
    return _neoArkSavannaZoneStartEvent(out, &event);
message15:
    if (in->areaId == GAME_AREA_NEO_ARK_SHRINE) {
        snd            = 0x55120001;
        cmd            = 2;
        event.stageSnd = snd;
        flag           = 0x15F;
        goto start_event;
    }
    return 1;
}

/// Refuses every collected key-item use with zero, leaving the item unused.
static s32 _neoArkSavannaZoneRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores CAP room commands and returns zero without retaining either argument.
static s32 _neoArkSavannaZoneIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores direction actions and returns zero without reading the borrowed request.
static s32 _neoArkSavannaZoneIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Installs the savanna message receiver and advances to its idle state.
///
/// Called in state 0 with a live room task and initialized gameplay resources.
/// Registers the borrowed task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
static void _neoArkSavannaZoneInitializeRoom(Task* task)
{
    task->msgTable = D_neo_ark_savanna_zone_8017F9AC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room's message task idle in state 1 until an external state change or teardown.
static void _neoArkSavannaZoneSetupIdleState(Task* task)
{
}

/// State table of the room setup task, indexed by `Task::state`.
static const TaskFuncTable3 D_neo_ark_savanna_zone_8017D5D8 = { {
    _neoArkSavannaZoneInitializeRoom,
    _neoArkSavannaZoneSetupIdleState,
    taskKill,
} };

void neoArkSavannaZoneRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_neo_ark_savanna_zone_8017D5D8;
    stateHandlers.funcs[task->state](task);
}

void neoArkSavannaZoneConfigureEffectsTask(Task* task)
{
    enum { CONFIGURE_EFFECTS_INITIALIZE,
           CONFIGURE_EFFECTS_IDLE };

    if (task->state == CONFIGURE_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId               = EFFECT_NEO_ARK_SAVANNA_ZONE_FLASH;
        gRoomEffectTwinTrailId           = EFFECT_NEO_ARK_SAVANNA_ZONE_TWIN_TRAIL;
        gRoomEffectSparkBurstId          = EFFECT_NEO_ARK_SAVANNA_ZONE_SPARK_BURST;
        gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
        task->state                      = CONFIGURE_EFFECTS_IDLE;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void neoArkSavannaZoneRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void neoArkSavannaZoneRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_savanna_zone_8017ED58(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
