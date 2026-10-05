#include "types.h"

#include "main/task_types.h"
#include "../../shared/glow_draw.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
Task* gRoomCutsceneSoundTask;

/// The cutscene task `func_dryfield_gas_station_801807E0` publishes once its
/// `_DryfieldGasStationCutsceneWork` block is set up, so the room's script helpers can reach it.
Task* D_dryfield_gas_station_80184BD4;

#include "rooms/dryfield_gas_station.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_gas_station_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
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
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/screen_fade.h"

#define D_dryfield_gas_station_80182E5C (D_dryfield_gas_station_80182E44[1])
#define D_dryfield_gas_station_80182E74 (D_dryfield_gas_station_80182E44[2])

/// Commands the gas-station cutscene script leaves for the cutscene task.
///
/// Each one is stored over the previous command and restarts `commandStep`.
/// The task carries it out on later updates and then clears it. A command
/// that takes several updates advances `commandStep` and returns until its
/// last step.
enum {
    /// Nothing pending.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_NONE = 0,
    /// Places the player at the first entry of `D_dryfield_gas_station_80182E44`
    /// and starts the cutscene loop plus area sound 0x12.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_START_AUDIO = 1,
    /// Plays animation 1 of `D_dryfield_gas_station_80182E30` from its start
    /// and sets the playback rate to 8.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLAY_ANIMATION = 2,
    /// Plays area sound 0x13, places the player at the second entry, and
    /// blends into animation 2 over 30 frames.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_BLEND = 3,
    /// Plays animation 3 and selects player movement mode 1, then sends one
    /// thirtieth of the first-to-third placement delta each update. After 31
    /// steps, blends into animation 0 over 15 frames.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_WALK_PLACEMENTS = 4,
    /// Restores suppressed player effects, places the player at the third
    /// entry, plays animation 0 from its start, stops the cutscene loop and
    /// enables the display.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_RESTORE_AND_SHOW = 5,
    /// Spawns the fade-in task, waits one frame, and enables the display.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_SPAWN_FADE_IN = 6,
};

/// Work block of the gas-station cutscene task.
///
/// The task allocates and zeroes the whole block when the cutscene starts,
/// then publishes itself so the room's script callbacks can reach it.
typedef struct {
    Task* player;                  // Player task captured when the cutscene starts. Animation installs test NULL; placement, rate and movement sends do not
    u16   command;                 // Pending `DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_*`, cleared once carried out
    u16   commandStep;             // Step within a command that takes several updates; zeroed with each new command
    u16   walkFrames;              // Placement-walk steps already sent. Other commands leave it unread
    byte  field_A[2];              // Allocated and cleared with the block; no access. Role and width unproven
    u16   playerEffectsSuppressed; // 0 none pending; 1 player effects were killed and still need to be spawned back
    byte  field_E[2];              // Trailing bytes of the 0x10 block; no access. Role and width unproven
} _DryfieldGasStationCutsceneWork;
STATIC_ASSERT_SIZEOF(_DryfieldGasStationCutsceneWork, 0x10);

extern AnimationSet* D_dryfield_gas_station_80182E30[5];
extern EvsCommand    D_dryfield_gas_station_80182E8C[];
extern EvsCommand    D_dryfield_gas_station_8018303C[];

extern SVECTOR D_dryfield_gas_station_80183144;

// Indexed views below share one contiguous table.
void func_dryfield_gas_station_80180944(void);
void func_dryfield_gas_station_80180B2C(s16);

extern WorldCollisionGrid    D_dryfield_gas_station_80183EA4[1];
extern WorldCollisionTrigger D_dryfield_gas_station_80184350[11];
extern WorldCollisionTrigger D_dryfield_gas_station_80184694[10];
extern WorldCoordRoomLights  D_dryfield_gas_station_80184B48[1];
extern TaskDesc              Actor04400_D107E4;
extern TaskDesc              Actor00100_D1BA84;
void                         func_dryfield_gas_station_801807E0(Task*);
void                         func_dryfield_gas_station_80180984(Task*);
void                         func_dryfield_gas_station_80180A60(void);

TaskDesc D_dryfield_gas_station_80181E7C[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_gas_station_801802C0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_gas_station_8017FFE4, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeInTask, { .value = 0 } },
};

static AnimationPackedPose _gDryfieldGasStationAnimation04A70Bank1[2] = {
#include "assets/dryfield_gas_station_animation_04A70_bank1.inc"
};

static AnimationPackedRotation _gDryfieldGasStationAnimation04A70Bank4[8] = {
#include "assets/dryfield_gas_station_animation_04A70_bank4.inc"
};

static AnimationRecord _gDryfieldGasStationAnimation04A70Records[76] = {
#include "assets/dryfield_gas_station_animation_04A70_records.inc"
};

static u16 _gDryfieldGasStationAnimation04A70Indices[20] = {
#include "assets/dryfield_gas_station_animation_04A70_indices.inc"
};

static AnimationSet _gDryfieldGasStationAnimation04A70 = {
    _gDryfieldGasStationAnimation04A70Records,
    _gDryfieldGasStationAnimation04A70Indices,
    { NULL, _gDryfieldGasStationAnimation04A70Bank1, NULL, NULL, _gDryfieldGasStationAnimation04A70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldGasStationAnimation05210Bank1[13] = {
#include "assets/dryfield_gas_station_animation_05210_bank1.inc"
};

static AnimationPackedRotation _gDryfieldGasStationAnimation05210Bank4[179] = {
#include "assets/dryfield_gas_station_animation_05210_bank4.inc"
};

static AnimationRecord _gDryfieldGasStationAnimation05210Records[250] = {
#include "assets/dryfield_gas_station_animation_05210_records.inc"
};

static u16 _gDryfieldGasStationAnimation05210Indices[20] = {
#include "assets/dryfield_gas_station_animation_05210_indices.inc"
};

static AnimationSet _gDryfieldGasStationAnimation05210 = {
    _gDryfieldGasStationAnimation05210Records,
    _gDryfieldGasStationAnimation05210Indices,
    { NULL, _gDryfieldGasStationAnimation05210Bank1, NULL, NULL, _gDryfieldGasStationAnimation05210Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldGasStationAnimation055A4Bank1[7] = {
#include "assets/dryfield_gas_station_animation_055A4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldGasStationAnimation055A4Bank4[65] = {
#include "assets/dryfield_gas_station_animation_055A4_bank4.inc"
};

static AnimationRecord _gDryfieldGasStationAnimation055A4Records[123] = {
#include "assets/dryfield_gas_station_animation_055A4_records.inc"
};

static u16 _gDryfieldGasStationAnimation055A4Indices[20] = {
#include "assets/dryfield_gas_station_animation_055A4_indices.inc"
};

static AnimationSet _gDryfieldGasStationAnimation055A4 = {
    _gDryfieldGasStationAnimation055A4Records,
    _gDryfieldGasStationAnimation055A4Indices,
    { NULL, _gDryfieldGasStationAnimation055A4Bank1, NULL, NULL, _gDryfieldGasStationAnimation055A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldGasStationAnimation05848Bank1[2] = {
#include "assets/dryfield_gas_station_animation_05848_bank1.inc"
};

static AnimationPackedRotation _gDryfieldGasStationAnimation05848Bank4[48] = {
#include "assets/dryfield_gas_station_animation_05848_bank4.inc"
};

static AnimationRecord _gDryfieldGasStationAnimation05848Records[95] = {
#include "assets/dryfield_gas_station_animation_05848_records.inc"
};

static u16 _gDryfieldGasStationAnimation05848Indices[20] = {
#include "assets/dryfield_gas_station_animation_05848_indices.inc"
};

static AnimationSet _gDryfieldGasStationAnimation05848 = {
    _gDryfieldGasStationAnimation05848Records,
    _gDryfieldGasStationAnimation05848Indices,
    { NULL, _gDryfieldGasStationAnimation05848Bank1, NULL, NULL, _gDryfieldGasStationAnimation05848Bank4, NULL, NULL, NULL },
};

AnimationSet* D_dryfield_gas_station_80182E30[5] = {
    &_gDryfieldGasStationAnimation04A70,
    &_gDryfieldGasStationAnimation055A4,
    &_gDryfieldGasStationAnimation05848,
    &_gDryfieldGasStationAnimation05210,
    NULL,
};

ActorTransform D_dryfield_gas_station_80182E44[3] = {
    { { 14408, 0, -2630, 0 }, { 0, 3584, 0, 0 } },
    { { 14158, 0, -2380, 0 }, { 0, 2560, 0, 0 } },
    { { 14158, 0, -2380, 0 }, { 0, 3072, 0, 0 } },
};

EvsCommand D_dryfield_gas_station_80182E8C[18] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_gas_station_80180944 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_START_AUDIO }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_SPAWN_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_WALK_PLACEMENTS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLAY_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_BLEND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_RESTORE_AND_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_gas_station_8018303C[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_gas_station_80180A60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_gas_station_8018312C[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_gas_station_801807E0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_gas_station_80180984, { .value = 0 } },
};

SVECTOR D_dryfield_gas_station_80183144 = { 4378, -1383, -215, 0 };

WorldCollisionRoomResources D_dryfield_gas_station_8018314C[1] = {
    { D_dryfield_gas_station_80183EA4, D_dryfield_gas_station_80184350, D_dryfield_gas_station_80184694, NULL },
};

u8* D_dryfield_gas_station_8018315C[1] = {
    gViewIdentityMap,
};

WorldCoordRoomLighting D_dryfield_gas_station_80183160[1] = {
    { D_dryfield_gas_station_80184B48, NULL },
};

ViewCount D_dryfield_gas_station_80183168[1] = { 14 };

DirectionWarpEntry D_dryfield_gas_station_8018316C[3] = {
    { { { .word = 2816 }, 0x3848, 0, -2630 }, { 0, 0, 0, 0 }, { { .word = 2816 }, 0x3848, 0, -1440 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 294, -5, -3482 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 294, -5, -4466 }, { 0, 0, 0, 0 }, 0x52010002, 0x52010001, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 488 },
    { { { .word = 2048 }, 3039, 0, -433 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 4316, 0, -535 }, { 0, 0, 0, 0 }, 0x52010004, 0x52010003, 0x5201000F, 6, DIRECTION_WARP_FLAG_NONE, 489 },
};

static SVECTOR _gDryfieldGasStationCollision068E4Normals[32] = {
#include "assets/dryfield_gas_station_collision_068E4_normals.inc"
};

static SVECTOR _gDryfieldGasStationCollision068E4Verts[126] = {
#include "assets/dryfield_gas_station_collision_068E4_verts.inc"
};

static WorldCollisionGridFace _gDryfieldGasStationCollision068E4Faces[63] = {
#include "assets/dryfield_gas_station_collision_068E4_faces.inc"
};

static s16 _gDryfieldGasStationCollision068E4Cells[502] = {
#include "assets/dryfield_gas_station_collision_068E4_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldGasStationCollision068E4Cells[i])
static s16* _gDryfieldGasStationCollision068E4Table[48] = {
#include "assets/dryfield_gas_station_collision_068E4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_gas_station_80183EA4[1] = {
    { NULL, _gDryfieldGasStationCollision068E4Normals, _gDryfieldGasStationCollision068E4Verts, _gDryfieldGasStationCollision068E4Faces, _gDryfieldGasStationCollision068E4Table, 5300, 0x3A98, 8, 6, 4000, 63 },
};

ViewCamera D_dryfield_gas_station_80183EC8[14] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2710, 0x7530, 1000 } }, 380 },
    { { { { 1609, 0, -3766 }, { -1268, 3856, -541 }, { 3546, 1379, 1515 } }, { -7536, 2780, 4972 } }, 230 },
    { { { { 1495, 0, -3813 }, { -23, 4095, -9 }, { 3813, 25, 1495 } }, { -1316, 1337, 5204 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { 546, 0, -4059 }, { -3221, 2492, -433 }, { 2469, 3250, 332 } }, { -0x2EA0, 3781, 2877 } }, 329 },
    { { { { 3396, 0, 2288 }, { -180, 4083, 267 }, { -2281, -322, 3386 } }, { -5346, 792, 3101 } }, 230 },
    { { { { 1107, 0, -3943 }, { -2662, 3021, -747 }, { 2908, 2765, 816 } }, { -0x3206, 1776, 5127 } }, 230 },
    { { { { 3985, 0, 945 }, { 289, 3899, -1220 }, { -900, 1254, 3793 } }, { -4447, 1449, 747 } }, 230 },
    { { { { 1679, 0, -3735 }, { 590, 4044, 265 }, { 3689, -646, 1658 } }, { -0x2D82, 940, 3440 } }, 541 },
    { { { { 3799, 0, 1530 }, { 233, 4047, -580 }, { -1512, 626, 3754 } }, { -0x3815, 1503, 2328 } }, 230 },
    { { { { 2694, 0, 3085 }, { -516, 4038, 451 }, { -3041, -685, 2656 } }, { -0x29C2, 716, 3608 } }, 230 },
    { { { { 1200, 0, 3916 }, { -1855, 3607, 568 }, { -3448, -1940, 1057 } }, { -9280, 950, 3960 } }, 230 },
    { { { { 97, 0, -4094 }, { -3605, 1942, -85 }, { 1941, 3606, 46 } }, { -0x33FE, 930, 3500 } }, 230 },
    { { { { 3432, 0, -2234 }, { -527, 3980, -810 }, { 2171, 967, 3335 } }, { -0x2DBF, 1537, 1164 } }, 230 },
};

SpriteBatch D_dryfield_gas_station_801840C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_gas_station_801840D0[6] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -32, 2362, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -40, 2329, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -40, 2325, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -40, 2325, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -40, 2325, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 40, -40, 2325, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_gas_station_80184148[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_gas_station_80184160[6] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 0, 3701, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, -8, 3300, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 0, 3424, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 0, 2250, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 0, 2250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, 0, 2342, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_gas_station_801841D8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_801841F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184208[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184218[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184228[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184238[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184248[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184258[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184268[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184278[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184288[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184298[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_gas_station_801842A8[14] = {
    { { .empty = D_dryfield_gas_station_801840C0 }, D_dryfield_gas_station_801840C0, NULL },
    { { .elements = D_dryfield_gas_station_801840D0 }, D_dryfield_gas_station_80184148, NULL },
    { { .elements = D_dryfield_gas_station_80184160 }, D_dryfield_gas_station_801841D8, NULL },
    { { .empty = D_dryfield_gas_station_801841F8 }, D_dryfield_gas_station_801841F8, NULL },
    { { .empty = D_dryfield_gas_station_80184208 }, D_dryfield_gas_station_80184208, NULL },
    { { .empty = D_dryfield_gas_station_80184218 }, D_dryfield_gas_station_80184218, NULL },
    { { .empty = D_dryfield_gas_station_80184228 }, D_dryfield_gas_station_80184228, NULL },
    { { .empty = D_dryfield_gas_station_80184238 }, D_dryfield_gas_station_80184238, NULL },
    { { .empty = D_dryfield_gas_station_80184248 }, D_dryfield_gas_station_80184248, NULL },
    { { .empty = D_dryfield_gas_station_80184258 }, D_dryfield_gas_station_80184258, NULL },
    { { .empty = D_dryfield_gas_station_80184268 }, D_dryfield_gas_station_80184268, NULL },
    { { .empty = D_dryfield_gas_station_80184278 }, D_dryfield_gas_station_80184278, NULL },
    { { .empty = D_dryfield_gas_station_80184288 }, D_dryfield_gas_station_80184288, NULL },
    { { .empty = D_dryfield_gas_station_80184298 }, D_dryfield_gas_station_80184298, NULL },
};

WorldCollisionTrigger D_dryfield_gas_station_80184350[11] = {
    { NULL, NULL, NULL, { 5791, -2544, -3505, 0 }, { { 147, -3568, 2869, 0 }, { -146, -3568, -2868, 0 }, { 147, 3568, 2869, 0 }, { -146, 3568, -2868, 0 } }, { -4101, 0, 209, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5503, -2544, -3521, 0 }, { { -146, -3568, -2868, 0 }, { 147, -3568, 2869, 0 }, { -146, 3568, -2868, 0 }, { 147, 3568, 2869, 0 } }, { 4100, 0, -210, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4319, -2544, -1009, 0 }, { { -210, -3568, -804, 0 }, { 211, -3568, 805, 0 }, { -210, 3568, -804, 0 }, { 211, 3568, 805, 0 } }, { 3963, 0, -1038, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2752, -2528, -1264, 0 }, { { -1410, -3552, 524, 0 }, { 1411, -3552, -523, 0 }, { -1410, 3552, 524, 0 }, { 1411, 3552, -523, 0 } }, { -1432, 0, -3857, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2623, -2608, -1345, 0 }, { { 1716, -3632, -661, 0 }, { -1716, -3632, 662, 0 }, { 1716, 3632, -661, 0 }, { -1716, 3632, 662, 0 } }, { 1474, 0, 3824, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4431, -2624, -1297, 0 }, { { 283, -3648, 1064, 0 }, { -283, -3648, -1064, 0 }, { 283, 3648, 1064, 0 }, { -283, 3648, -1064, 0 } }, { -3966, 0, 1054, 0 }, { 0, 0, 4096, 0 }, 3805, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28FF, -2608, -1952, 0 }, { { -933, -3632, -2562, 0 }, { 911, -3632, 2537, 0 }, { -933, 3632, -2562, 0 }, { 911, 3632, 2537, 0 } }, { 3855, 0, -1395, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A06, -2592, -1961, 0 }, { { 875, -3616, 2397, 0 }, { -875, -3616, -2396, 0 }, { 875, 3616, 2397, 0 }, { -875, 3616, -2396, 0 } }, { -3858, 0, 1407, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x361F, -2624, -2482, 0 }, { { 857, -3616, -1637, 0 }, { -856, -3616, 1638, 0 }, { 857, 3616, -1637, 0 }, { -856, 3616, 1638, 0 } }, { 3633, 0, 1900, 0 }, { 0, 0, 4096, 0 }, 4055, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x29DD, -2624, -5092, 0 }, { { -867, -3616, 888, 0 }, { 867, -3616, -888, 0 }, { -867, 3616, 888, 0 }, { 867, 3616, -888, 0 } }, { -2937, 0, -2867, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28AD, -2688, -5043, 0 }, { { 899, -3616, -888, 0 }, { -899, -3616, 888, 0 }, { 899, 3616, -888, 0 }, { -899, 3616, 888, 0 } }, { 2895, 0, 2931, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_gas_station_80184694[10] = {
    { NULL, NULL, NULL, { 2657, -108, -331, 0 }, { { -1024, 0, -384, 0 }, { 1024, 0, -384, 0 }, { -1024, 0, 384, 0 }, { 1024, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 192, -80, -3328, 0 }, { { 384, 0, -1440, 0 }, { 384, 0, 1024, 0 }, { -384, 0, -1440, 0 }, { -384, 0, 1024, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1487, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3430, -96, -224, 0 }, { { -1776, 0, -384, 0 }, { 1776, 0, -384, 0 }, { -1776, 0, 384, 0 }, { 1776, 0, 384, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x36EF, -96, -4817, 0 }, { { -1877, 0, -131, 0 }, { -397, 0, -2069, 0 }, { -1650, 0, 1270, 0 }, { 694, 0, -764, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -4096, 0 }, 2095, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6960, -64, -1104, 0 }, { { -1407, 0, -624, 0 }, { 1408, 0, -624, 0 }, { -1407, 0, 624, 0 }, { 1408, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4448, -96, 256, 0 }, { { -880, 0, -1408, 0 }, { 720, 0, -1408, 0 }, { -880, 0, -320, 0 }, { 720, 0, -320, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1659, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x36C0, -64, -3488, 0 }, { { -501, 0, -973, 0 }, { 1016, 0, 403, 0 }, { -1497, 0, -20, 0 }, { 20, 0, 1356, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4120, -64, 0, 0 }, { { -736, 0, -624, 0 }, { 736, 0, -624, 0 }, { -736, 0, 624, 0 }, { 736, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9264, -64, -1184, 0 }, { { -911, 0, -624, 0 }, { 912, 0, -624, 0 }, { -911, 0, 624, 0 }, { 912, 0, 624, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_CAP, 26, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -528, 0 }, { { -544, 0, -576, 0 }, { 544, 0, -576, 0 }, { -544, 0, 576, 0 }, { 544, 0, 576, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 791, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_gas_station_8018498C[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_gas_station_80184998[2] = {
    { 44, 44, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor04400_D107E4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_gas_station_801849B0[5] = {
    { 44, 0, 0, 4226, 0, -4480, 0, 0, 0, 2, 0 },
    { 44, 0, 1, 472, -4624, -2512, 1024, 0, 0, 2, 0 },
    { 44, 0, 0, 7277, 0, -3553, 512, 0, 0, 2, 0 },
    { 44, 0, 0, 6255, 0, -5322, -1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_dryfield_gas_station_80184A00[2] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_gas_station_80184A18[2] = {
    { 1, 0, 0, 6255, 0, -5322, -1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_dryfield_gas_station_80184A38[12] = {
    { NULL, NULL },
    { D_map_dryfield_8017AD34, D_dryfield_gas_station_8018498C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_gas_station_801849B0, D_dryfield_gas_station_80184998 },
    { D_dryfield_gas_station_80184A18, D_dryfield_gas_station_80184A00 },
    { NULL, NULL },
};

/// The gas station's two directional model lights, contributing in every room view.
///
/// Local translations supply direction vectors, normalized when shading;
/// RGB intensities have 12 fractional bits (`ONE` is 1.0). The loaded room
/// overlay owns these records. Coordinate updates attach the view parent and
/// compose the transforms; shading overwrites attenuation, so they stay writable.
static WorldCoordLight _gDryfieldGasStationDirectionalLights[] = {
    {
        .transform = {
            .lighting = {
                .composeStamp = GRAPHICS_COORD_DIRTY,
                .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 2000, -4000, -2000 } },
                .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                .unknown_46   = { 0, 0, 0, 0 },
                .attenuation  = 0,
                .parent       = NULL,
            },
        },
        .color      = { .r = 0x2856, .g = 8167, .b = 7585 },
        .unknown_56 = { 0, 0 },
    },
    {
        .transform = {
            .lighting = {
                .composeStamp = GRAPHICS_COORD_DIRTY,
                .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -2000, 2000, -1000 } },
                .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                .unknown_46   = { 0, 0, 0, 0 },
                .attenuation  = 0,
                .parent       = NULL,
            },
        },
        .color      = { .r = 2457, .g = 1966, .b = 2048 },
        .unknown_56 = { 0, 0 },
    },
};

WorldCoordRoomLights D_dryfield_gas_station_80184B48[1] = {
    { ARRAY_SIZE(_gDryfieldGasStationDirectionalLights), _gDryfieldGasStationDirectionalLights, 0, NULL, 0, NULL },
};

WorldCollisionFootstepSounds D_dryfield_gas_station_80184B60 = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionFootstepSounds D_dryfield_gas_station_80184B6C = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

WorldCollisionFootstepSounds D_dryfield_gas_station_80184B78 = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionSurfaceProperties D_dryfield_gas_station_80184B84[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_gas_station_80184B8C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_gas_station_80184B60 },
};

WorldCollisionSurfaceProperties D_dryfield_gas_station_80184B94[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_gas_station_80184B6C },
};

WorldCollisionSurfaceProperties D_dryfield_gas_station_80184B9C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_gas_station_80184B78 },
};

WorldCollisionSurfaceProperties D_dryfield_gas_station_80184BA4[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_dryfield_gas_station_80184BAC[8] = {
    D_dryfield_gas_station_80184B84,
    D_dryfield_gas_station_80184BA4,
    D_dryfield_gas_station_80184B94,
    D_dryfield_gas_station_80184B9C,
    D_dryfield_gas_station_80184B8C,
    D_dryfield_gas_station_80184B84,
    D_dryfield_gas_station_80184B84,
    D_dryfield_gas_station_80184B84,
};

Task* D_dryfield_gas_station_80184BCC = NULL;

RoomCutsceneRec D_dryfield_gas_station_80184BD8;

static void func_dryfield_gas_station_801803C0(Task* task);

/// Carries out `_DryfieldGasStationCutsceneWork::command`, then clears it.
/// A command that takes several updates advances `commandStep` and returns
/// until its last step.
static void func_dryfield_gas_station_801803C0(Task* task)
{
    _DryfieldGasStationCutsceneWork* work;
    _DryfieldGasStationCutsceneWork* cur;
    _DryfieldGasStationCutsceneWork* sharedWork;
    Task*                            shared;
    union {
        AnimationPlayRequest rec;
        GameActorMoveBy      move;
    } msg;
    AnimationPlayRequest  script;
    AnimationPlayRequest* rec;
    u16                   step;

    work = task->work;
    switch (work->command) {
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_NONE:
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_START_AUDIO:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_gas_station_80182E44[0], 0);
            sndEvtRequestScriptStart(SOUND_GAS_STATION_CUTSCENE_LOOP, 0, 0);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x12), 0, 0);
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLAY_ANIMATION:
            cur = task->work;
            if (cur->player != NULL) {
                msg.rec.source.sets          = D_dryfield_gas_station_80182E30;
                msg.rec.animationId          = 1;
                msg.rec.blend                = ANIMATION_BLEND_RESET;
                msg.rec.blendFrames          = 0;
                msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(cur->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
            }
            taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, 8, 0);
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_BLEND:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x13), 0, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_gas_station_80182E5C, 0);
            cur = task->work;
            if (cur->player != NULL) {
                msg.rec.source.sets          = D_dryfield_gas_station_80182E30;
                msg.rec.animationId          = 2;
                msg.rec.blend                = ANIMATION_BLEND_INTERPOLATE;
                msg.rec.blendFrames          = 0x1E;
                msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(cur->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
            }
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_WALK_PLACEMENTS:
            step = work->commandStep;
            switch (step) {
                case 0:
                    cur = task->work;
                    if (cur->player != NULL) {
                        msg.rec.source.sets          = D_dryfield_gas_station_80182E30;
                        msg.rec.animationId          = 3;
                        msg.rec.blend                = ANIMATION_BLEND_RESET;
                        msg.rec.blendFrames          = 0;
                        msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(cur->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
                    }
                    taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, 8, 0);
                    // Message 0x3FC sets the player's movement mode to 1.
                    taskMessageDispatch(work->player, 0x3FC, 0, 0);
                    work->walkFrames = 0;
                    work->commandStep++;
                    return;
                case 1:
                    msg.move.displacement.vx   = (D_dryfield_gas_station_80182E44[2].pos.vx - D_dryfield_gas_station_80182E44[0].pos.vx) / 30;
                    msg.move.displacement.vy   = 0;
                    msg.move.displacement.vz   = (D_dryfield_gas_station_80182E44[2].pos.vz - D_dryfield_gas_station_80182E44[0].pos.vz) / 30;
                    msg.move.collisionRequests = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_MOVE_BY, &msg.move, 0);
                    work->walkFrames++;
                    if (work->walkFrames < 31) {
                        return;
                    }
                    // Taken before the player check, the record's address is in
                    // $a2 early enough that the two register-valued fields are
                    // stored through it; the constant ones still go off $sp.
                    rec = &script;
                    cur = task->work;
                    if (cur->player != NULL) {
                        script.source.sets          = D_dryfield_gas_station_80182E30;
                        script.animationId          = 0;
                        rec->blend                  = step;
                        rec->blendFrames            = 0xF;
                        script.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(cur->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, rec, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_RESTORE_AND_SHOW:
            shared     = D_dryfield_gas_station_80184BD4;
            sharedWork = shared->work;
            if (sharedWork->playerEffectsSuppressed != 0) {
                Gp_SpawnWeaponEff();
                sharedWork->playerEffectsSuppressed = 0;
                Gp_MsgPlayerWeapon(0);
            }
            TASK_MESSAGE_DISPATCH_POINTER(sharedWork->player, GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_gas_station_80182E74, 0);
            cur = shared->work;
            if (cur->player != NULL) {
                msg.rec.source.sets          = D_dryfield_gas_station_80182E30;
                msg.rec.animationId          = 0;
                msg.rec.blend                = ANIMATION_BLEND_RESET;
                msg.rec.blendFrames          = 0;
                msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(cur->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
            }
            sndEvtRequestScriptStop(SOUND_GAS_STATION_CUTSCENE_LOOP, 0x3C);
            SetDispMask(1);
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_SPAWN_FADE_IN:
            switch (work->commandStep) {
                case 0:
                    taskSpawnFromTable(D_dryfield_gas_station_8018312C, 1, 0x1E, 0);
                    // The spawn and the one-frame wait share this update.
                case 1:
                    work->commandStep++;
                    return;
                case 2:
                    SetDispMask(1);
                    break;
            }
            break;
    }
    work->command = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_NONE;
}

/// Cutscene task. State 0 returns while the attachment wheel is open or a
/// display transition is pending. Otherwise it allocates and zeroes this
/// task's `_DryfieldGasStationCutsceneWork`, records the player task and
/// publishes itself as `D_dryfield_gas_station_80184BD4`. A failed allocation
/// kills the task without returning, so the reload of `Task::work` below
/// still runs. When that block has a player, state 0 installs animation 0,
/// starts the room's two event scripts and advances. State 1 asks to be
/// killed once `eventState` is idle, and otherwise carries out `command`.
void func_dryfield_gas_station_801807E0(Task* task)
{
    _DryfieldGasStationCutsceneWork* work;
    _DryfieldGasStationCutsceneWork* work2;
    AnimationPlayRequest             script;

    switch (task->state) {
        case 0:
            if ((Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                work       = memMalloc(sizeof(*work), false);
                task->work = work;
                if (work == NULL) {
                    taskKill(task);
                } else {
                    memFillBytes(work, 0, sizeof(*work));
                    work->player                    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    D_dryfield_gas_station_80184BD4 = task;
                }
                work2 = task->work;
                if (work2->player != NULL) {
                    script.source.sets          = D_dryfield_gas_station_80182E30;
                    script.animationId          = 0;
                    script.blend                = ANIMATION_BLEND_RESET;
                    script.blendFrames          = 0;
                    script.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(work2->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &script, 0);
                }
                func_800E3FAC(0xA2, 9);
                func_800E8634(D_dryfield_gas_station_80182E8C, 0,
                              D_dryfield_gas_station_8018303C);
                task->state = task->state + 1;
                return;
            }
            return;

        case 1:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(task, 0);
                return;
            }
            func_dryfield_gas_station_801803C0(task);
            break;
    }
}

/// Kills the player's effects once. The latch is stored before the call, which
/// leaves that store in the call's delay slot.
void func_dryfield_gas_station_80180944(void)
{
    _DryfieldGasStationCutsceneWork* work = D_dryfield_gas_station_80184BD4->work;
    if (work->playerEffectsSuppressed == 0) {
        work->playerEffectsSuppressed = 1;
        Gp_KillPlayerEffs();
    }
}

/// A second copy of the fade-in task, which this file's own task table names.
#define screenFadeInTask func_dryfield_gas_station_80180984
#include "../../shared/screen_fade_in.inc.c"
#undef screenFadeInTask

/// Opens the cutscene's view of the player: restores effects when
/// `playerEffectsSuppressed` is set, places the player at the third placement
/// and, when the cutscene task has a player, installs animation 0. Stops the
/// cutscene loop and enables the display. This is
/// `DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_RESTORE_AND_SHOW` written out for
/// the second script.
void func_dryfield_gas_station_80180A60(void)
{
    Task*                            task;
    _DryfieldGasStationCutsceneWork* work;
    _DryfieldGasStationCutsceneWork* work2;
    AnimationPlayRequest             script;

    task = D_dryfield_gas_station_80184BD4;
    work = task->work;
    if (work->playerEffectsSuppressed != 0) {
        Gp_SpawnWeaponEff();
        work->playerEffectsSuppressed = 0;
        Gp_MsgPlayerWeapon(0);
    }
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_gas_station_80182E74, 0);
    work2 = task->work;
    if (work2->player != NULL) {
        script.source.sets          = D_dryfield_gas_station_80182E30;
        script.animationId          = 0;
        script.blend                = ANIMATION_BLEND_RESET;
        script.blendFrames          = 0;
        script.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work2->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &script, 0);
    }
    sndEvtRequestScriptStop(SOUND_GAS_STATION_CUTSCENE_LOOP, 0x3C);
    SetDispMask(1);
}

/// Stores `arg0` as the cutscene command and restarts its step. The block is
/// the work of the task published in `D_dryfield_gas_station_80184BD4`.
void func_dryfield_gas_station_80180B2C(s16 arg0)
{
    _DryfieldGasStationCutsceneWork* work = D_dryfield_gas_station_80184BD4->work;

    work->command     = arg0;
    work->commandStep = 0;
}

#include "../../shared/glow_draw_star_local.inc.c"

#define GLOW_DRAW_RAY_STAR_OUTER(p, c, h) setRGB2(p, 0, h, h)
#define GLOW_DRAW_RAY_STAR_INNER(p, c, h) setRGB2(p, 0, c, c)
#define GLOW_DRAW_RAY_STAR_RAY(p, c)      setRGB2(p, 0, c, c)
#include "../../shared/glow_draw_ray_star.inc.c"

/// Per-frame effect: draws the gas station's shaft with the task's own
/// coordinate, then enables `gRoomEffectState->roomEffectMode`.
/// `Task::extra.coordBody->coord` is the coordinate both draws share. The
/// stage-visit byte `gGameSession->location.loc.view` is used as a bit index: bits 4, 6,
/// 11 and 12 (`0x1850`) select `glowDrawStarLocal` with the wide half-extent 0x80,
/// and any other non-zero bit selects `glowDrawRayStar`
/// with 0x40.
void func_dryfield_gas_station_80181A78(Task* arg0)
{
    s32       mask;
    GfxCoord* coord;

    mask  = 1 << gGameSession->location.loc.view;
    coord = arg0->extra.coordBody->coord;
    if (mask & 0x1850) {
        glowDrawStarLocal(coord, &D_dryfield_gas_station_80183144, 0x60, 0x80);
    } else if (mask != 0) {
        glowDrawRayStar(coord, &D_dryfield_gas_station_80183144, 0x60, 0x40);
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
