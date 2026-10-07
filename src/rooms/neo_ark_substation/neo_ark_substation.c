#include "rooms/neo_ark_substation.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

/// The room's own `TaskMessageEntry[]` - the message table the message task publishes.
extern TaskMessageEntry D_neo_ark_substation_8017E294[];
/// Spawn table for the ambience task the message task starts.
extern TaskDesc D_neo_ark_substation_8017E2BC[];
/// The room's ambience table, one `(panOffset, attenuation)` entry per view slot.
extern RoomAmbienceEntry D_neo_ark_substation_8017E2C8[];

extern SVECTOR D_neo_ark_substation_8017E310[];
extern SVECTOR D_neo_ark_substation_8017E330[];
extern SVECTOR D_neo_ark_substation_8017E350[];
extern SVECTOR D_neo_ark_substation_8017E360[];
extern SVECTOR D_neo_ark_substation_8017E380[];

static void func_neo_ark_substation_8017D7AC(Task* task);
static void _neoArkSubstationMessageTaskIdle(Task* unusedTask);

/// State table of the room's message task: set-up
/// (`func_neo_ark_substation_8017D7AC`), an empty per-frame state and
/// `taskKill`. Its bytes open the room's rodata, ahead of the ambience task's
/// jump table.
static const TaskFuncTable3 D_neo_ark_substation_8017D5C4 = {
    func_neo_ark_substation_8017D7AC,
    _neoArkSubstationMessageTaskIdle,
    taskKill,
};

void       func_neo_ark_substation_8017D608(Task*);
static s32 _neoArkSubstationRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
s32        func_neo_ark_substation_8017D724(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_neo_ark_substation_8017D768(Task*, s32, s32, s32);
static s32 _neoArkSubstationIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);

/// Key-item menu request and the reply that displays the cannot-use notice.
enum {
    NEO_ARK_SUBSTATION_MESSAGE_USE_KEY_ITEM = 0x13F1,
    NEO_ARK_SUBSTATION_KEY_ITEM_UNUSABLE    = 0,
};

extern WorldCollisionGrid    D_neo_ark_substation_8017E8A4[1];
extern WorldCollisionTrigger D_neo_ark_substation_8017FC5C[12];
extern WorldCollisionTrigger D_neo_ark_substation_8017FFEC[10];
extern WorldCoordRoomLights  D_neo_ark_substation_8017FC44[1];

TaskMessageEntry D_neo_ark_substation_8017E294[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_substation_8017D724 },
    { NEO_ARK_SUBSTATION_MESSAGE_USE_KEY_ITEM, _neoArkSubstationRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkSubstationIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_substation_8017D768 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_neo_ark_substation_8017E2BC[1] = {
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_substation_8017D608, { .value = 0 } },
};

RoomAmbienceEntry D_neo_ark_substation_8017E2C8[9] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { -5, 0, 80, 0 },
    { 6, 0, 76, 0 },
    { -14, 0, 56, 0 },
    { 9, 0, 36, 0 },
    { -12, 0, 12, 0 },
    { 12, 0, 0, 0 },
    { 14, 0, 16, 0 },
};

SVECTOR D_neo_ark_substation_8017E310[4] = {
    { 1320, -220, 75, 0 },
    { 1680, -220, 75, 0 },
    { 3320, -220, 75, 0 },
    { 3680, -220, 75, 0 },
};

SVECTOR D_neo_ark_substation_8017E330[4] = {
    { 5320, -220, 75, 0 },
    { 5680, -220, 75, 0 },
    { 7320, -220, 75, 0 },
    { 7680, -220, 75, 0 },
};

SVECTOR D_neo_ark_substation_8017E350[2] = {
    { 9065, -220, -1320, 0 },
    { 9065, -220, -1680, 0 },
};

SVECTOR D_neo_ark_substation_8017E360[4] = {
    { 9065, -220, -3320, 0 },
    { 9065, -220, -3680, 0 },
    { 9065, -220, -5320, 0 },
    { 9065, -220, -5680, 0 },
};

SVECTOR D_neo_ark_substation_8017E380[14] = {
    { 9065, -220, -7320, 0 },
    { 9065, -220, -7680, 0 },
    { 1500, -2440, -1170, 0 },
    { 1500, -2440, -1830, 0 },
    { 3500, -2440, -1170, 0 },
    { 3500, -2440, -1830, 0 },
    { 8560, -2090, -1175, 0 },
    { 8560, -2090, -1835, 0 },
    { 8560, -2090, -3175, 0 },
    { 8560, -2090, -3835, 0 },
    { 8560, -2090, -5175, 0 },
    { 8560, -2090, -5835, 0 },
    { 8560, -2090, -7175, 0 },
    { 8560, -2090, -7835, 0 },
};

WorldCollisionRoomResources D_neo_ark_substation_8017E3F0[1] = {
    { D_neo_ark_substation_8017E8A4, D_neo_ark_substation_8017FC5C, D_neo_ark_substation_8017FFEC, NULL },
};

WorldCoordRoomLighting D_neo_ark_substation_8017E400[1] = {
    { D_neo_ark_substation_8017FC44, NULL },
};

u8* D_neo_ark_substation_8017E408[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_substation_8017E40C[1] = { 8 };

DirectionWarpEntry D_neo_ark_substation_8017E410[2] = {
    { { { .word = 1024 }, 514, 0, -1440 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 514, 0, -1440 }, { 0, 0, 0, 0 }, 0x55210002, 0x55210001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_ALTAR },
    { { { .word = 3072 }, 3720, 0, -1580 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 3720, 0, -1580 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkSubstationCollision012E4Normals[8] = {
#include "assets/neo_ark_substation_collision_012E4_normals.inc"
};

static SVECTOR _gNeoArkSubstationCollision012E4Verts[42] = {
#include "assets/neo_ark_substation_collision_012E4_verts.inc"
};

static WorldCollisionGridFace _gNeoArkSubstationCollision012E4Faces[25] = {
#include "assets/neo_ark_substation_collision_012E4_faces.inc"
};

static s16 _gNeoArkSubstationCollision012E4Cells[156] = {
#include "assets/neo_ark_substation_collision_012E4_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkSubstationCollision012E4Cells[i])
static s16* _gNeoArkSubstationCollision012E4Table[12] = {
#include "assets/neo_ark_substation_collision_012E4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_substation_8017E8A4[1] = {
    { NULL, _gNeoArkSubstationCollision012E4Normals, _gNeoArkSubstationCollision012E4Verts, _gNeoArkSubstationCollision012E4Faces, _gNeoArkSubstationCollision012E4Table, 100, 0x2EE0, 3, 4, 4000, 25 },
};

ViewCamera D_neo_ark_substation_8017E8C8[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x55F0, 6000 } }, 289 },
    { { { { -1290, 0, 3887 }, { 3321, 2127, 1102 }, { -2019, 3500, -670 } }, { -2841, 4068, 609 } }, 257 },
    { { { { -806, 0, -4015 }, { -3023, 2695, 606 }, { 2643, 3083, -530 } }, { -341, 3748, 1039 } }, 257 },
    { { { { -170, 0, 4092 }, { -439, 4072, -18 }, { -4068, -439, -169 } }, { -8481, 628, 299 } }, 257 },
    { { { { -191, 0, -4091 }, { 356, 4080, -16 }, { 4076, -356, -190 } }, { -3691, 728, 299 } }, 257 },
    { { { { 4075, 0, 408 }, { -9, 4094, 92 }, { -407, -92, 4074 } }, { -8751, 828, 5429 } }, 246 },
    { { { { -4063, 0, 513 }, { 11, 4094, 91 }, { -513, 92, -4062 } }, { -8751, 1028, 1029 } }, 246 },
    { { { { -3921, 0, 1182 }, { 817, 2960, 2710 }, { -854, 2830, -2834 } }, { -8751, 2868, 4999 } }, 246 },
};

SpriteBatch D_neo_ark_substation_8017E9E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_substation_8017E9F8[45] = {
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -120, 304, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -88, 334, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -56, 391, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 120, -16, 478, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 120, 16, 379, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -120, 509, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -96, 531, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 88, -56, 407, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -24, 477, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, 56, 458, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, 16, 405, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 80, 88, 431, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 48, 470, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 56, 72, 443, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 48, 104, 423, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, 0, 403, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, -32, 554, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -72, 615, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 56, -104, 532, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, 0, 435, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, 48, 470, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 64, 451, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 88, 434, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 104, 422, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 0, 413, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 0, -8, 412, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -32, -16, 431, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -56, -24, 428, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, -32, 439, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, -32, 445, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, -40, 452, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -120, -40, 460, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, -48, 453, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -48, 782, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 24, -104, 546, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, -112, 557, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, -112, 566, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -40, -120, 578, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -56, -120, 587, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, -120, 601, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 704, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, -120, 705, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -88, 632, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -144, -88, 648, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 104, 425, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_substation_8017ED7C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 45, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_substation_8017ED94[75] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -24, 300, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -120, 303, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -144, -120, 309, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 313, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -96, -120, 305, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -104, 304, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -152, -80, 300, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, -80, 303, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, -88, 300, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, -64, 314, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, -64, 300, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -144, -64, 311, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -64, 303, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -40, 322, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -40, 314, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, -40, 300, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -16, 300, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 0, 315, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 0, 321, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 24, 309, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 56, 301, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -144, 56, 292, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 64, 289, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 280, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 88, 367, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, 48, 299, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, 48, 309, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 40, 313, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -24, 40, 318, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 32, 315, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 24, 32, 327, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 24, 330, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 24, 332, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, 16, 329, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 16, 343, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, 8, 339, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, -120, 306, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -112, 317, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -120, 316, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -112, 315, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -24, -120, 309, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -112, 309, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, -120, 301, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -112, 317, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -120, 745, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, -104, 755, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -120, 300, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 80, -120, 757, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 104, -120, 307, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 128, -120, 311, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -72, -96, 314, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -72, -72, 312, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -48, -96, 300, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -48, -72, 306, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, -96, 300, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, -72, 493, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 0, -72, 324, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, -96, 300, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, -96, 317, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, -72, 314, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -96, 310, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, -96, 315, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -96, 300, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 120, -96, 311, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 144, -96, 300, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 48, -72, 529, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, -72, 310, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 96, -72, 315, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, -56, 315, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 120, -72, 300, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 144, -72, 300, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 120, -48, 300, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -48, 300, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 0, 300, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -24, 300, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_substation_8017F370[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 75, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_substation_8017F388[19] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -64, 555, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -32, 666, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -112, -104, 581, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, -88, 547, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, -96, 548, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -112, -80, 611, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, -120, 575, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -104, 807, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -72, 898, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, -72, 675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -56, 1003, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -32, 811, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, -48, 700, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, -32, 550, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -112, -40, 721, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -112, -48, 705, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -112, -56, 659, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, -64, 588, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -112, -72, 651, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_substation_8017F504[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_substation_8017F51C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_neo_ark_substation_8017F52C[2] = {
    { { 0, 0, 194, 186 }, 975 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_neo_ark_substation_8017F540[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_neo_ark_substation_8017F550[2] = {
    { { 136, 0, 184, 172 }, 1075 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_neo_ark_substation_8017F564[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_substation_8017F574[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_substation_8017F584[8] = {
    { { .empty = D_neo_ark_substation_8017E9E8 }, D_neo_ark_substation_8017E9E8, NULL },
    { { .elements = D_neo_ark_substation_8017E9F8 }, D_neo_ark_substation_8017ED7C, NULL },
    { { .elements = D_neo_ark_substation_8017ED94 }, D_neo_ark_substation_8017F370, NULL },
    { { .elements = D_neo_ark_substation_8017F388 }, D_neo_ark_substation_8017F504, NULL },
    { { .empty = D_neo_ark_substation_8017F51C }, D_neo_ark_substation_8017F51C, D_neo_ark_substation_8017F52C },
    { { .empty = D_neo_ark_substation_8017F540 }, D_neo_ark_substation_8017F540, D_neo_ark_substation_8017F550 },
    { { .empty = D_neo_ark_substation_8017F564 }, D_neo_ark_substation_8017F564, NULL },
    { { .empty = D_neo_ark_substation_8017F574 }, D_neo_ark_substation_8017F574, NULL },
};

WorldCoordPointLight D_neo_ark_substation_8017F5E4[17] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7420, -220, -60 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 1001, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5500, -220, -60 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 1021, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3500, -220, -60 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1500, -220, -60 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3500, -2370, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2252, 2867, 3276 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4000, -1300, -0x2904 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1925, 2048 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1690, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3891, 3686 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7500, -1690, -6500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3891, 3686 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8560, -2060, -7500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8560, -2060, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1500, -2370, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2252, 2867, 3276 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8920, -220, -1386 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 1038, 1600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8920, -220, -3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 580, 2261 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8920, -220, -5500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8920, -220, -7320 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 1882, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8560, -2060, -3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 3891, 4096 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8560, -2060, -5500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, 4096 }, { 0, 0 } }, 500, 1000 },
};

WorldCoordRoomLights D_neo_ark_substation_8017FC44[1] = {
    { 0, NULL, ARRAY_SIZE(D_neo_ark_substation_8017F5E4), D_neo_ark_substation_8017F5E4, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_substation_8017FC5C[12] = {
    { NULL, NULL, NULL, { 1887, -1632, -1521, 0 }, { { 206, -2256, 2489, 0 }, { -205, -2256, -2489, 0 }, { 206, 2256, 2489, 0 }, { -205, 2256, -2489, 0 } }, { -4091, 0, 337, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1759, -1632, -1472, 0 }, { { -205, -2256, -2489, 0 }, { 206, -2256, 2489, 0 }, { -205, 2256, -2489, 0 }, { 206, 2256, 2489, 0 } }, { 4089, 0, -338, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4015, -1633, 64, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4095, -1568, 32, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5728, -1632, 32, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5600, -1632, 32, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8608, -1568, -321, 0 }, { { -894, -2256, -814, 0 }, { 894, -2256, 814, 0 }, { -894, 2256, -814, 0 }, { 894, 2256, 814, 0 } }, { 2760, 0, -3033, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8672, -1568, -385, 0 }, { { 894, -2256, 814, 0 }, { -894, -2256, -814, 0 }, { 894, 2256, 814, 0 }, { -894, 2256, -814, 0 } }, { -2762, 0, 3031, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8607, -1696, -3649, 0 }, { { 1209, -2256, -4, 0 }, { -1208, -2256, 5, 0 }, { 1209, 2256, -4, 0 }, { -1208, 2256, 5, 0 } }, { 13, 0, 4098, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8574, -1632, -3554, 0 }, { { -1208, -2256, 5, 0 }, { 1209, -2256, -4, 0 }, { -1208, 2256, 5, 0 }, { 1209, 2256, -4, 0 } }, { -16, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8511, -1664, -5858, 0 }, { { -1209, -2256, -1, 0 }, { 1209, -2256, 1, 0 }, { -1209, 2256, -1, 0 }, { 1209, 2256, 1, 0 } }, { 3, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8543, -1632, -5921, 0 }, { { 1209, -2256, -1, 0 }, { -1209, -2256, 1, 0 }, { 1209, 2256, -1, 0 }, { -1209, 2256, 1, 0 } }, { 3, 0, 4099, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_substation_8017FFEC[10] = {
    { NULL, NULL, NULL, { 3744, -64, -1520, 0 }, { { -320, 0, -336, 0 }, { 320, 0, -336, 0 }, { -320, 0, 336, 0 }, { 320, 0, 336, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 463, WORLD_COLLISION_TRIGGER_ACTION_FACING, 37, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4064, -5056, -480, 0 }, { { -336, 0, -496, 0 }, { 336, 0, -496, 0 }, { -336, 0, 496, 0 }, { 336, 0, 496, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 596, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 5, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4607, -769, -1600, 0 }, { { -16, -1391, -496, 0 }, { 17, 1392, -496, 0 }, { -16, -1391, 496, 0 }, { 17, 1392, 496, 0 } }, { -4101, 42, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 16, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4927, -4673, -480, 0 }, { { -17, 1392, 496, 0 }, { 16, -1391, 496, 0 }, { -17, 1392, -496, 0 }, { 16, -1391, -496, 0 } }, { -4101, -49, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 418, -48, -1536, 0 }, { { -320, 0, -496, 0 }, { 320, 0, -496, 0 }, { -320, 0, 496, 0 }, { 320, 0, 496, 0 } }, { 0, 4113, 0, 0 }, { 4096, 0, 0, 0 }, 590, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7920, -64, -6528, 0 }, { { -976, 0, -560, 0 }, { 976, 0, -560, 0 }, { -976, 0, 560, 0 }, { 976, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8448, -64, -7648, 0 }, { { -560, 0, -336, 0 }, { 560, 0, -336, 0 }, { -560, 0, 336, 0 }, { 560, 0, 336, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 652, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8256, -64, -3952, 0 }, { { -432, 0, -1888, 0 }, { 432, 0, -1888, 0 }, { -432, 0, 1888, 0 }, { 432, 0, 1888, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 1932, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8288, -64, -7568, 0 }, { { -432, 0, -336, 0 }, { 432, 0, -336, 0 }, { -432, 0, 336, 0 }, { 432, 0, 336, 0 } }, { 0, 4113, 0, 0 }, { 4096, 0, 0, 0 }, 546, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1936, -64, -2720, 0 }, { { -2064, 0, -336, 0 }, { 2064, 0, -336, 0 }, { -2064, 0, 336, 0 }, { 2064, 0, 336, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 2079, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_substation_801802E4 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_neo_ark_substation_801802F0 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionFootstepSounds D_neo_ark_substation_801802FC = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionSurfaceProperties D_neo_ark_substation_80180308[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_substation_80180310[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_substation_801802FC },
};

WorldCollisionSurfaceProperties D_neo_ark_substation_80180318[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_substation_801802E4 },
};

WorldCollisionSurfaceProperties D_neo_ark_substation_80180320[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_substation_801802F0 },
};

WorldCollisionSurfaceProperties* D_neo_ark_substation_80180328[8] = {
    D_neo_ark_substation_80180308,
    D_neo_ark_substation_80180308,
    D_neo_ark_substation_80180310,
    D_neo_ark_substation_80180318,
    D_neo_ark_substation_80180320,
    D_neo_ark_substation_80180308,
    D_neo_ark_substation_80180308,
    D_neo_ark_substation_80180308,
};

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Keeps the substation's looping ambience in step with the area the session is
/// in: `gGameSession->location.loc.view` selects one of the room's nine `(panOffset, attenuation)`
/// entries, and state 0 starts that loop with `sndEvtRequestScriptStart`. States 1
/// through 4 then watch for the session's index to stop matching the area
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` publishes - state 1 tests the pair and 2, 3 and 4 walk the task
/// along - and state 5 updates the playing loop's pan and attenuation from
/// the new view's table entry with `sndEvtRequestScriptMix`, then returns
/// to state 1 to keep watching.
void func_neo_ark_substation_8017D608(Task* task)
{
    s32 pan;
    s32 attenuation;
    u8  idx;

    idx = gGameSession->location.loc.view;
    if (idx < ARRAY_SIZE(D_neo_ark_substation_8017E2C8)) {
        pan         = D_neo_ark_substation_8017E2C8[idx].panOffset;
        attenuation = D_neo_ark_substation_8017E2C8[idx].attenuation;
    } else {
        pan         = 0;
        attenuation = 0;
    }

    switch (task->state) {
        case 0:
            sndEvtRequestScriptStart(SOUND_NEO_ARK_SUBSTATION_AMBIENCE, (s8)pan, (s8)attenuation);
            task->state = task->state + 1;
            break;
        case 1:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != gGameSession->location.loc.view) {
                task->state = task->state + 1;
            }
            break;
        case 2:
        case 3:
        case 4:
            task->state = task->state + 1;
            break;
        case 5:
            sndEvtRequestScriptMix(SOUND_NEO_ARK_SUBSTATION_AMBIENCE, (s8)pan, (s8)attenuation);
            task->state = 1;
            break;
    }
}

/// Refuses every key-item use in this room with the cannot-use reply.
///
/// `itemId` is the selected inventory item ID; all arguments are ignored.
static s32 _neoArkSubstationRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return NEO_ARK_SUBSTATION_KEY_ITEM_UNUSABLE;
}

/// Handler the room's message table gives message 0x13EE: copies the incoming
/// `RoomEventMsg` onto the outgoing one and passes both on to `mapNeoArkResolveRoomVariant`.
/// Always returns 1.
s32 func_neo_ark_substation_8017D724(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapNeoArkResolveRoomVariant(in, out);
    return 1;
}

s32 func_neo_ark_substation_8017D768(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) != 0 ? 3 : 5);
    }
    return 0;
}

/// Ignores room-action requests and returns zero without changing room state.
///
/// `request` is borrowed for synchronous dispatch and is never read or retained.
/// The receiver, message ID and unused second payload are also ignored.
static s32 _neoArkSubstationIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    return 0;
}

/// State 0 of the room's message task: park the room's message table in
/// `Task::msgTable`, publish the task in pointer slot 7, start the ambience
/// task (`func_neo_ark_substation_8017D608`) only while game flag 0xDF is
/// clear, and advance to state 1.
static void func_neo_ark_substation_8017D7AC(Task* task)
{
    task->msgTable = D_neo_ark_substation_8017E294;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
        taskSpawnFromTable(D_neo_ark_substation_8017E2BC, 0, 0, 0);
    }
    task->state = (s32)(task->state + 1);
}

/// Keeps the initialized room message task idle while its message table remains available.
///
/// Leaves the task's state and lifetime unchanged; synchronous messages do the work.
static void _neoArkSubstationMessageTaskIdle(Task* unusedTask)
{
}

/// Runs the room's message task's current state through a stack copy of
/// `D_neo_ark_substation_8017D5C4`.
void func_neo_ark_substation_8017D81C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_substation_8017D5C4;
    sp.funcs[task->state](task);
}

void neoArkSubstationDrawLightGlowsTask(Task* unusedTask)
{
    // The capsule drawer expands each RGB444 nibble by 16 and flickers by 8.
    enum {
        NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE  = 512, // Pixel radius = scale * 64 / (camera Z / 4)
        NEO_ARK_SUBSTATION_GLOW_DIM_RGB444    = 0x222,
        NEO_ARK_SUBSTATION_GLOW_MEDIUM_RGB444 = 0x333,
        NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444 = 0x444,
    };
    u8 mappedView;

    // Keep one endpoint base per view; some pairs occupy the following tables.
    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2: {
            const SVECTOR* lightPoints = D_neo_ark_substation_8017E310;
            _glowDrawCapsule(&lightPoints[0], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            break;
        }
        case 3: {
            const SVECTOR* lightPoints = D_neo_ark_substation_8017E310;
            _glowDrawCapsule(&lightPoints[0], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            _glowDrawCapsule(&lightPoints[2], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            _glowDrawCapsule(&lightPoints[4], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            _glowDrawCapsule(&lightPoints[6], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            break;
        }
        case 4: {
            const SVECTOR* lightPoints = D_neo_ark_substation_8017E310;
            _glowDrawCapsule(&lightPoints[0], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_DIM_RGB444);
            _glowDrawCapsule(&lightPoints[2], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_MEDIUM_RGB444);
            _glowDrawCapsule(&lightPoints[4], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            _glowDrawCapsule(&lightPoints[6], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            _glowDrawCapsule(&lightPoints[16], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_DIM_RGB444);
            _glowDrawCapsule(&lightPoints[18], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_MEDIUM_RGB444);
            break;
        }
        case 5: {
            const SVECTOR* lightPoints = D_neo_ark_substation_8017E330;
            _glowDrawCapsule(&lightPoints[0], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            _glowDrawCapsule(&lightPoints[2], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_MEDIUM_RGB444);
            break;
        }
        case 6: {
            const SVECTOR* lightPoints = D_neo_ark_substation_8017E350;
            _glowDrawCapsule(&lightPoints[0], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_MEDIUM_RGB444);
            _glowDrawCapsule(&lightPoints[2], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            _glowDrawCapsule(&lightPoints[12], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            break;
        }
        case 7: {
            const SVECTOR* lightPoints = D_neo_ark_substation_8017E360;
            _glowDrawCapsule(&lightPoints[0], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            _glowDrawCapsule(&lightPoints[2], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_MEDIUM_RGB444);
            _glowDrawCapsule(&lightPoints[4], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_DIM_RGB444);
            _glowDrawCapsule(&lightPoints[12], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            _glowDrawCapsule(&lightPoints[14], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_MEDIUM_RGB444);
            _glowDrawCapsule(&lightPoints[16], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_DIM_RGB444);
            break;
        }
        case 8: {
            const SVECTOR* lightPoints = D_neo_ark_substation_8017E380;
            _glowDrawCapsule(&lightPoints[0], NEO_ARK_SUBSTATION_GLOW_RADIUS_SCALE, NEO_ARK_SUBSTATION_GLOW_BRIGHT_RGB444);
            break;
        }
    }
}

#include "../../shared/glow_draw_capsule.inc.c"
