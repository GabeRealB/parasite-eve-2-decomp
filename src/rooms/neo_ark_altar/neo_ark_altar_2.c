#include "rooms/neo_ark_altar.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"

#include "neo_ark_altar_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// End marker of an altar-tile table, stored in `_NeoArkAltarTile::id`.
///
/// A scan stops on the entry that carries this value and does not test that
/// entry's rectangle.
enum { NEO_ARK_ALTAR_TILE_END = -1 };

/// One floor pad of the Neo Ark altar.
///
/// An axis-aligned rectangle in world units, the same units as an actor's
/// matrix translation, plus the pad's number. Both edges belong to the pad.
/// A table ends with an entry whose `id` is `NEO_ARK_ALTAR_TILE_END`; the
/// other fields of that entry are zero and are not read. The four pads are
/// numbered 1, 2, 3 and 4. A point on no pad is reported as 0, which is not
/// an `id` stored in the table.
typedef struct {
    s16 x;     // Starting X in world units
    s16 z;     // Starting Z in world units
    s16 width; // Extent along +X in world units
    s16 depth; // Extent along +Z in world units
    s16 id;    // Pad number (1, 2, 3 or 4), or NEO_ARK_ALTAR_TILE_END
    u16 pad;   // Unread. Zero in every entry; keeps each record 12 bytes
} _NeoArkAltarTile;
STATIC_ASSERT_SIZEOF(_NeoArkAltarTile, 0xC);

/// Height the walls around an altar tile ease towards while they are raised,
/// in world units.
enum { NEO_ARK_ALTAR_WALL_HEIGHT_FULL = 3000 };

/// Work block of the altar's tile-sequence task.
///
/// Allocated zeroed when the task starts and kept in `Task::work`. While the
/// player walks the floor tiles, it follows which tile the player is on from
/// one frame to the next and the walls that rise around a tile. A tile here is
/// an `id` of `_NeoArkAltarTile` (1, 2, 3 or 4), with 0 standing for no tile;
/// `wallTileIndex` instead counts the same four tiles from 0, the way their
/// table is indexed.
typedef struct {
    Task* movieLauncher; // Task spawned to start the movie that follows the second solved sequence; stored and not read back
    u8    field_4[2];    // Never accessed; role and type unproven
    s16   previousTile;  // `currentTile` as it was on the previous frame
    s16   currentTile;   // Tile the player stands on, 0 when on none
    s16   wallHeight;    // How far the walls of tile `wallTileIndex` rise above the floor, in world units: eases towards NEO_ARK_ALTAR_WALL_HEIGHT_FULL while they are raised and back to 0 afterwards
    u16   wallTileIndex; // Tile whose walls were raised last, as an index 0 to 3
    s16   enteredTile;   // Tile the player stepped onto this frame from no tile, 0 on every other frame
} _NeoArkAltarTileSequenceWork;
STATIC_ASSERT_SIZEOF(_NeoArkAltarTileSequenceWork, 0x10);

extern _NeoArkAltarTile D_neo_ark_altar_8017F014[];

extern s16 D_neo_ark_altar_801800AC;
extern s16 D_neo_ark_altar_801800AE;
extern s16 D_neo_ark_altar_801800B0[];

extern s16 D_neo_ark_altar_8017F050[];
extern s16 D_neo_ark_altar_8017F068[];

extern _NeoArkAltarTile D_neo_ark_altar_8017EFD8[];
extern AreaApplyRec     D_neo_ark_altar_8018007C[];

static void func_neo_ark_altar_8017E658(SVECTOR* p0, SVECTOR* p1, SVECTOR* p2, SVECTOR* p3);
static s16  func_neo_ark_altar_8017EC34(_NeoArkAltarTile* table, s16 x, s16 z);
static s16  func_neo_ark_altar_8017E260(Task* task);
static void func_neo_ark_altar_8017E92C(s16 arg0, s32 arg1);

extern WorldCollisionGrid    D_neo_ark_altar_8017F57C[1];
extern WorldCollisionTrigger D_neo_ark_altar_8017FF08[4];
extern WorldCoordRoomLights  D_neo_ark_altar_8017FEF0[1];
void                         func_neo_ark_altar_8017ECE0(Task*);

void func_neo_ark_altar_8017DA40(Task*);
void func_neo_ark_altar_8017DBF0(Task*);

TaskDesc D_neo_ark_altar_8017EFC0[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_neo_ark_altar_8017DBF0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_neo_ark_altar_8017DA40, { .value = 0 } },
};

_NeoArkAltarTile D_neo_ark_altar_8017EFD8[5] = {
    { 8640, -9840, 700, 700, 1, 0 },
    { 8640, -5830, 700, 700, 2, 0 },
    { 0x2C9C, -9870, 700, 750, 3, 0 },
    { 0x2C92, -5930, 700, 750, 4, 0 },
    { 0, 0, 0, 0, NEO_ARK_ALTAR_TILE_END, 0 },
};

_NeoArkAltarTile D_neo_ark_altar_8017F014[5] = {
    { 8640, -9840, 700, 700, 1, 0 },
    { 8640, -5830, 700, 700, 2, 0 },
    { 0x2C9C, -9870, 700, 700, 3, 0 },
    { 0x2C92, -5930, 700, 700, 4, 0 },
    { 0, 0, 0, 0, NEO_ARK_ALTAR_TILE_END, 0 },
};

s16 D_neo_ark_altar_8017F050[12] = {
    3,
    4,
    1,
    2,
    1,
    2,
    3,
    4,
    2,
    1,
    4,
    3,
};

s16 D_neo_ark_altar_8017F068[16] = {
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    3,
    3,
    3,
    4,
    4,
    4,
    4,
    4,
};

TaskDesc D_neo_ark_altar_8017F088[1] = {
    { { { TASK_BODY_NONE, 32 } }, func_neo_ark_altar_8017ECE0, { .value = 0 } },
};

WorldCollisionRoomResources D_neo_ark_altar_8017F094[3] = {
    { D_neo_ark_altar_8017F57C, NULL, D_neo_ark_altar_8017FF08, NULL },
    { D_neo_ark_altar_8017F57C, NULL, D_neo_ark_altar_8017FF08, NULL },
    { D_neo_ark_altar_8017F57C, NULL, D_neo_ark_altar_8017FF08, NULL },
};

WorldCoordRoomLighting D_neo_ark_altar_8017F0C4[3] = {
    { D_neo_ark_altar_8017FEF0, NULL },
    { D_neo_ark_altar_8017FEF0, NULL },
    { D_neo_ark_altar_8017FEF0, NULL },
};

u8 D_neo_ark_altar_8017F0DC[8] = {
    1,
    4,
    3,
    2,
    5,
    6,
    7,
    8,
};

u8 D_neo_ark_altar_8017F0E4[8] = {
    1,
    7,
    3,
    4,
    5,
    6,
    2,
    8,
};

u8* D_neo_ark_altar_8017F0EC[3] = {
    gViewIdentityMap,
    D_neo_ark_altar_8017F0DC,
    D_neo_ark_altar_8017F0E4,
};

ViewCount D_neo_ark_altar_8017F0F8[2] = { 8, 8 };

DirectionWarpEntry D_neo_ark_altar_8017F0FC[1] = {
    { { { .word = 1024 }, 7533, -3600, -7520 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -2944, 0, -2035 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkAltarCollision01FBCNormals[9] = {
#include "assets/neo_ark_altar_collision_01FBC_normals.inc"
};

static SVECTOR _gNeoArkAltarCollision01FBCVerts[58] = {
#include "assets/neo_ark_altar_collision_01FBC_verts.inc"
};

static WorldCollisionGridFace _gNeoArkAltarCollision01FBCFaces[29] = {
#include "assets/neo_ark_altar_collision_01FBC_faces.inc"
};

static s16 _gNeoArkAltarCollision01FBCCells[94] = {
#include "assets/neo_ark_altar_collision_01FBC_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkAltarCollision01FBCCells[i])
static s16* _gNeoArkAltarCollision01FBCTable[6] = {
#include "assets/neo_ark_altar_collision_01FBC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_altar_8017F57C[1] = {
    { NULL, _gNeoArkAltarCollision01FBCNormals, _gNeoArkAltarCollision01FBCVerts, _gNeoArkAltarCollision01FBCFaces, _gNeoArkAltarCollision01FBCTable, -2640, 0x2710, 3, 2, 4000, 29 },
};

ViewCamera D_neo_ark_altar_8017F5A0[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5270, 0x7530, 7060 } }, 380 },
    { { { { -37, 0, -4095 }, { -3560, 2024, 32 }, { 2024, 3560, -18 } }, { -7700, 0x27DF, 7433 } }, 230 },
    { { { { -1439, 0, -3834 }, { 422, 4071, -158 }, { 3811, -450, -1431 } }, { -0x2807, 5092, 6594 } }, 230 },
    { { { { -37, 0, -4095 }, { -3560, 2024, 32 }, { 2024, 3560, -18 } }, { -7700, 0x27DF, 7433 } }, 230 },
    { { { { -1246, 0, -3901 }, { 654, 4037, -209 }, { 3846, -687, -1229 } }, { -0x2B94, 4629, 7012 } }, 312 },
    { { { { -37, 0, -4095 }, { -3560, 2024, 32 }, { 2024, 3560, -18 } }, { -7700, 0x27DF, 7433 } }, 230 },
    { { { { -1439, 0, -3834 }, { 422, 4071, -158 }, { 3811, -450, -1431 } }, { -5375, 4872, 5719 } }, 230 },
    { { { { -1439, 0, -3834 }, { 422, 4071, -158 }, { 3811, -450, -1431 } }, { -5375, 4872, 5719 } }, 230 },
};

SpriteBatch D_neo_ark_altar_8017F6C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_altar_8017F6D0[24] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 72, 1380, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 72, 1371, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 72, 1380, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 72, 1370, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 80, 1357, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 80, 1354, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 80, 1357, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 80, 1359, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 88, 1333, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 88, 1328, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 88, 1333, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 88, 1339, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 96, 1336, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 96, 1334, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 96, 1338, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 96, 1334, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 104, 1338, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 104, 1337, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 104, 1340, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 104, 1339, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 112, 1344, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 112, 1342, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 112, 1346, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 112, 1345, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_altar_8017F8B0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_altar_8017F8D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_altar_8017F8E0[33] = {
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -8, -64, 1802, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 72, 1380, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 72, 1371, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 72, 1380, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 72, 1370, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 80, 1357, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 80, 1354, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 80, 1357, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 80, 1359, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 88, 1333, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 88, 1328, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 88, 1333, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 88, 1339, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 96, 1336, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 96, 1334, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 96, 1338, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 96, 1334, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 104, 1338, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 104, 1337, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 104, 1340, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 104, 1339, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 112, 1344, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 112, 1342, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 112, 1346, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 112, 1345, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -40, -120, 1438, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -40, -112, 1267, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -40, -104, 1349, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, -96, 1441, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, -88, 1485, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, -80, 1541, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, -72, 1584, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, -64, 1652, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_altar_8017FB74[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_altar_8017FB94[6] = {
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -8, -8, 386, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 72 } }, -8, -8, 389, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 56, 72 } }, -8, -8, 391, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 56, 72 } }, -8, -8, 394, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x40C0, { .fields = { 56, 72 } }, -8, -8, 396, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4100, { .fields = { 56, 72 } }, -8, -8, 399, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_altar_8017FC0C[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { 1, 1, 0, 0, { 1, 0 } },
    { 2, 1, 0, 0, { 2, 0 } },
    { 3, 1, 0, 0, { 3, 0 } },
    { 4, 1, 0, 0, { 4, 0 } },
    { 5, 1, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_altar_8017FC4C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_altar_8017FC5C[21] = {
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -32, 16, 1897, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 64, 839, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 72, 693, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 40, 1012, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 40, 982, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 40, 936, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, 40, 891, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 48, 869, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 48, 888, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 56, 863, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 48, 826, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, 48, 717, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 48, 724, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 48, 728, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, 56, 731, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 32, 1887, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 32, 1833, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 32, 1887, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -64, -48, 1922, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -40, -48, 1922, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -16, -48, 1922, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_altar_8017FE00[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 14, 0, 0, { 2, 0 } },
    { 15, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_altar_8017FE28[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_altar_8017FE38[8] = {
    { { .empty = D_neo_ark_altar_8017F6C0 }, D_neo_ark_altar_8017F6C0, NULL },
    { { .elements = D_neo_ark_altar_8017F6D0 }, D_neo_ark_altar_8017F8B0, NULL },
    { { .empty = D_neo_ark_altar_8017F8D0 }, D_neo_ark_altar_8017F8D0, NULL },
    { { .elements = D_neo_ark_altar_8017F8E0 }, D_neo_ark_altar_8017FB74, NULL },
    { { .elements = D_neo_ark_altar_8017FB94 }, D_neo_ark_altar_8017FC0C, NULL },
    { { .empty = D_neo_ark_altar_8017FC4C }, D_neo_ark_altar_8017FC4C, NULL },
    { { .elements = D_neo_ark_altar_8017FC5C }, D_neo_ark_altar_8017FE00, NULL },
    { { .empty = D_neo_ark_altar_8017FE28 }, D_neo_ark_altar_8017FE28, NULL },
};

WorldCoordLight D_neo_ark_altar_8017FE98[1] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4000, -0x3A98, 8000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4915, 4915, 4915 }, { 0, 0 } },
};

WorldCoordRoomLights D_neo_ark_altar_8017FEF0[1] = {
    { ARRAY_SIZE(D_neo_ark_altar_8017FE98), D_neo_ark_altar_8017FE98, 0, NULL, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_altar_8017FF08[4] = {
    { NULL, NULL, NULL, { 7645, -3808, -7473, 0 }, { { 14, -5344, -974, 0 }, { -13, -5344, 975, 0 }, { 14, 5344, -974, 0 }, { -13, 5344, 975, 0 } }, { 4109, 0, 56, 0 }, { 4076, 0, -401, 0 }, 5418, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 32, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8656, -4256, -7488, 0 }, { { 208, 14, -974, 0 }, { 208, -13, 975, 0 }, { -208, 14, -974, 0 }, { -208, -13, 975, 0 } }, { 0, 4103, 41, 0 }, { 4096, 0, 0, 0 }, 995, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 74, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7216, -3804, -7488, 0 }, { { 688, -2, -974, 0 }, { 688, 3, 975, 0 }, { -688, -2, -974, 0 }, { -688, 3, 975, 0 } }, { 0, 4104, -13, 0 }, { -4096, 0, -5, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_FACING | WORLD_COLLISION_TRIGGER_AUTOMATIC, 67, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2FC0, -4256, -7552, 0 }, { { 416, 14, -974, 0 }, { 416, -13, 975, 0 }, { -416, 14, -974, 0 }, { -416, -13, 975, 0 } }, { 0, 4099, 51, 0 }, { -4096, 0, 0, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_altar_80180038 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionSurfaceProperties D_neo_ark_altar_80180044[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_altar_8018004C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_altar_80180054[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_altar_80180038 },
};

WorldCollisionSurfaceProperties* D_neo_ark_altar_8018005C[8] = {
    D_neo_ark_altar_80180044,
    D_neo_ark_altar_8018004C,
    D_neo_ark_altar_80180044,
    D_neo_ark_altar_80180044,
    D_neo_ark_altar_80180054,
    D_neo_ark_altar_80180044,
    D_neo_ark_altar_80180044,
    D_neo_ark_altar_80180044,
};

AreaApplyRec D_neo_ark_altar_8018007C[9] = {
    { 5, 11, 3, 1 },
    { 5, 13, 3, 1 },
    { 5, 15, 3, 1 },
    { 5, 27, 3, 17 },
    { 5, 27, 11, 33 },
    { 5, 29, 2, 1 },
    { 5, 32, 2, 16 },
    { 5, 32, 11, 33 },
    { 255, 0, 0, 0 },
};

AreaApplyRec D_neo_ark_altar_801800A0[3] = {
    { 5, 17, 2, 1 },
    { 5, 18, 3, 1 },
    { 255, 0, 0, 0 },
};

s16 D_neo_ark_altar_801800AC = 0;

s16 D_neo_ark_altar_801800B0[16];

static void func_neo_ark_altar_8017ED60(Task* task);

static void func_neo_ark_altar_8017EDBC(Task* task);

static void func_neo_ark_altar_8017EDF8(Task* task);

static void func_neo_ark_altar_8017EE30(Task* task);

static void func_neo_ark_altar_8017EE90(Task* task);

static void func_neo_ark_altar_8017EF00(Task* task);

static void func_neo_ark_altar_8017EF34(Task* task);

static void func_neo_ark_altar_8017DF0C(Task* task);
static void func_neo_ark_altar_8017E148(void);

void func_neo_ark_altar_8017DA40(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state++;
            break;
        case 1:
            key = gGameSession->location;
            if (task->spawnArg1.value == 0) {
                key.loc.view = 0x64;
            } else if (task->spawnArg1.value == 1) {
                key.loc.view = 0x65;
            } else {
                key.loc.view = 0x66;
            }
            slotParam[0] = Stream_FindSlot((u8*)&key, 0, 0);
            CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state++;
            break;
        case 2:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case 3:
            if (CdCmd_IsIdle()) {
                SetDispMask(0);
                task->state++;
            } else if (Pad_CheckFlag800()) {
                SetDispMask(0);
                CdCmd_ActivatePhase1();
                task->state++;
            }
            break;
        case 4:
            if (CdCmd_IsIdle()) {
                Stream_ResetRestoreState();
                task->state++;
            }
            break;
        case 5:
            if (Stream_RestoreAfterLoad(0, 1)) {
                taskKill(task);
                Display_ResetHeapWrapper();
            }
            break;
    }
}

/// Entry 0 of `D_neo_ark_altar_8017EFC0`: spawns that table's entry 1 (the
/// streaming task `func_neo_ark_altar_8017DA40`) with an ordering table,
/// passing on this task's `spawnArg1`, sets `gDisplayState.control.flags.flipMode`, calls
/// `Gp_SpawnViewTasks` and ends.
void func_neo_ark_altar_8017DBF0(Task* arg0)
{
    Display_SpawnWithOt(D_neo_ark_altar_8017EFC0, 1, arg0->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

/// Steps the altar's switch state `D_neo_ark_altar_801800AE` by one per call,
/// in the direction `arg0` (the 0xD9 game-flag nibble) selects: 0 sets
/// `field_4` of the second command in `rec[3]` and `rec[6]` and counts up
/// towards 6, 1 clears them and counts down towards 0; any other value, or a
/// state already at the end of its range, changes nothing. Each step rewrites
/// the six commands after the first in `rec[4]` so exactly one holds 0: index
/// `6 - s`, where `s` is the lower of the old and new states.
void func_neo_ark_altar_8017DC40(s32 arg0)
{
    GameLocationKey* sess;
    SpriteView*      rec;
    SpriteBatch*     batches;

    sess  = &gGameSession->location.loc;
    rec   = Gp_SprtTables[sess->stage - 1][0].areaViews[sess->area - 1];
    arg0 &= 0xFF;
    if (arg0 == 0) {
        batches           = rec[3].batches;
        batches[1].hidden = 1;
        batches           = rec[6].batches;
        batches[1].hidden = 1;
        switch (D_neo_ark_altar_801800AE) {
            case 0:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 1;
                batches[3].hidden        = 1;
                batches[4].hidden        = 1;
                batches[5].hidden        = 1;
                batches[6].hidden        = 0;
                D_neo_ark_altar_801800AE = 1;
                break;
            case 1:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 1;
                batches[3].hidden        = 1;
                batches[4].hidden        = 1;
                batches[5].hidden        = 0;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 2;
                break;
            case 2:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 1;
                batches[3].hidden        = 1;
                batches[4].hidden        = 0;
                batches[5].hidden        = 1;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 3;
                break;
            case 3:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 1;
                batches[3].hidden        = 0;
                batches[4].hidden        = 1;
                batches[5].hidden        = 1;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 4;
                break;
            case 4:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 0;
                batches[3].hidden        = 1;
                batches[4].hidden        = 1;
                batches[5].hidden        = 1;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 5;
                break;
            case 5:
                batches                  = rec[4].batches;
                batches[1].hidden        = 0;
                batches[2].hidden        = 1;
                batches[3].hidden        = 1;
                batches[4].hidden        = 1;
                batches[5].hidden        = 1;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 6;
                break;
        }
    } else if (arg0 == 1) {
        batches           = rec[3].batches;
        batches[1].hidden = 0;
        batches           = rec[6].batches;
        batches[1].hidden = 0;
        switch (D_neo_ark_altar_801800AE) {
            case 6:
                batches                  = rec[4].batches;
                batches[1].hidden        = 0;
                batches[2].hidden        = 1;
                batches[3].hidden        = 1;
                batches[4].hidden        = 1;
                batches[5].hidden        = 1;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 5;
                break;
            case 5:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 0;
                batches[3].hidden        = 1;
                batches[4].hidden        = 1;
                batches[5].hidden        = 1;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 4;
                break;
            case 4:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 1;
                batches[3].hidden        = 0;
                batches[4].hidden        = 1;
                batches[5].hidden        = 1;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 3;
                break;
            case 3:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 1;
                batches[3].hidden        = 1;
                batches[4].hidden        = 0;
                batches[5].hidden        = 1;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 2;
                break;
            case 2:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 1;
                batches[3].hidden        = 1;
                batches[4].hidden        = 1;
                batches[5].hidden        = 0;
                batches[6].hidden        = 1;
                D_neo_ark_altar_801800AE = 1;
                break;
            case 1:
                batches                  = rec[4].batches;
                batches[1].hidden        = 1;
                batches[2].hidden        = 1;
                batches[3].hidden        = 1;
                batches[4].hidden        = 1;
                batches[5].hidden        = 1;
                batches[6].hidden        = 0;
                D_neo_ark_altar_801800AE = 0;
                break;
        }
    }
}

/// Altar state 2: records the tile the player walks onto and, while
/// `func_neo_ark_altar_8017E260` reports the altar sequence has matched, raises
/// the wall of the tile the player stands on. `func_neo_ark_altar_8017EC34`
/// resolves the player coordinate to a tile id, which is pushed onto
/// `D_neo_ark_altar_801800B0` whenever it changes; the returned sequence state
/// picks the sound and area record set for the frame and, at 3, arms
/// `var_s2`, which raises the matching tile by half the remaining distance to
/// `NEO_ARK_ALTAR_WALL_HEIGHT_FULL` per frame. With no tile raised,
/// `wallHeight` instead decays by a quarter towards 0 while `wallTileIndex`
/// still names a valid tile.
static void func_neo_ark_altar_8017DF0C(Task* task)
{
    _NeoArkAltarTileSequenceWork* work;
    GfxCoord*                     coord;
    Task*                         actor;
    s32                           prev;
    s16                           cur;
    s16                           level;
    s32                           i;
    s32                           grow;
    s16                           found;

    work               = task->work;
    actor              = *gPlayerActorTasks;
    work->previousTile = work->currentTile;
    grow               = 0;
    coord              = actor->extra.tmd->coords;
    cur                = func_neo_ark_altar_8017EC34(D_neo_ark_altar_8017EFD8, (s16)coord->coord.t[0], (s16)coord->coord.t[2]);
    prev               = work->previousTile;
    work->currentTile  = cur;
    if (cur != prev && prev == 0) {
        work->enteredTile                                  = cur;
        D_neo_ark_altar_801800B0[D_neo_ark_altar_801800AC] = work->currentTile;
        D_neo_ark_altar_801800AC                           = (u16)D_neo_ark_altar_801800AC + 1;
    } else {
        work->enteredTile = 0;
    }
    switch (func_neo_ark_altar_8017E260(task)) {
        case 1:
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_ALTAR, 0);
            sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 3), SOUND_SCRIPT_STOP_NO_FADE);
            sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED, 0, 0);
            Gp_RunCapCmd1(1);
            Gp_ApplyAreaRecs(D_neo_ark_altar_8018007C);
            break;
        case 2:
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED, 1);
            task->state = 3;
            break;
        case 3:
            grow = 1;
            break;
    }
    found = 0;
    for (i = 0; i < 4; i++) {
        if (grow == 1 && (work->currentTile - 1) == i) {
            work->wallTileIndex = i;
            /* Two dead stores: loop.c only keeps the `grow == 1` constant
               inside the loop (and so in a caller-saved register, remade in
               the back-edge delay slot) while the loop holds 30 RTL insns.
               At the 28 this body otherwise compiles to it is hoisted, which
               costs an extra saved register and an 8-byte frame. */
            level             = 0;
            level             = 1;
            work->wallHeight += (NEO_ARK_ALTAR_WALL_HEIGHT_FULL - work->wallHeight) >> 1;
            level             = work->wallHeight;
            func_neo_ark_altar_8017E92C((s16)i, level);
            found = 1;
        }
    }
    if (found == 0 && work->wallTileIndex < 4) {
        work->wallHeight += (-work->wallHeight) >> 2;
        level             = work->wallHeight;
        if (level >= 0xB) {
            func_neo_ark_altar_8017E92C(work->wallTileIndex, level);
        }
    }
}

/// Altar state 0: gates the wall sprites of the current view's record on game
/// flag 0xD9 and resets the altar's work area. The switch state written to
/// `D_neo_ark_altar_801800AE` is 6 while the flag is clear, 0 otherwise; the
/// six sprite commands reached through `rec[3]` / `rec[6]` / `rec[4]` are
/// skipped (1) or linked (0) to match, and the 17 halfwords at
/// `D_neo_ark_altar_801800B0` are cleared for `func_neo_ark_altar_8017E260`.
static void func_neo_ark_altar_8017E148(void)
{
    GameLocationKey* sess;
    SpriteView*      rec;
    SpriteBatch*     batches;
    s32              i;

    sess = &gGameSession->location.loc;
    rec  = Gp_SprtTables[sess->stage - 1][0].areaViews[sess->area - 1];
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) == 0) {
        batches                  = rec[3].batches;
        batches[1].hidden        = 1;
        batches                  = rec[6].batches;
        batches[1].hidden        = 1;
        batches                  = rec[4].batches;
        batches[1].hidden        = 0;
        batches[2].hidden        = 1;
        batches[3].hidden        = 1;
        batches[4].hidden        = 1;
        batches[5].hidden        = 1;
        batches[6].hidden        = 1;
        D_neo_ark_altar_801800AE = 6;
    } else {
        batches                  = rec[3].batches;
        batches[1].hidden        = 0;
        batches                  = rec[6].batches;
        batches[1].hidden        = 0;
        batches                  = rec[4].batches;
        batches[1].hidden        = 1;
        batches[2].hidden        = 1;
        batches[3].hidden        = 1;
        batches[4].hidden        = 1;
        batches[5].hidden        = 1;
        batches[6].hidden        = 0;
        D_neo_ark_altar_801800AE = 0;
    }
    D_neo_ark_altar_801800AC = 0;
    for (i = 0x10; i >= 0; i--) {
        D_neo_ark_altar_801800B0[i] = 0;
    }
}

static s16 func_neo_ark_altar_8017E260(Task* task)
{
    _NeoArkAltarTileSequenceWork* work;
    s32                           i;
    s32                           bad1;
    s32                           bad2;

    bad1 = 0;
    work = task->work;
    bad2 = 0;
    if (D_neo_ark_altar_801800AC == 0) {
        return 0;
    }
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED) == 0) {
        for (i = 0; i < D_neo_ark_altar_801800AC; i++) {
            if (D_neo_ark_altar_8017F050[i] != D_neo_ark_altar_801800B0[i]) {
                goto fail1;
            }
            if (i == 11) {
                return 1;
            }
        }
    } else {
    fail1:
        bad1 = 1;
    }
    if (work->enteredTile != 0) {
        if (D_neo_ark_altar_8017F050[D_neo_ark_altar_801800AC - 1] != D_neo_ark_altar_801800B0[D_neo_ark_altar_801800AC - 1] || bad1 == 1) {
            if (work->enteredTile == 1) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_1_WRONG, 0, 0);
            }
            if (work->enteredTile == 2) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_2_WRONG, 0, 0);
            }
            if (work->enteredTile == 3) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_3_WRONG, 0, 0);
            }
            if (work->enteredTile == 4) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_4_WRONG, 0, 0);
            }
        } else {
            if (work->enteredTile == 1) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_1_CORRECT, 0, 0);
            }
            if (work->enteredTile == 2) {
                sndEvtRequestScriptStart(0x55140000 | work->enteredTile, 0, 0);
            }
            if (work->enteredTile == 3) {
                sndEvtRequestScriptStart(0x55140000 | work->enteredTile, 0, 0);
            }
            if (work->enteredTile == 4) {
                sndEvtRequestScriptStart(0x55140000 | work->enteredTile, 0, 0);
            }
        }
    }
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED) == 0) {
        for (i = 0; i < D_neo_ark_altar_801800AC; i++) {
            if (D_neo_ark_altar_8017F068[i] != D_neo_ark_altar_801800B0[i]) {
                goto fail2;
            }
            if (i == 15) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED, 0, 0);
                return 2;
            }
        }
    } else {
    fail2:
        bad2 = 1;
    }
    if (work->enteredTile != 0) {
        if (D_neo_ark_altar_8017F068[D_neo_ark_altar_801800AC - 1] != D_neo_ark_altar_801800B0[D_neo_ark_altar_801800AC - 1] || bad2 == 1) {
            if (work->enteredTile == 1) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_1_WRONG, 0, 0);
            }
            if (work->enteredTile == 2) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_2_WRONG, 0, 0);
            }
            if (work->enteredTile == 3) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_3_WRONG, 0, 0);
            }
            if (work->enteredTile == 4) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_4_WRONG, 0, 0);
            }
        } else {
            if (work->enteredTile == 1) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_1_CORRECT, 0, 0);
            }
            if (work->enteredTile == 2) {
                sndEvtRequestScriptStart(0x55140000 | work->enteredTile, 0, 0);
            }
            if (work->enteredTile == 3) {
                sndEvtRequestScriptStart(0x55140000 | work->enteredTile, 0, 0);
            }
            if (work->enteredTile == 4) {
                sndEvtRequestScriptStart(0x55140000 | work->enteredTile, 0, 0);
            }
        }
    }
    if (bad1 == 1 && bad2 == bad1) {
        func_neo_ark_altar_8017E148();
        return 0;
    }
    return 3;
}

/// Draws one side of a raised altar tile as 32 horizontal strips. `p0` and
/// `p1` are the side's top corners and `p2` the corner below `p0`; only the
/// height difference `p2 - p0` is used, split into 32 equal steps, and `p3` is
/// not read. Each strip that projects without a clipping error becomes a
/// Gouraud quad whose top edge is shade `c` and bottom edge `c - 6`, linked at
/// its projected depth together with a 0xE100002A draw-mode packet; the shade
/// only steps down for strips that are drawn.
static void func_neo_ark_altar_8017E658(SVECTOR* p0, SVECTOR* p1, SVECTOR* p2, SVECTOR* p3)
{
    SVECTOR  v0;
    SVECTOR  v1;
    SVECTOR  v2;
    SVECTOR  v3;
    long     sxy0;
    long     sxy1;
    long     sxy2;
    long     sxy3;
    long     p;
    long     flag;
    s32      otz;
    s16      step;
    s32      i;
    u8       c;
    u8       c2;
    POLY_G4* poly;
    DR_MODE* dr;

    c     = 0xC0;
    step  = (p2->vy - p0->vy) / 32;
    v0.vx = p0->vx;
    v0.vy = p0->vy;
    v0.vz = p0->vz;
    v1.vx = p1->vx;
    v1.vy = p1->vy;
    v1.vz = p1->vz;
    v2.vx = p0->vx;
    v2.vy = p0->vy + step;
    v2.vz = p0->vz;
    v3.vx = p1->vx;
    v3.vy = p1->vy + step;
    v3.vz = p1->vz;
    for (i = 0; i < 32; i++) {
        otz    = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
        v0.vy  = v2.vy;
        v2.vy += step;
        v1.vy  = v3.vy;
        v3.vy += step;
        if (flag >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 8);
            setcode(poly, 0x3A);
            GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
            GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
            GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
            GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
            c2                             = c - 6;
            poly->r0                       = c;
            poly->g0                       = c;
            poly->b0                       = c;
            poly->r1                       = c;
            poly->g1                       = c;
            poly->b1                       = c;
            poly->r2                       = c2;
            poly->g2                       = c2;
            poly->b2                       = c2;
            poly->r3                       = c2;
            poly->g3                       = c2;
            poly->b3                       = c2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), poly);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE100002A;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), dr);
            c = c2;
        }
    }
}

/// Walls in one altar tile. The view matrix is re-derived from
/// `gGfxViewCoord` and `gGfxViewCoord.workm` pushed into the GTE first, then each
/// of the tile's four sides goes to `func_neo_ark_altar_8017E658` as its two
/// corners at the floor height `y0` and at `y0 - arg1`, so `arg1` is how far a
/// side drops below the tile. The sides walk the tile rectangle
/// `(x, z) -> (x + width, z) -> (x + width, z + depth) -> (x, z + depth)` as
/// `arg0` selects the tile in the table.
static void func_neo_ark_altar_8017E92C(s16 arg0, s32 arg1)
{
    _NeoArkAltarTile* tile;
    _NeoArkAltarTile* base;
    SVECTOR           p0;
    SVECTOR           p1;
    SVECTOR           p2;
    SVECTOR           p3;
    s16               y0;
    s16               y1;

    base = D_neo_ark_altar_8017F014;
    y0   = -0x1086;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);

    tile = &base[arg0];
    y1   = y0 - arg1;

    p0.vx = tile->x;
    p0.vy = y0;
    p0.vz = tile->z;
    p1.vx = tile->x + tile->width;
    p1.vy = y0;
    p1.vz = tile->z;
    p2.vx = tile->x;
    p2.vy = y1;
    p2.vz = tile->z;
    p3.vx = tile->x + tile->width;
    p3.vy = y1;
    p3.vz = tile->z;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);

    p0.vx = tile->x + tile->width;
    p0.vy = y0;
    p0.vz = tile->z;
    p1.vx = tile->x + tile->width;
    p1.vy = y0;
    p1.vz = tile->z + tile->depth;
    p2.vx = tile->x + tile->width;
    p2.vy = y1;
    p2.vz = tile->z;
    p3.vx = tile->x + tile->width;
    p3.vy = y1;
    p3.vz = tile->z + tile->depth;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);

    p0.vx = tile->x;
    p0.vy = y0;
    p0.vz = tile->z + tile->depth;
    p1.vx = tile->x + tile->width;
    p1.vy = y0;
    p1.vz = tile->z + tile->depth;
    p2.vx = tile->x;
    p2.vy = y1;
    p2.vz = tile->z + tile->depth;
    p3.vx = tile->x + tile->width;
    p3.vy = y1;
    p3.vz = tile->z + tile->depth;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);

    p0.vx = tile->x;
    p0.vy = y0;
    p0.vz = tile->z;
    p1.vx = tile->x;
    p1.vy = y0;
    p1.vz = tile->z + tile->depth;
    p2.vx = tile->x;
    p2.vy = y1;
    p2.vz = tile->z;
    p3.vx = tile->x;
    p3.vy = y1;
    p3.vz = tile->z + tile->depth;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);
}

/// Returns the `id` of the first tile in `table` whose rectangle contains
/// `(x, z)`, edges inclusive, or 0 when none does. The scan ends at the entry
/// whose `id` is `NEO_ARK_ALTAR_TILE_END`.
static s16 func_neo_ark_altar_8017EC34(_NeoArkAltarTile* table, s16 x, s16 z)
{
    for (; table->id != NEO_ARK_ALTAR_TILE_END; table++) {
        if (table->x <= x && x <= table->x + table->width && table->z <= z && z <= table->z + table->depth) {
            return table->id;
        }
    }
    return 0;
}

/// State handlers of the altar task, dispatched by
/// `func_neo_ark_altar_8017ECE0` off `Task::state`: allocation and set-up,
/// a short wait, the tile sequence (`func_neo_ark_altar_8017DF0C`), then, once
/// the sequence completes, a fade-out, a spawn from `D_neo_ark_altar_8017EFC0`
/// and a view change before control returns to the tile sequence.
static const TaskFuncTable8 D_neo_ark_altar_8017D648 = {
    func_neo_ark_altar_8017ED60,
    func_neo_ark_altar_8017EDBC,
    func_neo_ark_altar_8017DF0C,
    func_neo_ark_altar_8017EDF8,
    func_neo_ark_altar_8017EE30,
    func_neo_ark_altar_8017EE90,
    func_neo_ark_altar_8017EF00,
    func_neo_ark_altar_8017EF34,
};

void func_neo_ark_altar_8017ECE0(Task* arg0)
{
    TaskFuncTable8 sp = D_neo_ark_altar_8017D648;

    sp.funcs[arg0->state](arg0);
}

static void func_neo_ark_altar_8017ED60(Task* arg0)
{
    _NeoArkAltarTileSequenceWork* work;

    work       = memCalloc(sizeof(*work), 0);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    func_neo_ark_altar_8017E148();
    arg0->killCountdown = 0;
    arg0->state         = (s32)(arg0->state + 1);
}

static void func_neo_ark_altar_8017EDBC(Task* arg0)
{
    arg0->killCountdown = arg0->killCountdown + 1;
    if (arg0->killCountdown >= 3) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

static void func_neo_ark_altar_8017EDF8(Task* arg0)
{
    Gp_MsgPlayerWeapon(0);
    arg0->killCountdown = 0;
    arg0->state         = (s32)(arg0->state + 1);
}

static void func_neo_ark_altar_8017EE30(Task* arg0)
{
    u8 temp_a0;

    arg0->killCountdown = arg0->killCountdown + 6;
    if (arg0->killCountdown >= 0x100) {
        arg0->killCountdown = 0xFF;
        arg0->state         = (s32)(arg0->state + 1);
    }
    temp_a0 = (u8)arg0->killCountdown;
    fadeDrawOverlay(temp_a0, temp_a0, temp_a0, GPU_BLEND_SUBTRACT);
}

static void func_neo_ark_altar_8017EE90(Task* arg0)
{
    _NeoArkAltarTileSequenceWork* work;

    work = arg0->work;
    Gp_MsgPlayer3F3(0);
    gGameSession->hideHud = 1;
    work->movieLauncher   = taskSpawnFromTable(D_neo_ark_altar_8017EFC0, 0, 2, 0);
    arg0->state           = (s32)(arg0->state + 1);
}

static void func_neo_ark_altar_8017EF00(Task* arg0)
{
    s16* viewDirty;

    /* Through a pointer rather than as a member: a member store is struct
       memory, which the scheduler lets the store to `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room` pass, and the
       original keeps the two in source order. */
    viewDirty                                                  = &gGameSession->viewDirty;
    *viewDirty                                                 = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
    gGameSession->location.loc.room                            = 2;
    arg0->state                                                = (s32)(arg0->state + 1);
}

static void func_neo_ark_altar_8017EF34(Task* arg0)
{
    SetDispMask(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayerWeapon(1);
    gGameSession->hideHud = 0;
    arg0->state           = 2;
}

void func_neo_ark_altar_8017EF84(Task* unused)
{
}
