#include "rooms/dryfield_night_water_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room.h"
#include "../../shared/glow_draw.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `waterDrawSpinU16` and `waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s16 D_dryfield_night_water_hole_8018362C[2];

/// Parameter block of `roomVariantResolveShelter`, the room-local
/// resolver `func_dryfield_night_water_hole_8017DC28` calls with one pointer as
/// both its input and its output.
///
/// `field_0` is the code the resolver switches on: its jump table spans 2..0x2D
/// and anything outside that range falls through untouched. `field_2` passes
/// through unchanged, `field_3` is the byte the resolver writes, and `field_5`
/// is a busy flag - non-zero makes the resolver return immediately without
/// reading or writing anything else. The caller stages the block from the
/// `RoomDeparture` it is about to publish and copies `field_3` back into it.
typedef struct DnwhUtilParam {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
} DnwhUtilParam;
STATIC_ASSERT_SIZEOF(DnwhUtilParam, 0x6);

/// One entry of the NULL-terminated override list
/// `func_dryfield_night_water_hole_8017DE88` walks: the record the entry
/// installs in the room's parameter table, `Gp_RoomParamTables[stage][room]`,
/// and the slot it goes in.
typedef struct DnwhParamOverride {
    /* 0x0 */ WorldCollisionSurfaceProperties* rec;
    /* 0x4 */ s32                              index;
} DnwhParamOverride;
STATIC_ASSERT_SIZEOF(DnwhParamOverride, 0x8);

/// One rectangle of water surface drawn by
/// `func_dryfield_night_water_hole_8017DF28`, in world coordinates: it spans
/// `width` along X from `x` and `depth` along Z from `z`, at height `y`. The
/// table ends at the first entry whose `y` word is -1; the drawing code reads
/// only its low half as the height.
typedef struct DnwhSurface {
    s16 x;
    s16 z;
    s16 width;
    u16 depth;
    s32 y;
} DnwhSurface;

/// Block the room's splash task receives as `spawnArg2`. Only the halfword at
/// 0x26 is touched: an effect strength, set from how far a tracked part moved
/// this frame and used as the odds of spawning each of the two effects.
typedef struct _DryfieldNightWaterHoleSplash {
    byte pad_0[0x26];
    s16  strength;
} _DryfieldNightWaterHoleSplash;

/// Resident task table the ending task is spawned from, descriptor 1.
extern TaskDesc D_801351FC[];

/// Descriptor the room's event task is spawned from, index 0 of the table
/// `func_dryfield_night_water_hole_8017DC28` hands `Task_SpawnFromTable`. Its
/// callback is that same task, `roomDepartureTask`.
extern TaskDesc D_dryfield_night_water_hole_801805EC;
/// The room's message table, the `GpMsgEntry` list the room task publishes in
/// `Task::msgTable` for `Gp_DispatchMsg` to walk: 0x13EE, 0x13F1, 0x13EF and
/// 0x13F0.
extern GpMsgEntry D_dryfield_night_water_hole_801805F8[];
/// The two four-byte records this room hands the slot-4 task as the message
/// 0x7DB payload, picked by `gGameSession::location.loc.warp`. They are the last two of
/// the four-record run at 0x80180654, which differ only in the halfword at 0x2.
extern s32 D_dryfield_night_water_hole_8018065C;
extern s32 D_dryfield_night_water_hole_80180660;
/// The records message 0x13EF passes to `func_800E8614` on the first visit
/// through sub-id 1, for `field_2` 2 and 1 respectively.
extern EvsCommand D_dryfield_night_water_hole_8018067C[];
extern EvsCommand D_dryfield_night_water_hole_801807FC[];
/// Descriptor of the room's water task, spawned while progress nibble 0xB8 is
/// still clear. Its callback is `func_dryfield_night_water_hole_8017E630`.
extern TaskDesc D_dryfield_night_water_hole_80180964[];
/// The room's water surfaces, terminated by an entry with `y == -1`.
extern DnwhSurface D_dryfield_night_water_hole_80180970[];
/// Point pairs of the glowing beams the splash task draws, one table per group
/// of views.
extern SVECTOR D_dryfield_night_water_hole_80180994[];
extern SVECTOR D_dryfield_night_water_hole_801809B4[];
extern SVECTOR D_dryfield_night_water_hole_801809D4[];
/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_dryfield_night_water_hole_801809F4[];
/// Override list applied once nibble 0xB8 is set.
extern DnwhParamOverride D_dryfield_night_water_hole_801835D8[];
/// Cursor into the primitive area the room's water surface is written to,
/// reset each frame to the half of that area belonging to the ordering table
/// being built.
extern u8* D_dryfield_night_water_hole_80183628;
/// Frame counter the water surface's wave is phased by.
/// The staged event descriptor, read by the room's event task.
extern RoomDeparture gRoomDeparture;

static void func_dryfield_night_water_hole_8017DE20(Task* task);
static void func_dryfield_night_water_hole_8017DE88(DnwhParamOverride* list);
static void func_dryfield_night_water_hole_8017E690(Task* arg0);

void func_dryfield_night_water_hole_8017E630(Task*);

extern WorldCollisionGrid         D_dryfield_night_water_hole_80180F50[1];
extern GpObj3A                    D_dryfield_night_water_hole_80182D58[2];
extern WorldCollisionTrigger      D_dryfield_night_water_hole_801824BC[12];
extern WorldCollisionTrigger      D_dryfield_night_water_hole_8018284C[9];
extern WorldCollisionTrigger      D_dryfield_night_water_hole_80182AF8[8];
extern WorldCoordRoomAmbientEntry D_dryfield_night_water_hole_801834C8[12];
extern WorldCoordRoomAmbientEntry D_dryfield_night_water_hole_80183528[12];
extern WorldCoordRoomLights       D_dryfield_night_water_hole_8018307C[1];
extern WorldCoordRoomLights       D_dryfield_night_water_hole_801833A0[1];

extern AnimationSet* D_dryfield_night_water_hole_80180620[1];

extern WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835A0[1];
extern WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835A8[1];
extern WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835B0[1];
extern WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835B8[1];
extern WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835C0[1];

s32 func_dryfield_night_water_hole_8017DAD4(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_night_water_hole_8017DADC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_water_hole_8017DC28(Task*, s32, s32, s32);
s32 func_dryfield_night_water_hole_8017DD5C(Task*, s32, RoomEventMsg*, RoomEventMsg*);

AnimationPackedPose D_dryfield_night_water_hole_80180220[6] = {
#include "assets/dryfield_night_water_hole_animation_03004_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_water_hole_80180268[64] = {
#include "assets/dryfield_night_water_hole_animation_03004_bank4.inc"
};

AnimationRecord D_dryfield_night_water_hole_80180368[141] = {
#include "assets/dryfield_night_water_hole_animation_03004_records.inc"
};

u16 D_dryfield_night_water_hole_8018059C[20] = {
#include "assets/dryfield_night_water_hole_animation_03004_indices.inc"
};

AnimationSet D_dryfield_night_water_hole_801805C4 = {
    D_dryfield_night_water_hole_80180368,
    D_dryfield_night_water_hole_8018059C,
    { NULL, D_dryfield_night_water_hole_80180220, NULL, NULL, D_dryfield_night_water_hole_80180268, NULL, NULL, NULL },
};

TaskDesc D_dryfield_night_water_hole_801805EC = { { { TASK_BODY_NONE, 32 } }, roomDepartureTask, { .value = 0 } };

GpMsgEntry D_dryfield_night_water_hole_801805F8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_water_hole_8017DADC },
    { 5105, func_dryfield_night_water_hole_8017DAD4 },
    { 5103, func_dryfield_night_water_hole_8017DD5C },
    { 5104, func_dryfield_night_water_hole_8017DC28 },
    { 0x7FFFFFFF, NULL },
};

AnimationSet* D_dryfield_night_water_hole_80180620[1] = {
    &D_dryfield_night_water_hole_801805C4,
};

GpCopyArg D_dryfield_night_water_hole_80180624 = { { .sets = D_dryfield_night_water_hole_80180620 }, 1 };

AnimationPlayRequest D_dryfield_night_water_hole_8018062C = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_water_hole_80180640 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_dryfield_night_water_hole_80180654 = { { .loc = { 3, 32 } }, 0 };

ActorCommand D_dryfield_night_water_hole_80180658 = { { .loc = { 3, 32 } }, 1 };

s32 D_dryfield_night_water_hole_8018065C = 0x22003;

s32 D_dryfield_night_water_hole_80180660 = 0x32003;

ActorTransform D_dryfield_night_water_hole_80180664 = { { 0, 0, 0, 0 }, { 0, 1024, 0, 0 } };

EvsCommand D_dryfield_night_water_hole_8018067C[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_dryfield_night_water_hole_80180624 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_hole_80180640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_hole_8018062C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_night_water_hole_80180658 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_water_hole_801807FC[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_dryfield_night_water_hole_80180624 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_hole_80180640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_hole_8018062C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_night_water_hole_80180654 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_dryfield_night_water_hole_80180664 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_night_water_hole_80180964[1] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_water_hole_8017E630, { .value = 0 } },
};

DnwhSurface D_dryfield_night_water_hole_80180970[3] = {
    { 4000, -2000, 8000, 2000, -420 },
    { 0x2710, -4000, 0x32C8, 2000, -420 },
    { 0, 0, 0, 0, -1 },
};

SVECTOR D_dryfield_night_water_hole_80180994[4] = {
    { 9500, -1835, -100, 0 },
    { 0x2904, -1835, -100, 0 },
    { 9500, -1750, -75, 0 },
    { 0x2904, -1750, -75, 0 },
};

SVECTOR D_dryfield_night_water_hole_801809B4[4] = {
    { 0x37DC, -1835, -3900, 0 },
    { 0x3BC4, -1835, -3900, 0 },
    { 0x37DC, -1750, -3925, 0 },
    { 0x3BC4, -1750, -3925, 0 },
};

SVECTOR D_dryfield_night_water_hole_801809D4[4] = {
    { 0x477C, -1835, -2120, 0 },
    { 0x4B64, -1835, -2120, 0 },
    { 0x477C, -1750, -2095, 0 },
    { 0x4B64, -1750, -2095, 0 },
};

SVECTOR D_dryfield_night_water_hole_801809F4[2] = { 0 };

GpRoomObjRec D_dryfield_night_water_hole_80180A04[4] = {
    { D_dryfield_night_water_hole_80180F50, D_dryfield_night_water_hole_801824BC, D_dryfield_night_water_hole_8018284C, D_dryfield_night_water_hole_80182D58 },
    { D_dryfield_night_water_hole_80180F50, D_dryfield_night_water_hole_801824BC, D_dryfield_night_water_hole_80182AF8, D_dryfield_night_water_hole_80182D58 },
    { D_dryfield_night_water_hole_80180F50, D_dryfield_night_water_hole_801824BC, D_dryfield_night_water_hole_8018284C, D_dryfield_night_water_hole_80182D58 },
    { D_dryfield_night_water_hole_80180F50, D_dryfield_night_water_hole_801824BC, D_dryfield_night_water_hole_80182AF8, D_dryfield_night_water_hole_80182D58 },
};

WorldCoordRoomLighting D_dryfield_night_water_hole_80180A44[4] = {
    { D_dryfield_night_water_hole_8018307C, D_dryfield_night_water_hole_801834C8 },
    { D_dryfield_night_water_hole_801833A0, D_dryfield_night_water_hole_80183528 },
    { D_dryfield_night_water_hole_8018307C, D_dryfield_night_water_hole_801834C8 },
    { D_dryfield_night_water_hole_801833A0, D_dryfield_night_water_hole_80183528 },
};

u8 D_dryfield_night_water_hole_80180A64[12] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    21,
    22,
    23,
    0,
};

u8 D_dryfield_night_water_hole_80180A70[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    24,
    25,
    26,
    0,
};

u8 D_dryfield_night_water_hole_80180A7C[12] = {
    1,
    2,
    3,
    4,
    5,
    6,
    10,
    9,
    21,
    22,
    23,
    0,
};

u8 D_dryfield_night_water_hole_80180A88[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    20,
    19,
    24,
    25,
    26,
    0,
};

u8* D_dryfield_night_water_hole_80180A94[4] = {
    D_dryfield_night_water_hole_80180A64,
    D_dryfield_night_water_hole_80180A70,
    D_dryfield_night_water_hole_80180A7C,
    D_dryfield_night_water_hole_80180A88,
};

GpViewCountRec D_dryfield_night_water_hole_80180AA4[4] = {
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
};

GpWarpRec D_dryfield_night_water_hole_80180AAC[3] = {
    { { .words = { 0, 7400, 0, -1300 } }, { 0, 0, 0, 0 }, { .words = { 0, 7400, 0, -1300 } }, { 0, 0, 0, 0 }, 0x53200003, 0x53200003, 0, 3, 2, 0 },
    { { .words = { 3072, 0x57C0, -534, -3076 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x57C0, -534, -3076 } }, { 0, 0, 0, 0 }, 0x53200002, 0x53200001, 0, 8, 0, 462 },
    { { .words = { 1024, 4680, 0, -954 } }, { 0, 0, 0, 0 }, { .words = { 1024, 4680, 0, -954 } }, { 0, 0, 0, 0 }, 0x53200008, 0, 0, 2, 0, 445 },
};

SVECTOR D_dryfield_night_water_hole_80180B54[10] = {
#include "assets/dryfield_night_water_hole_collision_03990_normals.inc"
};

SVECTOR D_dryfield_night_water_hole_80180BA4[54] = {
#include "assets/dryfield_night_water_hole_collision_03990_verts.inc"
};

WorldCollisionGridFace D_dryfield_night_water_hole_80180D54[23] = {
#include "assets/dryfield_night_water_hole_collision_03990_faces.inc"
};

s16 D_dryfield_night_water_hole_80180E68[92] = {
#include "assets/dryfield_night_water_hole_collision_03990_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_water_hole_80180E68[i])
s16* D_dryfield_night_water_hole_80180F20[12] = {
#include "assets/dryfield_night_water_hole_collision_03990_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_water_hole_80180F50[1] = {
    { NULL, D_dryfield_night_water_hole_80180B54, D_dryfield_night_water_hole_80180BA4, D_dryfield_night_water_hole_80180D54, D_dryfield_night_water_hole_80180F20, -4000, 5000, 6, 2, 4000, 23 },
};

GpViewRec D_dryfield_night_water_hole_80180F74[26] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x34B7, 0x78AD, 3935 } }, 329 },
    { { { { -1708, 0, 3722 }, { 704, 4022, 323 }, { -3655, 774, -1677 } }, { -7853, 1372, 107 } }, 230 },
    { { { { -1222, 0, -3909 }, { -357, 4078, 111 }, { 3892, 374, -1217 } }, { -4081, 1167, 312 } }, 230 },
    { { { { -1694, 0, -3728 }, { -261, 4085, 118 }, { 3719, 287, -1690 } }, { -7539, 1082, 155 } }, 243 },
    { { { { -3680, 0, -1797 }, { -224, 4063, 459 }, { 1783, 511, -3651 } }, { -0x2893, 1214, 35 } }, 207 },
    { { { { -713, 0, 4033 }, { 463, 4068, 82 }, { -4006, 471, -708 } }, { -0x497F, 1141, 2467 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x34B7, 0x8C35, 3935 } }, 329 },
    { { { { -1708, 0, 3722 }, { 704, 4022, 323 }, { -3655, 774, -1677 } }, { -7853, 1372, 107 } }, 230 },
    { { { { -1222, 0, -3909 }, { -357, 4078, 111 }, { 3892, 374, -1217 } }, { -4081, 1167, 312 } }, 230 },
    { { { { -1694, 0, -3728 }, { -261, 4085, 118 }, { 3719, 287, -1690 } }, { -7539, 1082, 155 } }, 243 },
    { { { { -3680, 0, -1797 }, { -224, 4063, 459 }, { 1783, 511, -3651 } }, { -0x2893, 1214, 35 } }, 207 },
    { { { { -713, 0, 4033 }, { 463, 4068, 82 }, { -4006, 471, -708 } }, { -0x497F, 1141, 2467 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -3186, 0, -2573 }, { -25, 4095, 31 }, { 2573, 40, -3186 } }, { -8630, 1300, -20 } }, 246 },
    { { { { -2620, 0, 3148 }, { 219, 4086, 182 }, { -3140, 285, -2613 } }, { -9960, 1360, 430 } }, 246 },
    { { { { -2395, 0, -3322 }, { -237, 4085, 171 }, { 3313, 293, -2389 } }, { -9120, 1360, 430 } }, 246 },
    { { { { -3590, 0, -1971 }, { -90, 4091, 164 }, { 1968, 188, -3586 } }, { -7990, 1450, -1820 } }, 541 },
    { { { { -1831, 0, 3663 }, { 153, 4092, 76 }, { -3660, 171, -1830 } }, { -0x2C2E, 1460, 130 } }, 541 },
    { { { { -3154, 0, -2612 }, { -258, 4075, 311 }, { 2599, 404, -3139 } }, { -7920, 1660, -1120 } }, 541 },
};

SpriteBatch D_dryfield_night_water_hole_8018131C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_8018132C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_8018133C[21] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, -24, 2000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 2000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -72, 2000, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -72, 2000, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, -72, 2000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, -72, 2000, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -24, 2000, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -24, 2000, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -72, 1375, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, -72, 1375, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -72, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -24, 1375, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -24, 1375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1375, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, -120, 875, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, -120, 875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, -120, 875, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -80, 875, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -80, 875, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_801814E0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 8, 8, 0, 0, { 2, 0 } },
    { 16, 5, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_80181508[33] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 625, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 625, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -120, 625, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -120, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, -120, 625, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -120, 625, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -72, 625, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -24, 625, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 24, 625, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -72, 625, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -24, 625, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 24, 625, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -120, 1250, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -72, 1250, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -24, 1250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -120, 1250, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -72, 1250, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -24, 1250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -72, 24, 1250, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -120, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -72, 1250, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -24, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -120, 24, 1250, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 24, 1250, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1250, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 8, -120, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -8, -72, 1250, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 24, -112, 1250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -112, 1250, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 56, -112, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_8018179C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_801817BC[18] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -96, 625, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 24, 625, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -112, 24, 625, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -112, -104, 625, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, -112, 625, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -24, 625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 24, 625, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 24, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -24, 625, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -72, 625, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -120, 625, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, -120, 625, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -112, 625, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_80181924[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_8018193C[19] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -72, 1375, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -32, 1375, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, -72, 1375, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -72, 1375, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 112, -32, 1375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -72, 1375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -72, 1375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -32, 1375, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -32, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -32, 2000, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -72, 2000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -72, 2000, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -72, 2000, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -32, 2000, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -72, 2000, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -72, 2000, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -32, 2000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -64, 3000, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 3000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_80181AB8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 8, 0, 0, { 2, 0 } },
    { 17, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181AE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181AF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181B00[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181B10[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181B20[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181B30[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_80181B40[21] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -72, 1375, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, -72, 1375, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -24, 1375, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -24, 1375, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -72, 1375, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -72, 2000, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, -72, 2000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -72, 2000, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -24, 2000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, -24, 2000, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, -72, 2000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -24, 2000, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 2000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, -120, 875, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, -120, 875, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, -120, 875, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -80, 875, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -80, 875, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_80181CE4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 2, 0 } },
    { 8, 8, 0, 0, { 0, 0 } },
    { 16, 5, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_80181D0C[33] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, -120, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -120, 625, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 625, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -120, 625, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -120, 625, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -72, 625, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -24, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 24, 625, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 24, 625, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -24, 625, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -72, 625, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -120, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -120, 1250, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -120, 1250, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1250, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -120, 1250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 24, -112, 1250, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -112, 1250, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 56, -112, 1250, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -72, 1250, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -24, 1250, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -120, 24, 1250, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -72, -72, 1250, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -24, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -72, 24, 1250, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 24, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -24, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -72, 1250, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -8, -72, 1250, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_80181FA0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_80181FC0[18] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 24, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -24, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -72, 625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -120, 625, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 24, 625, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 625, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, -112, 625, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 24, 625, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -96, 625, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -112, 24, 625, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 625, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -112, -104, 625, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, -120, 625, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -112, 625, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_80182128[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_80182140[19] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -32, 2000, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -72, 2000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -72, 2000, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -72, 2000, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -72, 2000, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -32, 2000, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -32, 2000, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -72, 2000, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -32, 1375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -72, 1375, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -32, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -32, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 112, -32, 1375, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -72, 1375, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -72, 1375, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, -72, 1375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -72, 1375, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 3000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -64, 3000, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_801822BC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 9, 0, 0, { 2, 0 } },
    { 17, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_801822E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_801822F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182304[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182314[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182324[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182334[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182344[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182354[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182364[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182374[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_water_hole_80182384[26] = {
    { { .empty = D_dryfield_night_water_hole_8018131C }, D_dryfield_night_water_hole_8018131C, NULL },
    { { .empty = D_dryfield_night_water_hole_8018132C }, D_dryfield_night_water_hole_8018132C, NULL },
    { { .elements = D_dryfield_night_water_hole_8018133C }, D_dryfield_night_water_hole_801814E0, NULL },
    { { .elements = D_dryfield_night_water_hole_80181508 }, D_dryfield_night_water_hole_8018179C, NULL },
    { { .elements = D_dryfield_night_water_hole_801817BC }, D_dryfield_night_water_hole_80181924, NULL },
    { { .elements = D_dryfield_night_water_hole_8018193C }, D_dryfield_night_water_hole_80181AB8, NULL },
    { { .empty = D_dryfield_night_water_hole_80181AE0 }, D_dryfield_night_water_hole_80181AE0, NULL },
    { { .empty = D_dryfield_night_water_hole_80181AF0 }, D_dryfield_night_water_hole_80181AF0, NULL },
    { { .empty = D_dryfield_night_water_hole_80181B00 }, D_dryfield_night_water_hole_80181B00, NULL },
    { { .empty = D_dryfield_night_water_hole_80181B10 }, D_dryfield_night_water_hole_80181B10, NULL },
    { { .empty = D_dryfield_night_water_hole_80181B20 }, D_dryfield_night_water_hole_80181B20, NULL },
    { { .empty = D_dryfield_night_water_hole_80181B30 }, D_dryfield_night_water_hole_80181B30, NULL },
    { { .elements = D_dryfield_night_water_hole_80181B40 }, D_dryfield_night_water_hole_80181CE4, NULL },
    { { .elements = D_dryfield_night_water_hole_80181D0C }, D_dryfield_night_water_hole_80181FA0, NULL },
    { { .elements = D_dryfield_night_water_hole_80181FC0 }, D_dryfield_night_water_hole_80182128, NULL },
    { { .elements = D_dryfield_night_water_hole_80182140 }, D_dryfield_night_water_hole_801822BC, NULL },
    { { .empty = D_dryfield_night_water_hole_801822E4 }, D_dryfield_night_water_hole_801822E4, NULL },
    { { .empty = D_dryfield_night_water_hole_801822F4 }, D_dryfield_night_water_hole_801822F4, NULL },
    { { .empty = D_dryfield_night_water_hole_80182304 }, D_dryfield_night_water_hole_80182304, NULL },
    { { .empty = D_dryfield_night_water_hole_80182314 }, D_dryfield_night_water_hole_80182314, NULL },
    { { .empty = D_dryfield_night_water_hole_80182324 }, D_dryfield_night_water_hole_80182324, NULL },
    { { .empty = D_dryfield_night_water_hole_80182334 }, D_dryfield_night_water_hole_80182334, NULL },
    { { .empty = D_dryfield_night_water_hole_80182344 }, D_dryfield_night_water_hole_80182344, NULL },
    { { .empty = D_dryfield_night_water_hole_80182354 }, D_dryfield_night_water_hole_80182354, NULL },
    { { .empty = D_dryfield_night_water_hole_80182364 }, D_dryfield_night_water_hole_80182364, NULL },
    { { .empty = D_dryfield_night_water_hole_80182374 }, D_dryfield_night_water_hole_80182374, NULL },
};

WorldCollisionTrigger D_dryfield_night_water_hole_801824BC[12] = {
    { NULL, NULL, NULL, { 6101, -1152, -1034, 0 }, { { 199, -2176, -1004, 0 }, { -200, -2176, 1004, 0 }, { 199, 2176, -1004, 0 }, { -200, 2176, 1004, 0 } }, { 4021, 0, 797, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6304, -1104, -1024, 0 }, { { -154, -2128, 1004, 0 }, { 144, -2128, -1022, 0 }, { -154, 2128, 1004, 0 }, { 144, 2128, -1022, 0 } }, { -4056, 0, -597, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9499, -1248, -1060, 0 }, { { -345, -2272, -963, 0 }, { 345, -2272, 963, 0 }, { -345, 2272, -963, 0 }, { 345, 2272, 963, 0 } }, { 3869, 0, -1388, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9632, -1200, -1026, 0 }, { { 340, -2224, 960, 0 }, { -350, -2224, -966, 0 }, { 340, 2224, 960, 0 }, { -350, 2224, -966, 0 } }, { -3865, 0, 1383, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A9F, -1088, -1922, 0 }, { { 919, -2112, 432, 0 }, { -929, -2112, -443, 0 }, { 919, 2112, 432, 0 }, { -929, 2112, -443, 0 } }, { -1765, 0, 3723, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A7E, -1168, -1858, 0 }, { { -927, -2192, -439, 0 }, { 923, -2192, 434, 0 }, { -927, 2192, -439, 0 }, { 923, 2192, 434, 0 } }, { 1748, 0, -3710, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x319F, -1248, -2945, 0 }, { { 435, -2272, -931, 0 }, { -441, -2272, 919, 0 }, { 435, 2272, -931, 0 }, { -441, 2272, 919, 0 } }, { 3717, 0, 1758, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x323F, -1136, -2976, 0 }, { { -441, -2160, 919, 0 }, { 435, -2160, -931, 0 }, { -441, 2160, 919, 0 }, { 435, 2160, -931, 0 } }, { -3706, 0, -1755, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x415F, -1200, -2912, 0 }, { { 198, -2224, 1001, 0 }, { -202, -2224, -1006, 0 }, { 198, 2224, 1001, 0 }, { -202, 2224, -1006, 0 } }, { -4027, 0, 801, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40FF, -1328, -2977, 0 }, { { -205, -2352, -1008, 0 }, { 195, -2352, 999, 0 }, { -205, 2352, -1008, 0 }, { 195, 2352, 999, 0 } }, { 4021, 0, -803, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x531F, -1376, -2978, 0 }, { { 0, -2400, 1024, 0 }, { 0, -2400, -1023, 0 }, { 0, 2400, 1024, 0 }, { 0, 2400, -1023, 0 } }, { -4116, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x523F, -1408, -2978, 0 }, { { 51, -2432, -1022, 0 }, { -50, -2432, 1022, 0 }, { 51, 2432, -1022, 0 }, { -50, 2432, 1022, 0 } }, { 4093, 0, 200, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_water_hole_8018284C[9] = {
    { NULL, NULL, NULL, { 7376, -48, -1616, 0 }, { { -719, 0, -336, 0 }, { 720, 0, -336, 0 }, { -719, 0, 336, 0 }, { 720, 0, 336, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x58B0, -1280, -2992, 0 }, { { 0, 1344, -1199, 0 }, { 0, 1344, 1200, 0 }, { 0, -1344, -1199, 0 }, { 0, -1344, 1200, 0 } }, { -4101, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1801, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 38, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5860, -403, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 18, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING, 21, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x34B0, -64, -2961, 0 }, { { 398, 0, -1136, 0 }, { 718, 0, -951, 0 }, { -718, 0, 952, 0 }, { -398, 0, 1137, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1200, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4100, -64, -5409, 0 }, { { -838, 0, -1072, 0 }, { 491, 0, -1269, 0 }, { -491, 0, 1270, 0 }, { 838, 0, 1073, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1360, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 7376, -64, -2176, 0 }, { { -959, 0, 176, 0 }, { 960, 0, 176, 0 }, { -959, 0, 848, 0 }, { 960, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x40A0, -64, -4416, 0 }, { { -1007, 0, 464, 0 }, { 944, 0, 464, 0 }, { -1007, 0, 1392, 0 }, { 944, 0, 1392, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1717, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_water_hole_80182AF8[8] = {
    { NULL, NULL, NULL, { 7376, -48, -1616, 0 }, { { -719, 0, -336, 0 }, { 720, 0, -336, 0 }, { -719, 0, 336, 0 }, { 720, 0, 336, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x58B0, -1280, -2992, 0 }, { { 0, 1344, -1199, 0 }, { 0, 1344, 1200, 0 }, { 0, -1344, -1199, 0 }, { 0, -1344, 1200, 0 } }, { -4101, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1801, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 38, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5860, -403, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 18, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING, 21, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x34B0, -64, -2961, 0 }, { { 398, 0, -1136, 0 }, { 718, 0, -951, 0 }, { -718, 0, 952, 0 }, { -398, 0, 1137, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1200, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4100, -64, -5409, 0 }, { { -838, 0, -1072, 0 }, { 491, 0, -1269, 0 }, { -491, 0, 1270, 0 }, { 838, 0, 1073, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1360, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 7376, -64, -2240, 0 }, { { -959, 0, 208, 0 }, { 960, 0, 208, 0 }, { -959, 0, 976, 0 }, { 960, 0, 976, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 1366, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

GpObj3A D_dryfield_night_water_hole_80182D58[2] = {
    { NULL, NULL, { 8544, -1520, -3664, 0 }, { { -1536, -2224, -1840, 0 }, { -1536, 2224, -1840, 0 }, { 1536, -2224, 1840, 0 }, { 1536, 2224, 1840, 0 } }, { -3150, 0, 2629, 0 }, { -60, 12 }, 1, 0 },
    { NULL, NULL, { 0x34E0, -1568, -176, 0 }, { { -1504, -2224, -1936, 0 }, { -1504, 2224, -1936, 0 }, { 1504, -2224, 1936, 0 }, { 1504, 2224, 1936, 0 } }, { -3237, 0, 2514, 0 }, { -20, 12 }, 129, 0 },
};

WorldCoordPointLight D_dryfield_night_water_hole_80182DD0[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9250, -1650, -600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3604, 2867 }, { 0, 0 } }, 1500, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x374A, -1650, -3400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3604, 2867 }, { 0, 0 } }, 1500, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x499C, -1650, -2600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3604, 2867 }, { 0, 0 } }, 1500, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x63C9, -4206, -2304 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 3000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -1290, -663 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1441, 1146 }, { 0, 0 } }, 1000, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2B64, -1277, -2296 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1441, 1146 }, { 0, 0 } }, 1000, 2700 },
};

WorldCoordSpotLight D_dryfield_night_water_hole_80183010[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7394, -3968, -1496 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3604, 2867 }, { 0, 0 } }, { 0, 4096, 0, 0 }, 3000, 6000, 113 },
};

WorldCoordRoomLights D_dryfield_night_water_hole_8018307C[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_water_hole_80182DD0), D_dryfield_night_water_hole_80182DD0, ARRAY_SIZE(D_dryfield_night_water_hole_80183010), D_dryfield_night_water_hole_80183010 },
};

WorldCoordPointLight D_dryfield_night_water_hole_80183094[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8650, -1650, -1100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3A06, -1650, -3100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x48A2, -1650, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x5FE1, -4206, -2344 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2621, 1638 }, { 0, 0 } }, 0, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5500, -1650, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2A9C, -1277, -2296 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x5208, -1277, -2996 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
};

WorldCoordSpotLight D_dryfield_night_water_hole_80183334[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7444, -3447, -1496 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3014, 2621 }, { 0, 0 } }, { 0, 4096, 0, 0 }, 2500, 5000, 113 },
};

WorldCoordRoomLights D_dryfield_night_water_hole_801833A0[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_water_hole_80183094), D_dryfield_night_water_hole_80183094, ARRAY_SIZE(D_dryfield_night_water_hole_80183334), D_dryfield_night_water_hole_80183334 },
};

GpAreaTmdRec D_dryfield_night_water_hole_801833B8[2] = {
    { 6, 6, 3, 0, { 0, 0 }, D_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_water_hole_801833D0[2] = {
    { 11, 11, 0, 0, { 0, 0 }, D_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_water_hole_801833E8[2] = {
    { 101, 460, 0, 0, { 0, 0 }, D_801351FC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_water_hole_80183400[2] = {
    { 6, 6, 3, 0, { 0, 0 }, D_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_water_hole_80183418[22] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017CCE8, D_dryfield_night_water_hole_801833B8 },
    { D_map_dryfield_full_8017CD08, D_dryfield_night_water_hole_801833D0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017CD38, D_dryfield_night_water_hole_801833E8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017CD48, D_dryfield_night_water_hole_80183400 },
};

WorldCoordRoomAmbientEntry D_dryfield_night_water_hole_801834C8[12] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_water_hole_801834C8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 509, 508, 509, 508 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCoordRoomAmbientEntry D_dryfield_night_water_hole_80183528[12] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_water_hole_80183528) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 118, 117, 118, 117 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_dryfield_night_water_hole_80183588 = {
    0x10000025,
    0x10000027,
    0x10000029,
};

WorldCollisionFootstepSounds D_dryfield_night_water_hole_80183594 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835A0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835A8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_water_hole_80183588 },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835B0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_water_hole_80183588 },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835B8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835C0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_water_hole_80183594 },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835C8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_water_hole_80183594 },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835D0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_water_hole_80183594 },
};

DnwhParamOverride D_dryfield_night_water_hole_801835D8[4] = {
    { D_dryfield_night_water_hole_801835C8, 4 },
    { D_dryfield_night_water_hole_801835C8, 1 },
    { D_dryfield_night_water_hole_801835D0, 2 },
    { NULL, 0 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_water_hole_801835F8[8] = {
    D_dryfield_night_water_hole_801835A0,
    D_dryfield_night_water_hole_801835A8,
    D_dryfield_night_water_hole_801835B0,
    D_dryfield_night_water_hole_801835B8,
    D_dryfield_night_water_hole_801835A0,
    D_dryfield_night_water_hole_801835A0,
    D_dryfield_night_water_hole_801835C0,
    D_dryfield_night_water_hole_801835A0,
};

GpAreaApplyRec D_dryfield_night_water_hole_80183618[4] = {
    { 4, 42, 1, 1 },
    { 4, 44, 4, 1 },
    { 3, 21, 10, 1 },
    { 255, 0, 0, 0 },
};

u8* D_dryfield_night_water_hole_80183628 = NULL;

s16 D_dryfield_night_water_hole_8018362C[2] = {
    0,
    -0x3000,
};

RoomDeparture gRoomDeparture;

static void func_dryfield_night_water_hole_8017D958(Task* arg0);
static void func_dryfield_night_water_hole_8017DF28(Task* task);

#include "../../shared/room_variants_shelter.inc.c"

#include "../../shared/room_event_departure_task.inc.c"

/// Room entry task tick: publish the room's message table in `Task::msgTable`
/// and claim game pointer slot 7. Progress nibble 0xB8 then picks the opening
/// move: while it is clear the room's water task is spawned from
/// `D_dryfield_night_water_hole_80180964`, and once it is set the parameter
/// overrides are applied instead.
///
/// On the visit whose sub-id (`gGameSession::location.loc.variant`) is 1 and that has
/// already latched nibble 0x95, and with the slot-4 task present, the room
/// announces itself to it with message 0x7DB, carrying the payload record
/// `gGameSession::location.loc.warp` selects. On sub-id 0xA, with pointer slot 0xA
/// filled and nibble 0xCF still clear, it latches 0xCF, arms
/// `func_800E3FAC(0xA2, 0x25)` and spawns the ending task. Then advances state.
static void func_dryfield_night_water_hole_8017D958(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_water_hole_801805F8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0xB8) == 0) {
        Task_SpawnFromTable(D_dryfield_night_water_hole_80180964, 0, 0, 0);
    } else {
        func_dryfield_night_water_hole_8017DE88(D_dryfield_night_water_hole_801835D8);
    }
    if (gGameSession->location.loc.variant == 1 && Gp_LookupSlot4(0) != 0 && GameFlag_GetNibble(0x95) != 0) {
        if (gGameSession->location.loc.warp == 2) {
            TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_water_hole_80180660, 0);
        } else {
            TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_water_hole_8018065C, 0);
        }
    }
    if (gGameSession->location.loc.variant == 0xA && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0 && GameFlag_GetNibble(0xCF) == 0) {
        GameFlag_SetNibble(0xCF, 2);
        func_800E3FAC(0xA2, 0x25);
        Task_SpawnFromTable(D_801351FC, 1, 0, 0);
    }
    arg0->state = arg0->state + 1;
}

/// The room task's three states, run from a stack copy by
/// `func_dryfield_night_water_hole_8017DE30`: the entry tick, the idle state,
/// then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_water_hole_8017D688 = {
    { func_dryfield_night_water_hole_8017D958, func_dryfield_night_water_hole_8017DE20, taskKill },
};

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_night_water_hole_8017DAD4(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table. It copies the
/// incoming record to `out` and, unless `in->queryOnly` is set, answers two
/// queries in `out->room`:
///
/// - 0x19: while the session's stage is 2, 2 once progress nibble 0x3A has
///   reached 2 and 1 before; in any other stage, nibble 0x61 plus one.
/// - 0x26: with nibble 0xC9 set, 2 or 1 by nibble 0x53, plus 2 while nibble
///   0x51 is clear; with 0xC9 clear, 5 or 6 by whether nibble 0x51 is set.
///
/// Always returns 1.
s32 func_dryfield_night_water_hole_8017DADC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp;

    *out = *in;
    if (in->areaId == 0x19) {
        temp = gGameSession->location.loc.stage;
        if (temp == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->room = temp;
                } else {
                    out->room = 1;
                }
            }
        } else if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->room = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->areaId == 0x26 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                out->room = 2;
            } else {
                out->room = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->room += 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                out->room = 5;
            } else {
                out->room = 6;
            }
        }
    }
    return 1;
}

/// Message 0x13F0 handler. Slot 7 dispatches it with the sender's command in
/// `arg2`, and only 2 concerns this room.
///
/// With progress nibble 0xB8 set the room's event task is spawned: this stages
/// a `RoomDeparture` for it, hands the code in `area` to the room's resolver
/// for one last say over `room`, publishes the descriptor to
/// `gRoomDeparture` and spawns the task from
/// `D_dryfield_night_water_hole_801805EC`. The code staged is 0x2E, past the end
/// of the resolver's jump table, so the byte comes back as it went in.
///
/// Without it the event never ran: cap command 2 is armed, nibble 0x1BD records
/// it, and the sound is enqueued here instead of by the spawned task.
s32 func_dryfield_night_water_hole_8017DC28(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    RoomDeparture work;
    DnwhUtilParam param;

    if (arg2 == 2) {
        if (GameFlag_GetNibble(0xB8) != 0) {
            RoomDeparture* wp;
            s32            (*resolve)(DnwhUtilParam*, DnwhUtilParam*) =
                (s32 (*)(DnwhUtilParam*, DnwhUtilParam*))roomVariantResolveShelter;

            work.stage    = 4;
            work.area     = 0x2E;
            work.room     = 1;
            work.warp     = 3;
            work.sndEvent = 0x53200007;
            work.facing   = 0xC00;
            Gp_MsgPlayerWeapon(0);
            wp            = &work;
            param.field_0 = wp->area;
            param.field_2 = wp->warp;
            param.field_3 = wp->room;
            param.field_5 = 0;
            resolve(&param, &param);
            wp->area       = param.field_0;
            wp->warp       = param.field_2;
            wp->room       = param.field_3;
            gRoomDeparture = work;
            Task_SpawnFromTable(&D_dryfield_night_water_hole_801805EC, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(2);
            GameFlag_SetNibble(0x1BD, 2);
            SndEvt_EnqueueType6(0x53200004, 0, 0);
        }
    }
    return 0;
}

/// Message 0x13EF handler. On a visit through sub-id 1 with progress nibble
/// 0x95 still clear, a record whose `field_2` is 2 or 1 latches the nibble and
/// passes `D_dryfield_night_water_hole_8018067C` or
/// `D_dryfield_night_water_hole_801807FC` respectively to `func_800E8614`.
/// Always returns 0.
s32 func_dryfield_night_water_hole_8017DD5C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp_s0;

    if ((in->warp == 2) && (GameFlag_GetNibble(0x95) == 0) && (gGameSession->location.loc.variant == 1)) {
        GameFlag_SetNibble(0x95, 1);
        func_800E8614(D_dryfield_night_water_hole_8018067C, 0);
    }
    temp_s0 = in->warp;
    if ((temp_s0 == 1) && (GameFlag_GetNibble(0x95) == 0) && (gGameSession->location.loc.variant == temp_s0)) {
        GameFlag_SetNibble(0x95, 1);
        func_800E8614(D_dryfield_night_water_hole_801807FC, 0);
    }
    return 0;
}

/// The room task's idle state, entry 1 of its three-state table: does nothing.
/// The 0x10-byte local is never used, but the original reserved the frame.
static void func_dryfield_night_water_hole_8017DE20(Task* task)
{
    char pad[0x10];
}

/// The room task: copies the three-state table
/// `D_dryfield_night_water_hole_8017D688` onto the stack and runs the entry for
/// the task's current state - the entry tick, the idle state, then `taskKill`.
void func_dryfield_night_water_hole_8017DE30(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_water_hole_8017D688;
    sp.funcs[task->state](task);
}

/// Applies the override list `func_dryfield_night_water_hole_8017D958` holds:
/// each entry replaces a surface-class record and its cached `suppressPushback`
/// flag in `Gp_RoomParams`. The list ends at the first NULL record.
static void func_dryfield_night_water_hole_8017DE88(DnwhParamOverride* list)
{
    GameLocationKey*                  sess;
    s32                               i;
    WorldCollisionSurfaceProperties** surfaceProperties;

    sess = &gGameSession->location.loc;
    for (i = 0; list[i].rec != 0; i++) {
        surfaceProperties                = Gp_RoomParamTables[sess->stage - 1][sess->area - 1];
        surfaceProperties[list[i].index] = list[i].rec;
        Gp_RoomParams[list[i].index]     = surfaceProperties[list[i].index]->suppressPushback;
    }
}

/// Draws each surface in `D_dryfield_night_water_hole_80180970` as two strips
/// of 64 semi-transparent Gouraud quads laid side by side along Z, projected
/// through the view matrix. The seam between the strips is lifted by a sine
/// wave that runs along X and scrolls with
/// `D_dryfield_night_water_hole_8018362C[0]`, which only advances while
/// `gSceneCombatState.actorControl` is clear. The outer edges are coloured (0xFF, 0, 0) and the seam
/// (0x20, 0x20, 0x20); each quad is followed by a draw-mode packet selecting
/// blend mode 2. Quads the projection flags as invalid are skipped. `task` is
/// unused.
static void func_dryfield_night_water_hole_8017DF28(Task* task)
{
    SVECTOR      v0, v1, v2, v3;
    long         sxy0, sxy1, sxy2, sxy3;
    long         p, flag;
    s32          step;
    s32          phase;
    DnwhSurface* e;
    POLY_G4*     poly;
    DR_MODE*     dr;
    s32          otz;
    s32          i;
    s32          half;
    s32          wave;

    e = D_dryfield_night_water_hole_80180970;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        D_dryfield_night_water_hole_80183628 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_dryfield_night_water_hole_80183628 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        D_dryfield_night_water_hole_8018362C[0]++;
    }
    phase                      = -(D_dryfield_night_water_hole_8018362C[0] * 16);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    for (; e->y != -1; e++) {
        step = e->width / 64;
        half = (s16)e->depth / 2;
        for (i = 0; i < 64; i++) {
            v0.vx = e->x + step * i;
            v0.vy = e->y;
            v0.vz = e->z;
            v1.vx = e->x + step * (i + 1);
            v1.vy = e->y;
            v1.vz = e->z;
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v2.vx = e->x + step * i;
            v2.vy = e->y + wave;
            v2.vz = e->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v3.vx = e->x + step * (i + 1);
            v3.vy = e->y + wave;
            v3.vz = e->z + half;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                                 = (POLY_G4*)D_dryfield_night_water_hole_80183628;
                D_dryfield_night_water_hole_80183628 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0xFF;
                poly->r1                       = 0xFF;
                poly->g0                       = 0;
                poly->b0                       = 0;
                poly->g1                       = 0;
                poly->b1                       = 0;
                poly->r2                       = 0x20;
                poly->g2                       = 0x20;
                poly->b2                       = 0x20;
                poly->r3                       = 0x20;
                poly->g3                       = 0x20;
                poly->b3                       = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                                   = (DR_MODE*)D_dryfield_night_water_hole_80183628;
                D_dryfield_night_water_hole_80183628 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
        for (i = 0; i < 64; i++) {
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v0.vx = e->x + step * i;
            v0.vy = e->y + wave;
            v0.vz = e->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v1.vx = e->x + step * (i + 1);
            v1.vy = e->y + wave;
            v1.vz = e->z + half;
            v2.vx = e->x + step * i;
            v2.vy = e->y;
            v2.vz = e->z + half * 2;
            v3.vx = e->x + step * (i + 1);
            v3.vy = e->y;
            v3.vz = e->z + half * 2;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                                 = (POLY_G4*)D_dryfield_night_water_hole_80183628;
                D_dryfield_night_water_hole_80183628 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r2                       = 0xFF;
                poly->r3                       = 0xFF;
                poly->g2                       = 0;
                poly->b2                       = 0;
                poly->g3                       = 0;
                poly->b3                       = 0;
                poly->r0                       = 0x20;
                poly->g0                       = 0x20;
                poly->b0                       = 0x20;
                poly->r1                       = 0x20;
                poly->g1                       = 0x20;
                poly->b1                       = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                                   = (DR_MODE*)D_dryfield_night_water_hole_80183628;
                D_dryfield_night_water_hole_80183628 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
}

/// The room's water task: runs its current state -
/// `func_dryfield_night_water_hole_8017E690` once, then
/// `func_dryfield_night_water_hole_8017DF28`, which draws the surfaces - and
/// each tick sets the session's water height to -0x1A4.
void func_dryfield_night_water_hole_8017E630(Task* task)
{
    TaskFunc states[2] = { func_dryfield_night_water_hole_8017E690, func_dryfield_night_water_hole_8017DF28 };

    states[task->state](task);
    gGameSession->waterY = -0x1A4;
}

/// The water task's first state: clears the session halfword `field_80`, or
/// `field_7E` while `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` is set, then advances to the drawing state.
static void func_dryfield_night_water_hole_8017E690(Task* arg0)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Room task. State 0 installs effect ids 0x600FF / 0x6011F in the two shared
/// effect-id slots while progress nibble 0xB8 is clear, records the world
/// positions of parts 14 and 17 of the slot-3 task's model, and advances.
/// State 1, while nibble 0xB8 is clear, no event is running and `waterY` is
/// below that model's root, spawns each effect at water level under each part
/// with odds that grow with how far the part moved since last frame, then, once
/// game-flag nibble 0x51 is 1, draws the glowing beams
/// `glowDrawShaft` renders between the point pairs
/// the current view selects.
void func_dryfield_night_water_hole_8017E6D0(Task* arg0)
{
    Task*                          ctl;
    s32                            mask;
    _DryfieldNightWaterHoleSplash* splash;
    GfxCoord*                      ctlCoords;
    GfxCoord*                      part;
    GfxCoord*                      view;
    GfxCoord                       surface;
    s32                            i;
    u32                            rnd;

    ctl       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    splash    = arg0->spawnArg2.pointer;
    mask      = 1 << gGameSession->location.loc.view;
    ctlCoords = ctl->extra.tmd->coords;
    switch (arg0->state) {
        case 0:
            if (GameFlag_GetNibble(0xB8) == 0) {
                D_8011574C = 0x600FF;
                D_80115738 = 0x6011F;
            }
            arg0->state = 1;
            for (i = 0; i < 2; i++) {
                part                                       = &ctl->extra.tmd->coords[14 + i * 3];
                D_dryfield_night_water_hole_801809F4[i].vx = part->workm.t[0];
                D_dryfield_night_water_hole_801809F4[i].vy = part->workm.t[1];
                D_dryfield_night_water_hole_801809F4[i].vz = part->workm.t[2];
            }
            break;
        case 1:
            if (GameFlag_GetNibble(0xB8) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING &&
                gGameSession->waterY < ctlCoords->coord.t[1]) {
                view = &gGfxViewCoord;
                for (i = 0; i < 2; i++) {
                    part = &ctl->extra.tmd->coords[14 + i * 3];
                    Gp_UpdateCoord(part);
                    splash->strength = ABS(D_dryfield_night_water_hole_801809F4[i].vx - part->workm.t[0]) +
                                       ABS(D_dryfield_night_water_hole_801809F4[i].vy - part->workm.t[1]) +
                                       ABS(D_dryfield_night_water_hole_801809F4[i].vz - part->workm.t[2]) + 0x20;
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &part->workm, &surface.coord);
                    surface.parent       = view;
                    surface.coord.t[1]   = gGameSession->waterY;
                    surface.composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(&surface);
                    rnd = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT);
                    if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                        Gp_SpawnEff(D_8011574C, &surface, 0x40, 0);
                    }
                    splash->strength -= 0x20;
                    rnd               = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT);
                    if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                        Gp_SpawnEff(D_80115738, &surface, 0x1202180, 0);
                    }
                    D_dryfield_night_water_hole_801809F4[i].vx = part->workm.t[0];
                    D_dryfield_night_water_hole_801809F4[i].vy = part->workm.t[1];
                    D_dryfield_night_water_hole_801809F4[i].vz = part->workm.t[2];
                }
            }
            if (GameFlag_GetNibble(0x51) == 1) {
                if (mask & 0x18) {
                    glowDrawShaft(&D_dryfield_night_water_hole_80180994[0], 0x100);
                    glowDrawShaft(&D_dryfield_night_water_hole_80180994[2], 0x100);
                }
                if (mask & 0xA50) {
                    glowDrawShaft(&D_dryfield_night_water_hole_801809B4[0], 0x100);
                    glowDrawShaft(&D_dryfield_night_water_hole_801809B4[2], 0x100);
                }
                if (mask & 0x80) {
                    glowDrawShaft(&D_dryfield_night_water_hole_801809D4[0], 0x100);
                    glowDrawShaft(&D_dryfield_night_water_hole_801809D4[2], 0x100);
                }
            }
            break;
    }
}

#include "../../shared/glow_draw_shaft.inc.c"

#include "../../shared/water_ripple_task.inc.c"

void func_dryfield_night_water_hole_8017F254(Task* task)
{
    waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16.inc.c"

void func_dryfield_night_water_hole_8017F6DC(Task* task)
{
    waterDriftTaskU16(task);
}

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"
