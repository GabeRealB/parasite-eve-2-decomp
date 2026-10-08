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
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
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

// Results shared by tile-sequence evaluation and the tracking state.
enum {
    NEO_ARK_ALTAR_SEQUENCE_IDLE           = 0,
    NEO_ARK_ALTAR_SEQUENCE_FIRST_SOLVED   = 1,
    NEO_ARK_ALTAR_SEQUENCE_SECOND_SOLVED  = 2,
    NEO_ARK_ALTAR_SEQUENCE_PREFIX_MATCHED = 3,
};

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

static void _neoArkAltarDrawTileWallSide(const SVECTOR* floorStart, const SVECTOR* floorEnd, const SVECTOR* raisedStart, const SVECTOR* unusedRaisedEnd);
static s16  _neoArkAltarFindTile(const _NeoArkAltarTile* table, s16 x, s16 z);
static s16  _neoArkAltarEvaluateTileSequence(Task* task);
static void _neoArkAltarDrawTileWalls(s16 tileIndex, s32 height);

extern WorldCollisionGrid    D_neo_ark_altar_8017F57C[1];
extern WorldCollisionTrigger D_neo_ark_altar_8017FF08[4];
extern WorldCoordRoomLights  D_neo_ark_altar_8017FEF0[1];
static void                  _neoArkAltarTileSequenceTask(Task* task);

static void _neoArkAltarPlayMovieTask(Task* task);
static void _neoArkAltarLaunchMovieTask(Task* task);

TaskDesc D_neo_ark_altar_8017EFC0[2] = {
    { { { TASK_BODY_NONE, 192 } }, _neoArkAltarLaunchMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _neoArkAltarPlayMovieTask, { .value = 0 } },
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
    { { { TASK_BODY_NONE, 32 } }, _neoArkAltarTileSequenceTask, { .value = 0 } },
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

static void _neoArkAltarInitializeTileSequence(Task* task);

static void _neoArkAltarWaitForTileSequence(Task* task);

static void _neoArkAltarBeginSequenceMovieFade(Task* task);

static void _neoArkAltarFadeToBlack(Task* task);

static void _neoArkAltarStartSequenceMovie(Task* task);

static void _neoArkAltarSelectPostSequenceRoom(Task* task);

static void _neoArkAltarResumeTileSequence(Task* task);

static void _neoArkAltarTrackTileSequence(Task* task);
static void _neoArkAltarResetTileSequence(void);

/// Queues the current room's altar movie selected by the task's spawn argument.
///
/// Borrows `task`; spawnArg1 0 selects stream ID 100, 1 selects 101, and all
/// other values select 102. Requires a loaded matching room/sub-ID-0 slot
/// (0..14) and space in the CD ring. Enqueue copies the full argument envelope
/// immediately; only its slot byte is initialized and interpreted. The zero
/// file-key address reads low RAM under the resident enqueue contract.
static inline void _neoArkAltarQueueMovie(const Task* task)
{
    enum {
        NEO_ARK_ALTAR_MOVIE_SWITCH_SET      = 0,
        NEO_ARK_ALTAR_MOVIE_SWITCH_CLEAR    = 1,
        NEO_ARK_ALTAR_MOVIE_SWITCH_SET_ID   = 100,
        NEO_ARK_ALTAR_MOVIE_SWITCH_CLEAR_ID = 101,
        NEO_ARK_ALTAR_MOVIE_SEQUENCE_ID     = 102,
    };
    u8      commandArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    GameLoc movieLocation;

    movieLocation = gGameSession->location;
    if (task->spawnArg1.value == NEO_ARK_ALTAR_MOVIE_SWITCH_SET) {
        movieLocation.loc.view = NEO_ARK_ALTAR_MOVIE_SWITCH_SET_ID;
    } else if (task->spawnArg1.value == NEO_ARK_ALTAR_MOVIE_SWITCH_CLEAR) {
        movieLocation.loc.view = NEO_ARK_ALTAR_MOVIE_SWITCH_CLEAR_ID;
    } else {
        movieLocation.loc.view = NEO_ARK_ALTAR_MOVIE_SEQUENCE_ID;
    }
    // Enqueue copies four bytes; this opcode interprets only the slot byte.
    commandArgs[0] = streamFindMovieSlot(&movieLocation.loc, 0, 0);
    cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
}

/// Plays an altar movie, then restores game resources and frame presentation.
///
/// `spawnArg1.value` selects stream ID 100 for 0 (switch flag set), 101 for 1
/// (switch flag clear), or 102 otherwise (second tile sequence solved), using
/// the current room. The selected movie slot must be loaded. The task must own
/// display presentation, and the session, movie resources and saved VRAM images
/// must remain available through restoration. Start requests cancellation;
/// completion and cancellation both wait for CD idle before restoring model
/// buffers and sprite images. The task ends after restoration, with no work
/// block allocated, and returns presentation to the game loop.
static void _neoArkAltarPlayMovieTask(Task* task)
{
    enum {
        NEO_ARK_ALTAR_MOVIE_PREPARE,
        NEO_ARK_ALTAR_MOVIE_QUEUE,
        NEO_ARK_ALTAR_MOVIE_WAIT_READY,
        NEO_ARK_ALTAR_MOVIE_PLAY,
        NEO_ARK_ALTAR_MOVIE_WAIT_IDLE,
        NEO_ARK_ALTAR_MOVIE_RESTORE_GAME,
    };
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case NEO_ARK_ALTAR_MOVIE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state++;
            break;
        case NEO_ARK_ALTAR_MOVIE_QUEUE:
            _neoArkAltarQueueMovie(task);
            task->state++;
            break;
        case NEO_ARK_ALTAR_MOVIE_WAIT_READY:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case NEO_ARK_ALTAR_MOVIE_PLAY:
            if (cdCmdIsIdle()) {
                SetDispMask(0);
                task->state++;
            } else if (padIsStartPressed()) {
                SetDispMask(0);
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        // Playback and cancellation must drain before game memory is reused.
        case NEO_ARK_ALTAR_MOVIE_WAIT_IDLE:
            if (cdCmdIsIdle()) {
                streamResetGameRestore();
                task->state++;
            }
            break;
        case NEO_ARK_ALTAR_MOVIE_RESTORE_GAME:
            if (streamPollGameRestore(0, 1)) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

/// Hands frame presentation to an altar movie task and releases the launcher.
///
/// Forwards `spawnArg1` to `_neoArkAltarPlayMovieTask`. Requires game-loop
/// presentation ownership and an available display-task list; captures the
/// current camera and room packets before the movie task starts on that list.
/// The movie task owns restoration and the eventual return to game presentation.
/// Display-task spawn failure is not handled here.
static void _neoArkAltarLaunchMovieTask(Task* task)
{
    enum { NEO_ARK_ALTAR_MOVIE_PLAYBACK_ENTRY = 1 };

    displaySpawnTaskFromTable(D_neo_ark_altar_8017EFC0, NEO_ARK_ALTAR_MOVIE_PLAYBACK_ENTRY, task->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

/// Shows one switch-animation batch while hiding the other five.
///
/// `batches` must be a stable writable pointer with entries 1..6, evaluated
/// six times; entry 0 is untouched. `visibleBatch` is a stable index 1..6,
/// evaluated six times. Captures no identifiers and expands to a braced block.
#define NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, visibleBatch) \
    {                                                            \
        (batches)[1].hidden = (visibleBatch) != 1;               \
        (batches)[2].hidden = (visibleBatch) != 2;               \
        (batches)[3].hidden = (visibleBatch) != 3;               \
        (batches)[4].hidden = (visibleBatch) != 4;               \
        (batches)[5].hidden = (visibleBatch) != 5;               \
        (batches)[6].hidden = (visibleBatch) != 6;               \
    }

void neoArkAltarStepSwitchSprites(s32 switchChoice)
{
    enum { NEO_ARK_ALTAR_SWITCH_CHOICE_CLEAR = 0,
           NEO_ARK_ALTAR_SWITCH_CHOICE_SET   = 1 };
    GameLocationKey* location;
    SpriteView*      areaViews;
    SpriteBatch*     batches;

    location      = &gGameSession->location.loc;
    areaViews     = gSpriteAreaTables[location->stage - 1][0].areaViews[location->area - 1];
    switchChoice &= 0xFF;
    // Each direction changes the endpoint scenery even when no animation step remains.
    if (switchChoice == NEO_ARK_ALTAR_SWITCH_CHOICE_CLEAR) {
        batches           = areaViews[3].batches;
        batches[1].hidden = 1;
        batches           = areaViews[6].batches;
        batches[1].hidden = 1;
        switch (D_neo_ark_altar_801800AE) {
            case 0:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 6);
                D_neo_ark_altar_801800AE = 1;
                break;
            case 1:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 5);
                D_neo_ark_altar_801800AE = 2;
                break;
            case 2:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 4);
                D_neo_ark_altar_801800AE = 3;
                break;
            case 3:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 3);
                D_neo_ark_altar_801800AE = 4;
                break;
            case 4:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 2);
                D_neo_ark_altar_801800AE = 5;
                break;
            case 5:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 1);
                D_neo_ark_altar_801800AE = 6;
                break;
        }
    } else if (switchChoice == NEO_ARK_ALTAR_SWITCH_CHOICE_SET) {
        batches           = areaViews[3].batches;
        batches[1].hidden = 0;
        batches           = areaViews[6].batches;
        batches[1].hidden = 0;
        switch (D_neo_ark_altar_801800AE) {
            case 6:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 1);
                D_neo_ark_altar_801800AE = 5;
                break;
            case 5:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 2);
                D_neo_ark_altar_801800AE = 4;
                break;
            case 4:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 3);
                D_neo_ark_altar_801800AE = 3;
                break;
            case 3:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 4);
                D_neo_ark_altar_801800AE = 2;
                break;
            case 2:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 5);
                D_neo_ark_altar_801800AE = 1;
                break;
            case 1:
                batches = areaViews[4].batches;
                NEO_ARK_ALTAR_SELECT_SWITCH_FRAME(batches, 6);
                D_neo_ark_altar_801800AE = 0;
                break;
        }
    }
}

#undef NEO_ARK_ALTAR_SELECT_SWITCH_FRAME

/// Tracks newly entered altar tiles, sequence progress and rising tile walls.
///
/// Runs in state 2 with initialized owned work, a live player model and loaded
/// tile/area resources. Records only no-tile-to-tile entries (IDs 1..4), then
/// commits the first solution or enters the second solution's movie fade.
/// A valid sequence prefix raises the current tile's walls halfway toward
/// 3000 world units each frame; otherwise they decay by a quarter and are drawn
/// down to 11 units. Requires one controller sharing the history: mismatches
/// reset its count, and a 16-entry solution pauses tracking with a nonzero
/// current tile, so resuming evaluates/resets before another entry is appended.
static void _neoArkAltarTrackTileSequence(Task* task)
{
    _NeoArkAltarTileSequenceWork* work;
    GfxCoord*                     playerRoot;
    Task*                         playerTask;
    s32                           previousTile;
    s16                           currentTile;
    s16                           height;
    s32                           tileIndex;
    s32                           raiseWalls;
    s16                           wallRaised;

    enum {
        NEO_ARK_ALTAR_TILE_COUNT                    = 4,
        NEO_ARK_ALTAR_FIRST_SOLVED_CAP_COMMAND      = 1,
        NEO_ARK_ALTAR_FIRST_SOLVED_STOP_SOUND_ENTRY = 3,
        NEO_ARK_ALTAR_STATE_BEGIN_SEQUENCE_MOVIE    = 3,
        NEO_ARK_ALTAR_WALL_MIN_DRAW_HEIGHT          = 11,
    };

    work               = task->work;
    playerTask         = *gPlayerActorTasks;
    work->previousTile = work->currentTile;
    raiseWalls         = 0;
    playerRoot         = playerTask->extra.tmd->coords;
    currentTile        = _neoArkAltarFindTile(D_neo_ark_altar_8017EFD8, (s16)playerRoot->coord.t[0], (s16)playerRoot->coord.t[2]);
    previousTile       = work->previousTile;
    work->currentTile  = currentTile;
    // Record only entry from outside a tile, not a direct tile-to-tile crossing.
    if (currentTile != previousTile && previousTile == 0) {
        work->enteredTile                                  = currentTile;
        D_neo_ark_altar_801800B0[D_neo_ark_altar_801800AC] = work->currentTile;
        D_neo_ark_altar_801800AC                           = (u16)D_neo_ark_altar_801800AC + 1;
    } else {
        work->enteredTile = 0;
    }
    switch (_neoArkAltarEvaluateTileSequence(task)) {
        case NEO_ARK_ALTAR_SEQUENCE_FIRST_SOLVED:
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_ALTAR, 0);
            sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, NEO_ARK_ALTAR_FIRST_SOLVED_STOP_SOUND_ENTRY), SOUND_SCRIPT_STOP_NO_FADE);
            sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED, 0, 0);
            capRunCommandWithTransition(NEO_ARK_ALTAR_FIRST_SOLVED_CAP_COMMAND);
            areaApplySavedUpdates(D_neo_ark_altar_8018007C);
            break;
        case NEO_ARK_ALTAR_SEQUENCE_SECOND_SOLVED:
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED, 1);
            task->state = NEO_ARK_ALTAR_STATE_BEGIN_SEQUENCE_MOVIE;
            break;
        case NEO_ARK_ALTAR_SEQUENCE_PREFIX_MATCHED:
            raiseWalls = 1;
            break;
    }
    // Ease the matching tile upward; otherwise lower the last raised walls.
    wallRaised = 0;
    for (tileIndex = 0; tileIndex < NEO_ARK_ALTAR_TILE_COUNT; tileIndex++) {
        if (raiseWalls == 1 && (work->currentTile - 1) == tileIndex) {
            work->wallTileIndex = tileIndex;
            // Retain the intermediate stores required by this loop's matching form.
            height            = 0;
            height            = 1;
            work->wallHeight += (NEO_ARK_ALTAR_WALL_HEIGHT_FULL - work->wallHeight) >> 1;
            height            = work->wallHeight;
            _neoArkAltarDrawTileWalls((s16)tileIndex, height);
            wallRaised = 1;
        }
    }
    if (wallRaised == 0 && work->wallTileIndex < NEO_ARK_ALTAR_TILE_COUNT) {
        work->wallHeight += (-work->wallHeight) >> 2;
        height            = work->wallHeight;
        if (height >= NEO_ARK_ALTAR_WALL_MIN_DRAW_HEIGHT) {
            _neoArkAltarDrawTileWalls(work->wallTileIndex, height);
        }
    }
}

/// Restores the altar switch scenery and restarts its entered-tile sequence.
///
/// Uses the current area's first sprite variant, updating batch 1 in views 4
/// and 7 and batches 1..6 in view 5. A clear persistent switch flag selects
/// switch-animation endpoint 6; a set flag selects endpoint 0. Resets the
/// entered-tile count and preserves the binary's descending 17-halfword clear.
/// The last store reaches beyond the declared history array into the image's
/// final halfword; that storage's role is unproven.
static void _neoArkAltarResetTileSequence(void)
{
    enum {
        NEO_ARK_ALTAR_SWITCH_CLEAR_ENDPOINT    = 6,
        NEO_ARK_ALTAR_SWITCH_SET_ENDPOINT      = 0,
        NEO_ARK_ALTAR_HISTORY_CLEAR_LAST_INDEX = 16,
    };
    GameLocationKey* location;
    SpriteView*      areaViews;
    SpriteBatch*     batches;
    s32              historyIndex;

    location  = &gGameSession->location.loc;
    areaViews = gSpriteAreaTables[location->stage - 1][0].areaViews[location->area - 1];
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) == 0) {
        batches                  = areaViews[3].batches;
        batches[1].hidden        = 1;
        batches                  = areaViews[6].batches;
        batches[1].hidden        = 1;
        batches                  = areaViews[4].batches;
        batches[1].hidden        = 0;
        batches[2].hidden        = 1;
        batches[3].hidden        = 1;
        batches[4].hidden        = 1;
        batches[5].hidden        = 1;
        batches[6].hidden        = 1;
        D_neo_ark_altar_801800AE = NEO_ARK_ALTAR_SWITCH_CLEAR_ENDPOINT;
    } else {
        batches                  = areaViews[3].batches;
        batches[1].hidden        = 0;
        batches                  = areaViews[6].batches;
        batches[1].hidden        = 0;
        batches                  = areaViews[4].batches;
        batches[1].hidden        = 1;
        batches[2].hidden        = 1;
        batches[3].hidden        = 1;
        batches[4].hidden        = 1;
        batches[5].hidden        = 1;
        batches[6].hidden        = 0;
        D_neo_ark_altar_801800AE = NEO_ARK_ALTAR_SWITCH_SET_ENDPOINT;
    }
    D_neo_ark_altar_801800AC = 0;
    // This is the observed transfer extent, not the declared history array bound.
    for (historyIndex = NEO_ARK_ALTAR_HISTORY_CLEAR_LAST_INDEX; historyIndex >= 0; historyIndex--) {
        D_neo_ark_altar_801800B0[historyIndex] = 0;
    }
}

/// Plays feedback for one solution, capturing the evaluator's stable work pointer.
///
/// `solution` is stable readable storage through the current last-entry index;
/// `prefixMismatch` is a stable 0/1 flag. Each argument is evaluated at most
/// once, only for a newly entered tile; an entry mismatch skips the flag test.
/// Expands to one braced block and preserves separate sound tests for tile IDs.
#define NEO_ARK_ALTAR_PLAY_ENTERED_TILE_FEEDBACK(solution, prefixMismatch)                                                                     \
    {                                                                                                                                          \
        if (work->enteredTile != 0) {                                                                                                          \
            if ((solution)[D_neo_ark_altar_801800AC - 1] != D_neo_ark_altar_801800B0[D_neo_ark_altar_801800AC - 1] || (prefixMismatch) == 1) { \
                if (work->enteredTile == 1) {                                                                                                  \
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_1_WRONG, 0, 0);                                                          \
                }                                                                                                                              \
                if (work->enteredTile == 2) {                                                                                                  \
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_2_WRONG, 0, 0);                                                          \
                }                                                                                                                              \
                if (work->enteredTile == 3) {                                                                                                  \
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_3_WRONG, 0, 0);                                                          \
                }                                                                                                                              \
                if (work->enteredTile == 4) {                                                                                                  \
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_4_WRONG, 0, 0);                                                          \
                }                                                                                                                              \
            } else {                                                                                                                           \
                if (work->enteredTile == 1) {                                                                                                  \
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_TILE_1_CORRECT, 0, 0);                                                        \
                }                                                                                                                              \
                if (work->enteredTile == 2) {                                                                                                  \
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 0) | work->enteredTile, 0, 0);    \
                }                                                                                                                              \
                if (work->enteredTile == 3) {                                                                                                  \
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 0) | work->enteredTile, 0, 0);    \
                }                                                                                                                              \
                if (work->enteredTile == 4) {                                                                                                  \
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 0) | work->enteredTile, 0, 0);    \
                }                                                                                                                              \
            }                                                                                                                                  \
        }                                                                                                                                      \
    }

/// Checks entered-tile history against both altar solutions and plays tile feedback.
///
/// Returns idle/reset (0), first solved (1), second solved (2), or a still-valid
/// prefix (3). Completed persistent solutions count as mismatches. The first
/// solution ends at 12 entries and the second at 16; completion returns before
/// that solution's tile feedback. Otherwise each solution emits feedback for
/// this frame's entered tile, and two mismatches reset scenery/history.
/// Requires initialized work, live flags/sound and readable shared history and
/// solution storage for every indexed entry; the code does not enforce bounds.
static s16 _neoArkAltarEvaluateTileSequence(Task* task)
{
    _NeoArkAltarTileSequenceWork* work;
    s32                           historyIndex;
    s32                           firstMismatch;
    s32                           secondMismatch;

    firstMismatch  = 0;
    work           = task->work;
    secondMismatch = 0;
    if (D_neo_ark_altar_801800AC == 0) {
        return NEO_ARK_ALTAR_SEQUENCE_IDLE;
    }
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED) == 0) {
        for (historyIndex = 0; historyIndex < D_neo_ark_altar_801800AC; historyIndex++) {
            if (D_neo_ark_altar_8017F050[historyIndex] != D_neo_ark_altar_801800B0[historyIndex]) {
                firstMismatch = 1;
                break;
            }
            if (historyIndex == ARRAY_SIZE(D_neo_ark_altar_8017F050) - 1) {
                return NEO_ARK_ALTAR_SEQUENCE_FIRST_SOLVED;
            }
        }
    } else {
        firstMismatch = 1;
    }
    NEO_ARK_ALTAR_PLAY_ENTERED_TILE_FEEDBACK(D_neo_ark_altar_8017F050, firstMismatch);
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED) == 0) {
        for (historyIndex = 0; historyIndex < D_neo_ark_altar_801800AC; historyIndex++) {
            if (D_neo_ark_altar_8017F068[historyIndex] != D_neo_ark_altar_801800B0[historyIndex]) {
                secondMismatch = 1;
                break;
            }
            if (historyIndex == ARRAY_SIZE(D_neo_ark_altar_8017F068) - 1) {
                sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED, 0, 0);
                return NEO_ARK_ALTAR_SEQUENCE_SECOND_SOLVED;
            }
        }
    } else {
        secondMismatch = 1;
    }
    NEO_ARK_ALTAR_PLAY_ENTERED_TILE_FEEDBACK(D_neo_ark_altar_8017F068, secondMismatch);
    if (firstMismatch == 1 && secondMismatch == firstMismatch) {
        _neoArkAltarResetTileSequence();
        return NEO_ARK_ALTAR_SEQUENCE_IDLE;
    }
    return NEO_ARK_ALTAR_SEQUENCE_PREFIX_MATCHED;
}

#undef NEO_ARK_ALTAR_PLAY_ENTERED_TILE_FEEDBACK

/// Sets a wall strip's grayscale gradient from its lower edge to its upper edge.
///
/// Vertices 0 and 1 form the lower edge, and 2 and 3 the upper edge. Both
/// shades are unsigned eight-bit RGB intensities; the packet's tag, command
/// byte and projected coordinates are preserved.
static inline void _neoArkAltarShadeWallStrip(POLY_G4* strip, u8 lowerShade, u8 upperShade)
{
    strip->r0 = lowerShade;
    strip->g0 = lowerShade;
    strip->b0 = lowerShade;
    strip->r1 = lowerShade;
    strip->g1 = lowerShade;
    strip->b1 = lowerShade;
    strip->r2 = upperShade;
    strip->g2 = upperShade;
    strip->b2 = upperShade;
    strip->r3 = upperShade;
    strip->g3 = upperShade;
    strip->b3 = upperShade;
}

/// Draws one raised altar-tile side as additive grayscale Gouraud strips.
///
/// The floor endpoints use world coordinates; only `raisedStart->vy` supplies
/// the raised height, toward negative Y. The signed height difference is divided
/// into 32 steps with truncation toward zero. `unusedRaisedEnd` is not read.
/// The caller must install the world-to-view GTE transform and provide a current
/// ordering table and word-aligned packet arena with room for up to 32 pairs
/// of `POLY_G4` and `DR_MODE`. Packets remain live until GPU drawing completes.
static void _neoArkAltarDrawTileWallSide(const SVECTOR* floorStart, const SVECTOR* floorEnd, const SVECTOR* raisedStart, const SVECTOR* unusedRaisedEnd)
{
    enum {
        NEO_ARK_ALTAR_WALL_STRIP_COUNT = 32,
        NEO_ARK_ALTAR_WALL_FLOOR_SHADE = 0xC0,
        NEO_ARK_ALTAR_WALL_SHADE_STEP  = 6,
    };
    SVECTOR  lowerStart;
    SVECTOR  lowerEnd;
    SVECTOR  upperStart;
    SVECTOR  upperEnd;
    long     screenLowerStart;
    long     screenLowerEnd;
    long     screenUpperStart;
    long     screenUpperEnd;
    long     unusedDepthCue;
    long     projectionFlags;
    s32      depth;
    s16      heightStep;
    s32      stripIndex;
    u8       lowerShade;
    u8       upperShade;
    POLY_G4* wallStrip;
    DR_MODE* blendCommand;

    lowerShade    = NEO_ARK_ALTAR_WALL_FLOOR_SHADE;
    heightStep    = (raisedStart->vy - floorStart->vy) / NEO_ARK_ALTAR_WALL_STRIP_COUNT;
    lowerStart.vx = floorStart->vx;
    lowerStart.vy = floorStart->vy;
    lowerStart.vz = floorStart->vz;
    lowerEnd.vx   = floorEnd->vx;
    lowerEnd.vy   = floorEnd->vy;
    lowerEnd.vz   = floorEnd->vz;
    upperStart.vx = floorStart->vx;
    upperStart.vy = floorStart->vy + heightStep;
    upperStart.vz = floorStart->vz;
    upperEnd.vx   = floorEnd->vx;
    upperEnd.vy   = floorEnd->vy + heightStep;
    upperEnd.vz   = floorEnd->vz;
    for (stripIndex = 0; stripIndex < NEO_ARK_ALTAR_WALL_STRIP_COUNT; stripIndex++) {
        depth          = RotTransPers4(&lowerStart, &lowerEnd, &upperStart, &upperEnd, &screenLowerStart, &screenLowerEnd, &screenUpperStart, &screenUpperEnd, &unusedDepthCue, &projectionFlags);
        lowerStart.vy  = upperStart.vy;
        upperStart.vy += heightStep;
        lowerEnd.vy    = upperEnd.vy;
        upperEnd.vy   += heightStep;
        // Rejected projections advance the geometry but leave the shade unchanged.
        if (projectionFlags >= 0) {
            wallStrip      = gGpuPrimCursor;
            gGpuPrimCursor = wallStrip + 1;
            setPolyG4(wallStrip);
            setSemiTrans(wallStrip, true);
            GPU_PRIMITIVE_XY_WORD(wallStrip, 0) = screenLowerStart;
            GPU_PRIMITIVE_XY_WORD(wallStrip, 1) = screenLowerEnd;
            GPU_PRIMITIVE_XY_WORD(wallStrip, 2) = screenUpperStart;
            GPU_PRIMITIVE_XY_WORD(wallStrip, 3) = screenUpperEnd;
            upperShade                          = lowerShade - NEO_ARK_ALTAR_WALL_SHADE_STEP;
            _neoArkAltarShadeWallStrip(wallStrip, lowerShade, upperShade);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), wallStrip);
            blendCommand   = gGpuPrimCursor;
            gGpuPrimCursor = blendCommand + 1;
            // Prepending the mode at the same depth makes it execute before the strip.
            setDrawTPage(blendCommand, false, false, getTPage(0, GPU_BLEND_ADD, 640, 0));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), blendCommand);
            lowerShade = upperShade;
        }
    }
}

/// Builds a wall's four endpoints using the drawer's local vectors and Y levels.
///
/// Arguments are side-effect-free world X/Z expressions, each evaluated twice.
/// Captures `floorStart`, `floorEnd`, `raisedStart`, `raisedEnd`, `floorY` and
/// `raisedY`; the four vectors must be distinct writable SVECTOR values, and
/// argument expressions must not depend on those vectors' contents. Narrows
/// each coordinate to a signed halfword without changing the vectors' pad fields.
#define NEO_ARK_ALTAR_SET_WALL_SIDE_CORNERS(startX, startZ, endX, endZ) \
    {                                                                   \
        floorStart.vx  = (startX);                                      \
        floorStart.vy  = floorY;                                        \
        floorStart.vz  = (startZ);                                      \
        floorEnd.vx    = (endX);                                        \
        floorEnd.vy    = floorY;                                        \
        floorEnd.vz    = (endZ);                                        \
        raisedStart.vx = (startX);                                      \
        raisedStart.vy = raisedY;                                       \
        raisedStart.vz = (startZ);                                      \
        raisedEnd.vx   = (endX);                                        \
        raisedEnd.vy   = raisedY;                                       \
        raisedEnd.vz   = (endZ);                                        \
    }

/// Draws the four additive light walls rising around an altar tile.
///
/// `tileIndex` is 0..3 in the wall-footprint table; `height` is a nonnegative
/// height in world units, up to `NEO_ARK_ALTAR_WALL_HEIGHT_FULL`. The floor
/// lies at Y = -4230 and the raised edge at floor Y minus height. Composes and
/// installs the current world-to-view GTE transform. Requires a live ordering
/// table and word-aligned packet arena with space for up to 128 pairs of
/// `POLY_G4` and `DR_MODE`; packets must survive until GPU drawing completes.
static void _neoArkAltarDrawTileWalls(s16 tileIndex, s32 height)
{
    enum { NEO_ARK_ALTAR_TILE_FLOOR_Y = -4230 };
    const _NeoArkAltarTile* tile;
    const _NeoArkAltarTile* footprints;
    SVECTOR                 floorStart;
    SVECTOR                 floorEnd;
    SVECTOR                 raisedStart;
    SVECTOR                 raisedEnd;
    s16                     floorY;
    s16                     raisedY;

    footprints = D_neo_ark_altar_8017F014;
    floorY     = NEO_ARK_ALTAR_TILE_FLOOR_Y;

    // All four walls share the composed world-to-view transform.
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);

    tile    = &footprints[tileIndex];
    raisedY = floorY - height;

    NEO_ARK_ALTAR_SET_WALL_SIDE_CORNERS(tile->x, tile->z, tile->x + tile->width, tile->z);
    _neoArkAltarDrawTileWallSide(&floorStart, &floorEnd, &raisedStart, &raisedEnd);

    NEO_ARK_ALTAR_SET_WALL_SIDE_CORNERS(tile->x + tile->width, tile->z, tile->x + tile->width, tile->z + tile->depth);
    _neoArkAltarDrawTileWallSide(&floorStart, &floorEnd, &raisedStart, &raisedEnd);

    NEO_ARK_ALTAR_SET_WALL_SIDE_CORNERS(tile->x, tile->z + tile->depth, tile->x + tile->width, tile->z + tile->depth);
    _neoArkAltarDrawTileWallSide(&floorStart, &floorEnd, &raisedStart, &raisedEnd);

    NEO_ARK_ALTAR_SET_WALL_SIDE_CORNERS(tile->x, tile->z, tile->x, tile->z + tile->depth);
    _neoArkAltarDrawTileWallSide(&floorStart, &floorEnd, &raisedStart, &raisedEnd);
}

#undef NEO_ARK_ALTAR_SET_WALL_SIDE_CORNERS

/// Returns the first altar pad containing a world X/Z point, or 0 on a miss.
///
/// `table` is borrowed read-only storage ending at `NEO_ARK_ALTAR_TILE_END`.
/// Both rectangle edges are included. Live entries carry pad IDs 1..4; the
/// sentinel's rectangle is not tested. X and Z are signed halfword world units.
static s16 _neoArkAltarFindTile(const _NeoArkAltarTile* table, s16 x, s16 z)
{
    enum { NEO_ARK_ALTAR_TILE_NONE = 0 };

    for (; table->id != NEO_ARK_ALTAR_TILE_END; table++) {
        if (table->x <= x && x <= table->x + table->width && table->z <= z && z <= table->z + table->depth) {
            return table->id;
        }
    }
    return NEO_ARK_ALTAR_TILE_NONE;
}

/// State handlers of the altar task, dispatched by
/// `_neoArkAltarTileSequenceTask` off `Task::state`: allocation and set-up,
/// a short wait, the tile sequence (`_neoArkAltarTrackTileSequence`), then, once
/// the sequence completes, a fade-out, a spawn from `D_neo_ark_altar_8017EFC0`
/// and a view change before control returns to the tile sequence.
static const TaskFuncTable8 D_neo_ark_altar_8017D648 = {
    _neoArkAltarInitializeTileSequence,
    _neoArkAltarWaitForTileSequence,
    _neoArkAltarTrackTileSequence,
    _neoArkAltarBeginSequenceMovieFade,
    _neoArkAltarFadeToBlack,
    _neoArkAltarStartSequenceMovie,
    _neoArkAltarSelectPostSequenceRoom,
    _neoArkAltarResumeTileSequence,
};

/// Runs the altar's tile puzzle and its second-sequence movie transition.
///
/// `state` must be 0..7: initialize, wait, track the tile sequence, hold the
/// player, fade to black, launch the movie, select the post-movie room, and
/// restore player presentation and control. The last state returns to state 2.
/// Initialization owns a zeroed work allocation in `Task::work`, released by
/// normal task teardown; an allocation failure may kill the task during dispatch.
static void _neoArkAltarTileSequenceTask(Task* task)
{
    TaskFuncTable8 stateHandlers = D_neo_ark_altar_8017D648;

    stateHandlers.funcs[task->state](task);
}

/// Allocates and resets the altar's tile-sequence controller.
///
/// Entry is state 0. Owns zeroed primary-heap work through task teardown;
/// failure kills the controller. Success resets the shared switch scenery
/// and tile history, clears the elapsed-frame counter, and enters the wait state.
/// Requires loaded altar sprites, live saved flags and the shared tile history.
static void _neoArkAltarInitializeTileSequence(Task* task)
{
    _NeoArkAltarTileSequenceWork* work;

    work       = memCalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    _neoArkAltarResetTileSequence();
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// Waits three callback frames before entering the altar's tile-sequence state.
///
/// The preceding initialization state sets `Task::killCountdown` to zero;
/// this state uses that signed 16-bit storage as an elapsed-frame counter.
static void _neoArkAltarWaitForTileSequence(Task* task)
{
    enum { NEO_ARK_ALTAR_START_WAIT_FRAMES = 3 };

    task->killCountdown = task->killCountdown + 1;
    if (task->killCountdown >= NEO_ARK_ALTAR_START_WAIT_FRAMES) {
        task->state = task->state + 1;
    }
}

/// Holds the player and begins the fade for the second solved tile sequence.
///
/// Runs in state 3; clears the signed halfword fade accumulator reused from
/// `killCountdown`, then advances to the fade state.
static void _neoArkAltarBeginSequenceMovieFade(Task* task)
{
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// Fades the scene to black before the movie for the second solved tile sequence.
///
/// The preceding state clears `Task::killCountdown`; each callback adds six
/// brightness units. On reaching 256 it clamps to 255 and advances the state.
/// The clamped frame still draws the full-screen subtractive overlay.
static void _neoArkAltarFadeToBlack(Task* task)
{
    enum {
        NEO_ARK_ALTAR_FADE_STEP = 6,
        NEO_ARK_ALTAR_FADE_MAX  = 0xFF,
    };
    u8 shade;

    task->killCountdown = task->killCountdown + NEO_ARK_ALTAR_FADE_STEP;
    if (task->killCountdown >= NEO_ARK_ALTAR_FADE_MAX + 1) {
        task->killCountdown = NEO_ARK_ALTAR_FADE_MAX;
        task->state         = task->state + 1;
    }
    shade = (u8)task->killCountdown;
    fadeDrawOverlay(shade, shade, shade, GPU_BLEND_SUBTRACT);
}

/// Hides the player/HUD and launches the second tile solution's movie.
///
/// Runs in state 5 after the fade, with initialized task work and loaded movie
/// resources. Spawns launcher entry 0 with variant 2 (stream ID 102), stores
/// its borrowed task handle and advances to room selection without waiting.
/// Spawn failure leaves a NULL handle and still advances; no task is adopted.
static void _neoArkAltarStartSequenceMovie(Task* task)
{
    enum { NEO_ARK_ALTAR_MOVIE_LAUNCHER_ENTRY  = 0,
           NEO_ARK_ALTAR_MOVIE_SECOND_SEQUENCE = 2 };
    _NeoArkAltarTileSequenceWork* work;

    work = task->work;
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    gGameSession->hideHud = 1;
    work->movieLauncher   = taskSpawnFromTable(D_neo_ark_altar_8017EFC0, NEO_ARK_ALTAR_MOVIE_LAUNCHER_ENTRY, NEO_ARK_ALTAR_MOVIE_SECOND_SEQUENCE, 0);
    task->state           = task->state + 1;
}

/// Selects altar room 2 in live and saved state and requests a view reload.
///
/// Runs after the second solved sequence launches its movie task, then advances
/// to the state that restores player control and tile-sequence processing.
static void _neoArkAltarSelectPostSequenceRoom(Task* task)
{
    enum { NEO_ARK_ALTAR_POST_SEQUENCE_ROOM = 2 };
    s16* viewDirty;

    // Keep the reload request before both location updates; the pointer store
    // preserves that order under this compiler's memory scheduler.
    viewDirty                                                  = &gGameSession->viewDirty;
    *viewDirty                                                 = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = NEO_ARK_ALTAR_POST_SEQUENCE_ROOM;
    gGameSession->location.loc.room                            = NEO_ARK_ALTAR_POST_SEQUENCE_ROOM;
    task->state                                                = task->state + 1;
}

/// Restores the player, HUD and display and resumes the altar's tile puzzle.
///
/// Runs after selecting the post-sequence room. Returns to tile tracking in
/// state 2, keeping the task's existing work and puzzle progress alive.
static void _neoArkAltarResumeTileSequence(Task* task)
{
    enum { NEO_ARK_ALTAR_STATE_TILE_SEQUENCE = 2 };

    SetDispMask(1);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    gGameSession->hideHud = 0;
    task->state           = NEO_ARK_ALTAR_STATE_TILE_SEQUENCE;
}

void neoArkAltarEffectNoopTask(Task* unusedTask)
{
}
