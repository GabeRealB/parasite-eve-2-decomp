#include "rooms/dryfield_warehouse.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_warehouse_private.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflow.h"
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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/screen_fade.h"
#include "../../shared/glow_draw.h"

/// Commands the warehouse cutscene script leaves for the cutscene task.
///
/// Each one is stored over the previous command and restarts `commandStep`.
/// The task carries it out on its later updates. Only the fade-out and the
/// restore clear themselves; the others keep running until the script stores
/// the next command.
enum {
    /// Nothing pending.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_NONE = 0,
    /// Enables the display, spawns the fade-in task, suppresses the player's
    /// effects, installs the room's animation set on the player and plays its
    /// animation 1, and places the player at the cutscene placement. Then
    /// keeps the looping area sound going.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_PLACE_AND_FADE_IN = 1,
    /// Spawns the fade-out task and records it as the room's fade task.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_SPAWN_FADE_OUT = 2,
    /// Restores suppressed player effects, plays the equipped weapon's
    /// animation 1 and places the player at the closing placement on every
    /// update. Spawns the fade-in task first, then raises staggered dust
    /// puffs for `DRYFIELD_WAREHOUSE_CUTSCENE_DUST_FRAMES` updates.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_RESTORE_AND_FADE_IN = 3,
    /// Holds the screen black, selects room 2 in the saved and live location
    /// and, one update later, marks the view and room objects dirty. Plays
    /// area sound 4 on its eleventh update.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_SWITCH_ROOM = 4,
    /// Holds the screen black and keeps the looping area sound going,
    /// restarting its interval.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_LOOP_SOUND = 5,
};

/// Updates between two plays of the cutscene's looping area sound.
#define DRYFIELD_WAREHOUSE_CUTSCENE_SOUND_LOOP_FRAMES 60

/// Updates the restore command spends raising dust before it clears itself.
#define DRYFIELD_WAREHOUSE_CUTSCENE_DUST_FRAMES 36

/// Work block of the warehouse cutscene task.
///
/// The task allocates and zeroes the whole block when the cutscene starts,
/// then publishes itself so the room's script callbacks can reach it.
typedef struct {
    Task* player;                  // Player task captured when the cutscene starts. The animation install tests NULL; the other sends do not
    u16   command;                 // Pending `DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_*`
    u16   commandStep;             // Step within the command; zeroed with each new command
    u16   commandFrames;           // Updates the restore and room-switch commands have run since their first step. The placement command zeroes it and never reads it
    byte  field_A[2];              // Allocated and cleared with the block; no access. Role and width unproven
    u16   playerEffectsSuppressed; // 0 none pending; 1 player effects were killed and still need to be spawned back
    u16   soundLoopFrames;         // Updates counted towards the looping area sound, played each time the count divides by its interval
} _DryfieldWarehouseCutsceneWork;
STATIC_ASSERT_SIZEOF(_DryfieldWarehouseCutsceneWork, 0x10);

extern AnimationSet*  D_dryfield_warehouse_8017F848[2];
extern ActorTransform D_dryfield_warehouse_8017F850;
extern ActorTransform D_dryfield_warehouse_8017F868;

extern EvsCommand D_dryfield_warehouse_8017F880[];
extern EvsCommand D_dryfield_warehouse_8017FA00[];

/// Points in the space of the coordinate drawn under: ring centres, one per
/// circle, for the ring drawer, and prism corners for the prism drawer, which
/// the room's only caller points at `[8..15]`.
extern SVECTOR gGlowPrismCorners[];
/// Ring radii, parallel to the centres.
extern s16 D_dryfield_warehouse_8017FBAC[];

extern WorldCollisionGrid D_dryfield_warehouse_801802A8[1];
extern WorldCollisionGrid D_dryfield_warehouse_801809AC[1];
extern WorldCollisionGrid D_dryfield_warehouse_80181038[1];

void func_dryfield_warehouse_8017E090(Task*);
void func_dryfield_warehouse_8017E308(Task*);

void func_dryfield_warehouse_8017DA58(s32);
void func_dryfield_warehouse_8017E3F4(s16);

TaskMessageEntry D_dryfield_warehouse_8017F554[3] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_warehouse_8017D824 },
    { 5105, func_dryfield_warehouse_8017D764 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_warehouse_8017F56C[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_warehouse_8017D8D4, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_warehouse_8017D5E8, { .value = 0 } },
};

static AnimationPackedPose _gDryfieldWarehouseAnimation02260Bank1[2] = {
#include "assets/dryfield_warehouse_animation_02260_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWarehouseAnimation02260Bank4[28] = {
#include "assets/dryfield_warehouse_animation_02260_bank4.inc"
};

static AnimationRecord _gDryfieldWarehouseAnimation02260Records[123] = {
#include "assets/dryfield_warehouse_animation_02260_records.inc"
};

static u16 _gDryfieldWarehouseAnimation02260Indices[20] = {
#include "assets/dryfield_warehouse_animation_02260_indices.inc"
};

static AnimationSet _gDryfieldWarehouseAnimation02260 = {
    _gDryfieldWarehouseAnimation02260Records,
    _gDryfieldWarehouseAnimation02260Indices,
    { NULL, _gDryfieldWarehouseAnimation02260Bank1, NULL, NULL, _gDryfieldWarehouseAnimation02260Bank4, NULL, NULL, NULL },
};

AnimationSet* D_dryfield_warehouse_8017F848[2] = {
    NULL,
    &_gDryfieldWarehouseAnimation02260,
};

ActorTransform D_dryfield_warehouse_8017F850 = { { 5540, 0, -2300, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_dryfield_warehouse_8017F868 = { { 4654, 0, -1968, 0 }, { 0, 512, 0, 0 } };

EvsCommand D_dryfield_warehouse_8017F880[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_LOOP_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_PLACE_AND_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_SPAWN_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_SWITCH_ROOM }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_RESTORE_AND_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_warehouse_8017FA00[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_warehouse_8017DA58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_warehouse_8017DA58 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_warehouse_8017FB08[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_warehouse_8017E090, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_warehouse_8017E308, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeInTask, { .value = 0 } },
};

SVECTOR gGlowPrismCorners[16] = {
    { 1190, -2950, -1540, 0 },
    { 310, -510, -180, 0 },
    { 2130, -2950, -3235, 0 },
    { 1200, -400, -1770, 0 },
    { 4340, -2950, -2940, 0 },
    { 3270, 0, -1290, 0 },
    { 3990, -2950, -1990, 0 },
    { 3030, -290, -510, 0 },
    { 5140, -2100, -3920, 0 },
    { 3860, -2100, -3920, 0 },
    { 3860, -660, -3920, 0 },
    { 5140, -660, -3920, 0 },
    { 4340, 0, -1930, 0 },
    { 3060, 0, -1930, 0 },
    { 3500, 0, -3030, 0 },
    { 4780, 0, -3030, 0 },
};

s16 D_dryfield_warehouse_8017FBAC[8] = {
    100,
    150,
    125,
    175,
    135,
    185,
    100,
    150,
};

WorldCollisionRoomResources D_dryfield_warehouse_8017FBBC[3] = {
    { D_dryfield_warehouse_801802A8, D_dryfield_warehouse_801816A4, D_dryfield_warehouse_801817D4, NULL },
    { D_dryfield_warehouse_801809AC, D_dryfield_warehouse_801816A4, D_dryfield_warehouse_80181BB0, NULL },
    { D_dryfield_warehouse_80181038, D_dryfield_warehouse_801816A4, D_dryfield_warehouse_801817D4, NULL },
};

u8 D_dryfield_warehouse_8017FBEC[12] = {
    1,
    2,
    6,
    7,
    5,
    6,
    7,
    8,
    9,
    0,
    0,
    0,
};

u8 D_dryfield_warehouse_8017FBF8[12] = {
    1,
    2,
    9,
    8,
    5,
    6,
    7,
    8,
    9,
    0,
    0,
    0,
};

u8* D_dryfield_warehouse_8017FC04[3] = {
    gViewIdentityMap,
    D_dryfield_warehouse_8017FBEC,
    D_dryfield_warehouse_8017FBF8,
};

ViewCount D_dryfield_warehouse_8017FC10[3] = { 9, 9, 9 };

WorldCoordRoomLighting D_dryfield_warehouse_8017FC18[3] = {
    { D_dryfield_warehouse_801820E8, NULL },
    { D_dryfield_warehouse_801820E8, NULL },
    { D_dryfield_warehouse_801820E8, NULL },
};

DirectionWarpEntry D_dryfield_warehouse_8017FC30[2] = {
    { { { .word = 0 }, 2631, 0, -3535 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 1721, 0, -2640 }, { 0, 0, 0, 0 }, 0x52070002, 0x52070001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 468 },
    { { { .word = 3072 }, 5437, 2, -1084 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4856, 2, -2146 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldWarehouseCollision02CE8Normals[7] = {
#include "assets/dryfield_warehouse_collision_02CE8_normals.inc"
};

static SVECTOR _gDryfieldWarehouseCollision02CE8Verts[90] = {
#include "assets/dryfield_warehouse_collision_02CE8_verts.inc"
};

static WorldCollisionGridFace _gDryfieldWarehouseCollision02CE8Faces[50] = {
#include "assets/dryfield_warehouse_collision_02CE8_faces.inc"
};

static s16 _gDryfieldWarehouseCollision02CE8Cells[80] = {
#include "assets/dryfield_warehouse_collision_02CE8_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldWarehouseCollision02CE8Cells[i])
static s16* _gDryfieldWarehouseCollision02CE8Table[2] = {
#include "assets/dryfield_warehouse_collision_02CE8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_warehouse_801802A8[1] = {
    { NULL, _gDryfieldWarehouseCollision02CE8Normals, _gDryfieldWarehouseCollision02CE8Verts, _gDryfieldWarehouseCollision02CE8Faces, _gDryfieldWarehouseCollision02CE8Table, 0, 4000, 2, 1, 4000, 50 },
};

static SVECTOR _gDryfieldWarehouseCollision033ECNormals[7] = {
#include "assets/dryfield_warehouse_collision_033EC_normals.inc"
};

static SVECTOR _gDryfieldWarehouseCollision033ECVerts[99] = {
#include "assets/dryfield_warehouse_collision_033EC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldWarehouseCollision033ECFaces[56] = {
#include "assets/dryfield_warehouse_collision_033EC_faces.inc"
};

static s16 _gDryfieldWarehouseCollision033ECCells[112] = {
#include "assets/dryfield_warehouse_collision_033EC_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldWarehouseCollision033ECCells[i])
static s16* _gDryfieldWarehouseCollision033ECTable[4] = {
#include "assets/dryfield_warehouse_collision_033EC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_warehouse_801809AC[1] = {
    { NULL, _gDryfieldWarehouseCollision033ECNormals, _gDryfieldWarehouseCollision033ECVerts, _gDryfieldWarehouseCollision033ECFaces, _gDryfieldWarehouseCollision033ECTable, 0, 4100, 2, 2, 4000, 56 },
};

static SVECTOR _gDryfieldWarehouseCollision03A78Normals[7] = {
#include "assets/dryfield_warehouse_collision_03A78_normals.inc"
};

static SVECTOR _gDryfieldWarehouseCollision03A78Verts[91] = {
#include "assets/dryfield_warehouse_collision_03A78_verts.inc"
};

static WorldCollisionGridFace _gDryfieldWarehouseCollision03A78Faces[52] = {
#include "assets/dryfield_warehouse_collision_03A78_faces.inc"
};

static s16 _gDryfieldWarehouseCollision03A78Cells[108] = {
#include "assets/dryfield_warehouse_collision_03A78_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldWarehouseCollision03A78Cells[i])
static s16* _gDryfieldWarehouseCollision03A78Table[4] = {
#include "assets/dryfield_warehouse_collision_03A78_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_warehouse_80181038[1] = {
    { NULL, _gDryfieldWarehouseCollision03A78Normals, _gDryfieldWarehouseCollision03A78Verts, _gDryfieldWarehouseCollision03A78Faces, _gDryfieldWarehouseCollision03A78Table, 0, 4100, 2, 2, 4000, 52 },
};

ViewCamera D_dryfield_warehouse_8018105C[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3000, 7700, 2000 } }, 230 },
    { { { { -981, 0, 3976 }, { -168, 4092, -41 }, { -3972, -173, -981 } }, { -5338, 1066, 1266 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 2311, 0, -3381 }, { -2032, 3273, -1389 }, { 2702, 2461, 1847 } }, { -5038, 1666, 2016 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
};

SpriteBatch D_dryfield_warehouse_801811A0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_warehouse_801811B0[21] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, 16, 1012, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, 24, 1025, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -112, 64, 450, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, 64, 425, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 56, 650, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 88, 480, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 104, 399, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 64, 650, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 80, 559, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 88, 32, 650, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 96, 64, 553, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 104, 80, 349, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 88, 481, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 88, 346, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 32, 616, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 144, 32, 564, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, 32, 521, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -160, 72, 612, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 96, 612, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -160, 0, 600, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 56, 612, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_warehouse_80181354[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 2, 0, 0, { 0, 0 } },
    { 4, 13, 0, 0, { 2, 0 } },
    { 17, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_warehouse_80181384[25] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 32, 962, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 32, 875, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 48, 32, 825, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 32, 750, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 80, 600, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -120, 96, 550, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -88, 96, 550, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -120, 72, 550, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -88, 72, 550, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 72, 600, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 56, 625, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 56, 625, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 56, 700, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 40, 725, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 40, 725, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 40, 725, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 32, 725, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 24, 750, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 24, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 32, 725, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 48, 625, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 120, 48, 625, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 16, 625, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 120, 16, 625, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 0, 625, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_warehouse_80181578[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 1, 0, 0, { 0, 0 } },
    { 3, 17, 0, 0, { 2, 0 } },
    { 20, 5, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_warehouse_801815A8[2] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 80, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 96, 550, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_warehouse_801815D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_warehouse_801815E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

static void func_dryfield_warehouse_8017DBB0(Task* arg0);
static void func_dryfield_warehouse_8017ED34(GfxCoord* coord, s16 arg1, s16 arg2);

/// Message handler of the warehouse's cutscene task. Message 0 re-opens the
/// room: it kills the screen-fade task still on `D_dryfield_warehouse_801821C0`,
/// turns the display back on and, while
/// `_DryfieldWarehouseCutsceneWork::playerEffectsSuppressed` is set, spawns the
/// weapon effect back, clears the latch and re-sends the player-weapon record.
/// The player is then handed an equipped-weapon bank request with animation 1, blending disabled
/// and world collision enabled, followed by the room's placement message.
///
/// The session's weapon id is synced to 2 once, and `D_dryfield_warehouse_801821C4`
/// records whether this handler did that: message 1 mirrors the session's view
/// and object tables back onto that flag. The 1 shared by the record and the
/// flag is one callee-saved value because both outlive the dispatches.
void func_dryfield_warehouse_8017DA58(s32 arg0)
{
    _DryfieldWarehouseCutsceneWork* work;
    AnimationPlayRequest            rec;
    s32                             weaponId;
    s32                             anim;

    switch (arg0) {
        case 0:
            if (D_dryfield_warehouse_801821C0 != 0) {
                taskKill(D_dryfield_warehouse_801821C0);
            }
            SetDispMask(1);
            work = D_dryfield_warehouse_801821BC->work;
            if (work->playerEffectsSuppressed != 0) {
                Gp_SpawnWeaponEff();
                work->playerEffectsSuppressed = 0;
                Gp_MsgPlayerWeapon(0);
            }
            weaponId                 = gPlayerStatus.weapon;
            anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.source.index         = anim;
            rec.animationId          = 1;
            rec.blend                = ANIMATION_BLEND_RESET;
            rec.blendFrames          = 0;
            rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_PLAY, &rec, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->player, 0x3E9, &D_dryfield_warehouse_8017F868, 0);
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room != 2) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
                gGameSession->location.loc.room                            = 2;
                D_dryfield_warehouse_801821C4                              = 1;
                return;
            }
            D_dryfield_warehouse_801821C4 = 0;
            return;
        case 1:
            if (D_dryfield_warehouse_801821C4 != 0) {
                gGameSession->viewDirty     = arg0;
                gGameSession->roomObjsDirty = arg0;
            }
            return;
    }
}

/// Carries out `_DryfieldWarehouseCutsceneWork::command` for one update.
///
/// The fade-out command, an unknown command and a restore command whose dust
/// has run out leave the command cleared. The others return with it still
/// pending, so they run again until the script stores the next one.
static void func_dryfield_warehouse_8017DBB0(Task* arg0)
{
    _DryfieldWarehouseCutsceneWork* work;
    _DryfieldWarehouseCutsceneWork* sharedWork;
    _DryfieldWarehouseCutsceneWork* cur;
    union {
        AnimationPlayRequest rec;
        SVECTOR              pos;
    } msg;

    s32 weaponId;
    s32 anim;

    work = arg0->work;
    switch (work->command) {
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_NONE:
            break;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_PLACE_AND_FADE_IN:
            switch (work->commandStep) {
                case 0:
                    SetDispMask(1);
                    Task_SpawnFromTable(D_dryfield_warehouse_8017FB08, 2, 8, 0);
                    Gp_KillPlayerEffs();
                    work->playerEffectsSuppressed = 1;
                    cur                           = arg0->work;
                    if (cur->player != NULL) {
                        msg.rec.source.sets          = D_dryfield_warehouse_8017F848;
                        msg.rec.animationId          = 1;
                        msg.rec.blend                = ANIMATION_BLEND_RESET;
                        msg.rec.blendFrames          = 0;
                        msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(cur->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(work->player, 0x3E9, &D_dryfield_warehouse_8017F850, 0);
                    work->commandFrames = 0;
                    work->commandStep++;
                    break;
                case 1:
                    if ((work->soundLoopFrames % DRYFIELD_WAREHOUSE_CUTSCENE_SOUND_LOOP_FRAMES) == 0) {
                        SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 3), 0, 0);
                    }
                    break;
            }
            work->soundLoopFrames++;
            return;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_SPAWN_FADE_OUT:
            D_dryfield_warehouse_801821C0 = Task_SpawnFromTable(D_dryfield_warehouse_8017FB08, 1, 8, 0);
            break;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_RESTORE_AND_FADE_IN:
            // The restore goes through the published cutscene task's block.
            sharedWork = D_dryfield_warehouse_801821BC->work;
            if (sharedWork->playerEffectsSuppressed != 0) {
                Gp_SpawnWeaponEff();
                sharedWork->playerEffectsSuppressed = 0;
                Gp_MsgPlayerWeapon(0);
            }
            weaponId                     = gPlayerStatus.weapon;
            anim                         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.rec.source.index         = anim;
            msg.rec.animationId          = 1;
            msg.rec.blend                = ANIMATION_BLEND_RESET;
            msg.rec.blendFrames          = 0;
            msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

            TASK_MESSAGE_DISPATCH_POINTER(sharedWork->player, ANIMATION_MESSAGE_PLAY, &msg.rec, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sharedWork->player, 0x3E9, &D_dryfield_warehouse_8017F868, 0);
            switch (work->commandStep) {
                case 0:
                    Task_SpawnFromTable(D_dryfield_warehouse_8017FB08, 2, 8, 0);
                    work->commandFrames = 0;
                    work->commandStep++;
                    return;
                case 1:
                    // Five puffs along the Z axis, one per update in turn, each
                    // repeating every eighth update. The block is fetched again
                    // from the task for the phase tests.
                    work->commandFrames++;
                    sharedWork = arg0->work;
                    msg.pos.vx = 0x1644;
                    msg.pos.vy = 0;
                    if (!(sharedWork->commandFrames & 7)) {
                        msg.pos.vz = -500;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((sharedWork->commandFrames + 1) & 7)) {
                        msg.pos.vz = -700;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((sharedWork->commandFrames + 2) & 7)) {
                        msg.pos.vz = -900;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((sharedWork->commandFrames + 3) & 7)) {
                        msg.pos.vz = -1100;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((sharedWork->commandFrames + 4) & 7)) {
                        msg.pos.vz = -1300;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, NULL, 0x80002300, &msg.pos);
                    }
                    if (work->commandFrames >= DRYFIELD_WAREHOUSE_CUTSCENE_DUST_FRAMES) {
                        work->command = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_NONE;
                    }
                    SetDispMask(1);
                    return;
            }
            break;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_SWITCH_ROOM:
            fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
            switch (work->commandStep) {
                case 0:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
                    gGameSession->location.loc.room                            = 2;
                    work->commandFrames                                        = 0;
                    work->commandStep++;
                    break;
                case 1:
                    gGameSession->viewDirty     = 1;
                    gGameSession->roomObjsDirty = 1;
                    work->commandStep++;
                    break;
                case 2:
                    break;
            }
            if (work->commandFrames == 10) {
                SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 4), 0, 0);
            }
            work->commandFrames++;
            return;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_LOOP_SOUND:
            fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
            switch (work->commandStep) {
                case 0:
                    D_80115768            = 0;
                    work->soundLoopFrames = 0;
                    work->commandStep++;
                    break;
                case 1:
                    if ((work->soundLoopFrames % DRYFIELD_WAREHOUSE_CUTSCENE_SOUND_LOOP_FRAMES) == 0) {
                        SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 3), 0, 0);
                    }
                    break;
            }
            work->soundLoopFrames++;
            return;
    }
    work->command = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_NONE;
}

/// Main loop of the warehouse's cutscene task, the owner of the
/// `_DryfieldWarehouseCutsceneWork` block. State 0 arms the script once: it waits while the attachment wheel
/// is open (`Gp_StateC08.mode`) or `gDisplayState.pendingMode` is live, so it does nothing.
/// Otherwise it parks the zeroed work block in `Task::work` -- a failed
/// `memMalloc` kills the task, but the record below is dispatched either way --
/// fills `player` from pointer slot 3 and republishes this task as
/// `D_dryfield_warehouse_801821BC` so the room's script helpers reach that block.
///
/// The initial equipped-weapon request selects animation 1 without blending
/// or world collision. It is sent synchronously to a freshly fetched slot 3.
///
/// State 0 then falls into state 1, which only steps the machine, so a task
/// entering at 1 runs the step alone. State 2 kills the task once the session
/// has torn down (`gGameSession->eventState`), otherwise runs the script.
void func_dryfield_warehouse_8017E090(Task* arg0)
{
    _DryfieldWarehouseCutsceneWork* work;
    AnimationPlayRequest            rec;
    s32                             weaponId;
    s32                             anim;

    switch (arg0->state) {
        case 0:
            if ((Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                work       = memMalloc(sizeof(*work), false);
                arg0->work = work;
                if (work == NULL) {
                    taskKill(arg0);
                } else {
                    memFillBytes(work, 0, sizeof(*work));
                    work->player                  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    D_dryfield_warehouse_801821BC = arg0;
                }
                weaponId                 = gPlayerStatus.weapon;
                anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.source.index         = anim;
                rec.animationId          = 1;
                rec.blend                = ANIMATION_BLEND_RESET;
                rec.blendFrames          = 0;
                rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &rec, 0);
                D_dryfield_warehouse_801821C0 = NULL;
                D_80115768                    = 1;
                arg0->state                   = arg0->state + 1;
                case 1:
                    func_800E8634(D_dryfield_warehouse_8017F880, 0,
                                  D_dryfield_warehouse_8017FA00);
                    arg0->state = arg0->state + 1;
                    return;
            }
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(arg0, 0);
                return;
            }
            func_dryfield_warehouse_8017DBB0(arg0);
            break;
    }
}

#include "../../shared/screen_fade_in.inc.c"

/// Screen-fade task: on its first tick it allocates the 8-byte `r`/`g`/`b`
/// block and seeds all three channels to 0, then every frame it draws the fade
/// overlay and steps each channel up by `Task::spawnArg1`. `r` is the one the
/// end-of-fade test watches, so once it has reached 0x100 the display is
/// switched back on, the room's fade-task handle is cleared and the task kills
/// itself.
void func_dryfield_warehouse_8017E308(Task* arg0)
{
    ScreenFadeWork* fade;
    ScreenFadeWork* alloc;

    fade = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            fade->r += (u16)arg0->spawnArg1.value;
            fade->g += (u16)arg0->spawnArg1.value;
            fade->b += (u16)arg0->spawnArg1.value;
            if (fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            D_dryfield_warehouse_801821C0 = NULL;
            taskKill(arg0);
            break;
    }
}

/// Leaves the `DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_*` value `arg0` for the
/// warehouse's cutscene task, to be carried out from its first step.
void func_dryfield_warehouse_8017E3F4(s16 arg0)
{
    _DryfieldWarehouseCutsceneWork* work = D_dryfield_warehouse_801821BC->work;

    work->command     = arg0;
    work->commandStep = 0;
}

#include "../../shared/glow_draw_grey_prism.inc.c"

/// Draws one ring of gouraud `POLY_G4` segments between two circles in the XZ
/// plane of `coord`: circle `arg1` of the room's centre/radius tables forms the
/// lit edge and circle `arg1 + 1` the black one. `arg2` segments cover the full
/// turn, starting at a phase that advances with the frame counter. Each corner
/// is placed in `coord`'s space through its `workm`, then projected through
/// `GsWSMATRIX`; the lit edge glows at 0x14 plus a small pulse.
static void func_dryfield_warehouse_8017ED34(GfxCoord* coord, s16 arg1, s16 arg2)
{
    EffectQuadCornersScratch* blk;
    POLY_G4*                  prim;
    s16                       level;
    s16                       step;
    s16                       start;
    s32                       angle;
    s32                       next;

    level = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x14;
    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    blk   = SCRATCH_STACK_CURSOR(EffectQuadCornersScratch);
    start = gDisplayState.animFrame & 0xFFF;
    step  = 0x1000 / arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (angle = start; angle < start + step * arg2; angle = next) {
        blk->vertices[0].vx = gGlowPrismCorners[arg1].vx + ((rsin(angle) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        blk->vertices[0].vy = gGlowPrismCorners[arg1].vy;
        blk->vertices[0].vz = gGlowPrismCorners[arg1].vz + ((rcos(angle) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->vertices[0]);
        gte_rtv0();
        gte_stsv(&blk->vertices[0]);
        blk->vertices[0].vx += coord->workm.t[0];
        blk->vertices[0].vy += coord->workm.t[1];
        next                 = angle + step;
        blk->vertices[0].vz += coord->workm.t[2];

        blk->vertices[1].vx = gGlowPrismCorners[arg1].vx + ((rsin(next) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        blk->vertices[1].vy = gGlowPrismCorners[arg1].vy;
        blk->vertices[1].vz = gGlowPrismCorners[arg1].vz + ((rcos(next) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->vertices[1]);
        gte_rtv0();
        gte_stsv(&blk->vertices[1]);
        blk->vertices[1].vx += coord->workm.t[0];
        blk->vertices[1].vy += coord->workm.t[1];
        blk->vertices[1].vz += coord->workm.t[2];

        blk->vertices[2].vx = gGlowPrismCorners[arg1 + 1].vx + ((rsin(angle) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        blk->vertices[2].vy = gGlowPrismCorners[arg1 + 1].vy;
        blk->vertices[2].vz = gGlowPrismCorners[arg1 + 1].vz + ((rcos(angle) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->vertices[2]);
        gte_rtv0();
        gte_stsv(&blk->vertices[2]);
        blk->vertices[2].vx += coord->workm.t[0];
        blk->vertices[2].vy += coord->workm.t[1];
        blk->vertices[2].vz += coord->workm.t[2];

        blk->vertices[3].vx = gGlowPrismCorners[arg1 + 1].vx + ((rsin(next) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        blk->vertices[3].vy = gGlowPrismCorners[arg1 + 1].vy;
        blk->vertices[3].vz = gGlowPrismCorners[arg1 + 1].vz + ((rcos(next) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->vertices[3]);
        gte_rtv0();
        gte_stsv(&blk->vertices[3]);
        blk->vertices[3].vx += coord->workm.t[0];
        blk->vertices[3].vy += coord->workm.t[1];
        blk->vertices[3].vz += coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->vertices[0]);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->vertices[1], &blk->vertices[2], &blk->vertices[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->depth);
        setRGB0(prim, level, level, level);
        setRGB1(prim, level, level, level);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(blk->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, blk->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}

/// Per-frame effect on the room's coordinate task: draws the room geometry for the
/// current stage visit, then publishes variant 2 as the room's
/// `gRoomEffectState->roomEffectMode` index. `Task::extra.coordBody->coord` is the
/// coordinate every draw shares. The stage-visit byte
/// `gGameSession->location.loc.view` is used as a bit index: bits 2, 3, 6 and 9 (`0x24C`)
/// pose through `glowDrawGreyPrism`, bit 2 (`4`) also drives
/// `func_dryfield_warehouse_8017ED34` to step 0, those same `0x24C` visits also
/// drive it to step 2, and bits 2, 3, 4 and 6-9 (`0x3DC`) drive it to steps 4
/// and 6.
void func_dryfield_warehouse_8017F494(Task* arg0)
{
    s32       mask;
    s32       poseMask;
    GfxCoord* coord;

    mask     = 1 << gGameSession->location.loc.view;
    poseMask = mask & 0x24C;
    coord    = arg0->extra.coordBody->coord;
    if (poseMask != 0) {
        glowDrawGreyPrism(coord, 8);
    }
    if (mask & 4) {
        func_dryfield_warehouse_8017ED34(coord, 0, 8);
    }
    if (poseMask != 0) {
        func_dryfield_warehouse_8017ED34(coord, 2, 8);
    }
    if (mask & 0x3DC) {
        func_dryfield_warehouse_8017ED34(coord, 4, 8);
        func_dryfield_warehouse_8017ED34(coord, 6, 8);
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
