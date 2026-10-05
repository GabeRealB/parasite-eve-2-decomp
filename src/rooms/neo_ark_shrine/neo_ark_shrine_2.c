#include "rooms/neo_ark_shrine.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_shrine_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_menu.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/pad_script.h"
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
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/action_prompt.h"

/// Work block of the shrine's two falling-prop tasks.
///
/// Allocated zeroed when a prop spawns and kept in `Task::work`. The two
/// matrices are the storage the prop's `TmdObject::colorMtx` and `lightMtx`
/// point at; the rest is the drop that brings the prop down from above the
/// room onto the floor, which starts from rest and whose acceleration itself
/// grows by a fixed step every frame.
typedef struct {
    MATRIX color;            // The model's colour matrix
    MATRIX light;            // The model's light matrix
    s16    fallAcceleration; // Added to `fallVelocity` every frame, in world units per frame squared; grows by the prop's own step each frame
    s16    fallVelocity;     // Added to the prop's height every frame, in world units per frame (positive is down)
    s16    fallFrames;       // Frames the drop has lasted; times its sound and its pad effect
} _NeoArkShrineFallingPropWork;
STATIC_ASSERT_SIZEOF(_NeoArkShrineFallingPropWork, 0x48);

static void func_neo_ark_shrine_8017F86C(Task* task);
static void func_neo_ark_shrine_8017FC14(SVECTOR* pos, s32 arg1, s32 arg2);

extern SVECTOR D_neo_ark_shrine_8018268C[];
extern SVECTOR D_neo_ark_shrine_80182694[];
extern SVECTOR D_neo_ark_shrine_8018269C[];
extern SVECTOR D_neo_ark_shrine_801826AC[];
extern SVECTOR D_neo_ark_shrine_801826C4[];
extern SVECTOR D_neo_ark_shrine_801826D4[];
extern SVECTOR D_neo_ark_shrine_80182704[];

/// Offset of the beam's near end from the effect's parent coordinate. The far
/// end's offset follows it directly; the beam's set-up state reaches that one
/// as element 1.

/// Offset of the beam's far end from the effect's parent coordinate.

static void func_neo_ark_shrine_8017ECC4(Task* task);
static void func_neo_ark_shrine_8017EDAC(Task* task);
static void func_neo_ark_shrine_8017EDE0(Task* task);
static void func_neo_ark_shrine_8017EE44(Task* task);
static void func_neo_ark_shrine_8017EED4(Task* task);
static void func_neo_ark_shrine_8017EF68(Task* task);
static void func_neo_ark_shrine_8017EFE4(Task* task);
static void func_neo_ark_shrine_8017F094(Task* task);
static void func_neo_ark_shrine_8017F0F0(Task* task);
static void func_neo_ark_shrine_8017F178(Task* task);
static void func_neo_ark_shrine_8017F21C(Task* task);
static void func_neo_ark_shrine_8017F274(Task* task);
static void func_neo_ark_shrine_8017F320(Task* task);
static void func_neo_ark_shrine_8017F398(Task* task);
static void func_neo_ark_shrine_8017F4C8(Task* task);
static void func_neo_ark_shrine_8017F578(Task* task);
static void func_neo_ark_shrine_8017F640(Task* task);
static void func_neo_ark_shrine_8017F688(Task* task);
static void func_neo_ark_shrine_8017F738(Task* task);

/// State table of the shrine's cap script task, indexed by `Task::state`.
static const TaskFuncTable16 D_neo_ark_shrine_8017D5D0 = {
    {
        func_neo_ark_shrine_8017ECC4,
        func_neo_ark_shrine_8017EDAC,
        func_neo_ark_shrine_8017D9A0,
        func_neo_ark_shrine_8017EDE0,
        func_neo_ark_shrine_8017EE44,
        func_neo_ark_shrine_8017EED4,
        func_neo_ark_shrine_8017DB10,
        func_neo_ark_shrine_8017EF68,
        func_neo_ark_shrine_8017EFE4,
        func_neo_ark_shrine_8017F094,
        func_neo_ark_shrine_8017F0F0,
        func_neo_ark_shrine_8017F178,
        func_neo_ark_shrine_8017F21C,
        func_neo_ark_shrine_8017F274,
        func_neo_ark_shrine_8017F320,
        func_neo_ark_shrine_8017F398,
    },
};
/// State table of the shrine's first falling prop, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_shrine_8017D610 = {
    { func_neo_ark_shrine_8017F4C8, func_neo_ark_shrine_8017F578, func_neo_ark_shrine_8017F640, taskKill },
};
/// State table of the shrine's second falling prop, indexed by `Task::state`.
static const TaskFuncTable3 D_neo_ark_shrine_8017D620 = {
    { func_neo_ark_shrine_8017F688, func_neo_ark_shrine_8017F738, taskKill },
};

extern WorldCollisionGrid     D_neo_ark_shrine_80182D2C[1];
extern WorldCollisionGrid     D_neo_ark_shrine_801831D8[1];
extern WorldCollisionGrid     D_neo_ark_shrine_80183698[1];
extern WorldCollisionOccluder D_neo_ark_shrine_80186730[4];
extern WorldCollisionTrigger  D_neo_ark_shrine_80185A80[14];
extern WorldCollisionTrigger  D_neo_ark_shrine_80185EA8[9];
extern WorldCollisionTrigger  D_neo_ark_shrine_80186154[8];
extern WorldCollisionTrigger  D_neo_ark_shrine_801863B4[8];
extern WorldCoordRoomLights   D_neo_ark_shrine_80185A68[1];

SVECTOR D_neo_ark_shrine_8018268C[1] = {
    { 3560, -1200, 6310, 0 },
};

SVECTOR D_neo_ark_shrine_80182694[1] = {
    { 4990, -1200, 6280, 0 },
};

SVECTOR D_neo_ark_shrine_8018269C[2] = {
    { 3560, -1200, 3690, 0 },
    { 4920, -1200, 3690, 0 },
};

SVECTOR D_neo_ark_shrine_801826AC[3] = {
    { 5770, -1200, 2980, 0 },
    { 5790, -1200, 1740, 0 },
    { 5780, -1200, 440, 0 },
};

SVECTOR D_neo_ark_shrine_801826C4[2] = {
    { 8320, -1200, 5070, 0 },
    { 8340, -1200, 3610, 0 },
};

SVECTOR D_neo_ark_shrine_801826D4[6] = {
    { 9060, -1200, 2750, 0 },
    { 0x28BE, -1200, 2880, 0 },
    { 0x2DBE, -1200, 2750, 0 },
    { 9060, -1200, 260, 0 },
    { 0x28AA, -1200, 260, 0 },
    { 0x2DBE, -1200, 260, 0 },
};

SVECTOR D_neo_ark_shrine_80182704[2] = {
    { 6300, -1870, -4490, 0 },
    { 7760, -1880, -4490, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_neo_ark_shrine_80182724[6] = {
    { D_neo_ark_shrine_80182D2C, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_80185EA8, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_801831D8, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_80186154, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_80183698, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_801863B4, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_80182D2C, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_80185EA8, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_801831D8, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_80186154, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_80183698, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_801863B4, D_neo_ark_shrine_80186730 },
};

WorldCoordRoomLighting D_neo_ark_shrine_80182784[6] = {
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
};

u8 D_neo_ark_shrine_801827B4[20] = {
    1,
    2,
    3,
    16,
    18,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    0,
    0,
};

u8 D_neo_ark_shrine_801827C8[20] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    17,
    11,
    12,
    13,
    14,
    15,
    16,
    10,
    18,
    0,
    0,
};

u8 D_neo_ark_shrine_801827DC[20] = {
    1,
    2,
    3,
    16,
    18,
    6,
    7,
    8,
    9,
    17,
    11,
    12,
    13,
    14,
    15,
    16,
    10,
    18,
    0,
    0,
};

u8* D_neo_ark_shrine_801827F0[6] = {
    gViewIdentityMap,
    D_neo_ark_shrine_801827B4,
    gViewIdentityMap,
    D_neo_ark_shrine_801827C8,
    D_neo_ark_shrine_801827DC,
    D_neo_ark_shrine_801827C8,
};

ViewCount D_neo_ark_shrine_80182808[6] = { 18, 18, 18, 18, 18, 18 };

DirectionWarpEntry D_neo_ark_shrine_80182814[3] = {
    { { { .word = 1024 }, 525, 0, 1200 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 525, 0, 1200 }, { 0, 0, 0, 0 }, 0x55150004, 0x55150003, 0x55150005, 9, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHRINE },
    { { { .word = 3072 }, 0x32C8, 0, 1530 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x32C8, 0, 1530 }, { 0, 0, 0, 0 }, 0x55150002, 0x55150001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 6950, 0, -3700 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 6950, 0, -3700 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 10, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkShrineCollision0576CNormals[10] = {
#include "assets/neo_ark_shrine_collision_0576C_normals.inc"
};

static SVECTOR _gNeoArkShrineCollision0576CVerts[46] = {
#include "assets/neo_ark_shrine_collision_0576C_verts.inc"
};

static WorldCollisionGridFace _gNeoArkShrineCollision0576CFaces[30] = {
#include "assets/neo_ark_shrine_collision_0576C_faces.inc"
};

static s16 _gNeoArkShrineCollision0576CCells[140] = {
#include "assets/neo_ark_shrine_collision_0576C_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkShrineCollision0576CCells[i])
static s16* _gNeoArkShrineCollision0576CTable[12] = {
#include "assets/neo_ark_shrine_collision_0576C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_shrine_80182D2C[1] = {
    { NULL, _gNeoArkShrineCollision0576CNormals, _gNeoArkShrineCollision0576CVerts, _gNeoArkShrineCollision0576CFaces, _gNeoArkShrineCollision0576CTable, 0, 5000, 4, 3, 4000, 30 },
};

static SVECTOR _gNeoArkShrineCollision05C18Normals[9] = {
#include "assets/neo_ark_shrine_collision_05C18_normals.inc"
};

static SVECTOR _gNeoArkShrineCollision05C18Verts[48] = {
#include "assets/neo_ark_shrine_collision_05C18_verts.inc"
};

static WorldCollisionGridFace _gNeoArkShrineCollision05C18Faces[31] = {
#include "assets/neo_ark_shrine_collision_05C18_faces.inc"
};

static s16 _gNeoArkShrineCollision05C18Cells[142] = {
#include "assets/neo_ark_shrine_collision_05C18_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkShrineCollision05C18Cells[i])
static s16* _gNeoArkShrineCollision05C18Table[12] = {
#include "assets/neo_ark_shrine_collision_05C18_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_shrine_801831D8[1] = {
    { NULL, _gNeoArkShrineCollision05C18Normals, _gNeoArkShrineCollision05C18Verts, _gNeoArkShrineCollision05C18Faces, _gNeoArkShrineCollision05C18Table, 0, 5000, 4, 3, 4000, 31 },
};

static SVECTOR _gNeoArkShrineCollision060D8Normals[9] = {
#include "assets/neo_ark_shrine_collision_060D8_normals.inc"
};

static SVECTOR _gNeoArkShrineCollision060D8Verts[50] = {
#include "assets/neo_ark_shrine_collision_060D8_verts.inc"
};

static WorldCollisionGridFace _gNeoArkShrineCollision060D8Faces[31] = {
#include "assets/neo_ark_shrine_collision_060D8_faces.inc"
};

static s16 _gNeoArkShrineCollision060D8Cells[144] = {
#include "assets/neo_ark_shrine_collision_060D8_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkShrineCollision060D8Cells[i])
static s16* _gNeoArkShrineCollision060D8Table[12] = {
#include "assets/neo_ark_shrine_collision_060D8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_shrine_80183698[1] = {
    { NULL, _gNeoArkShrineCollision060D8Normals, _gNeoArkShrineCollision060D8Verts, _gNeoArkShrineCollision060D8Faces, _gNeoArkShrineCollision060D8Table, 0, 5000, 4, 3, 4000, 31 },
};

ViewCamera D_neo_ark_shrine_801836BC[18] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7000, 0x5A28, -1050 } }, 329 },
    { { { { 642, 0, -4045 }, { -628, 4046, -99 }, { 3996, 636, 634 } }, { -7100, 1660, -950 } }, 329 },
    { { { { 617, 0, 4049 }, { 342, 4081, -52 }, { -4034, 346, 615 } }, { -0x32C8, 1390, -860 } }, 275 },
    { { { { 4049, 0, -613 }, { -103, 4037, -681 }, { 604, 689, 3991 } }, { -6330, 1660, 4750 } }, 257 },
    { { { { 4082, 0, -331 }, { -43, 4061, -529 }, { 329, 531, 4047 } }, { -6720, 1475, 5 } }, 329 },
    { { { { -574, 0, -4055 }, { -1, 4096, 0 }, { 4055, 1, -574 } }, { 1295, 970, -5540 } }, 329 },
    { { { { -802, 0, 4016 }, { 90, 4094, 18 }, { -4015, 92, -801 } }, { -6180, 1120, -5590 } }, 329 },
    { { { { 4022, 0, -772 }, { -101, 4060, -528 }, { 765, 537, 3987 } }, { -365, 1325, 425 } }, 282 },
    { { { { -4086, 0, -277 }, { -137, 3557, 2026 }, { 240, 2030, -3548 } }, { -610, 2540, -4060 } }, 282 },
    { { { { -4063, 0, -515 }, { -123, 3977, 970 }, { 500, 978, -3945 } }, { -6730, 1660, -480 } }, 225 },
    { { { { -4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, -4096 } }, { -7000, 1554, 3630 } }, 541 },
    { { { { 4085, 0, -297 }, { -133, 3662, -1828 }, { 265, 1832, 3653 } }, { -6700, 2300, -1490 } }, 348 },
    { { { { -3829, 0, -1452 }, { 701, 3587, -1848 }, { 1272, -1977, -3353 } }, { -7890, 1790, 2780 } }, 312 },
    { { { { 3986, 0, -938 }, { 71, 4084, 303 }, { 936, -312, 3975 } }, { -6140, 980, 4450 } }, 269 },
    { { { { 407, 0, -4075 }, { -498, 4065, -49 }, { 4045, 500, 404 } }, { -900, 1100, -770 } }, 380 },
    { { { { 4049, 0, -613 }, { -103, 4037, -681 }, { 604, 689, 3991 } }, { -6330, 1660, 4750 } }, 257 },
    { { { { -4063, 0, -515 }, { -123, 3977, 970 }, { 500, 978, -3945 } }, { -6730, 1660, -480 } }, 225 },
    { { { { 4082, 0, -331 }, { -43, 4061, -529 }, { 329, 531, 4047 } }, { -6720, 1475, 5 } }, 329 },
};

SpriteBatch D_neo_ark_shrine_80183944[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80183954[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_80183964[57] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, -72, 1175, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 48 } }, -136, -120, 750, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 48 } }, -136, -72, 750, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -136, -24, 750, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -136, 24, 750, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -136, 72, 750, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, -24, 1062, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, -72, 1062, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, -120, 1062, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -120, 1153, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, 24, 1062, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, 72, 1106, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -72, 40, 1153, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -72, 72, 1153, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, -120, 1187, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 1150, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 64, 72, 1162, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1150, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1150, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1150, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1150, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 1162, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 1162, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 1162, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 1162, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, 24, 1187, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 40, 72, 1187, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 1187, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, -120, 1187, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 48, -72, 1187, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -56, 1950, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, 24, 1950, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, -8, 1950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 24, -8, 1950, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, -56, 1950, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 24, -120, 1950, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 24, -104, 1950, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -56, 1950, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -104, 1950, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -120, 1950, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 104, -120, 1950, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -56, 1950, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -8, 1950, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -56, 1950, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -8, 1950, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 462, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 462, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, -24, 462, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, -72, 462, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, -120, 462, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 48, 812, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -136, 56, 812, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, -88, 56, 812, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 8, 812, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, -40, 812, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, -88, 812, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -136, -104, 812, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80183DD8[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 14, 16, 0, 0, { 3, 0 } },
    { 30, 15, 0, 0, { 2, 0 } },
    { 45, 5, 0, 0, { 4, 0 } },
    { 50, 7, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_80183E10[36] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -120, 1200, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -72, 1200, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1087, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1087, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1087, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1087, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -120, 1125, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -72, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -24, 1125, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 24, 1125, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -120, 1187, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -72, 1187, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -24, 1187, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 48, 24, 1187, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 24, 1200, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 1200, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 1062, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 987, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 987, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 987, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 987, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, 72, 987, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -120, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -72, 1000, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -24, 1000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, 24, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, 72, 1000, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -120, 1075, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, -72, 1062, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, -24, 1062, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, 24, 1062, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -56, 0, 1075, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, 24, 1075, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -32, 2125, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -80, 2125, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -72, -120, 2125, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_801840E0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 17, 0, 0, { 2, 0 } },
    { 33, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_80184108[9] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 862, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 862, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 862, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 862, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 72, 862, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -112, 72, 1000, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 24, 1000, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 0, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -120, 1000, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_801841BC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_801841D4[61] = {
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, 24, 987, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, -32, 987, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, -88, 987, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 32 } }, 104, -120, 987, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, 24, 987, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, -32, 987, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, -88, 987, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 64, -120, 987, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 32, -120, 1025, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 0, -120, 1025, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 40, 950, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, 40, 950, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -8, 950, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -56, 950, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -104, 950, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -160, -120, 950, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -112, -8, 950, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -56, 950, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -104, 950, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -112, -120, 950, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -72, -120, 950, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -32, -120, 950, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -56, 1950, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 40, 1950, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, 40, 1950, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -8, 1950, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -56, 1950, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 24, -104, 1950, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 24, -120, 1950, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -8, -120, 1950, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 48 } }, -8, -104, 1950, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -120, 1950, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -104, 1950, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -40, -64, 3125, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -40, -16, 3125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 8, -16, 3125, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 8, -64, 3125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 40, 8, 1750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 48, -40, 1750, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, 48, -72, 1750, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -112, 600, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -104, 600, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -160, -120, 600, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, -112, -104, 600, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -144, 40, 550, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -144, -8, 550, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -144, -56, 550, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -128, -72, 550, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -24, 550, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -96, 40, 550, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -96, 24, 550, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -8, 550, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -48, 48, 550, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 72, 625, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -136, 24, 625, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -136, -24, 625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -136, -48, 625, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -88, -8, 625, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -88, 24, 625, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, -88, 72, 625, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 40, 625, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80184698[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 6, 0 } },
    { 10, 12, 0, 0, { 1, 0 } },
    { 22, 11, 0, 0, { 4, 0 } },
    { 33, 4, 0, 0, { 0, 0 } },
    { 37, 3, 0, 0, { 5, 0 } },
    { 40, 4, 0, 0, { 3, 0 } },
    { 44, 9, 0, 0, { 7, 0 } },
    { 53, 8, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_801846E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_801846F8[16] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 925, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 925, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 925, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 925, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 925, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 925, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 925, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 925, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 24, 1125, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 24, 1137, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1125, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1125, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -120, 1125, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, -120, 1125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, -72, 1125, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -24, 1125, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80184838[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_80184850[18] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 937, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 937, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 937, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 937, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 937, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -120, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 1125, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, 24, 1125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 72, 1125, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -120, 1275, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -72, 1275, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -24, 1275, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 24, 1275, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -96, 1425, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -72, 1425, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -24, 1425, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 24, 1425, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_801849B8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_801849D0[14] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 450, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 104, 450, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 32, 450, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 0, 450, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -40, 450, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 450, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -104, 450, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 450, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 450, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 0, 450, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -32, 450, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -72, 450, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -104, 450, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 104, 450, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80184AE8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80184B08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80184B18[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_80184B28[28] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -48, 575, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, -120, 475, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -112, -120, 475, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -64, -120, 475, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -16, -120, 475, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 32, -120, 475, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, -120, 475, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, -120, 475, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -88, 525, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -112, -88, 525, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -64, -88, 525, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -16, -88, 525, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -88, 525, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, -64, 575, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -112, -64, 575, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, -64, 575, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, -48, 575, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 88, -120, 475, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -120, 475, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 88, -88, 475, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, -88, 475, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 88, -56, 525, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -56, 525, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 88, -32, 550, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -32, 550, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 88, -16, 562, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -16, 562, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 0, 575, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80184D58[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 1, 0 } },
    { 17, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80184D78[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80184D88[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_80184D98[34] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 32, 962, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -16, 962, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 32, 962, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -16, 962, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -64, 962, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -112, 962, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, -120, 962, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -64, 962, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -112, 962, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, -120, 962, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 32, 962, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -16, 962, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -64, 962, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -88, 962, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 16, 1000, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -32, 1000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -80, 1000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 112, -120, 1000, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 72, 16, 1000, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 72, -32, 1000, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 72, -80, 1000, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 72, -120, 1000, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 16, 1112, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, 16, 1250, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, -32, 1112, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -32, 1250, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, -80, 1112, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -80, 1250, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -120, 1125, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 24, -120, 1250, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -32, 1950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -80, 1950, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -72, -120, 1950, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80185040[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 17, 0, 0, { 2, 0 } },
    { 31, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_80185068[14] = {
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 475, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -160, 32, 475, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -160, 0, 475, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -160, -32, 475, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -160, -72, 475, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, -160, -104, 475, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, -112, 104, 485, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 475, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 475, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, 128, 0, 475, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, 136, -32, 475, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 152, -104, 475, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 144, -72, 475, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 104, 104, 475, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80185180[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_shrine_801851A0[10] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 875, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, 72, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 72, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 875, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 24, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 0, 875, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 875, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 875, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 875, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -120, 875, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80185268[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_shrine_80185280[18] = {
    { { .empty = D_neo_ark_shrine_80183944 }, D_neo_ark_shrine_80183944, NULL },
    { { .empty = D_neo_ark_shrine_80183954 }, D_neo_ark_shrine_80183954, NULL },
    { { .elements = D_neo_ark_shrine_80183964 }, D_neo_ark_shrine_80183DD8, NULL },
    { { .elements = D_neo_ark_shrine_80183E10 }, D_neo_ark_shrine_801840E0, NULL },
    { { .elements = D_neo_ark_shrine_80184108 }, D_neo_ark_shrine_801841BC, NULL },
    { { .elements = D_neo_ark_shrine_801841D4 }, D_neo_ark_shrine_80184698, NULL },
    { { .empty = D_neo_ark_shrine_801846E8 }, D_neo_ark_shrine_801846E8, NULL },
    { { .elements = D_neo_ark_shrine_801846F8 }, D_neo_ark_shrine_80184838, NULL },
    { { .elements = D_neo_ark_shrine_80184850 }, D_neo_ark_shrine_801849B8, NULL },
    { { .elements = D_neo_ark_shrine_801849D0 }, D_neo_ark_shrine_80184AE8, NULL },
    { { .empty = D_neo_ark_shrine_80184B08 }, D_neo_ark_shrine_80184B08, NULL },
    { { .empty = D_neo_ark_shrine_80184B18 }, D_neo_ark_shrine_80184B18, NULL },
    { { .elements = D_neo_ark_shrine_80184B28 }, D_neo_ark_shrine_80184D58, NULL },
    { { .empty = D_neo_ark_shrine_80184D78 }, D_neo_ark_shrine_80184D78, NULL },
    { { .empty = D_neo_ark_shrine_80184D88 }, D_neo_ark_shrine_80184D88, NULL },
    { { .elements = D_neo_ark_shrine_80184D98 }, D_neo_ark_shrine_80185040, NULL },
    { { .elements = D_neo_ark_shrine_80185068 }, D_neo_ark_shrine_80185180, NULL },
    { { .elements = D_neo_ark_shrine_801851A0 }, D_neo_ark_shrine_80185268, NULL },
};

WorldCoordLight D_neo_ark_shrine_80185358[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -332, -462, -388 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 750, 745, 740 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 267, -272, 272 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 348, 286, 245 }, { 0, 0 } },
};

WorldCoordPointLight D_neo_ark_shrine_80185408[17] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1198, -2188, 3758 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4100, 4038, 3936 }, { 0, 0 } }, 832, 4161 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6232, -1290, -505 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1845, 1765, 1703 }, { 0, 0 } }, 1685, 3529 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6055, -1410, 3087 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1475, 1311, 1232 }, { 0, 0 } }, 992, 2101 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3560, -1430, 3820 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1843, 1515, 1187 }, { 0, 0 } }, 600, 1400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4910, -1430, 3820 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2170, 1843 }, { 0, 0 } }, 900, 1180 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4579, -1430, 5537 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1621, 1496, 1375 }, { 0, 0 } }, 900, 1902 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3560, -1430, 6155 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1293, 1272, 1232 }, { 0, 0 } }, 363, 941 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7688, -1235, 4382 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1478, 1434, 1372 }, { 0, 0 } }, 1201, 1931 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CEB, -1360, 713 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2460, 2170, 1844 }, { 0, 0 } }, 881, 1663 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2E28, -1320, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2461, 2174, 1847 }, { 0, 0 } }, 1159, 1985 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x27BB, -1360, 2967 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 0, 0 }, { 0, 0 } }, 5, 357 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9402, -1360, 2037 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2458, 2171, 1844 }, { 0, 0 } }, 1182, 2084 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2869, -1360, 285 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 309, 226, 184 }, { 0, 0 } }, 150, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9060, -1360, 445 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2170, 1843 }, { 0, 0 } }, 900, 1603 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7005, -2049, -3922 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2376, 2338, 2276 }, { 0, 0 } }, 850, 3850 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -10, -2789, 1661 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4567, 4550, 4508 }, { 0, 0 } }, 2520, 6612 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -25, -2065, 1050 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1802, 1863, 1884 }, { 0, 0 } }, 200, 2200 },
};

WorldCoordRoomLights D_neo_ark_shrine_80185A68[1] = {
    { ARRAY_SIZE(D_neo_ark_shrine_80185358), D_neo_ark_shrine_80185358, ARRAY_SIZE(D_neo_ark_shrine_80185408), D_neo_ark_shrine_80185408, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_shrine_80185A80[14] = {
    { NULL, NULL, NULL, { 7039, -2432, -2210, 0 }, { { 2687, -2832, -10, 0 }, { -2698, -2832, 1, 0 }, { 2687, 2832, -10, 0 }, { -2698, 2832, 1, 0 } }, { 8, 0, 4099, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7087, -2401, -2096, 0 }, { { -2713, -2800, 14, 0 }, { 2705, -2800, -23, 0 }, { -2713, 2800, 14, 0 }, { 2705, 2800, -23, 0 } }, { -29, 0, -4099, 0 }, { 0, 0, 4096, 0 }, 3890, 0, 10, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8224, -2369, 1312, 0 }, { { 0, -2832, 1984, 0 }, { 0, -2832, -1984, 0 }, { 0, 2832, 1984, 0 }, { 0, 2832, -1984, 0 } }, { -4112, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3453, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8079, -2481, 1391, 0 }, { { 4, -2784, -1833, 0 }, { -3, -2784, 1834, 0 }, { 4, 2784, -1833, 0 }, { -3, 2784, 1834, 0 } }, { 4111, 0, 7, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28D1, -2416, 1408, 0 }, { { 208, -2880, -2032, 0 }, { -208, -2880, 2032, 0 }, { 208, 2880, -2032, 0 }, { -208, 2880, 2032, 0 } }, { 4088, 0, 418, 0 }, { 0, 0, 4096, 0 }, 3528, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2960, -2416, 1504, 0 }, { { -224, -2816, 2144, 0 }, { 224, -2816, -2144, 0 }, { -224, 2816, 2144, 0 }, { 224, 2816, -2144, 0 } }, { -4074, 0, -426, 0 }, { 0, 0, 4096, 0 }, 3537, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6767, -2545, 2671, 0 }, { { 1946, -2848, 68, 0 }, { -1945, -2848, -67, 0 }, { 1946, 2849, 68, 0 }, { -1945, 2849, -67, 0 } }, { -143, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6751, -2496, 2751, 0 }, { { -1810, -2800, -67, 0 }, { 1810, -2800, 68, 0 }, { -1810, 2800, -67, 0 }, { 1810, 2800, 68, 0 } }, { 152, 0, -4106, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1535, -2496, 5119, 0 }, { { 408, -2864, -2184, 0 }, { -418, -2864, 2174, 0 }, { 408, 2864, -2184, 0 }, { -418, 2864, 2174, 0 } }, { 4033, 0, 764, 0 }, { 0, 0, 4096, 0 }, 3620, 0, 6, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1630, -2480, 5215, 0 }, { { -415, -2848, 2178, 0 }, { 411, -2848, -2181, 0 }, { -415, 2848, 2178, 0 }, { 411, 2848, -2181, 0 } }, { -4028, 0, -764, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 8, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1054, -2400, 2080, 0 }, { { -2215, -2928, 128, 0 }, { 2215, -2928, -129, 0 }, { -2215, 2928, 128, 0 }, { 2215, 2928, -129, 0 } }, { -238, 0, -4097, 0 }, { 0, 0, 4096, 0 }, 3665, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1086, -2384, 1984, 0 }, { { 2215, -2848, -129, 0 }, { -2215, -2848, 128, 0 }, { 2215, 2848, -129, 0 }, { -2215, 2848, 128, 0 } }, { 237, 0, 4092, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5631, -2368, 5472, 0 }, { { -71, -2848, 1942, 0 }, { 64, -2848, -1948, 0 }, { -71, 2849, 1942, 0 }, { 64, 2849, -1948, 0 } }, { -4109, 0, -143, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5535, -2464, 5472, 0 }, { { 65, -2848, -1947, 0 }, { -70, -2848, 1943, 0 }, { 65, 2849, -1947, 0 }, { -70, 2849, 1943, 0 } }, { 4107, 0, 141, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_shrine_80185EA8[9] = {
    { NULL, NULL, NULL, { 400, -48, 1088, 0 }, { { -496, 0, -768, 0 }, { 496, 0, -768, 0 }, { -496, 0, 768, 0 }, { 496, 0, 768, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 914, WORLD_COLLISION_TRIGGER_ACTION_WARP, 17, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3100, -48, 1504, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, 18, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7008, -64, -4224, 0 }, { { -400, 0, -320, 0 }, { 400, 0, -320, 0 }, { -400, 0, 320, 0 }, { 400, 0, 320, 0 } }, { 0, 4095, 0, 0 }, { 151, 0, 4093, 0 }, 512, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 976, 0 }, { { -496, 0, -656, 0 }, { 496, 0, -656, 0 }, { -496, 0, 656, 0 }, { 496, 0, 656, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 822, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6992, -64, 5616, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5216, -64, -3840, 0 }, { { -496, 0, -1184, 0 }, { 496, 0, -1184, 0 }, { -496, 0, 1184, 0 }, { 496, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8864, -64, -3840, 0 }, { { -496, 0, -1184, 0 }, { 496, 0, -1184, 0 }, { -496, 0, 1184, 0 }, { 496, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5599, -64, -2273, 0 }, { { -954, 0, -858, 0 }, { -57, 0, -1282, 0 }, { 58, 0, 1283, 0 }, { 955, 0, 859, 0 } }, { 0, 4099, 0, 0 }, { 3513, 0, -2106, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8479, -64, -2048, 0 }, { { 368, 0, -1230, 0 }, { 1135, 0, -600, 0 }, { -1135, 0, 600, 0 }, { -368, 0, 1230, 0 } }, { 0, 4099, 0, 0 }, { -3290, 0, -2440, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_shrine_80186154[8] = {
    { NULL, NULL, NULL, { 400, -48, 1344, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, 17, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3100, -48, 1504, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, 18, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7024, -64, -4048, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { 601, 0, 4052, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7008, -64, 3680, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { -601, 0, -4052, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5184, -64, -3696, 0 }, { { -496, 0, -1264, 0 }, { 496, 0, -1264, 0 }, { -496, 0, 1264, 0 }, { 496, 0, 1264, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8864, -64, -3744, 0 }, { { -496, 0, -1248, 0 }, { 496, 0, -1248, 0 }, { -496, 0, 1248, 0 }, { 496, 0, 1248, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1342, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8511, -64, -2153, 0 }, { { 249, 0, -1143, 0 }, { 1015, 0, -514, 0 }, { -1046, 0, 531, 0 }, { -216, 0, 1128, 0 } }, { 0, 4095, 0, 0 }, { -3035, 0, -2751, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5599, -64, -2209, 0 }, { { -941, 0, -913, 0 }, { -25, 0, -1293, 0 }, { 26, 0, 1422, 0 }, { 942, 0, 786, 0 } }, { 0, 4099, 0, 0 }, { 3166, 0, -2598, 0 }, 1419, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_shrine_801863B4[8] = {
    { NULL, NULL, NULL, { 400, -48, 1344, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, 17, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3100, -48, 1504, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, 18, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7024, -64, -4016, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { 601, 0, 4052, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7008, -64, -1248, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { 201, 0, -4092, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5184, -64, -3856, 0 }, { { -496, 0, -1232, 0 }, { 496, 0, -1232, 0 }, { -496, 0, 1232, 0 }, { 496, 0, 1232, 0 } }, { 0, 4112, 0, 0 }, { 4096, 0, 0, 0 }, 1324, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8960, -64, -3776, 0 }, { { -496, 0, -1280, 0 }, { 496, 0, -1280, 0 }, { -496, 0, 1280, 0 }, { 496, 0, 1280, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5631, -64, -2321, 0 }, { { -911, 0, -789, 0 }, { -14, 0, -1213, 0 }, { 15, 0, 1214, 0 }, { 912, 0, 790, 0 } }, { 0, 4113, 0, 0 }, { 3784, 0, -1567, 0 }, 1207, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8399, -64, -2161, 0 }, { { 317, 0, -1041, 0 }, { 1052, 0, -375, 0 }, { -1051, 0, 376, 0 }, { -316, 0, 1042, 0 } }, { 0, 4098, 0, 0 }, { -3290, 0, -2440, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_shrine_80186614[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_shrine_8018662C[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_shrine_80186644[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_shrine_8018665C[3] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, Actor02500_D05B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_shrine_80186680[2] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_shrine_80186698[2] = {
    { 22, 22, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_402200_80154188 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_shrine_801866B0[2] = {
    { 39, 39, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_403900_801540E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_shrine_801866C8[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017BEB0, D_neo_ark_shrine_80186614 },
    { D_map_neo_ark_8017BEF0, D_neo_ark_shrine_8018662C },
    { D_map_neo_ark_8017BF40, D_neo_ark_shrine_80186644 },
    { D_map_neo_ark_8017BF90, D_neo_ark_shrine_8018665C },
    { D_map_neo_ark_8017C020, D_neo_ark_shrine_80186680 },
    { NULL, NULL },
    { D_map_neo_ark_8017C050, D_neo_ark_shrine_80186698 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017C070, D_neo_ark_shrine_801866B0 },
    { NULL, NULL },
};

WorldCollisionOccluder D_neo_ark_shrine_80186730[4] = {
    { NULL, NULL, { 2736, -2352, 2159, 0 }, { { -784, 3376, 1841, 0 }, { 784, 3376, -1840, 0 }, { -784, -3376, 1841, 0 }, { 784, -3376, -1840, 0 } }, { 3777, 0, 1608, 0 }, 3924, 1, 0 },
    { NULL, NULL, { 3119, -2080, -289, 0 }, { { 2728, 3376, 4275, 0 }, { -2727, 3376, -4275, 0 }, { 2728, -3376, 4275, 0 }, { -2727, -3376, -4275, 0 } }, { 3469, 0, -2214, 0 }, 6079, 1, 0 },
    { NULL, NULL, { 0x2800, -1888, -2288, 0 }, { { -1840, 3376, 2657, 0 }, { 1840, 3376, -2657, 0 }, { -1840, -3376, 2657, 0 }, { 1840, -3376, -2657, 0 } }, { 3371, 0, 2334, 0 }, 4664, 1, 0 },
    { NULL, NULL, { 0x2840, -1984, 4256, 0 }, { { 1904, 3376, 1617, 0 }, { -1904, 3376, -1616, 0 }, { 1904, -3376, 1617, 0 }, { -1904, -3376, -1616, 0 } }, { 2664, 0, -3139, 0 }, 4190, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_shrine_80186820 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionSurfaceProperties D_neo_ark_shrine_8018682C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_shrine_80186820 },
};

WorldCollisionSurfaceProperties D_neo_ark_shrine_80186834[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_shrine_8018683C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_shrine_80186820 },
};

WorldCollisionSurfaceProperties* D_neo_ark_shrine_80186844[8] = {
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_80186834,
    D_neo_ark_shrine_8018683C,
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_8018682C,
};

Task* D_neo_ark_shrine_80186864 = NULL;

s16 D_neo_ark_shrine_80186868 = 0;

s16 D_neo_ark_shrine_8018686C[16] = { 0 };

NeoArkShrineTileOrigin D_neo_ark_shrine_8018688C[16] = { 0 };

NeoArkShrineTileOrigin D_neo_ark_shrine_801868CC[16] = { 0 };

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// Task callback of the action-prompt cursor: state 0 resets both prompt slots,
/// state 1 moves the cursor from the pad every frame after.
void func_neo_ark_shrine_8017EA70(Task* task)
{
    TaskFunc states[2] = { actionPromptReset, actionPromptMoveCursors };

    states[task->state](task);
}

/// Per-frame helper of the cap script: animates and draws the sliding-tile
/// puzzle.
void func_neo_ark_shrine_8017EAC0()
{
    func_neo_ark_shrine_8017DF7C();
}

/// Task callback of the shrine's cap script: dispatches `Task::state` through a
/// copy of the script's state table.
void func_neo_ark_shrine_8017EAE0(Task* task)
{
    TaskFuncTable16 sp;

    sp = D_neo_ark_shrine_8017D5D0;
    sp.funcs[task->state](task);
}

/// Task callback of the shrine's first falling prop: dispatches `Task::state`
/// through a copy of the prop's state table.
void func_neo_ark_shrine_8017EB54(Task* task)
{
    TaskFuncTable4 states;

    states = D_neo_ark_shrine_8017D610;
    states.funcs[task->state](task);
}

/// Task callback of the shrine's second falling prop: dispatches `Task::state`
/// through a copy of the prop's state table.
void func_neo_ark_shrine_8017EBB8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_shrine_8017D620;
    sp.funcs[task->state](task);
}

#include "../../shared/action_prompt_hit_test.inc.c"

/// Task callback of the descriptor at `D_neo_ark_shrine_80182404`: allocates
/// the puzzle's work block, sets the global mode byte, steps the task on one
/// state and clears the shrine's hotspot list.
static void func_neo_ark_shrine_8017ECC4(Task* task)
{
    NeoArkShrinePuzzleWork* work;
    ActionPromptHotspot*    hs;

    work = memCalloc(sizeof(NeoArkShrinePuzzleWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                    = taskSpawnFromTable(D_neo_ark_shrine_80182404, 0, 1, 0);
    task->work                                                 = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xB;
    /* The once-loop folds away, but flow counts its references at loop depth
       2: without it the parameter's priority (6*2/42) loses to the state
       pointer's (3*1/10) and the two swap callee-saved homes. Keeping the
       state load below the mode store is the same loop's scheduling edge. */
    do {
        task->state++;
    } while (0);
    Display_AcquireRef();
    for (hs = D_neo_ark_shrine_80182430; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

/// Cap script state 1: arms the first action prompt at `ACTION_PROMPT_SPEED_AIM`
/// with the idle cursor, zeroes its on-screen position, and steps the script on.
static void func_neo_ark_shrine_8017EDAC(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Spawns the action prompt for the script's current step: runs the shrine's
/// per-step helper, hides the cursor and stops it, then re-spawns the
/// prompt at the coordinates the gameplay side left in `D_80114D28` with the
/// display mode this step picked, and advances the task to state 4.
static void func_neo_ark_shrine_8017EDE0(Task* task)
{
    ActionPrompt*           prompt = D_80114D28;
    NeoArkShrinePuzzleWork* work   = task->work;

    func_neo_ark_shrine_8017EAC0(task);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Hides the action prompt's cursor, stops it, and runs the shrine's per-step
/// helper. When `func_800D4EC0` reports success, starts cap slot 2 if the
/// latched `selection` is `NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD`, and otherwise
/// sets `boardExamined` and starts cap slot 1. The task advances to state 2
/// on every path.
static void func_neo_ark_shrine_8017EE44(Task* task)
{
    ActionPrompt*           prompt = D_80114D28;
    NeoArkShrinePuzzleWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    func_neo_ark_shrine_8017EAC0(task);
    if (func_800D4EC0() == 0) {
        task->state = 2;
        return;
    }
    if (work->selection == NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD) {
        Gp_StartCapSlot(2, 0, 0);
        task->state = 2;
        return;
    }
    work->boardExamined = 1;
    Gp_StartCapSlot(1, 0, 0);
    task->state = 2;
}

static void func_neo_ark_shrine_8017EED4(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xA;
    /* Without this the scheduler hoists the `spawnArg2` load above the
       `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` byte store, which then fills `taskKill`'s delay slot. */
    taskKill(task->spawnArg2.pointer);
    Task_RequestKill(task, 0);
}

/// Same as `func_neo_ark_shrine_8017F320`, but it latches the script's pad
/// mode on rather than off.
static void func_neo_ark_shrine_8017EF68(Task* task)
{
    ActionPrompt*           prompt = D_80114D28;
    NeoArkShrinePuzzleWork* work   = task->work;

    Gp_SpawnPadLerp(0x12, 0x30, 0x90);
    D_neo_ark_shrine_80186868 = 1;
    prompt->mode              = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed       = ACTION_PROMPT_SPEED_STOPPED;
    func_neo_ark_shrine_8017EAC0(task);
    work->timer = 0;
    task->state++;
}

/// The script step that runs while the shrine's pad is idle: it re-clears the
/// prompt, ticks the step's timer, and once the step has run 0x1E frames latches
/// the shrine's mode — 2, or 5 when flag 0xE9 is set — into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room` and the
/// session, which makes the room rebuild its objects, and enters state 2.
///
/// The same literal is stored in both arms on purpose: `gGameSession` is read
/// per arm, and jump_optimize's cross-jumping (post-sched2) merges the arms'
/// identical `sb` pairs into the join. Written with one shared `var_v0` the
/// stores are one pair too but the constant's `li` precedes the address, the
/// merge swallows the `gGameSession` load as well, and the function comes out
/// four insns short.
static void func_neo_ark_shrine_8017EFE4(Task* task)
{
    ActionPrompt*           prompt = D_80114D28;
    NeoArkShrinePuzzleWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    work->timer         = work->timer + 1;
    func_neo_ark_shrine_8017EAC0(task);
    if (work->timer >= 0x1E) {
        if (gameFlagGetNibble(GAME_FLAG_0E9) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
            gGameSession->location.loc.room                            = 2;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 5;
            gGameSession->location.loc.room                            = 5;
        }
        gGameSession->roomObjsDirty = 1;
        task->state                 = 2;
    }
}

static void func_neo_ark_shrine_8017F094(Task* task)
{
    NeoArkShrinePuzzleWork* work;

    work                      = task->work;
    D_neo_ark_shrine_8018686A = 1;
    func_neo_ark_shrine_8017EAC0();
    taskKill(task->spawnArg2.pointer);
    work->timer = 0;
    task->state++;
}

static void func_neo_ark_shrine_8017F0F0(Task* task)
{
    NeoArkShrinePuzzleWork* work;
    u16                     timer;

    work = task->work;
    func_neo_ark_shrine_8017EAC0();
    timer       = work->timer + 1;
    work->timer = timer;
    if (timer >= 0x1EU) {
        taskSpawnFromTable(D_neo_ark_shrine_80182508, 1, 0, 0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xE;
        /* Without this the scheduler hoists the `task->state` reload above the
           `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` byte store to fill its load-delay slot. */
        work->timer = 0;
        task->state++;
    }
}

static void func_neo_ark_shrine_8017F178(Task* task)
{
    NeoArkShrinePuzzleWork* work;
    u16                     timer;
    s32                     next;

    work        = task->work;
    timer       = work->timer + 1;
    work->timer = timer;
    if (timer >= 0x5AU) {
        work->timer = 0;
        if (gameFlagGetNibble(GAME_FLAG_0E9) == 0) {
            taskSpawnFromTable(D_neo_ark_shrine_80182508, 2, 0, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xD;
            gameFlagSetNibble(GAME_FLAG_0E9, 1);
            next = task->state + 1;
        } else {
            next = task->state + 2;
        }
        task->state = next;
    }
}

static void func_neo_ark_shrine_8017F21C(Task* task)
{
    NeoArkShrinePuzzleWork* work;
    u16                     timer;

    work        = task->work;
    timer       = work->timer + 1;
    work->timer = timer;
    if (timer == 0x1E) {
        gSceneCombatState.shrineEnemyPhase = SCENE_COMBAT_SHRINE_REVEALED;
    }
    if (work->timer >= 0x3CU) {
        task->state++;
    }
}

static void func_neo_ark_shrine_8017F274(Task* task)
{
    gSceneCombatState.shrineEnemyPhase                         = SCENE_COMBAT_SHRINE_RELEASED;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 6;
    gGameSession->location.loc.room                            = 6;
    gGameSession->roomObjsDirty                                = 1;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xA;
    Task_RequestKill(task, 0);
}

/// Runs the shrine's per-step helper and restarts the work block's `timer`:
/// raises a pad lerp, hides the cursor and stops it, and advances the
/// task to the next state.
static void func_neo_ark_shrine_8017F320(Task* task)
{
    ActionPrompt*           prompt = D_80114D28;
    NeoArkShrinePuzzleWork* work   = task->work;

    Gp_SpawnPadLerp(0x12, 0x30, 0x90);
    D_neo_ark_shrine_80186868 = 0;
    prompt->mode              = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed       = ACTION_PROMPT_SPEED_STOPPED;
    func_neo_ark_shrine_8017EAC0(task);
    work->timer = 0;
    task->state++;
}

/// Same as `func_neo_ark_shrine_8017EFE4`, but the mode it latches is 1, or 4
/// when flag 0xE9 is set.
static void func_neo_ark_shrine_8017F398(Task* task)
{
    ActionPrompt*           prompt = D_80114D28;
    NeoArkShrinePuzzleWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    work->timer         = work->timer + 1;
    func_neo_ark_shrine_8017EAC0(task);
    if (work->timer >= 0x1E) {
        if (gameFlagGetNibble(GAME_FLAG_0E9) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
            gGameSession->location.loc.room                            = 1;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 4;
            gGameSession->location.loc.room                            = 4;
        }
        gGameSession->roomObjsDirty = 1;
        task->state                 = 2;
    }
}

/// Resets the shrine's 16-slot arrangement puzzle to its starting state: clears
/// the two puzzle flags, puts every tile's drawn position back at its starting
/// origin, and re-seeds the slot arrangement with the room's starting order.
void func_neo_ark_shrine_8017F448(void)
{
    NeoArkShrineTileOrigin* dstOrigin;
    NeoArkShrineTileOrigin* srcOrigin;
    s16*                    dstOrder;
    u16*                    srcOrder;
    s32                     i;
    s16                     y;
    u16                     order;

    i                         = 0;
    dstOrigin                 = D_neo_ark_shrine_8018688C;
    srcOrigin                 = D_neo_ark_shrine_8018256C;
    D_neo_ark_shrine_8018686A = 0;
    D_neo_ark_shrine_80186868 = 0;
    do {
        i++;
        dstOrigin->x = srcOrigin->x;
        y            = srcOrigin->y;
        srcOrigin++;
        dstOrigin->y = y;
        dstOrigin++;
    } while (i < 0x10);

    i        = 0;
    dstOrder = D_neo_ark_shrine_8018686C;
    srcOrder = D_neo_ark_shrine_80182410;
    do {
        order = *srcOrder;
        srcOrder++;
        i++;
        *dstOrder = order;
        dstOrder++;
    } while (i < 0x10);
}

/// Second state of the shrine's first falling prop: allocates its 0x48-byte
/// scratch block, republishes the block's light / colour matrices onto the
/// model's `TmdObject`, parks the prop at its starting position parented to the
/// room's view coordinate system, and advances the task to the falling state.
static void func_neo_ark_shrine_8017F4C8(Task* task)
{
    TmdObject*                    extra;
    GfxCoord*                     coord;
    _NeoArkShrineFallingPropWork* work;

    extra      = task->extra.tmd;
    coord      = extra->coords;
    work       = memCalloc(sizeof(_NeoArkShrineFallingPropWork), 0);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    extra->lightMtx   = &work->light;
    extra->flags      = 0;
    extra->colorMtx   = &work->color;
    coord->parent     = &gGfxViewCoord;
    coord->coord.t[0] = 0x1B58;
    coord->coord.t[1] = -0xBB8;
    coord->coord.t[2] = -0x3E8;
    func_neo_ark_shrine_8017F86C(task);
    task->state++;
}

static void func_neo_ark_shrine_8017F578(Task* task)
{
    _NeoArkShrineFallingPropWork* work;
    GfxCoord*                     coord;

    work  = task->work;
    coord = task->extra.tmd->coords;
    work->fallFrames++;
    if (work->fallFrames == 4) {
        Gp_SpawnPadLerp(0x18, 0x40, 0xFF);
        sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_PROP_1_FALL, 0, 0);
    }
    // The drop accelerates harder every frame, and stops dead at floor height.
    work->fallAcceleration += 1;
    work->fallVelocity     += work->fallAcceleration;
    coord->coord.t[1]      += work->fallVelocity;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = 0;
        task->state++;
    }
    func_neo_ark_shrine_8017F86C(task);
}

static void func_neo_ark_shrine_8017F640(Task* task)
{
    func_neo_ark_shrine_8017F86C(task);
    if (D_neo_ark_shrine_8018686A == 0) {
        task->state++;
    }
}

/// Second state of the shrine's second falling prop: as `func_neo_ark_shrine_8017F4C8`,
/// but parked at the mirror position on the far side of the shrine.
static void func_neo_ark_shrine_8017F688(Task* task)
{
    TmdObject*                    extra;
    GfxCoord*                     coord;
    _NeoArkShrineFallingPropWork* work;

    extra      = task->extra.tmd;
    coord      = extra->coords;
    work       = memCalloc(sizeof(_NeoArkShrineFallingPropWork), 0);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    extra->lightMtx   = &work->light;
    extra->flags      = 0;
    extra->colorMtx   = &work->color;
    coord->parent     = &gGfxViewCoord;
    coord->coord.t[0] = 0x222E;
    coord->coord.t[1] = -0xBB8;
    coord->coord.t[2] = -0x11C6;
    func_neo_ark_shrine_8017F86C(task);
    task->state++;
}

static void func_neo_ark_shrine_8017F738(Task* task)
{
    _NeoArkShrineFallingPropWork* work;
    GfxCoord*                     coord;

    work  = task->work;
    coord = task->extra.tmd->coords;
    work->fallFrames++;
    if (work->fallFrames == 2) {
        sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_PROP_2_FALL, 0, 0);
    }
    if (work->fallFrames == 0x12) {
        Gp_SpawnPadLerp(0xA, 0xA0, 0xFF);
    }
    // The drop accelerates harder every frame, and stops dead at floor height.
    work->fallAcceleration += 2;
    work->fallVelocity     += work->fallAcceleration;
    coord->coord.t[1]      += work->fallVelocity;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = 0;
        task->state++;
    }
    func_neo_ark_shrine_8017F86C(task);
}

#include "../../shared/action_prompt_reset.inc.c"

/// Tail every falling-prop handler runs: clears the prop's root coordinate
/// flag, rebuilds its world matrix, and rebuilds its lighting through
/// `worldCoordSetModelLighting` at a sample position 0x320 below its world origin.
static void func_neo_ark_shrine_8017F86C(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj                 = task->extra.tmd;
    coord               = obj->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
}

/// On the task's first tick stores three ids (0x601DF, 0x601FB, 0x60217) into
/// the `gRoomEffectFlashId` / `gRoomEffectTwinTrailId` / `gRoomEffectSparkBurstId` slots; then, every tick, runs
/// `func_neo_ark_shrine_8017FC14` over the positions the current camera view
/// shows, drawn from one of the room's `SVECTOR` arrays.
void func_neo_ark_shrine_8017F8DC(Task* task)
{
    if (task->state == 0) {
        gRoomEffectFlashId      = EFFECT_NEO_ARK_SHRINE_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_NEO_ARK_SHRINE_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_NEO_ARK_SHRINE_SPARK_BURST;
        task->state             = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_neo_ark_shrine_801826D4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 3: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[8], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[9], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[10], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 5:
        case 18: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_neo_ark_shrine_8018269C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 7: {
            SVECTOR* p = D_neo_ark_shrine_8018268C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 12: {
            SVECTOR* p = D_neo_ark_shrine_80182694;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 14: {
            SVECTOR* p = D_neo_ark_shrine_801826C4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 16: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 10:
        case 17: {
            SVECTOR* p = D_neo_ark_shrine_80182704;
            func_neo_ark_shrine_8017FC14(&p[0], 0, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 0, 0x300);
            break;
        }
    }
}

/// Projects the world-space point `pos` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues one semi-transparent `POLY_FT4` sprite
/// centred on it (tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects
/// the 40-texel UV column `(s16)arg1 * 40` at v=0..0x27, and `arg2` is a signed
/// half-extent whose on-screen radius is `(s16)arg2 * 39 / otz`. All three RGB
/// channels take `0x20`, plus 0x10 on odd `animFrame` values, so the sprite
/// flickers frame to frame.
static void func_neo_ark_shrine_8017FC14(SVECTOR* pos, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    GlowCentreScratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                idx;
    s32                u0;
    s32                u1;
    s32                sarg;
    s32                blend;
    s16                xy;

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (GlowCentreScratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(pos);
    gte_rtps();
    ds    = &gDisplayState;
    blend = (((u8)ds->animFrame & 1) * 16) + 0x20;
    gte_stsxy(&((GlowCentreScratch*)(head - 0x10))->sx);
    gte_stflg(&((GlowCentreScratch*)(head - 0x10))->flag);
    if (((GlowCentreScratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        sarg        = (s16)arg2;
        setRGB0(prim, blend, blend, blend);
        prim->u0                          = u0;
        prim->v0                          = 0;
        prim->u1                          = u1;
        prim->v1                          = 0;
        prim->u2                          = u0;
        prim->v2                          = 0x27;
        prim->u3                          = u1;
        prim->v3                          = 0x27;
        prim->code                       |= 2;
        ((GlowCentreScratch*)tmp)->radius = (sarg * 40 - sarg) / ((GlowCentreScratch*)(head - 0x10))->otz;
        xy                                = ((GlowCentreScratch*)tmp)->sx - (u16)((GlowCentreScratch*)tmp)->radius;
        prim->x2                          = xy;
        prim->x0                          = xy;
        xy                                = ((GlowCentreScratch*)tmp)->sx + (u16)((GlowCentreScratch*)tmp)->radius;
        prim->x3                          = xy;
        prim->x1                          = xy;
        xy                                = ((GlowCentreScratch*)tmp)->sy - (u16)((GlowCentreScratch*)tmp)->radius;
        prim->y1                          = xy;
        prim->y0                          = xy;
        xy                                = ((GlowCentreScratch*)tmp)->sy + (u16)((GlowCentreScratch*)tmp)->radius;
        prim->y3                          = xy;
        prim->y2                          = xy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((GlowCentreScratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_neo_ark_shrine_8017FEA0(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_neo_ark_shrine_80180904(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_shrine_801811EC(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
